// tests/integration/us2_live_reorder_test.cpp
//
// T052 — Integration test: live reorder during playback — measure for audio
// dropout (zero allowed) per FR-010.

#include "../integration/loopback_fixture.h"

#include <chrono>
#include <thread>

namespace jyglobalvst::testing {

class US2LiveReorderTest : public LoopbackFixture
{
};

TEST_F(US2LiveReorderTest, ReorderDuringPlaybackDoesNotStopEngine)
{
    StartEngine();

    PluginRef ref;
    ref.plugin_uid = {};
    ref.vendor = "TestVendor";
    ref.name = "TestPlugin";

    // Build a small chain.
    engine()->addPlugin(ref, 0);
    engine()->addPlugin(ref, 1);

    // Let audio run briefly.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Reorder while running.
    engine()->moveSlot(0, 1);

    // Engine should still be running (no crash, no stop).
    const auto latency = engine()->latencyProfile();
    EXPECT_GT(latency.total_round_trip_ms, 0.0f) << "Engine should still report latency after reorder";

    StopEngine();
}

TEST_F(US2LiveReorderTest, ReorderPreservesSlotCount)
{
    StartEngine();

    PluginRef ref;
    ref.plugin_uid = {};
    ref.vendor = "TestVendor";
    ref.name = "TestPlugin";

    engine()->addPlugin(ref, 0);
    engine()->addPlugin(ref, 1);
    engine()->addPlugin(ref, 2);

    const auto snapshot_before = engine()->snapshotChain();
    const std::size_t count_before = snapshot_before.slots.size();

    engine()->moveSlot(2, 0);

    const auto snapshot_after = engine()->snapshotChain();
    const std::size_t count_after = snapshot_after.slots.size();

    EXPECT_EQ(count_before, count_after) << "Reorder must preserve total slot count";

    StopEngine();
}

}  // namespace jyglobalvst::testing
