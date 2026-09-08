// tests/integration/us3_schema_migration_test.cpp
//
// T069 — Integration test: schema-version migration (v1 + future v2 stub)
// preserves unknown fields on round-trip (FR-022b, FR-022k).
//
// Note: preset import uses strict validation (FR-022g-1) and rejects unknown
// fields. Forward-compatibility for unknown fields is implemented in the
// roaming settings store (T087). This test verifies that behavior.

#include "../integration/loopback_fixture.h"

#include "../../src/tray-app/settings/roaming_settings.cpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

namespace jyglobalvst::testing {

class US3SchemaMigrationTest : public LoopbackFixture
{
};

TEST_F(US3SchemaMigrationTest, SettingsRoundTripPreservesUnknownFields)
{
    jyglobalvst::tray::RoamingSettingsStore store;
    std::error_code ec;
    std::filesystem::remove(store.settingsPath(), ec);

    // Create a v1 settings file with an extra unknown field.
    nlohmann::json doc;
    doc["schema_version"] = 1;
    doc["custom_scan_paths"] = nlohmann::json::array();
    doc["disabled_default_paths"] = nlohmann::json::array();
    doc["default_buffer_size"] = 256;
    doc["theme"] = "dark";
    doc["default_hardware_device_friendly_name"] = nullptr;
    doc["update_check_endpoint_url"] = "https://example.com/update.json";
    doc["future_field_42"] = "preserved_value";
    doc["future_number"] = 123;

    std::filesystem::create_directories(store.settingsPath().parent_path());
    {
        std::ofstream ofs(store.settingsPath(), std::ios::binary);
        ofs << doc.dump(2);
    }

    // Load — unknown fields should be captured.
    auto settings = store.load();
    EXPECT_EQ(settings.schema_version, 1);
    EXPECT_EQ(settings.theme, "dark");
    EXPECT_TRUE(settings.unknown_fields.contains("future_field_42"));
    EXPECT_EQ(settings.unknown_fields["future_field_42"].get<std::string>(), "preserved_value");
    EXPECT_TRUE(settings.unknown_fields.contains("future_number"));
    EXPECT_EQ(settings.unknown_fields["future_number"].get<int>(), 123);

    // Save back.
    store.save(settings);

    // Verify the written file still contains the unknown fields.
    std::ifstream ifs(store.settingsPath());
    ASSERT_TRUE(ifs.is_open());
    nlohmann::json roundtrip;
    ifs >> roundtrip;

    EXPECT_TRUE(roundtrip.contains("future_field_42"));
    EXPECT_EQ(roundtrip["future_field_42"].get<std::string>(), "preserved_value");
    EXPECT_TRUE(roundtrip.contains("future_number"));
    EXPECT_EQ(roundtrip["future_number"].get<int>(), 123);

    std::filesystem::remove(store.settingsPath(), ec);
}

TEST_F(US3SchemaMigrationTest, PresetV1AcceptedV2Rejected)
{
    StartEngine();

    // Valid v1 preset should load (empty chain).
    const auto v1_path = std::filesystem::temp_directory_path() / "us3_v1_preset.jvst";
    {
        nlohmann::json doc;
        doc["schema_version"] = 1;
        doc["preset_name"] = "V1";
        doc["created_at"] = "2026-06-06T00:00:00Z";
        doc["updated_at"] = "2026-06-06T00:00:00Z";
        doc["target_buffer_size"] = 256;
        doc["slots"] = nlohmann::json::array();

        std::ofstream ofs(v1_path, std::ios::binary);
        ofs << doc.dump(2);
    }

    engine()->loadPreset(v1_path);
    auto snapshot = engine()->snapshotChain();
    EXPECT_TRUE(snapshot.slots.empty());

    // v2 preset should be rejected (unknown schema version).
    const auto v2_path = std::filesystem::temp_directory_path() / "us3_v2_preset.jvst";
    {
        nlohmann::json doc;
        doc["schema_version"] = 2;
        doc["preset_name"] = "V2";
        doc["created_at"] = "2026-06-06T00:00:00Z";
        doc["updated_at"] = "2026-06-06T00:00:00Z";
        doc["target_buffer_size"] = 256;
        doc["slots"] = nlohmann::json::array();

        std::ofstream ofs(v2_path, std::ios::binary);
        ofs << doc.dump(2);
    }

    engine()->loadPreset(v2_path);
    // Chain should remain unchanged because v2 was rejected.
    snapshot = engine()->snapshotChain();
    EXPECT_TRUE(snapshot.slots.empty());

    StopEngine();

    std::error_code ec;
    std::filesystem::remove(v1_path, ec);
    std::filesystem::remove(v2_path, ec);
}

}  // namespace jyglobalvst::testing
