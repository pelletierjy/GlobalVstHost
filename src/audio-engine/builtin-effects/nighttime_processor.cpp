#include "nighttime_processor.h"
#include "nighttime_editor.h"
#include "builtin_ids.h"
#include <cmath>
#include <nlohmann/json.hpp>

namespace jyglobalvst::engine {

NightTimeProcessor::NightTimeProcessor() : current_preset_index_(1), current_lookahead_ms_(10.0f)
{
}

NightTimeProcessor::~NightTimeProcessor() = default;

void NightTimeProcessor::prepareToPlay(double sample_rate, int samples_per_block)
{
    sample_rate_ = sample_rate;
    samples_per_block_ = samples_per_block;

    loudness_meter_.prepareToPlay(sample_rate, samples_per_block);

    max_lookahead_samples_ = static_cast<int>(sample_rate * 10.0 / 1000.0);
    lookahead_buffer_l_.assign(max_lookahead_samples_, 0.f);
    lookahead_buffer_r_.assign(max_lookahead_samples_, 0.f);
    lookahead_write_pos_ = 0;

    updateParameters();
}

void NightTimeProcessor::releaseResources()
{
    loudness_meter_.releaseResources();
    lookahead_buffer_l_.clear();
    lookahead_buffer_r_.clear();
}

void NightTimeProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    auto num_samples = buffer.getNumSamples();
    auto num_channels = buffer.getNumChannels();

    if (num_channels < 2)
        return;

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);

    updateParameters();
    loudness_meter_.process(left, right, num_samples);
    applyGain(left, right, num_samples);
    applyLimiter(left, right, num_samples);
}

void NightTimeProcessor::updateParameters()
{
    // Get look-ahead time from member
    latency_samples_ = static_cast<int>(current_lookahead_ms_ * sample_rate_ / 1000.0 + 0.5);
    lookahead_delay_samples_ = std::max(0, std::min(latency_samples_, max_lookahead_samples_ - 1));

    float attack_ms = 100.f;
    float release_ms = 300.f;
    attack_coeff_ = std::exp(-1.f / (attack_ms * static_cast<float>(sample_rate_) / 1000.f));
    release_coeff_ = std::exp(-1.f / (release_ms * static_cast<float>(sample_rate_) / 1000.f));

    limiter_attack_coeff_ = std::exp(-1.f / (1.f * static_cast<float>(sample_rate_) / 1000.f));
    limiter_release_coeff_ = std::exp(-1.f / (10.f * static_cast<float>(sample_rate_) / 1000.f));

    ceiling_linear_ = std::pow(10.f, -0.5f / 20.f);
}

NightTimeProcessor::PresetSettings NightTimeProcessor::getPresetSettings(int preset_index)
{
    // The trailing output_trim_db values level each preset back toward the
    // bypass level. They scale with the upward-gain amount (the main driver of
    // perceived loudness) and are starting estimates intended to be fine-tuned
    // by ear.
    switch (preset_index)
    {
        case 0:
            return {-18.f, 2.f, 300.f, 800.f, -0.5f, -1.5f};
        case 1:
            return {-23.f, 8.f, 80.f, 250.f, -0.5f, -4.5f};
        case 2:
            return {-28.f, 16.f, 30.f, 100.f, -0.5f, -8.5f};
        case 3:
            // Extreme: crushes almost all remaining dynamic range. Very high
            // target loudness with huge upward gain and near-instant attack/
            // release so quiet and loud passages sit at nearly the same level.
            return {-14.f, 36.f, 5.f, 40.f, -0.3f, -13.f};
        default:
            return {-23.f, 8.f, 80.f, 250.f, -0.5f, -4.5f};
    }
}

void NightTimeProcessor::applyGain(float* left, float* right, int num_samples) noexcept
{
    auto settings = getPresetSettings(current_preset_index_);
    float target_linear = std::pow(10.f, settings.target_loudness_db / 20.f);
    float max_gain_linear = std::pow(10.f, settings.max_upward_gain_db / 20.f);
    // Per-preset output attenuation applied after the AGC gain. Because the
    // loudness meter has already read the un-attenuated signal (in processBlock),
    // this trim only affects the final level, leaving the dynamics untouched.
    float output_trim_linear = std::pow(10.f, settings.output_trim_db / 20.f);

    for (int i = 0; i < num_samples; ++i)
    {
        float loudness_lufs = loudness_meter_.shortTermLoudnessLufs();
        if (std::isinf(loudness_lufs) || std::isnan(loudness_lufs))
            loudness_lufs = -100.f;

        float loudness_linear = std::pow(10.f, loudness_lufs / 20.f);
        float desired_gain = (loudness_linear > 0.f) ? target_linear / loudness_linear : 1.f;
        desired_gain = std::min(desired_gain, max_gain_linear);
        desired_gain = std::max(desired_gain, 0.1f);

        float coeff = (desired_gain > current_gain_linear_) ? attack_coeff_ : release_coeff_;
        current_gain_linear_ = coeff * current_gain_linear_ + (1.f - coeff) * desired_gain;

        float out_gain = current_gain_linear_ * output_trim_linear;
        left[i] *= out_gain;
        right[i] *= out_gain;
    }
}

void NightTimeProcessor::applyLimiter(float* left, float* right, int num_samples) noexcept
{
    const int buffer_size = max_lookahead_samples_;
    if (buffer_size <= 0)
        return;

    for (int i = 0; i < num_samples; ++i)
    {
        // Peak of the incoming (not-yet-output) sample drives the envelope, so
        // gain reduction has `lookahead_delay_samples_` samples to settle before
        // this sample is actually emitted below — this is what makes it a
        // look-ahead limiter instead of a same-instant envelope follower.
        float peak = std::abs(left[i]);
        peak = std::max(peak, std::abs(right[i]));

        float target_envelope = std::min(1.f, ceiling_linear_ / (peak + 1e-8f));
        envelope_ = limiter_attack_coeff_ * envelope_ + (1.f - limiter_attack_coeff_) * target_envelope;

        lookahead_buffer_l_[lookahead_write_pos_] = left[i];
        lookahead_buffer_r_[lookahead_write_pos_] = right[i];

        int read_pos = lookahead_write_pos_ - lookahead_delay_samples_;
        if (read_pos < 0)
            read_pos += buffer_size;

        left[i] = lookahead_buffer_l_[read_pos] * envelope_;
        right[i] = lookahead_buffer_r_[read_pos] * envelope_;

        lookahead_write_pos_ = lookahead_write_pos_ + 1;
        if (lookahead_write_pos_ >= buffer_size)
            lookahead_write_pos_ = 0;
    }
}

void NightTimeProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    nlohmann::json state;
    state["v"] = 1;
    state["preset"] = current_preset_index_;
    state["lookaheadMs"] = static_cast<double>(current_lookahead_ms_);

    std::string json_str = state.dump();
    dest.setSize(json_str.size());
    std::memcpy(dest.getData(), json_str.c_str(), json_str.size());
}

void NightTimeProcessor::setStateInformation(const void* data, int size)
{
    try
    {
        std::string json_str(static_cast<const char*>(data), size);
        auto state = nlohmann::json::parse(json_str);

        if (state.contains("preset"))
        {
            current_preset_index_ = state["preset"].get<int>();
        }

        // Look-ahead is fixed at 10 ms and no longer user-adjustable; any value
        // stored by older versions is intentionally ignored.
        current_lookahead_ms_ = 10.0f;
    }
    catch (...)
    {
    }
}

juce::AudioProcessorEditor* NightTimeProcessor::createEditor()
{
    return new NightTimeEditor(*this);
}

void NightTimeProcessor::fillInPluginDescription(juce::PluginDescription& description) const
{
    description.name = "Volume Leveler";
    description.pluginFormatName = "Built-in";
    description.category = "Fx";
    description.manufacturerName = "JyGlobalVST";
    description.version = "1.0.0";
    description.fileOrIdentifier = "";
    description.isInstrument = false;
    description.numInputChannels = 2;
    description.numOutputChannels = 2;
}

}  // namespace jyglobalvst::engine
