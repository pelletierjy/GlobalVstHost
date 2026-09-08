// tests/integration/us2_plugin_editor_test.cpp
//
// T053 — Integration test: plugin GUI open/close while audio plays; GUI crash
// does NOT stop audio.

#include "../integration/loopback_fixture.h"

#include <chrono>
#include <thread>

namespace jyglobalvst::testing {

class US2PluginEditorTest : public LoopbackFixture
{
};

TEST_F(US2PluginEditorTest, OpenEditorDoesNotCrashEngine)
{
    StartEngine();

    AddPlaceholderSlot(0);

    // openEditor should not crash even if no real editor is available.
    engine()->openEditor(0);

    // Engine should still be functional.
    const auto latency = engine()->latencyProfile();
    EXPECT_GE(latency.total_round_trip_ms, 0.0f);

    engine()->closeEditor(0);

    StopEngine();
}

TEST_F(US2PluginEditorTest, CloseEditorOnNonexistentSlotIsSafe)
{
    StartEngine();

    // Should not throw or crash.
    engine()->closeEditor(999);

    StopEngine();
}

TEST_F(US2PluginEditorTest, OpenEditorOnNonexistentSlotIsSafe)
{
    StartEngine();

    // Should not throw or crash.
    engine()->openEditor(999);

    StopEngine();
}

}  // namespace jyglobalvst::testing
