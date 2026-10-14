// tests/unit/stereo_width_processor_test.cpp
//
// DSP behavior of the built-in Stereo Width: mid/side scaling — width 0 =
// mono, width 1 = passthrough, width 2 = sides doubled.

#include "../../src/audio-engine/builtin-effects/stereo_width_processor.h"

#include <gtest/gtest.h>

namespace jyglobalvst::testing {

using engine::StereoWidthProcessor;

namespace {

constexpr int kBlockSize = 8;

void fillBlock(juce::AudioBuffer<float>& buf)
{
    // Deterministic asymmetric pattern so mid/side changes are observable.
    for (int i = 0; i < buf.getNumSamples(); ++i)
    {
        buf.getWritePointer(0)[i] = 1.0f;
        buf.getWritePointer(1)[i] = 0.0f;
    }
}

}  // namespace

TEST(StereoWidthProcessorTest, WidthZeroMakesMono)
{
    StereoWidthProcessor proc;
    proc.setWidth(0.0f);

    juce::AudioBuffer<float> buf(2, kBlockSize);
    fillBlock(buf);
    juce::MidiBuffer midi;
    proc.processBlock(buf, midi);

    for (int i = 0; i < kBlockSize; ++i)
    {
        EXPECT_FLOAT_EQ(buf.getReadPointer(0)[i], 0.5f);  // mid = (1+0)/2
        EXPECT_FLOAT_EQ(buf.getReadPointer(1)[i], 0.5f);
    }
}

TEST(StereoWidthProcessorTest, WidthOneIsPassthrough)
{
    StereoWidthProcessor proc;
    proc.setWidth(1.0f);

    juce::AudioBuffer<float> buf(2, kBlockSize);
    fillBlock(buf);
    juce::MidiBuffer midi;
    proc.processBlock(buf, midi);

    for (int i = 0; i < kBlockSize; ++i)
    {
        EXPECT_FLOAT_EQ(buf.getReadPointer(0)[i], 1.0f);
        EXPECT_FLOAT_EQ(buf.getReadPointer(1)[i], 0.0f);
    }
}

TEST(StereoWidthProcessorTest, WidthTwoDoublesSide)
{
    StereoWidthProcessor proc;
    proc.setWidth(2.0f);

    juce::AudioBuffer<float> buf(2, kBlockSize);
    fillBlock(buf);
    juce::MidiBuffer midi;
    proc.processBlock(buf, midi);

    for (int i = 0; i < kBlockSize; ++i)
    {
        // mid=0.5, side=0.5*2=1.0 → L=1.5, R=-0.5.
        EXPECT_FLOAT_EQ(buf.getReadPointer(0)[i], 1.5f);
        EXPECT_FLOAT_EQ(buf.getReadPointer(1)[i], -0.5f);
    }
}

TEST(StereoWidthProcessorTest, StateRoundTrip)
{
    StereoWidthProcessor proc;
    proc.setWidth(1.75f);

    juce::MemoryBlock state;
    proc.getStateInformation(state);

    StereoWidthProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    EXPECT_FLOAT_EQ(restored.getWidth(), 1.75f);
}

}  // namespace jyglobalvst::testing
