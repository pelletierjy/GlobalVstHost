// tests/unit/energy_saver_controller_test.cpp
//
// Unit tests for the Energy Saver decision logic (EnergySaverController).
// Drives the pure state machine with synthetic time / input levels so the
// idle-timeout and wake behaviour is verified deterministically, without any
// audio device or threading.

#include "../../src/audio-engine/routing/energy_saver_controller.h"

#include <gtest/gtest.h>

#include <cmath>

namespace
{
using jyglobalvst::engine::EnergySaverController;

constexpr int kIdleMs = 30'000;
constexpr float kWakeDb = -50.0f;

// A peak comfortably above the -50 dBFS wake threshold (~0.00316 linear).
constexpr float kLoud = 0.1f;   // ~-20 dBFS
constexpr float kSilent = 0.0f;

EnergySaverController makeController()
{
    EnergySaverController c(kIdleMs, kWakeDb);
    c.reset(0);
    return c;
}
}  // namespace

TEST(EnergySaverController, DefaultsToAwakeAndDisabled)
{
    auto c = makeController();
    EXPECT_FALSE(c.enabled());
    EXPECT_FALSE(c.sleeping());
}

TEST(EnergySaverController, WakeThresholdMatchesConfiguredDb)
{
    auto c = makeController();
    const float expected = std::pow(10.0f, kWakeDb / 20.0f);
    EXPECT_NEAR(c.wakePeak(), expected, 1e-6f);
}

TEST(EnergySaverController, DisabledNeverSleepsEvenAfterLongSilence)
{
    auto c = makeController();
    // Feature off: silence for well beyond the idle window must not sleep.
    for (long long t = 0; t <= kIdleMs * 3; t += 100)
    {
        const bool changed = c.update(t, kSilent);
        EXPECT_FALSE(changed);
        EXPECT_FALSE(c.sleeping());
    }
}

TEST(EnergySaverController, SleepsAfterSustainedSilence)
{
    auto c = makeController();
    c.setEnabled(true);
    c.reset(0);

    // Just before the threshold: still awake.
    EXPECT_FALSE(c.update(kIdleMs - 100, kSilent));
    EXPECT_FALSE(c.sleeping());

    // At the threshold: transitions to sleeping, and reports the change once.
    EXPECT_TRUE(c.update(kIdleMs, kSilent));
    EXPECT_TRUE(c.sleeping());

    // Staying silent does not re-fire the transition.
    EXPECT_FALSE(c.update(kIdleMs + 500, kSilent));
    EXPECT_TRUE(c.sleeping());
}

TEST(EnergySaverController, AudioBeforeThresholdKeepsAwakeAndResetsTimer)
{
    auto c = makeController();
    c.setEnabled(true);
    c.reset(0);

    // Silence almost to the threshold, then a burst of audio resets the timer.
    EXPECT_FALSE(c.update(kIdleMs - 100, kSilent));
    EXPECT_FALSE(c.update(kIdleMs - 100, kLoud));
    EXPECT_FALSE(c.sleeping());

    // From the burst, another near-full idle window is still not enough.
    EXPECT_FALSE(c.update(2 * kIdleMs - 200, kSilent));
    EXPECT_FALSE(c.sleeping());

    // Only kIdleMs after the last audio does it sleep.
    EXPECT_TRUE(c.update(2 * kIdleMs - 100, kSilent));
    EXPECT_TRUE(c.sleeping());
}

TEST(EnergySaverController, WakesImmediatelyWhenAudioReturns)
{
    auto c = makeController();
    c.setEnabled(true);
    c.reset(0);

    ASSERT_TRUE(c.update(kIdleMs, kSilent));
    ASSERT_TRUE(c.sleeping());

    // The first block of audio wakes it and reports the change.
    EXPECT_TRUE(c.update(kIdleMs + 100, kLoud));
    EXPECT_FALSE(c.sleeping());
}

TEST(EnergySaverController, DisablingWhileSleepingWakesImmediately)
{
    auto c = makeController();
    c.setEnabled(true);
    c.reset(0);
    ASSERT_TRUE(c.update(kIdleMs, kSilent));
    ASSERT_TRUE(c.sleeping());

    // Turning the feature off must not leave the engine suspended.
    c.setEnabled(false);
    EXPECT_FALSE(c.sleeping());
}

TEST(EnergySaverController, LevelJustBelowThresholdIsSilenceJustAboveIsAudio)
{
    auto c = makeController();
    c.setEnabled(true);
    c.reset(0);

    const float wake = c.wakePeak();
    // Just below → treated as silence → sleeps after the idle window.
    EXPECT_TRUE(c.update(kIdleMs, wake * 0.99f));
    EXPECT_TRUE(c.sleeping());

    // Just above → treated as audio → wakes.
    EXPECT_TRUE(c.update(kIdleMs + 100, wake * 1.01f));
    EXPECT_FALSE(c.sleeping());
}
