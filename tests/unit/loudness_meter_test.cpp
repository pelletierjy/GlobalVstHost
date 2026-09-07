#include <gtest/gtest.h>
#include "builtin-effects/loudness_meter.h"
#include <cmath>

using namespace jyglobalvst::engine;

TEST(LoudnessMeterTest, ConvertsAudioToLufs)
{
    LoudnessMeter meter;
    meter.prepareToPlay(48000.0, 256);

    // Process a silent block
    std::vector<float> silence(256, 0.f);
    meter.process(silence.data(), silence.data(), 256);
    float silent_loudness = meter.shortTermLoudnessLufs();
    EXPECT_TRUE(std::isinf(silent_loudness) || silent_loudness < -70.f);

    // Process a louder block (0.1 amplitude)
    std::vector<float> loud(256, 0.1f);
    meter.process(loud.data(), loud.data(), 256);
    float loud_loudness = meter.shortTermLoudnessLufs();
    EXPECT_FALSE(std::isinf(loud_loudness));
    EXPECT_GT(loud_loudness, -50.f);
}

TEST(LoudnessMeterTest, FilteringDoesNotCrash)
{
    LoudnessMeter meter;
    meter.prepareToPlay(44100.0, 512);

    std::vector<float> sweep(512);
    for (int i = 0; i < 512; ++i)
        sweep[i] = std::sin(2.f * 3.14159f * i / 512.f) * 0.5f;

    meter.process(sweep.data(), sweep.data(), 512);
    EXPECT_FALSE(std::isnan(meter.shortTermLoudnessLufs()));
}
