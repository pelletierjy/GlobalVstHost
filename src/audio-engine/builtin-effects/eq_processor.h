#pragma once

#include "builtin_ids.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <vector>

namespace jyglobalvst::engine {

class EqProcessor : public juce::AudioPluginInstance
{
public:
    EqProcessor();
    ~EqProcessor() override;

    void prepareToPlay(double sample_rate, int samples_per_block) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

    const juce::String getName() const override { return "EQ (Bass Boost)"; }

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

    int getLatencySamples() const { return 0; }

    static constexpr int kNumBands = builtin::eq::NUM_BANDS;
    static constexpr int kNumParams = builtin::eq::NUM_PARAMETERS;

    float getBandGain(int band_index) const;
    void setBandGain(int band_index, float gain_db);
    float getBassBoost() const;
    void setBassBoost(float gain_db);

private:
    struct Biquad
    {
        float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;
        float z1_l = 0.f, z2_l = 0.f, z1_r = 0.f, z2_r = 0.f;

        float processL(float in) noexcept;
        float processR(float in) noexcept;
        void reset() { z1_l = z2_l = z1_r = z2_r = 0.f; }
    };

    double sample_rate_ = 0.0;
    std::array<float, kNumBands> band_gains_;
    float bass_boost_db_ = 0.f;
    std::array<std::array<Biquad, kNumBands>, 2> band_filters_;  // [channel][band]
    std::array<Biquad, 2> bass_shelf_;  // Low shelf for bass boost

    void updateFilterCoefficients();
    void computeBiquadCoefficients(float center_hz, float gain_db, float q, Biquad& biquad);
};

}  // namespace jyglobalvst::engine
