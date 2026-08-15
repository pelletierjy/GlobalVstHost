// tests/unit/capture_drift_controller_test.cpp
//
// Unit tests for CaptureDriftController — the PI servo that reconciles the
// WASAPI capture clock with the output device clock on the ASIO / JUCE-callback
// transports.
//
// Before this controller existed the capture resampler ran at a fixed nominal
// ratio, so any real clock mismatch (which is always present — they are separate
// crystals) drove the ring's fill monotonically until it starved. The callback
// then emitted zero-filled blocks indefinitely, which the user experiences as the
// input silently ceasing to be detected until they toggle the engine.

#include <gtest/gtest.h>

#include <routing/capture_drift_controller.h>

#include <cmath>

using jyglobalvst::engine::CaptureDriftController;

namespace {

// Matches the tuning the engine uses (audio_engine_impl.cpp).
constexpr CaptureDriftController::Tuning kTuning{4.0e-4, 8.0e-6, 0.005, 0.004};

constexpr std::size_t kTargetFill = 2160;  // 45 ms @ 48 kHz

CaptureDriftController makeController(std::size_t target = kTargetFill)
{
    CaptureDriftController c;
    c.configure(target, kTuning);
    return c;
}

// Runs a closed-loop simulation: a producer delivering `producer_ppm` faster (or
// slower, if negative) than the consumer drains. Returns the final fill level.
double simulate(CaptureDriftController& c, double producer_ppm, int blocks,
                double block_frames = 256.0, double nominal_ratio = 1.0)
{
    double fill = static_cast<double>(kTargetFill);
    const double produced_per_block = block_frames * nominal_ratio * (1.0 + producer_ppm * 1e-6);

    for (int i = 0; i < blocks; ++i)
    {
        const bool silence = c.update(static_cast<std::size_t>(std::max(0.0, fill)));
        fill += produced_per_block;
        if (!silence)
        {
            fill -= block_frames * nominal_ratio * c.trim();
        }
        if (fill < 0.0)
            fill = 0.0;
    }
    return fill;
}

}  // namespace

TEST(CaptureDriftController, DisabledWhenTargetIsZero)
{
    CaptureDriftController c;
    c.configure(0, kTuning);

    EXPECT_FALSE(c.enabled());
    EXPECT_FALSE(c.update(0));
    EXPECT_FALSE(c.update(999999));
    EXPECT_DOUBLE_EQ(c.trim(), 1.0);
}

TEST(CaptureDriftController, PrimesBeforeConsuming)
{
    auto c = makeController();

    EXPECT_TRUE(c.enabled());
    EXPECT_TRUE(c.priming());

    // Below target: keep emitting silence, consume nothing.
    EXPECT_TRUE(c.update(0));
    EXPECT_TRUE(c.update(kTargetFill / 2));
    EXPECT_TRUE(c.update(kTargetFill - 1));

    // Cushion reached: start tracking.
    EXPECT_FALSE(c.update(kTargetFill));
    EXPECT_FALSE(c.priming());
}

// This is the specific failure the user reported: the ring runs dry and the
// engine must recover on its own instead of producing silence forever.
TEST(CaptureDriftController, ReArmsAfterCompleteStarvation)
{
    auto c = makeController();
    ASSERT_FALSE(c.update(kTargetFill));  // leave priming
    ASSERT_FALSE(c.priming());

    // Ring drains completely (e.g. a loopback endpoint with nothing playing).
    EXPECT_TRUE(c.update(0));
    EXPECT_TRUE(c.priming());
    EXPECT_DOUBLE_EQ(c.trim(), 1.0);
    EXPECT_EQ(c.underrunCount(), 1u);

    // Still starved → still silent.
    EXPECT_TRUE(c.update(10));

    // Audio returns and refills the cushion → tracking resumes by itself.
    EXPECT_FALSE(c.update(kTargetFill));
    EXPECT_FALSE(c.priming());
}

TEST(CaptureDriftController, TrimDirectionFollowsFillError)
{
    auto c = makeController();
    ASSERT_FALSE(c.update(kTargetFill));

    // Over-full → consume faster (ratio above 1).
    for (int i = 0; i < 50; ++i)
        c.update(kTargetFill * 2);
    EXPECT_GT(c.trim(), 1.0);

    auto c2 = makeController();
    ASSERT_FALSE(c2.update(kTargetFill));

    // Under-full → consume slower (ratio below 1).
    for (int i = 0; i < 50; ++i)
        c2.update(kTargetFill / 2);
    EXPECT_LT(c2.trim(), 1.0);
}

TEST(CaptureDriftController, TrimNeverLeavesTheAudibleSafeBand)
{
    auto c = makeController();
    ASSERT_FALSE(c.update(kTargetFill));

    // Hammer it with the worst possible errors in both directions.
    for (int i = 0; i < 10000; ++i)
        c.update(kTargetFill * 1000);
    EXPECT_LE(c.trim(), 1.0 + kTuning.max_trim + 1e-12);

    for (int i = 0; i < 10000; ++i)
        c.update(kTargetFill + 1);
    EXPECT_GE(c.trim(), 1.0 - kTuning.max_trim - 1e-12);
}

// The whole point of the integral term: with a persistent rate mismatch the loop
// must converge to a steady fill rather than drifting to starvation or overflow.
TEST(CaptureDriftController, ConvergesWithFastProducer)
{
    auto c = makeController();
    ASSERT_FALSE(c.update(kTargetFill));

    const double fill = simulate(c, +100.0, 20000);

    EXPECT_GT(fill, 0.0) << "ring starved despite the servo";
    EXPECT_NEAR(fill, static_cast<double>(kTargetFill), kTargetFill * 0.5)
        << "fill did not settle near target";
    EXPECT_EQ(c.underrunCount(), 0u);
}

TEST(CaptureDriftController, ConvergesWithSlowProducer)
{
    auto c = makeController();
    ASSERT_FALSE(c.update(kTargetFill));

    const double fill = simulate(c, -100.0, 20000);

    EXPECT_GT(fill, 0.0) << "ring starved despite the servo";
    EXPECT_NEAR(fill, static_cast<double>(kTargetFill), kTargetFill * 0.5)
        << "fill did not settle near target";
    EXPECT_EQ(c.underrunCount(), 0u);
}

// Without correction a 100 ppm mismatch drains a 45 ms cushion in a few minutes.
// This documents the failure mode the servo exists to prevent.
TEST(CaptureDriftController, UncorrectedDriftWouldStarve)
{
    CaptureDriftController disabled;
    disabled.configure(0, kTuning);

    double fill = static_cast<double>(kTargetFill);
    const double block_frames = 256.0;
    const double produced = block_frames * (1.0 - 100.0e-6);  // producer 100 ppm slow

    int blocks_until_empty = 0;
    while (fill > 0.0 && blocks_until_empty < 10'000'000)
    {
        disabled.update(static_cast<std::size_t>(fill));
        fill += produced;
        fill -= block_frames * disabled.trim();  // trim pinned at 1.0
        ++blocks_until_empty;
    }

    EXPECT_LT(blocks_until_empty, 10'000'000)
        << "sanity: an uncorrected 100 ppm mismatch must eventually empty the ring";
}
