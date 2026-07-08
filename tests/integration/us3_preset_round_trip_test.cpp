// tests/integration/us3_preset_round_trip_test.cpp
//
// T065 — Integration test: save chain → quit → relaunch → load preset → chain
// matches original (FR-021, FR-022a).

#include "../integration/loopback_fixture.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

namespace jyglobalvst::testing {

class US3PresetRoundTripTest : public LoopbackFixture
{
};

TEST_F(US3PresetRoundTripTest, SaveAndLoadPreservesPlaceholder)
{
    StartEngine();

    // Use a placeholder slot so we don't need a real VST3 file on disk.
    AddPlaceholderSlot(0);
    engine()->setBypass(0, true);

    // Save preset.
    const auto temp_path = std::filesystem::temp_directory_path() / "us3_round_trip_test.jvst";
    engine()->savePreset(temp_path, "RoundTrip");
    EXPECT_TRUE(std::filesystem::exists(temp_path));

    // Clear chain.
    engine()->removeSlot(0);
    EXPECT_TRUE(engine()->snapshotChain().slots.empty());

    // Load preset.
    engine()->loadPreset(temp_path);

    // Verify chain rebuilt with placeholder.
    const auto snapshot = engine()->snapshotChain();
    ASSERT_EQ(snapshot.slots.size(), 1u);
    EXPECT_EQ(snapshot.slots[0].kind, PluginSlotKind::Placeholder);
    EXPECT_TRUE(snapshot.slots[0].is_bypassed);

    StopEngine();

    // Cleanup.
    std::error_code ec;
    std::filesystem::remove(temp_path, ec);
}

TEST_F(US3PresetRoundTripTest, PresetFileIsValidJson)
{
    StartEngine();

    AddPlaceholderSlot(0);

    const auto temp_path = std::filesystem::temp_directory_path() / "us3_json_check.jvst";
    engine()->savePreset(temp_path, "JsonCheck");

    std::ifstream ifs(temp_path);
    ASSERT_TRUE(ifs.is_open());
    nlohmann::json doc;
    EXPECT_NO_THROW(ifs >> doc);
    EXPECT_TRUE(doc.contains("schema_version"));
    EXPECT_TRUE(doc.contains("slots"));
    EXPECT_TRUE(doc["slots"].is_array());

    StopEngine();
    std::error_code ec;
    std::filesystem::remove(temp_path, ec);
}

}  // namespace jyglobalvst::testing
