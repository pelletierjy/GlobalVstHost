// tests/unit/realtime_clock_test.cpp
//
// T030 — Unit tests for RealtimeClock (T013).
//
// Tests verify QueryPerformanceCounter integration, frequency querying,
// and accurate delta calculations. All operations are RT-safe with no
// allocation or syscalls on the hot path.

#include <gtest/gtest.h>
#include <platform/realtime_clock.h>

#include <thread>
#include <chrono>
#include <cmath>

using namespace jyglobalvst::shared;

class RealtimeClockTest : public ::testing::Test
{
protected:
    RealtimeClock clock_;
};

TEST_F(RealtimeClockTest, FrequencyIsPositive)
{
    const auto freq = clock_.frequency();
    EXPECT_GT(freq, 0);
}

TEST_F(RealtimeClockTest, NowIncreases)
{
    const auto t1 = clock_.now();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    const auto t2 = clock_.now();

    EXPECT_GT(t2, t1);
}

TEST_F(RealtimeClockTest, DeltaToNsBasic)
{
    const auto t1 = clock_.now();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const auto t2 = clock_.now();

    const auto delta_ns = clock_.deltaToNs(t1, t2);

    // Should be roughly 100 milliseconds = 100e6 nanoseconds.
    // Allow ±20% tolerance due to timer granularity and scheduling.
    const auto expected_ns = 100'000'000ULL;
    const auto tolerance_ns = expected_ns / 5;  // ±20%

    EXPECT_GE(delta_ns, expected_ns - tolerance_ns);
    EXPECT_LE(delta_ns, expected_ns + tolerance_ns);
}

TEST_F(RealtimeClockTest, DeltaToMsBasic)
{
    const auto t1 = clock_.now();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const auto t2 = clock_.now();

    const auto delta_ms = clock_.deltaToMs(t1, t2);

    // Should be roughly 50 milliseconds.
    // Allow ±25% tolerance.
    EXPECT_GE(delta_ms, 40.0);
    EXPECT_LE(delta_ms, 60.0);
}

TEST_F(RealtimeClockTest, ZeroDeltaWhenSameTime)
{
    const auto t = clock_.now();
    const auto delta_ns = clock_.deltaToNs(t, t);
    EXPECT_EQ(delta_ns, 0);
}

TEST_F(RealtimeClockTest, ReversedTimesReturnZero)
{
    const auto t1 = clock_.now();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    const auto t2 = clock_.now();

    // Normal order: deltaToNs(t1, t2) should be > 0.
    const auto delta = clock_.deltaToNs(t1, t2);
    EXPECT_GT(delta, 0);

    // Reversed: deltaToNs(t2, t1) where a > b should return 0 (no underflow).
    const auto reversed = clock_.deltaToNs(t2, t1);
    EXPECT_EQ(reversed, 0);
}

TEST_F(RealtimeClockTest, SmallDeltas)
{
    // Measure a very small operation.
    const auto t1 = clock_.now();
    // Minimal work: a few assignments.
    volatile int x = 1;
    int y = x + 1;
    (void)y;
    const auto t2 = clock_.now();

    const auto delta_ns = clock_.deltaToNs(t1, t2);

    // Should be positive but < 1 millisecond on modern systems.
    EXPECT_GE(delta_ns, 0);
    EXPECT_LT(delta_ns, 10'000'000ULL);  // < 10 ms
}

TEST_F(RealtimeClockTest, LongDeltaNoOverflow)
{
    // Test that multiplication in deltaToNs doesn't overflow even for large deltas.
    // Simulate a 10-second delta without actually sleeping (by constructing fake times).
    const auto freq = clock_.frequency();
    const auto t1 = 0ULL;
    const auto t2 = freq * 10;  // 10 seconds

    const auto delta_ns = clock_.deltaToNs(t1, t2);
    const auto expected_ns = 10'000'000'000ULL;  // 10 seconds

    EXPECT_EQ(delta_ns, expected_ns);
}

TEST_F(RealtimeClockTest, MsConversionAccuracy)
{
    const auto freq = clock_.frequency();
    const auto t1 = 0ULL;
    const auto t2 = freq / 1000;  // 1 millisecond

    const auto delta_ms = clock_.deltaToMs(t1, t2);

    // Should be very close to 1.0 ms.
    EXPECT_NEAR(delta_ms, 1.0, 0.01);
}

TEST_F(RealtimeClockTest, ConsistentFrequency)
{
    const auto freq1 = clock_.frequency();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const auto freq2 = clock_.frequency();

    // Frequency should never change during execution.
    EXPECT_EQ(freq1, freq2);
}
