#pragma once

#include "builtin_ids.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <cmath>

namespace jyglobalvst::engine {

// RMS-based loudness normalizer (AGC): steers the program level toward a
// target RMS with bounded boost/cut and smoothed gain. Audio thread touches
// only atomics and preallocated state (Constitution §V).
class LoudnessNormalizerProcessor : public juce::AudioPluginInstance
{
public:
    LoudnessNormalizerProcessor();
    ~LoudnessNormalizerProcessor() override;

    void prepareToPlay(double sample_rate, int samples_per_block) override;
    void releaseResources() override;
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
        return "Loudness Normalizer";
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
    static constexpr int kNumParams = builtin::loudness_normalizer::NUM_PARAMETERS;

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

    float getTargetDb() const
    {
        return target_db_.load(std::memory_order_relaxed);
    }

    void setTargetDb(float db)
    {
        target_db_.store(std::max(builtin::loudness_normalizer::TARGET_MIN_DB,
                                  std::min(builtin::loudness_normalizer::TARGET_MAX_DB, db)),
                         std::memory_order_relaxed);
    }

    float getMaxGainDb() const
    {
        return max_gain_db_.load(std::memory_order_relaxed);
    }

    void setMaxGainDb(float db)
    {
        max_gain_db_.store(std::max(builtin::loudness_normalizer::MAX_GAIN_MIN_DB,
                                    std::min(builtin::loudness_normalizer::MAX_GAIN_MAX_DB, db)),
                           std::memory_order_relaxed);
    }

private:
    std::atomic<float> target_db_;
    std::atomic<float> max_gain_db_;

    double sample_rate_{48000.0};
    float rms_state_{0.0f};     // one-pole mean-square estimate
    float applied_gain_{1.0f};  // smoothed output gain

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoudnessNormalizerProcessor)
};

}  // namespace jyglobalvst::engine
