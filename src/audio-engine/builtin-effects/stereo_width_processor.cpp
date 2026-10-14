#include "stereo_width_processor.h"

#include <nlohmann/json.hpp>

#include <algorithm>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)  // Deprecated JUCE legacy parameter API; see header.
#endif

namespace jyglobalvst::engine {

void StereoWidthProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int num_samples = buffer.getNumSamples();
    const int num_channels = buffer.getNumChannels();
    if (num_channels < 2 || num_samples == 0)
    {
        return;
    }

    const float width = width_.load(std::memory_order_relaxed);
    if (width == 1.0f)
    {
        return;  // Unchanged.
    }

    float* left = buffer.getWritePointer(0);
    float* right = buffer.getWritePointer(1);
    for (int i = 0; i < num_samples; ++i)
    {
        const float mid = 0.5f * (left[i] + right[i]);
        const float side = 0.5f * (left[i] - right[i]) * width;
        left[i] = mid + side;
        right[i] = mid - side;
    }
}

float StereoWidthProcessor::getParameter(int index)
{
    if (index == builtin::stereo_width::PARAM_WIDTH)
    {
        return (getWidth() - builtin::stereo_width::WIDTH_MIN) /
               (builtin::stereo_width::WIDTH_MAX - builtin::stereo_width::WIDTH_MIN);
    }
    return 0.0f;
}

void StereoWidthProcessor::setParameter(int index, float newValue)
{
    if (index == builtin::stereo_width::PARAM_WIDTH)
    {
        const float v = std::max(0.0f, std::min(1.0f, newValue));
        setWidth(builtin::stereo_width::WIDTH_MIN +
                 v * (builtin::stereo_width::WIDTH_MAX - builtin::stereo_width::WIDTH_MIN));
    }
}

const juce::String StereoWidthProcessor::getParameterName(int index)
{
    return (index == builtin::stereo_width::PARAM_WIDTH) ? juce::String("Width") : juce::String();
}

const juce::String StereoWidthProcessor::getParameterText(int index)
{
    return (index == builtin::stereo_width::PARAM_WIDTH) ? juce::String(getWidth(), 2) : juce::String();
}

void StereoWidthProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    nlohmann::json doc;
    doc["width"] = getWidth();
    const std::string s = doc.dump();
    dest.append(s.data(), s.size());
}

void StereoWidthProcessor::setStateInformation(const void* data, int size)
{
    try
    {
        const std::string s(static_cast<const char*>(data), static_cast<size_t>(size));
        auto doc = nlohmann::json::parse(s);
        if (doc.contains("width"))
            setWidth(doc["width"].get<float>());
    }
    catch (const std::exception&)
    {
        // Ignore corrupt state.
    }
}

juce::AudioProcessorEditor* StereoWidthProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void StereoWidthProcessor::fillInPluginDescription(juce::PluginDescription& description) const
{
    description.name = "Stereo Width";
    description.descriptiveName = "Stereo Width";
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
