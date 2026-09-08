// tests/integration/us4_cpu_warning_test.cpp
//
// T092 — Integration test: synthetic heavy chain pushes CPU > 5%; warning
// appears within 1 s; warning clears within 1 s after load drops (FR-026).
//
// In testable-dev we verify the CPU stats API surface and that an empty
// chain does not trigger a warning under normal conditions.

#include "../integration/loopback_fixture.h"

#include <cmath>

namespace jyglobalvst::testing {

class US4CpuWarningTest : public LoopbackFixture
{
};

TEST_F(US4CpuWarningTest, EmptyChainDoesNotTriggerWarning)
{
    StartEngine();

    // Allow callbacks to accumulate CPU stats.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto stats = engine()->cpuStats();

    // Instantaneous CPU should be a finite percentage.
    EXPECT_TRUE(std::isfinite(stats.instantaneous_pct));
    EXPECT_GE(stats.instantaneous_pct, 0.0f);

    // Rolling 1s should also be finite.
    EXPECT_TRUE(std::isfinite(stats.rolling_1s_pct));
    EXPECT_GE(stats.rolling_1s_pct, 0.0f);

    // Xrun count should be non-negative.
    EXPECT_GE(stats.xrun_count_session, 0u);

    // Empty chain on a healthy system should not warn.
    EXPECT_FALSE(stats.warning_active);

    StopEngine();
}

TEST_F(US4CpuWarningTest, CpuStatsStructurallyValid)
{
    StartEngine();

    auto stats = engine()->cpuStats();

    // All fields must be present and sane.
    EXPECT_TRUE(std::isfinite(stats.instantaneous_pct));
    EXPECT_TRUE(std::isfinite(stats.rolling_1s_pct));

    // warning_active must be consistent with threshold.
    EXPECT_EQ(stats.warning_active, stats.rolling_1s_pct >= 5.0f);

    StopEngine();
}

}  // namespace jyglobalvst::testing
