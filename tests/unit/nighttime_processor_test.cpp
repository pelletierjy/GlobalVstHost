#include <gtest/gtest.h>
#include "builtin-effects/nighttime_processor.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

using namespace jyglobalvst::engine;

TEST(NightTimeProcessorTest, ProcessBlockDoesNotCrash)
{
    NightTimeProcessor proc;
    proc.prepareToPlay(48000.0, 256);

    juce::AudioBuffer<float> buffer(2, 256);
    buffer.clear();

    // Fill with test tone
    for (int i = 0; i < 256; ++i)
        buffer.setSample(0, i, std::sin(2.f * 3.14159f * i / 256.f) * 0.5f);
    for (int i = 0; i < 256; ++i)
        buffer.setSample(1, i, std::sin(2.f * 3.14159f * i / 256.f) * 0.5f);

    juce::MidiBuffer midi;
    proc.processBlock(buffer, midi);

    // Verify output is not NaN/Inf
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 256; ++i)
            EXPECT_FALSE(std::isnan(buffer.getSample(ch, i)));
}

TEST(NightTimeProcessorTest, LimiterPreventsClipping)
{
    NightTimeProcessor proc;
    proc.prepareToPlay(48000.0, 256);

    juce::AudioBuffer<float> buffer(2, 256);
    buffer.clear();

    // Full scale input
    for (int i = 0; i < 256; ++i)
    {
        buffer.setSample(0, i, 1.0f);
        buffer.setSample(1, i, 1.0f);
    }

    juce::MidiBuffer midi;
    for (int block = 0; block < 10; ++block)
        proc.processBlock(buffer, midi);

    // Output should not exceed ~1.0
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 256; ++i)
            EXPECT_LE(std::abs(buffer.getSample(ch, i)), 1.01f);
}

TEST(NightTimeProcessorTest, StateRoundTrip)
{
    NightTimeProcessor proc;
    proc.prepareToPlay(48000.0, 256);

    juce::MemoryBlock state;
    proc.getStateInformation(state);

    NightTimeProcessor proc2;
    proc2.prepareToPlay(48000.0, 256);
    proc2.setStateInformation(state.getData(), static_cast<int>(state.getSize()));

    juce::MemoryBlock state2;
    proc2.getStateInformation(state2);

    EXPECT_EQ(state.getSize(), state2.getSize());
}
