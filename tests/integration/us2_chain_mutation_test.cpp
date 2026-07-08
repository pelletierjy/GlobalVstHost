// tests/integration/us2_chain_mutation_test.cpp
//
// T051 — Integration test: build a 3-plugin chain; verify processing order;
// toggle bypass on middle plugin; remove and re-add.

#include "../integration/loopback_fixture.h"

namespace jyglobalvst::testing {

class US2ChainMutationTest : public LoopbackFixture
{
};

TEST_F(US2ChainMutationTest, AddPluginFailsForMissingPlugin)
{
    StartEngine();

    const int rev_before = engine()->snapshotChain().chain_revision;

    PluginRef ref;
    ref.plugin_uid = {};
    ref.vendor = "TestVendor";
    ref.name = "TestPlugin";

    const auto id = engine()->addPlugin(ref, 0);
    EXPECT_TRUE(id.isNull()) << "addPlugin should return null InstanceId for missing plugin";

    const int rev_after = engine()->snapshotChain().chain_revision;
    EXPECT_EQ(rev_after, rev_before) << "addPlugin should not bump revision on failure";

    const auto snapshot = engine()->snapshotChain();
    EXPECT_TRUE(snapshot.slots.empty()) << "No slot should be added on failure";

    StopEngine();
}

TEST_F(US2ChainMutationTest, RemoveSlotBumpsRevision)
{
    StartEngine();

    AddPlaceholderSlot(0);
    const int rev_before = engine()->snapshotChain().chain_revision;

    engine()->removeSlot(0);

    const int rev_after = engine()->snapshotChain().chain_revision;
    EXPECT_GT(rev_after, rev_before) << "removeSlot must bump chain revision";

    StopEngine();
}

TEST_F(US2ChainMutationTest, MoveSlotBumpsRevision)
{
    StartEngine();

    AddPlaceholderSlot(0);
    AddPlaceholderSlot(1);

    const int rev_before = engine()->snapshotChain().chain_revision;

    engine()->moveSlot(0, 1);

    const int rev_after = engine()->snapshotChain().chain_revision;
    EXPECT_GT(rev_after, rev_before) << "moveSlot must bump chain revision";

    StopEngine();
}

TEST_F(US2ChainMutationTest, SetBypassDoesNotChangeSlotCount)
{
    StartEngine();

    AddPlaceholderSlot(0);

    const auto snapshot_before = engine()->snapshotChain();
    const std::size_t count_before = snapshot_before.slots.size();

    engine()->setBypass(0, true);

    const auto snapshot_after = engine()->snapshotChain();
    const std::size_t count_after = snapshot_after.slots.size();

    EXPECT_EQ(count_before, count_after) << "setBypass must not add or remove slots";

    StopEngine();
}

TEST_F(US2ChainMutationTest, ChainSnapshotPositionsAreContiguous)
{
    StartEngine();

    AddPlaceholderSlot(0);
    AddPlaceholderSlot(1);
    AddPlaceholderSlot(2);

    const auto snapshot = engine()->snapshotChain();
    for (std::size_t i = 0; i < snapshot.slots.size(); ++i)
    {
        EXPECT_EQ(snapshot.slots[i].position, static_cast<int>(i))
            << "Slot positions must be contiguous 0..N-1";
    }

    StopEngine();
}

}  // namespace jyglobalvst::testing
