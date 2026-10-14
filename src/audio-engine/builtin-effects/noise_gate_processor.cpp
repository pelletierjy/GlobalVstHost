#include "noise_gate_processor.h"

#include <nlohmann/json.hpp>

#include <cmath>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)  // Deprecated JUCE legacy parameter API; see header.
#endif

namespace jyglobalvst::engine {

NoiseGateProcessor::NoiseGateProcessor()
    : threshold_db_(builtin::noise_gate::THRESHOLD_DEFAULT_DB), attack_ms_(builtin::noise_gate::ATTACK_DEFAULT_MS),
      hold_ms_(builtin::noise_gate::HOLD_DEFAULT_MS), release_ms_(builtin::noise_gate::RELEASE_DEFAULT_MS)
{}

NoiseGateProcessor::~NoiseGateProcessor() = default;

void NoiseGateProcessor::prepareToPlay(double sample_rate, int /*samples_per_block*/)
{
    sample_rate_ = sample_rate;
    envelope_ = 0.0f;
    gate_gain_ = 0.0f;
    hold_remaining_ = 0;
}

void NoiseGateProcessor::releaseResources() {}

void NoiseGateProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int num_samples = buffer.getNumSamples();
    const int num_channels = buffer.getNumChannels();
    if (num_channels < 1 || num_samples == 0)
    {
        return;
    }

    applyGate(buffer.getWritePointer(0), num_channels >= 2 ? buffer.getWritePointer(1) : nullptr, num_samples);
}

void NoiseGateProcessor::applyGate(float* left, float* right, int num_samples) noexcept
{
    const float threshold_db = threshold_db_.load(std::memory_order_relaxed);
    const float threshold_linear = std::pow(10.0f, threshold_db / 20.0f);

    const float attack_ms = attack_ms_.load(std::memory_order_relaxed);
    const float release_ms = release_ms_.load(std::memory_order_relaxed);
    const float hold_ms = hold_ms_.load(std::memory_order_relaxed);

    const float sr_khz = static_cast<float>(sample_rate_) / 1000.0f;
    const float attack_coeff = std::exp(-1.0f / (attack_ms * sr_khz));
    const float release_coeff = std::exp(-1.0f / (release_ms * sr_khz));
    const int hold_samples = static_cast<int>(hold_ms * sr_khz);

    for (int i = 0; i < num_samples; ++i)
    {
        const float in_l = left[i];
        const float in_r = (right != nullptr) ? right[i] : in_l;

        // Linked-stereo peak detector with attack/release smoothing.
        const float input_peak = std::max(std::abs(in_l), std::abs(in_r));
        if (input_peak > envelope_)
        {
            envelope_ = attack_coeff * envelope_ + (1.0f - attack_coeff) * input_peak;
        }
        else
        {
            envelope_ = release_coeff * envelope_ + (1.0f - release_coeff) * input_peak;
        }

        // Open while above threshold; keep open through the hold phase.
        const bool open = envelope_ > threshold_linear;
        if (open)
        {
            hold_remaining_ = hold_samples;
        }
        else if (hold_remaining_ > 0)
        {
            --hold_remaining_;
        }

        const float target_gain = (open || hold_remaining_ > 0) ? 1.0f : 0.0f;

        // Smooth gate gain: attack coefficient while opening, release while
        // closing — reuses the same time constants as the detector.
        const float coeff = (target_gain > gate_gain_) ? attack_coeff : release_coeff;
        gate_gain_ = coeff * gate_gain_ + (1.0f - coeff) * target_gain;

        left[i] = in_l * gate_gain_;
        if (right != nullptr)
        {
            right[i] = in_r * gate_gain_;
        }
    }
}

void NoiseGateProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    nlohmann::json doc;
    doc["threshold_db"] = getThresholdDb();
    doc["attack_ms"] = getAttackMs();
    doc["hold_ms"] = getHoldMs();
    doc["release_ms"] = getReleaseMs();

    const std::string s = doc.dump();
    dest.append(s.data(), s.size());
}

float NoiseGateProcessor::getParameter(int index)
{
    namespace p = builtin::noise_gate;
    switch (index)
    {
    case p::PARAM_THRESHOLD:
        return (getThresholdDb() - p::THRESHOLD_MIN_DB) / (p::THRESHOLD_MAX_DB - p::THRESHOLD_MIN_DB);
    case p::PARAM_ATTACK:
        return (getAttackMs() - p::ATTACK_MIN_MS) / (p::ATTACK_MAX_MS - p::ATTACK_MIN_MS);
    case p::PARAM_HOLD:
        return (getHoldMs() - p::HOLD_MIN_MS) / (p::HOLD_MAX_MS - p::HOLD_MIN_MS);
    case p::PARAM_RELEASE:
        return (getReleaseMs() - p::RELEASE_MIN_MS) / (p::RELEASE_MAX_MS - p::RELEASE_MIN_MS);
    default:
        return 0.0f;
    }
}

void NoiseGateProcessor::setParameter(int index, float newValue)
{
    namespace p = builtin::noise_gate;
    const float v = std::max(0.0f, std::min(1.0f, newValue));
    switch (index)
    {
    case p::PARAM_THRESHOLD:
        setThresholdDb(p::THRESHOLD_MIN_DB + v * (p::THRESHOLD_MAX_DB - p::THRESHOLD_MIN_DB));
        break;
    case p::PARAM_ATTACK:
        setAttackMs(p::ATTACK_MIN_MS + v * (p::ATTACK_MAX_MS - p::ATTACK_MIN_MS));
        break;
    case p::PARAM_HOLD:
        setHoldMs(p::HOLD_MIN_MS + v * (p::HOLD_MAX_MS - p::HOLD_MIN_MS));
        break;
    case p::PARAM_RELEASE:
        setReleaseMs(p::RELEASE_MIN_MS + v * (p::RELEASE_MAX_MS - p::RELEASE_MIN_MS));
        break;
    default:
        break;
    }
}

const juce::String NoiseGateProcessor::getParameterName(int index)
{
    switch (index)
    {
    case builtin::noise_gate::PARAM_THRESHOLD:
        return "Threshold";
    case builtin::noise_gate::PARAM_ATTACK:
        return "Attack";
    case builtin::noise_gate::PARAM_HOLD:
        return "Hold";
    case builtin::noise_gate::PARAM_RELEASE:
        return "Release";
    default:
        return {};
    }
}

const juce::String NoiseGateProcessor::getParameterText(int index)
{
    switch (index)
    {
    case builtin::noise_gate::PARAM_THRESHOLD:
        return juce::String(getThresholdDb(), 1) + " dB";
    case builtin::noise_gate::PARAM_ATTACK:
        return juce::String(getAttackMs(), 1) + " ms";
    case builtin::noise_gate::PARAM_HOLD:
        return juce::String(getHoldMs(), 0) + " ms";
    case builtin::noise_gate::PARAM_RELEASE:
        return juce::String(getReleaseMs(), 0) + " ms";
    default:
        return {};
    }
}

void NoiseGateProcessor::setStateInformation(const void* data, int size)
{
    try
    {
        const std::string s(static_cast<const char*>(data), static_cast<size_t>(size));
        auto doc = nlohmann::json::parse(s);
        if (doc.contains("threshold_db"))
            setThresholdDb(doc["threshold_db"].get<float>());
        if (doc.contains("attack_ms"))
            setAttackMs(doc["attack_ms"].get<float>());
        if (doc.contains("hold_ms"))
            setHoldMs(doc["hold_ms"].get<float>());
        if (doc.contains("release_ms"))
            setReleaseMs(doc["release_ms"].get<float>());
    }
    catch (const std::exception&)
    {
        // Ignore corrupt state.
    }
}

juce::AudioProcessorEditor* NoiseGateProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void NoiseGateProcessor::fillInPluginDescription(juce::PluginDescription& description) const
{
    description.name = "Noise Gate";
    description.descriptiveName = "Noise Gate";
    description.pluginFormatName = "Built-in";
    description.category = "Fx";
    description.manufacturerName = "JyGlobalVST";
    description.version = "1.0.0";
    description.isInstrument = false;
    description.numInputChannels = 2;
    description.numOutputChannels = 2;
}

}  // namespace jyglobalvst::engine

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
