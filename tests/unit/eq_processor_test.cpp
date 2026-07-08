#include <gtest/gtest.h>
#include "builtin-effects/eq_processor.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

using namespace jyglobalvst::engine;

TEST(EqProcessorTest, ProcessBlockDoesNotCrash)
{
    EqProcessor proc;
    proc.prepareToPlay(48000.0, 256);

    juce::AudioBuffer<float> buffer(2, 256);
    buffer.clear();

    for (int i = 0; i < 256; ++i)
        buffer.setSample(0, i, std::sin(2.f * 3.14159f * i / 256.f) * 0.5f);
    for (int i = 0; i < 256; ++i)
        buffer.setSample(1, i, std::sin(2.f * 3.14159f * i / 256.f) * 0.5f);

    juce::MidiBuffer midi;
    proc.processBlock(buffer, midi);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 256; ++i)
            EXPECT_FALSE(std::isnan(buffer.getSample(ch, i)));
}

TEST(EqProcessorTest, BandGainAffectsOutput)
{
    EqProcessor proc;
    proc.prepareToPlay(48000.0, 256);

    juce::AudioBuffer<float> ref_buffer(2, 256);
    ref_buffer.clear();
    for (int i = 0; i < 256; ++i)
        ref_buffer.setSample(0, i, 0.1f);
    for (int i = 0; i < 256; ++i)
        ref_buffer.setSample(1, i, 0.1f);

    juce::MidiBuffer midi;
    proc.processBlock(ref_buffer, midi);

    proc.setBandGain(0, 6.f);

    juce::AudioBuffer<float> boosted_buffer(2, 256);
    boosted_buffer.clear();
    for (int i = 0; i < 256; ++i)
        boosted_buffer.setSample(0, i, 0.1f);
    for (int i = 0; i < 256; ++i)
        boosted_buffer.setSample(1, i, 0.1f);

    proc.processBlock(boosted_buffer, midi);

    EXPECT_GT(std::abs(boosted_buffer.getSample(0, 128)), std::abs(ref_buffer.getSample(0, 128)));
}

TEST(EqProcessorTest, FlatResetClearsGains)
{
    EqProcessor proc;

    proc.setBandGain(0, 6.f);
    proc.setBassBoost(3.f);

    proc.setBandGain(0, 0.f);
    proc.setBassBoost(0.f);

    EXPECT_EQ(proc.getBandGain(0), 0.f);
    EXPECT_EQ(proc.getBassBoost(), 0.f);
}
