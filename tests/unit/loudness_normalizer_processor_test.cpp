// tests/unit/loudness_normalizer_processor_test.cpp
//
// DSP behavior of the built-in Loudness Normalizer (AGC): boosts quiet
// programs toward the target RMS, respects the max-gain bound, and
// round-trips its state chunk.

#include "../../src/audio-engine/builtin-effects/loudness_normalizer_processor.h"

#include <gtest/gtest.h>

#include <cmath>

namespace jyglobalvst::testing {

using engine::LoudnessNormalizerProcessor;

namespace {

constexpr int kBlockSize = 512;
constexpr double kSampleRate = 48000.0;

float rmsOf(const juce::AudioBuffer<float>& buf)
{
    double sum = 0.0;
    int count = 0;
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            const double x = buf.getReadPointer(ch)[i];
            sum += x * x;
            ++count;
        }
    }
    return count > 0 ? static_cast<float>(std::sqrt(sum / count)) : 0.0f;
}

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

}  // namespace

TEST(LoudnessNormalizerProcessorTest, BoostsQuietSignal)
{
    LoudnessNormalizerProcessor norm;
    norm.prepareToPlay(kSampleRate, kBlockSize);
    norm.setTargetDb(-18.0f);
    norm.setMaxGainDb(24.0f);

    constexpr float kAmp = 0.01f;  // ~ -40 dBFS RMS, well below target
    juce::AudioBuffer<float> buf(2, kBlockSize);
    juce::MidiBuffer midi;
    for (int block = 0; block < 60; ++block)  // ~640 ms, past gain smoothing
    {
        fillSine(buf, kAmp);
        norm.processBlock(buf, midi);
    }

    // Quiet input must be boosted well above its original level.
    EXPECT_GT(rmsOf(buf), kAmp * 2.0f);
}

TEST(LoudnessNormalizerProcessorTest, RespectsMaxGain)
{
    LoudnessNormalizerProcessor norm;
    norm.prepareToPlay(kSampleRate, kBlockSize);
    norm.setTargetDb(-5.0f);  // aggressive target
    norm.setMaxGainDb(6.0f);  // but bounded boost (2x)

    constexpr float kAmp = 1.0e-4f;  // absurdly quiet: AGC would want ~600x
    juce::AudioBuffer<float> buf(2, kBlockSize);
    juce::MidiBuffer midi;
    for (int block = 0; block < 60; ++block)
    {
        fillSine(buf, kAmp);
        norm.processBlock(buf, midi);
    }

    const float peak = rmsOf(buf);
    // Gain is smoothed upward from 1.0 toward the 2x cap; it must never
    // exceed the cap.
    EXPECT_LE(peak, kAmp * 2.5f);
}

TEST(LoudnessNormalizerProcessorTest, StateRoundTrip)
{
    LoudnessNormalizerProcessor norm;
    norm.setTargetDb(-12.0f);
    norm.setMaxGainDb(6.0f);

    juce::MemoryBlock state;
    norm.getStateInformation(state);

    LoudnessNormalizerProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));

    EXPECT_FLOAT_EQ(restored.getTargetDb(), -12.0f);
    EXPECT_FLOAT_EQ(restored.getMaxGainDb(), 6.0f);
}

}  // namespace jyglobalvst::testing
