#pragma once

#include "builtin_ids.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <cmath>

namespace jyglobalvst::engine {

// Downward noise gate with linked-stereo peak detector, hold phase, and
// smoothed gain. Control-thread parameter access via atomics; the audio
// thread only loads atomics and touches preallocated state (Constitution §V).
class NoiseGateProcessor : public juce::AudioPluginInstance
{
public:
    NoiseGateProcessor();
    ~NoiseGateProcessor() override;

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
        return "Noise Gate";
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
    // The API is deprecated in JUCE but remains the bridge used by
    // PluginInstance (see plugin_instance.cpp — same 4996 suppression).
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
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

    static constexpr int kNumParams = builtin::noise_gate::NUM_PARAMETERS;

    float getThresholdDb() const
    {
        return threshold_db_.load(std::memory_order_relaxed);
    }

    void setThresholdDb(float db)
    {
        threshold_db_.store(
            std::max(builtin::noise_gate::THRESHOLD_MIN_DB, std::min(builtin::noise_gate::THRESHOLD_MAX_DB, db)),
            std::memory_order_relaxed);
    }

    float getAttackMs() const
    {
        return attack_ms_.load(std::memory_order_relaxed);
    }

    void setAttackMs(float ms)
    {
        attack_ms_.store(std::max(builtin::noise_gate::ATTACK_MIN_MS, std::min(builtin::noise_gate::ATTACK_MAX_MS, ms)),
                         std::memory_order_relaxed);
    }

    float getHoldMs() const
    {
        return hold_ms_.load(std::memory_order_relaxed);
    }

    void setHoldMs(float ms)
    {
        hold_ms_.store(std::max(builtin::noise_gate::HOLD_MIN_MS, std::min(builtin::noise_gate::HOLD_MAX_MS, ms)),
                       std::memory_order_relaxed);
    }

    float getReleaseMs() const
    {
        return release_ms_.load(std::memory_order_relaxed);
    }

    void setReleaseMs(float ms)
    {
        release_ms_.store(
            std::max(builtin::noise_gate::RELEASE_MIN_MS, std::min(builtin::noise_gate::RELEASE_MAX_MS, ms)),
            std::memory_order_relaxed);
    }

private:
    void applyGate(float* left, float* right, int num_samples) noexcept;

    std::atomic<float> threshold_db_;
    std::atomic<float> attack_ms_;
    std::atomic<float> hold_ms_;
    std::atomic<float> release_ms_;

    double sample_rate_{48000.0};
    float envelope_{0.0f};
    float gate_gain_{0.0f};
    int hold_remaining_{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoiseGateProcessor)
};

}  // namespace jyglobalvst::engine
