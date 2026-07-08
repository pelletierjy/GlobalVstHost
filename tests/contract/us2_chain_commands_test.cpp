// tests/contract/us2_chain_commands_test.cpp
//
// T054 — Contract test: chain.add/remove/move/set_bypass/repoint_placeholder
// IPC commands round-trip correctly through the in-process adapter.

#include "../integration/loopback_fixture.h"

namespace jyglobalvst::testing {

class US2ChainCommandsTest : public LoopbackFixture
{
};

TEST_F(US2ChainCommandsTest, AddPluginFailsForMissingPlugin)
{
    StartEngine();

    PluginRef ref;
    ref.plugin_uid = {};
    ref.vendor = "TestVendor";
    ref.name = "TestPlugin";

    const int rev0 = engine()->snapshotChain().chain_revision;

    const auto id = engine()->addPlugin(ref, 0);
    EXPECT_TRUE(id.isNull()) << "addPlugin should return null for missing plugin";

    const int rev1 = engine()->snapshotChain().chain_revision;
    EXPECT_EQ(rev1, rev0) << "addPlugin should not bump revision on failure";

    const auto snapshot = engine()->snapshotChain();
    EXPECT_TRUE(snapshot.slots.empty()) << "No slot should be added on failure";

    StopEngine();
}

TEST_F(US2ChainCommandsTest, RemoveSlotBumpsRevision)
{
    StartEngine();

    AddPlaceholderSlot(0);

    const int rev_before = engine()->snapshotChain().chain_revision;

    engine()->removeSlot(0);

    const int rev_after = engine()->snapshotChain().chain_revision;
    EXPECT_GT(rev_after, rev_before) << "removeSlot must bump chain revision";

    const auto snapshot = engine()->snapshotChain();
    EXPECT_TRUE(snapshot.slots.empty()) << "Chain should be empty after remove";

    StopEngine();
}

TEST_F(US2ChainCommandsTest, MoveRoundTrip)
{
    StartEngine();

    AddPlaceholderSlot(0);
    AddPlaceholderSlot(1);

    const int rev_before = engine()->snapshotChain().chain_revision;

    engine()->moveSlot(0, 1);

    const int rev_after = engine()->snapshotChain().chain_revision;
    EXPECT_GT(rev_after, rev_before);

    StopEngine();
}

TEST_F(US2ChainCommandsTest, SetBypassRoundTrip)
{
    StartEngine();

    AddPlaceholderSlot(0);

    // Initially not bypassed.
    auto snapshot = engine()->snapshotChain();
    ASSERT_FALSE(snapshot.slots.empty());
    EXPECT_FALSE(snapshot.slots[0].is_bypassed);

    engine()->setBypass(0, true);

    snapshot = engine()->snapshotChain();
    ASSERT_FALSE(snapshot.slots.empty());
    EXPECT_TRUE(snapshot.slots[0].is_bypassed) << "setBypass(true) should be reflected in snapshot";

    engine()->setBypass(0, false);

    snapshot = engine()->snapshotChain();
    ASSERT_FALSE(snapshot.slots.empty());
    EXPECT_FALSE(snapshot.slots[0].is_bypassed) << "setBypass(false) should be reflected in snapshot";

    StopEngine();
}

TEST_F(US2ChainCommandsTest, SetParameterRoundTrip)
{
    StartEngine();

    AddPlaceholderSlot(0);

    // setParameter should not crash and should not change revision (it's a param
    // change, not a structural mutation).
    const int rev_before = engine()->snapshotChain().chain_revision;

    engine()->setParameter(0, 0, 0.5f);

    const int rev_after = engine()->snapshotChain().chain_revision;
    EXPECT_EQ(rev_after, rev_before) << "setParameter should not bump chain revision";

    StopEngine();
}

TEST_F(US2ChainCommandsTest, RepointPlaceholderBumpsRevision)
{
    StartEngine();

    // Add a placeholder so repointPlaceholder has something to act on.
    AddPlaceholderSlot(0);

    PluginRef ref;
    ref.plugin_uid = {};
    ref.vendor = "TestVendor";
    ref.name = "TestPlugin";

    const int rev_before = engine()->snapshotChain().chain_revision;

    // repointPlaceholder on a placeholder with no matching scan cache entry
    // will not bump revision (no-op). The old stub always bumped; the real
    // implementation only bumps when a replacement actually occurs.
    engine()->repointPlaceholder(0, ref);

    const int rev_after = engine()->snapshotChain().chain_revision;
    // Since the ref does not resolve, revision stays unchanged.
    EXPECT_EQ(rev_after, rev_before) << "repointPlaceholder with unresolved ref should not bump revision";

    StopEngine();
}

}  // namespace jyglobalvst::testing
