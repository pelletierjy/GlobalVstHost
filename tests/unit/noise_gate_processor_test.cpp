// tests/unit/noise_gate_processor_test.cpp
//
// DSP behavior of the built-in Noise Gate: attenuates signals below the
// threshold, passes signals above it, and round-trips its state chunk.

#include "../../src/audio-engine/builtin-effects/noise_gate_processor.h"

#include <gtest/gtest.h>

#include <cmath>

namespace jyglobalvst::testing {

using engine::NoiseGateProcessor;

namespace {

constexpr int kBlockSize = 512;
constexpr double kSampleRate = 48000.0;

void fillSine(juce::AudioBuffer<float>& buf, float amplitude)
{
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            buf.getWritePointer(ch)[i] = amplitude * std::sin(2.0f * 3.14159265f * 1000.0f * static_cast<float>(i) /
                                                              static_cast<float>(kSampleRate));
        }
    }
}

float peakOf(const juce::AudioBuffer<float>& buf)
{
    float peak = 0.0f;
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            peak = std::max(peak, std::abs(buf.getReadPointer(ch)[i]));
        }
    }
    return peak;
}

}  // namespace

TEST(NoiseGateProcessorTest, GatesQuietSignal)
{
    NoiseGateProcessor gate;
    gate.prepareToPlay(kSampleRate, kBlockSize);
    gate.setThresholdDb(-50.0f);
    gate.setHoldMs(0.0f);

    juce::AudioBuffer<float> buf(2, kBlockSize);
    fillSine(buf, 1.0e-4f);  // -80 dBFS, far below threshold

    juce::MidiBuffer midi;
    for (int block = 0; block < 20; ++block)
    {
        fillSine(buf, 1.0e-4f);
        gate.processBlock(buf, midi);
    }

    // Gate starts closed and the input never crosses the threshold: output
    // must stay inaudible.
    EXPECT_LT(peakOf(buf), 1.0e-6f);
}

TEST(NoiseGateProcessorTest, PassesLoudSignal)
{
    NoiseGateProcessor gate;
    gate.prepareToPlay(kSampleRate, kBlockSize);
    gate.setThresholdDb(-50.0f);

    juce::AudioBuffer<float> buf(2, kBlockSize);
    juce::MidiBuffer midi;
    for (int block = 0; block < 40; ++block)  // ~426 ms, past attack + smoothing
    {
        fillSine(buf, 0.5f);
        gate.processBlock(buf, midi);
    }

    EXPECT_GT(peakOf(buf), 0.3f);
}

TEST(NoiseGateProcessorTest, StateRoundTrip)
{
    NoiseGateProcessor gate;
    gate.setThresholdDb(-30.0f);
    gate.setAttackMs(20.0f);
    gate.setHoldMs(100.0f);
    gate.setReleaseMs(200.0f);

    juce::MemoryBlock state;
    gate.getStateInformation(state);

    NoiseGateProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));

    EXPECT_FLOAT_EQ(restored.getThresholdDb(), -30.0f);
    EXPECT_FLOAT_EQ(restored.getAttackMs(), 20.0f);
    EXPECT_FLOAT_EQ(restored.getHoldMs(), 100.0f);
    EXPECT_FLOAT_EQ(restored.getReleaseMs(), 200.0f);
}

}  // namespace jyglobalvst::testing
