#include "loudness_normalizer_processor.h"

#include <nlohmann/json.hpp>

#include <algorithm>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)  // Deprecated JUCE legacy parameter API; see header.
#endif

namespace jyglobalvst::engine {

namespace {
// ~250 ms mean-square window and ~200 ms gain smoothing at any sample rate.
constexpr float kRmsWindowSec = 0.25f;
constexpr float kGainSmoothSec = 0.2f;
constexpr float kEpsilon = 1.0e-6f;
}  // namespace

LoudnessNormalizerProcessor::LoudnessNormalizerProcessor()
    : target_db_(builtin::loudness_normalizer::TARGET_DEFAULT_DB),
      max_gain_db_(builtin::loudness_normalizer::MAX_GAIN_DEFAULT_DB)
{}

LoudnessNormalizerProcessor::~LoudnessNormalizerProcessor() = default;

void LoudnessNormalizerProcessor::prepareToPlay(double sample_rate, int /*samples_per_block*/)
{
    sample_rate_ = sample_rate;
    rms_state_ = 0.0f;
    applied_gain_ = 1.0f;
}

void LoudnessNormalizerProcessor::releaseResources() {}

void LoudnessNormalizerProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int num_samples = buffer.getNumSamples();
    const int num_channels = buffer.getNumChannels();
    if (num_channels < 1 || num_samples == 0)
    {
        return;
    }

    const float target_db = target_db_.load(std::memory_order_relaxed);
    const float max_gain_db = max_gain_db_.load(std::memory_order_relaxed);
    const float target_lin = std::pow(10.0f, target_db / 20.0f);
    const float gain_limit_lin = std::pow(10.0f, max_gain_db / 20.0f);

    const float sr = static_cast<float>(sample_rate_);
    const float rms_coeff = std::exp(-1.0f / (kRmsWindowSec * sr));
    const float gain_coeff = std::exp(-1.0f / (kGainSmoothSec * sr));

    for (int i = 0; i < num_samples; ++i)
    {
        // Linked-stereo mean-square estimate of the program level.
        float x2 = 0.0f;
        for (int ch = 0; ch < num_channels; ++ch)
        {
            const float x = buffer.getReadPointer(ch)[i];
            x2 += x * x;
        }
        x2 /= static_cast<float>(num_channels);

        rms_state_ = rms_coeff * rms_state_ + (1.0f - rms_coeff) * x2;
        const float rms = std::sqrt(rms_state_);

        // Desired gain toward the target, bounded boost and cut.
        float desired = target_lin / std::max(rms, kEpsilon);
        desired = std::max(1.0f / gain_limit_lin, std::min(gain_limit_lin, desired));

        // Smooth applied gain to avoid zipper noise and pumping.
        applied_gain_ = gain_coeff * applied_gain_ + (1.0f - gain_coeff) * desired;

        for (int ch = 0; ch < num_channels; ++ch)
        {
            buffer.getWritePointer(ch)[i] *= applied_gain_;
        }
    }
}

float LoudnessNormalizerProcessor::getParameter(int index)
{
    namespace p = builtin::loudness_normalizer;
    switch (index)
    {
    case p::PARAM_TARGET:
        return (getTargetDb() - p::TARGET_MIN_DB) / (p::TARGET_MAX_DB - p::TARGET_MIN_DB);
    case p::PARAM_MAX_GAIN:
        return (getMaxGainDb() - p::MAX_GAIN_MIN_DB) / (p::MAX_GAIN_MAX_DB - p::MAX_GAIN_MIN_DB);
    default:
        return 0.0f;
    }
}

void LoudnessNormalizerProcessor::setParameter(int index, float newValue)
{
    namespace p = builtin::loudness_normalizer;
    const float v = std::max(0.0f, std::min(1.0f, newValue));
    switch (index)
    {
    case p::PARAM_TARGET:
        setTargetDb(p::TARGET_MIN_DB + v * (p::TARGET_MAX_DB - p::TARGET_MIN_DB));
        break;
    case p::PARAM_MAX_GAIN:
        setMaxGainDb(p::MAX_GAIN_MIN_DB + v * (p::MAX_GAIN_MAX_DB - p::MAX_GAIN_MIN_DB));
        break;
    default:
        break;
    }
}

const juce::String LoudnessNormalizerProcessor::getParameterName(int index)
{
    switch (index)
    {
    case builtin::loudness_normalizer::PARAM_TARGET:
        return "Target";
    case builtin::loudness_normalizer::PARAM_MAX_GAIN:
        return "Max Gain";
    default:
        return {};
    }
}

const juce::String LoudnessNormalizerProcessor::getParameterText(int index)
{
    switch (index)
    {
    case builtin::loudness_normalizer::PARAM_TARGET:
        return juce::String(getTargetDb(), 1) + " dB";
    case builtin::loudness_normalizer::PARAM_MAX_GAIN:
        return juce::String(getMaxGainDb(), 1) + " dB";
    default:
        return {};
    }
}

void LoudnessNormalizerProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    nlohmann::json doc;
    doc["target_db"] = getTargetDb();
    doc["max_gain_db"] = getMaxGainDb();
    const std::string s = doc.dump();
    dest.append(s.data(), s.size());
}

void LoudnessNormalizerProcessor::setStateInformation(const void* data, int size)
{
    try
    {
        const std::string s(static_cast<const char*>(data), static_cast<size_t>(size));
        auto doc = nlohmann::json::parse(s);
        if (doc.contains("target_db"))
            setTargetDb(doc["target_db"].get<float>());
        if (doc.contains("max_gain_db"))
            setMaxGainDb(doc["max_gain_db"].get<float>());
    }
    catch (const std::exception&)
    {
        // Ignore corrupt state.
    }
}

juce::AudioProcessorEditor* LoudnessNormalizerProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void LoudnessNormalizerProcessor::fillInPluginDescription(juce::PluginDescription& description) const
{
    description.name = "Loudness Normalizer";
    description.descriptiveName = "Loudness Normalizer";
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
