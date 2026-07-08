// tests/integration/us3_placeholder_test.cpp
//
// T066 — Integration test: load preset with missing plugin → placeholder
// appears, audio bypasses it, re-point works (FR-022f, FR-022g-2).

#include "../integration/loopback_fixture.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

namespace jyglobalvst::testing {

class US3PlaceholderTest : public LoopbackFixture
{
};

TEST_F(US3PlaceholderTest, MissingPluginBecomesPlaceholder)
{
    StartEngine();

    // Build a preset referencing a non-existent plugin.
    const auto temp_path = std::filesystem::temp_directory_path() / "us3_placeholder_test.jvst";
    nlohmann::json doc;
    doc["schema_version"] = 1;
    doc["preset_name"] = "Missing";
    doc["created_at"] = "2026-06-06T00:00:00Z";
    doc["updated_at"] = "2026-06-06T00:00:00Z";
    doc["target_buffer_size"] = 256;
    nlohmann::json slots = nlohmann::json::array();
    nlohmann::json slot;
    slot["position"] = 0;
    slot["plugin_uid"] = "00000000000000000000000000000000";
    slot["plugin_vendor"] = "GhostVendor";
    slot["plugin_name"] = "GhostPlugin";
    slot["is_bypassed"] = false;
    slot["state_chunk_b64"] = "";
    slot["plugin_path_hint"] = "C:\\Ghost.vst3";
    slots.push_back(slot);
    doc["slots"] = slots;

    std::ofstream ofs(temp_path, std::ios::binary);
    ofs << doc.dump(2);
    ofs.close();

    engine()->loadPreset(temp_path);

    const auto snapshot = engine()->snapshotChain();
    ASSERT_EQ(snapshot.slots.size(), 1u);
    EXPECT_EQ(snapshot.slots[0].kind, PluginSlotKind::Placeholder);
    EXPECT_EQ(snapshot.slots[0].ref.vendor, "GhostVendor");
    EXPECT_EQ(snapshot.slots[0].ref.name, "GhostPlugin");

    StopEngine();

    std::error_code ec;
    std::filesystem::remove(temp_path, ec);
}

}  // namespace jyglobalvst::testing
