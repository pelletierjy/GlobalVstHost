#pragma once

#include "builtin_ids.h"
#include "loudness_meter.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <nlohmann/json.hpp>
#include <vector>

// REALTIME CONSTRAINTS HEADER
// ============================================================================
// Volume Leveler: loudness-driven AGC + peak limiter. All DSP state preallocated
// in prepareToPlay. Parameter changes read atomically. No allocation, locking,
// or I/O in processBlock.
// ============================================================================

namespace jyglobalvst::engine {

class NightTimeProcessor : public juce::AudioPluginInstance
{
public:
    NightTimeProcessor();
    ~NightTimeProcessor() override;

    // --- AudioProcessor interface -----------------------------------------
    void prepareToPlay(double sample_rate, int samples_per_block) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

    const juce::String getName() const override { return "Volume Leveler"; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& dest) override;
    void setStateInformation(const void* data, int size) override;

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    void fillInPluginDescription(juce::PluginDescription& description) const override;

    int getLatencySamples() const { return latency_samples_; }
    void setLatencySamples(int latency) { latency_samples_ = latency; }

    // Parameters (data-model.md §3).
    static constexpr int kNumParams = builtin::nighttime::NUM_PARAMETERS;

    // Preset tuning.
    struct PresetSettings
    {
        float target_loudness_db = -23.f;
        float max_upward_gain_db = 6.f;
        float attack_ms = 100.f;
        float release_ms = 300.f;
        float ceiling_db = -0.5f;
        // Output attenuation (dB, <= 0) applied after the AGC/limiter so that each
        // preset sits at roughly the same overall level as bypass. Stronger presets
        // add more upward gain and therefore need more attenuation. This trim only
        // scales the final output — it is not fed back into the loudness meter, so
        // the dynamic manipulation is unchanged.
        float output_trim_db = 0.f;
    };

    static PresetSettings getPresetSettings(int preset_index);

    int getPresetIndex() const { return current_preset_index_; }
    void setPresetIndex(int index)
    {
        current_preset_index_ = std::max(0, std::min(3, index));
    }

    float getLookaheadMs() const { return current_lookahead_ms_; }
    void setLookaheadMs(float ms)
    {
        current_lookahead_ms_ = std::max(0.0f, std::min(10.0f, ms));
    }

private:
    struct Biquad
    {
        float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;
        float z1 = 0.f, z2 = 0.f;

        float process(float in) noexcept
        {
            float out = b0 * in + z1;
            z1 = b1 * in - a1 * out + z2;
            z2 = b2 * in - a2 * out;
            return out;
        }

        void reset() { z1 = z2 = 0.f; }
    };

    LoudnessMeter loudness_meter_;

    double sample_rate_ = 0.0;
    int samples_per_block_ = 0;
    int latency_samples_ = 0;

    int current_preset_index_ = 1;
    float current_lookahead_ms_ = 0.0f;

    // AGC state.
    float current_gain_linear_ = 1.f;
    float attack_coeff_ = 0.f;
    float release_coeff_ = 0.f;

    // Limiter state.
    std::vector<float> lookahead_buffer_l_;
    std::vector<float> lookahead_buffer_r_;
    int lookahead_write_pos_ = 0;
    int max_lookahead_samples_ = 0;
    int lookahead_delay_samples_ = 0;
    float envelope_ = 0.f;
    float limiter_attack_coeff_ = 0.f;
    float limiter_release_coeff_ = 0.f;
    float ceiling_linear_ = 0.99f;

    void updateParameters();
    void applyGain(float* left, float* right, int num_samples) noexcept;
    void applyLimiter(float* left, float* right, int num_samples) noexcept;
};

}  // namespace jyglobalvst::engine
