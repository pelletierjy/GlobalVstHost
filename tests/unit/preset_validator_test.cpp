// tests/unit/preset_validator_test.cpp
//
// T071 / T077 — Unit tests for preset JSON validation.

#include "../../src/audio-engine/chain/preset_serializer.h"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

using namespace jyglobalvst::engine;

TEST(PresetValidatorTest, AcceptsMinimalValidPreset)
{
    nlohmann::json doc;
    doc["schema_version"] = 1;
    doc["preset_name"] = "Test";
    doc["created_at"] = "2026-06-06T12:00:00Z";
    doc["updated_at"] = "2026-06-06T12:00:00Z";
    doc["target_buffer_size"] = 256;
    doc["slots"] = nlohmann::json::array();

    auto errors = validatePresetDocument(doc, 1024);
    EXPECT_TRUE(errors.empty());
}

TEST(PresetValidatorTest, RejectsUnknownTopLevelField)
{
    nlohmann::json doc;
    doc["schema_version"] = 1;
    doc["preset_name"] = "Test";
    doc["created_at"] = "2026-06-06T12:00:00Z";
    doc["updated_at"] = "2026-06-06T12:00:00Z";
    doc["target_buffer_size"] = 256;
    doc["slots"] = nlohmann::json::array();
    doc["evil_field"] = "hax";

    auto errors = validatePresetDocument(doc, 1024);
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_NE(errors[0].message.find("Unknown top-level field"), std::string::npos);
}

TEST(PresetValidatorTest, RejectsFileSizeOver50MB)
{
    nlohmann::json doc;
    auto errors = validatePresetDocument(doc, 52428801);
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_NE(errors[0].message.find("50 MB"), std::string::npos);
}

TEST(PresetValidatorTest, RejectsInvalidSchemaVersion)
{
    nlohmann::json doc;
    doc["schema_version"] = 99;
    doc["preset_name"] = "Test";
    doc["created_at"] = "2026-06-06T12:00:00Z";
    doc["updated_at"] = "2026-06-06T12:00:00Z";
    doc["target_buffer_size"] = 256;
    doc["slots"] = nlohmann::json::array();

    auto errors = validatePresetDocument(doc, 1024);
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_NE(errors[0].message.find("schema_version"), std::string::npos);
}

TEST(PresetValidatorTest, RejectsStateChunkOver16MB)
{
    nlohmann::json doc;
    doc["schema_version"] = 1;
    doc["preset_name"] = "Test";
    doc["created_at"] = "2026-06-06T12:00:00Z";
    doc["updated_at"] = "2026-06-06T12:00:00Z";
    doc["target_buffer_size"] = 256;

    nlohmann::json slot;
    slot["position"] = 0;
    slot["plugin_uid"] = std::string(32, '0');
    slot["plugin_vendor"] = "V";
    slot["plugin_name"] = "P";
    slot["is_bypassed"] = false;
    // ~17 MB decoded base64 string (ceil(17MB/3)*4 chars).
    slot["state_chunk_b64"] = std::string(22600000, 'A');
    doc["slots"] = nlohmann::json::array({slot});

    auto errors = validatePresetDocument(doc, 1024);
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_NE(errors[0].message.find("16 MB"), std::string::npos);
}

TEST(PresetValidatorTest, Base64RoundTrip)
{
    juce::MemoryBlock original;
    original.append("hello world", 11);
    std::string b64 = base64Encode(original);
    auto decoded = base64Decode(b64);
    juce::MemoryBlock result(decoded.data(), decoded.size());
    EXPECT_EQ(result.toString().toStdString(), "hello world");
}
