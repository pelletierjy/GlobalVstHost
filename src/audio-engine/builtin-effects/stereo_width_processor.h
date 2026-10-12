#pragma once

#include "builtin_ids.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <cmath>

namespace jyglobalvst::engine {

// Mid/side stereo width control: 0.0 = mono, 1.0 = unchanged, 2.0 = wide.
// Audio thread touches only the width atomic and the buffers (§V).
class StereoWidthProcessor : public juce::AudioPluginInstance
{
public:
    StereoWidthProcessor() : width_(builtin::stereo_width::WIDTH_DEFAULT) {}

    ~StereoWidthProcessor() override = default;

    void prepareToPlay(double, int) override {}

    void releaseResources() override {}

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    double getTailLengthSeconds() const override
    {
        return 0.0;
    }

    bool acceptsMidi() const override
    {
        return false;
    }

    bool producesMidi() const override
    {
        return false;
    }

    bool isMidiEffect() const override
    {
        return false;
    }

    const juce::String getName() const override
    {
        return "Stereo Width";
    }

    int getNumPrograms() override
    {
        return 1;
    }

    int getCurrentProgram() override
    {
        return 0;
    }

    void setCurrentProgram(int) override {}

    const juce::String getProgramName(int) override
    {
        return "Default";
    }

    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& dest) override;
    void setStateInformation(const void* data, int size) override;

    bool hasEditor() const override
    {
        return true;
    }

    juce::AudioProcessorEditor* createEditor() override;

    void fillInPluginDescription(juce::PluginDescription& description) const override;

    // Legacy parameter API (drives GenericAudioProcessorEditor and the
    // PluginInstance parameter bridge); values are normalized 0..1.
    // Deprecated in JUCE but kept as the bridge used by PluginInstance
    // (see plugin_instance.cpp — same 4996 suppression).
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
    static constexpr int kNumParams = builtin::stereo_width::NUM_PARAMETERS;

    int getNumParameters() override
    {
        return kNumParams;
    }

    float getParameter(int index) override;
    void setParameter(int index, float newValue) override;
    const juce::String getParameterName(int index) override;
    const juce::String getParameterText(int index) override;
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

    float getWidth() const
    {
        return width_.load(std::memory_order_relaxed);
    }

    void setWidth(float w)
    {
        width_.store(std::max(builtin::stereo_width::WIDTH_MIN, std::min(builtin::stereo_width::WIDTH_MAX, w)),
                     std::memory_order_relaxed);
    }

private:
    std::atomic<float> width_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoWidthProcessor)
};

}  // namespace jyglobalvst::engine
