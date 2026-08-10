#include "eq_processor.h"
#include "eq_editor.h"
#include "builtin_ids.h"
#include <cmath>
#include <nlohmann/json.hpp>

namespace jyglobalvst::engine {

constexpr double kPi = 3.14159265358979323846;

float EqProcessor::Biquad::processL(float in) noexcept
{
    float out = b0 * in + z1_l;
    z1_l = b1 * in - a1 * out + z2_l;
    z2_l = b2 * in - a2 * out;
    return out;
}

float EqProcessor::Biquad::processR(float in) noexcept
{
    float out = b0 * in + z1_r;
    z1_r = b1 * in - a1 * out + z2_r;
    z2_r = b2 * in - a2 * out;
    return out;
}

EqProcessor::EqProcessor()
{
    band_gains_.fill(0.f);
}

EqProcessor::~EqProcessor() = default;

void EqProcessor::prepareToPlay(double sample_rate, int)
{
    sample_rate_ = sample_rate;
    for (auto& ch : band_filters_)
        for (auto& biquad : ch)
            biquad.reset();
    for (auto& ch : band_analysis_filters_)
        for (auto& biquad : ch)
            biquad.reset();
    for (auto& biquad : bass_shelf_)
        biquad.reset();
    updateFilterCoefficients();
}

void EqProcessor::releaseResources()
{
}

void EqProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    auto num_samples = buffer.getNumSamples();
    auto num_channels = buffer.getNumChannels();

    if (num_channels < 2)
        return;

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);

    std::array<float, kNumBands> block_peak{};

    for (int i = 0; i < num_samples; ++i)
    {
        float l = left[i] * input_volume_linear_;
        float r = right[i] * input_volume_linear_;

        const float dry_l = l;
        const float dry_r = r;

        for (int b = 0; b < kNumBands; ++b)
        {
            // Analysis-only bandpass, run on the dry signal, feeding the band meter.
            float band_l = band_analysis_filters_[0][b].processL(dry_l);
            float band_r = band_analysis_filters_[1][b].processR(dry_r);
            float mag = std::max(std::abs(band_l), std::abs(band_r));
            if (mag > block_peak[b])
                block_peak[b] = mag;

            l = band_filters_[0][b].processL(l);
            r = band_filters_[1][b].processR(r);
        }

        l = bass_shelf_[0].processL(l);
        r = bass_shelf_[1].processR(r);

        left[i] = std::min(1.f, std::max(-1.f, l));
        right[i] = std::min(1.f, std::max(-1.f, r));
    }

    for (int b = 0; b < kNumBands; ++b)
        band_levels_[b].store(block_peak[b], std::memory_order_relaxed);
}

void EqProcessor::updateFilterCoefficients()
{
    const float q = 1.414f;  // Butterworth Q

    for (int b = 0; b < kNumBands; ++b)
    {
        float center_hz = builtin::eq::BAND_CENTERS_HZ[b];
        float gain_db = band_gains_[b];
        for (int ch = 0; ch < 2; ++ch)
        {
            computeBiquadCoefficients(center_hz, gain_db, q, band_filters_[ch][b]);
            computeBandpassCoefficients(center_hz, q, band_analysis_filters_[ch][b]);
        }
    }

    for (int ch = 0; ch < 2; ++ch)
        computeBiquadCoefficients(200.f, bass_boost_db_, 0.707f, bass_shelf_[ch]);
}

void EqProcessor::computeBiquadCoefficients(float center_hz, float gain_db, float q, Biquad& biquad)
{
    if (std::abs(gain_db) < 0.01f)
    {
        biquad.b0 = 1.f; biquad.b1 = 0.f; biquad.b2 = 0.f;
        biquad.a1 = 0.f; biquad.a2 = 0.f;
        return;
    }

    double w0 = 2.0 * kPi * center_hz / sample_rate_;
    double sin_w0 = std::sin(w0);
    double cos_w0 = std::cos(w0);
    double alpha = sin_w0 / (2.0 * q);
    double a = std::pow(10.0, gain_db / 40.0);

    double b0 = 1.0 + alpha * a;
    double b1 = -2.0 * cos_w0;
    double b2 = 1.0 - alpha * a;
    double a0 = 1.0 + alpha / a;
    double a1 = -2.0 * cos_w0;
    double a2 = 1.0 - alpha / a;

    biquad.b0 = static_cast<float>(b0 / a0);
    biquad.b1 = static_cast<float>(b1 / a0);
    biquad.b2 = static_cast<float>(b2 / a0);
    biquad.a1 = static_cast<float>(a1 / a0);
    biquad.a2 = static_cast<float>(a2 / a0);
}

void EqProcessor::computeBandpassCoefficients(float center_hz, float q, Biquad& biquad)
{
    double w0 = 2.0 * kPi * center_hz / sample_rate_;
    double sin_w0 = std::sin(w0);
    double cos_w0 = std::cos(w0);
    double alpha = sin_w0 / (2.0 * q);

    double b0 = alpha;
    double b1 = 0.0;
    double b2 = -alpha;
    double a0 = 1.0 + alpha;
    double a1 = -2.0 * cos_w0;
    double a2 = 1.0 - alpha;

    biquad.b0 = static_cast<float>(b0 / a0);
    biquad.b1 = static_cast<float>(b1 / a0);
    biquad.b2 = static_cast<float>(b2 / a0);
    biquad.a1 = static_cast<float>(a1 / a0);
    biquad.a2 = static_cast<float>(a2 / a0);
}

float EqProcessor::getBandGain(int band_index) const
{
    if (band_index >= 0 && band_index < kNumBands)
        return band_gains_[band_index];
    return 0.f;
}

void EqProcessor::setBandGain(int band_index, float gain_db)
{
    if (band_index >= 0 && band_index < kNumBands)
    {
        band_gains_[band_index] = gain_db;
        updateFilterCoefficients();
    }
}

float EqProcessor::getBassBoost() const
{
    return bass_boost_db_;
}

void EqProcessor::setBassBoost(float gain_db)
{
    bass_boost_db_ = gain_db;
    updateFilterCoefficients();
}

float EqProcessor::getInputVolume() const
{
    return input_volume_db_;
}

void EqProcessor::setInputVolume(float gain_db)
{
    input_volume_db_ = gain_db;
    input_volume_linear_ = std::pow(10.f, gain_db / 20.f);
}

float EqProcessor::getBandLevel(int band_index) const
{
    if (band_index >= 0 && band_index < kNumBands)
        return band_levels_[band_index].load(std::memory_order_relaxed);
    return 0.f;
}

void EqProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    nlohmann::json state;
    state["v"] = 1;
    state["bands"] = nlohmann::json::array();
    for (int i = 0; i < kNumBands; ++i)
        state["bands"].push_back(band_gains_[i]);
    state["bass"] = bass_boost_db_;
    state["input_volume"] = input_volume_db_;

    std::string json_str = state.dump();
    dest.setSize(json_str.size());
    std::memcpy(dest.getData(), json_str.c_str(), json_str.size());
}

void EqProcessor::setStateInformation(const void* data, int size)
{
    try
    {
        std::string json_str(static_cast<const char*>(data), size);
        auto state = nlohmann::json::parse(json_str);

        if (state.contains("bands") && state["bands"].is_array())
        {
            auto& bands = state["bands"];
            for (int i = 0; i < std::min((int)bands.size(), kNumBands); ++i)
                band_gains_[i] = bands[i].get<float>();
        }

        if (state.contains("bass"))
            bass_boost_db_ = state["bass"].get<float>();

        if (state.contains("input_volume"))
            setInputVolume(state["input_volume"].get<float>());

        updateFilterCoefficients();
    }
    catch (...)
    {
    }
}

void EqProcessor::fillInPluginDescription(juce::PluginDescription& description) const
{
    description.name = "Equalizer";
    description.pluginFormatName = "Built-in";
    description.category = "Fx";
    description.manufacturerName = "JyGlobalVST";
    description.version = "1.0.0";
    description.fileOrIdentifier = "";
    description.isInstrument = false;
    description.numInputChannels = 2;
    description.numOutputChannels = 2;
}

juce::AudioProcessorEditor* EqProcessor::createEditor()
{
    return new EqEditor(*this);
}

}  // namespace jyglobalvst::engine
