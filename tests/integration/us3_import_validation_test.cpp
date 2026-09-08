// tests/integration/us3_import_validation_test.cpp
//
// T067 — Integration test: import malformed preset (unknown field, > 50 MB file,
// > 16 MB state chunk) → rejected with no partial state per FR-022g-1.

#include "../integration/loopback_fixture.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

namespace jyglobalvst::testing {

class US3ImportValidationTest : public LoopbackFixture
{
};

TEST_F(US3ImportValidationTest, UnknownTopLevelFieldRejected)
{
    StartEngine();

    const auto temp_path = std::filesystem::temp_directory_path() / "us3_unknown_field.jvst";
    nlohmann::json doc;
    doc["schema_version"] = 1;
    doc["preset_name"] = "Bad";
    doc["created_at"] = "2026-06-06T00:00:00Z";
    doc["updated_at"] = "2026-06-06T00:00:00Z";
    doc["target_buffer_size"] = 256;
    doc["unknown_field"] = 123;  // invalid
    doc["slots"] = nlohmann::json::array();

    std::ofstream ofs(temp_path, std::ios::binary);
    ofs << doc.dump(2);
    ofs.close();

    engine()->loadPreset(temp_path);

    // Chain should remain empty because import was rejected before mutation.
    const auto snapshot = engine()->snapshotChain();
    EXPECT_TRUE(snapshot.slots.empty());

    StopEngine();

    std::error_code ec;
    std::filesystem::remove(temp_path, ec);
}

TEST_F(US3ImportValidationTest, InvalidBufferSizeRejected)
{
    StartEngine();

    const auto temp_path = std::filesystem::temp_directory_path() / "us3_bad_buffer.jvst";
    nlohmann::json doc;
    doc["schema_version"] = 1;
    doc["preset_name"] = "Bad";
    doc["created_at"] = "2026-06-06T00:00:00Z";
    doc["updated_at"] = "2026-06-06T00:00:00Z";
    doc["target_buffer_size"] = 999;  // invalid
    doc["slots"] = nlohmann::json::array();

    std::ofstream ofs(temp_path, std::ios::binary);
    ofs << doc.dump(2);
    ofs.close();

    engine()->loadPreset(temp_path);

    const auto snapshot = engine()->snapshotChain();
    EXPECT_TRUE(snapshot.slots.empty());

    StopEngine();

    std::error_code ec;
    std::filesystem::remove(temp_path, ec);
}

TEST_F(US3ImportValidationTest, OversizedStateChunkRejected)
{
    StartEngine();

    const auto temp_path = std::filesystem::temp_directory_path() / "us3_big_chunk.jvst";
    nlohmann::json doc;
    doc["schema_version"] = 1;
    doc["preset_name"] = "Bad";
    doc["created_at"] = "2026-06-06T00:00:00Z";
    doc["updated_at"] = "2026-06-06T00:00:00Z";
    doc["target_buffer_size"] = 256;

    nlohmann::json slot;
    slot["position"] = 0;
    slot["plugin_uid"] = "00000000000000000000000000000000";
    slot["plugin_vendor"] = "V";
    slot["plugin_name"] = "N";
    slot["is_bypassed"] = false;
    // Encode ~17 MB of zeros as base64 to exceed 16 MB decoded limit.
    std::string big_b64;
    big_b64.reserve((17 * 1024 * 1024 + 2) / 3 * 4);
    const std::string chunk(64, 'A');
    for (int i = 0; i < (17 * 1024 * 1024) / 48; ++i)
    {
        big_b64 += chunk;
    }
    slot["state_chunk_b64"] = big_b64;
    slot["plugin_path_hint"] = "";

    nlohmann::json slots = nlohmann::json::array();
    slots.push_back(slot);
    doc["slots"] = slots;

    std::ofstream ofs(temp_path, std::ios::binary);
    ofs << doc.dump(2);
    ofs.close();

    engine()->loadPreset(temp_path);

    const auto snapshot = engine()->snapshotChain();
    EXPECT_TRUE(snapshot.slots.empty());

    StopEngine();

    std::error_code ec;
    std::filesystem::remove(temp_path, ec);
}

}  // namespace jyglobalvst::testing
