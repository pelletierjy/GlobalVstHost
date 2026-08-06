#include "compressor_processor.h"
#include "compressor_editor.h"
#include "builtin_ids.h"
#include <cmath>
#include <nlohmann/json.hpp>

namespace jyglobalvst::engine {

CompressorProcessor::CompressorProcessor()
    : threshold_db_(builtin::compressor::THRESHOLD_DEFAULT_DB)
    , ratio_(builtin::compressor::RATIO_DEFAULT)
    , attack_ms_(builtin::compressor::ATTACK_DEFAULT_MS)
    , release_ms_(builtin::compressor::RELEASE_DEFAULT_MS)
    , makeup_db_(builtin::compressor::MAKEUP_DEFAULT_DB)
{
}

CompressorProcessor::~CompressorProcessor() = default;

void CompressorProcessor::prepareToPlay(double sample_rate, int /*samples_per_block*/)
{
    sample_rate_ = sample_rate;
    envelope_ = 0.0f;
    updateCoefficients();
}

void CompressorProcessor::releaseResources()
{
}

void CompressorProcessor::updateCoefficients()
{
    const float attack_ms = attack_ms_.load(std::memory_order_relaxed);
    const float release_ms = release_ms_.load(std::memory_order_relaxed);
    attack_coeff_ = std::exp(-1.0f / (attack_ms * static_cast<float>(sample_rate_) / 1000.0f));
    release_coeff_ = std::exp(-1.0f / (release_ms * static_cast<float>(sample_rate_) / 1000.0f));
}

void CompressorProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    auto num_samples = buffer.getNumSamples();
    auto num_channels = buffer.getNumChannels();

    if (num_channels < 2 || num_samples == 0)
        return;

    updateCoefficients();
    applyCompression(buffer.getWritePointer(0), buffer.getWritePointer(1), num_samples);
}

void CompressorProcessor::applyCompression(float* left, float* right, int num_samples) noexcept
{
    const float threshold_db = threshold_db_.load(std::memory_order_relaxed);
    const float ratio = ratio_.load(std::memory_order_relaxed);
    const float makeup_db = makeup_db_.load(std::memory_order_relaxed);

    const float threshold_linear = std::pow(10.0f, threshold_db / 20.0f);
    const float makeup_linear = std::pow(10.0f, makeup_db / 20.0f);

    for (int i = 0; i < num_samples; ++i)
    {
        const float in_l = left[i];
        const float in_r = right[i];

        // Side-chain envelope from max of left/right (linked stereo).
        const float input_peak = std::max(std::abs(in_l), std::abs(in_r));

        // Smooth the envelope.
        if (input_peak > envelope_)
            envelope_ = attack_coeff_ * envelope_ + (1.0f - attack_coeff_) * input_peak;
        else
            envelope_ = release_coeff_ * envelope_ + (1.0f - release_coeff_) * input_peak;

        // Compute gain reduction in dB.
        float gain_db = 0.0f;
        if (envelope_ > threshold_linear)
        {
            const float envelope_db = 20.0f * std::log10(envelope_);
            gain_db = (envelope_db - threshold_db) * (1.0f - 1.0f / ratio);
            gain_db = -gain_db;
        }

        const float gain_linear = std::pow(10.0f, gain_db / 20.0f) * makeup_linear;

        left[i] = in_l * gain_linear;
        right[i] = in_r * gain_linear;
    }
}

void CompressorProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    nlohmann::json doc;
    doc["threshold_db"] = getThresholdDb();
    doc["ratio"] = getRatio();
    doc["attack_ms"] = getAttackMs();
    doc["release_ms"] = getReleaseMs();
    doc["makeup_db"] = getMakeupDb();

    const std::string s = doc.dump();
    dest.append(s.data(), s.size());
}

void CompressorProcessor::setStateInformation(const void* data, int size)
{
    try
    {
        const std::string s(static_cast<const char*>(data), static_cast<size_t>(size));
        auto doc = nlohmann::json::parse(s);
        if (doc.contains("threshold_db"))
            setThresholdDb(doc["threshold_db"].get<float>());
        if (doc.contains("ratio"))
            setRatio(doc["ratio"].get<float>());
        if (doc.contains("attack_ms"))
            setAttackMs(doc["attack_ms"].get<float>());
        if (doc.contains("release_ms"))
            setReleaseMs(doc["release_ms"].get<float>());
        if (doc.contains("makeup_db"))
            setMakeupDb(doc["makeup_db"].get<float>());
    }
    catch (const std::exception&)
    {
        // Ignore corrupt state.
    }
}

juce::AudioProcessorEditor* CompressorProcessor::createEditor()
{
    return new CompressorEditor(*this);
}

void CompressorProcessor::fillInPluginDescription(juce::PluginDescription& description) const
{
    description.name = "Compressor";
    description.descriptiveName = "Compressor";
    description.pluginFormatName = "Built-in";
    description.category = "Fx";
    description.manufacturerName = "JyGlobalVST";
    description.version = "1.0.0";
    description.isInstrument = false;
    description.numInputChannels = 2;
    description.numOutputChannels = 2;
}

}  // namespace jyglobalvst::engine
