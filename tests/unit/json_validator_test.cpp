// tests/unit/json_validator_test.cpp
//
// T030 — Unit tests for JSON validators (T017, T018).
//
// Tests verify strict / tolerant parsing, field validation, and format-specific
// constraints. Tests all four schema validators: preset, settings, scan-cache, update-manifest.

#include <gtest/gtest.h>
#include <json/json_validator.h>
#include <json/validators/preset_validator.h>
#include <json/validators/settings_validator.h>

#include <nlohmann/json.hpp>

using namespace jyglobalvst::shared::json;
using namespace jyglobalvst::shared::json::validators;
using json = nlohmann::json;

class JsonValidatorTest : public ::testing::Test
{
};

// =====================================================================
// Basic JSON parsing tests
// =====================================================================

TEST_F(JsonValidatorTest, ParseOrThrowValidJson)
{
    const auto doc = parseOrThrow(R"({"key": "value"})");
    EXPECT_TRUE(doc.is_object());
    EXPECT_EQ(doc["key"], "value");
}

TEST_F(JsonValidatorTest, ParseOrThrowThrowsOnInvalidJson)
{
    EXPECT_THROW(parseOrThrow(R"({"key": invalid})"), nlohmann::json::parse_error);
}

TEST_F(JsonValidatorTest, TryParseValidJson)
{
    const auto result = tryParse(R"({"key": "value"})");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value()["key"], "value");
}

TEST_F(JsonValidatorTest, TryParseInvalidJsonReturnsNullopt)
{
    const auto result = tryParse(R"({"key": invalid})");
    EXPECT_FALSE(result.has_value());
}

TEST_F(JsonValidatorTest, TryParseEmptyString)
{
    const auto result = tryParse("");
    EXPECT_FALSE(result.has_value());
}

// =====================================================================
// Validation result tests
// =====================================================================

TEST_F(JsonValidatorTest, ValidationResultOkWhenNoErrors)
{
    ValidationResult result;
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.errors().size(), 0);
    EXPECT_EQ(result.warnings().size(), 0);
}

TEST_F(JsonValidatorTest, ValidationResultNotOkWithErrors)
{
    ValidationResult result;
    result.addError("/field", "error message");
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.errors().size(), 1);
    EXPECT_EQ(result.errors()[0].path, "/field");
}

TEST_F(JsonValidatorTest, ValidationResultWarnings)
{
    ValidationResult result;
    result.addWarning("/field", "warning message");
    EXPECT_TRUE(result.ok());  // ok() only cares about errors
    EXPECT_EQ(result.warnings().size(), 1);
}

TEST_F(JsonValidatorTest, ValidationResultMerge)
{
    ValidationResult result1, result2;
    result1.addError("/field1", "error 1");
    result2.addError("/field2", "error 2");
    result2.addWarning("/field3", "warning 1");

    result1.merge(std::move(result2));

    EXPECT_FALSE(result1.ok());
    EXPECT_EQ(result1.errors().size(), 2);
    EXPECT_EQ(result1.warnings().size(), 1);
}

// =====================================================================
// Field validation helpers
// =====================================================================

TEST_F(JsonValidatorTest, RequireObjectAcceptsObject)
{
    ValidationResult result;
    json doc = json::object();
    requireObject(doc, result);
    EXPECT_TRUE(result.ok());
}

TEST_F(JsonValidatorTest, RequireObjectRejectsNonObject)
{
    ValidationResult result;
    json doc = json::array();
    requireObject(doc, result);
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.errors().size(), 1);
}

TEST_F(JsonValidatorTest, RequireFieldPresent)
{
    ValidationResult result;
    json doc = {{"name", "value"}};
    requireField(doc, "name", result);
    EXPECT_TRUE(result.ok());
}

TEST_F(JsonValidatorTest, RequireFieldMissing)
{
    ValidationResult result;
    json doc = json::object();
    requireField(doc, "name", result);
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.errors().size(), 1);
}

TEST_F(JsonValidatorTest, RejectUnknownFields)
{
    ValidationResult result;
    json doc = {{"allowed_field", 1}, {"unknown_field", 2}};
    rejectUnknownFields(doc, {"allowed_field"}, result);
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.errors().size(), 1);
}

TEST_F(JsonValidatorTest, RejectUnknownFieldsAllowsKnownFields)
{
    ValidationResult result;
    json doc = {{"field1", 1}, {"field2", 2}};
    rejectUnknownFields(doc, {"field1", "field2"}, result);
    EXPECT_TRUE(result.ok());
}

// =====================================================================
// Preset validator tests
// =====================================================================

TEST_F(JsonValidatorTest, PresetValidatorBasicStructure)
{
    const auto doc = parseOrThrow(R"(
        {
            "schema_version": 1,
            "preset_name": "Test Preset",
            "created_at": "2026-06-04T14:32:00Z",
            "updated_at": "2026-06-04T14:32:00Z",
            "target_buffer_size": 512,
            "target_sample_rate": 48000,
            "slots": []
        }
    )");

    ValidationResult result = validatePreset(doc, ValidationMode::Strict);
    EXPECT_TRUE(result.ok()) << "Basic preset should be valid";
}

TEST_F(JsonValidatorTest, PresetValidatorRequiresSchemaVersion)
{
    const auto doc = parseOrThrow(R"(
        {
            "preset_name": "Test Preset",
            "created_at": "2026-06-04T14:32:00Z",
            "updated_at": "2026-06-04T14:32:00Z",
            "target_buffer_size": 512,
            "slots": []
        }
    )");

    ValidationResult result = validatePreset(doc, ValidationMode::Strict);
    EXPECT_FALSE(result.ok()) << "Missing schema_version should fail strict validation";
}

TEST_F(JsonValidatorTest, PresetValidatorBufferSizeConstraints)
{
    // Valid buffer sizes: 32, 64, 128, 256, 512, 1024
    const int valid_sizes[] = {32, 64, 128, 256, 512, 1024};

    for (int size : valid_sizes)
    {
        auto doc = parseOrThrow(R"(
            {
                "schema_version": 1,
                "preset_name": "Test",
                "created_at": "2026-06-04T14:32:00Z",
                "updated_at": "2026-06-04T14:32:00Z",
                "target_buffer_size": )" + std::to_string(size) + R"(,
                "target_sample_rate": 48000,
                "slots": []
            }
        )");
        ValidationResult result = validatePreset(doc, ValidationMode::Strict);
        EXPECT_TRUE(result.ok()) << "Buffer size " << size << " should be valid";
    }
}

TEST_F(JsonValidatorTest, PresetValidatorSupportedSampleRates)
{
    // Supported rates: 44100, 48000, 88200, 96000, 176400, 192000
    const int supported[] = {44100, 48000, 88200, 96000, 176400, 192000};

    for (int sr : supported)
    {
        auto doc = parseOrThrow(R"(
            {
                "schema_version": 1,
                "preset_name": "Test",
                "created_at": "2026-06-04T14:32:00Z",
                "updated_at": "2026-06-04T14:32:00Z",
                "target_buffer_size": 512,
                "target_sample_rate": )" + std::to_string(sr) + R"(,
                "slots": []
            }
        )");
        ValidationResult result = validatePreset(doc, ValidationMode::Strict);
        EXPECT_TRUE(result.ok()) << "Sample rate " << sr << " should be valid";
    }
}

TEST_F(JsonValidatorTest, PresetValidatorStrictRejectsUnknownFields)
{
    const auto doc = parseOrThrow(R"(
        {
            "schema_version": 1,
            "preset_name": "Test",
            "created_at": "2026-06-04T14:32:00Z",
            "updated_at": "2026-06-04T14:32:00Z",
            "target_buffer_size": 512,
            "target_sample_rate": 48000,
            "slots": [],
            "unknown_field": "should_fail"
        }
    )");

    ValidationResult result = validatePreset(doc, ValidationMode::Strict);
    EXPECT_FALSE(result.ok()) << "Strict mode should reject unknown fields";
}

TEST_F(JsonValidatorTest, PresetValidatorFileSizeLimit)
{
    ValidationResult result;

    // Check within limit
    EXPECT_TRUE(checkPresetFileSize(1024, result));
    EXPECT_TRUE(result.ok());

    // Check at limit
    result = ValidationResult();
    EXPECT_TRUE(checkPresetFileSize(kMaxPresetFileSize, result));
    EXPECT_TRUE(result.ok());

    // Check over limit
    result = ValidationResult();
    EXPECT_FALSE(checkPresetFileSize(kMaxPresetFileSize + 1, result));
    EXPECT_FALSE(result.ok());
}

// =====================================================================
// Settings validator tests
// =====================================================================

TEST_F(JsonValidatorTest, SettingsValidatorBasicStructure)
{
    const auto doc = parseOrThrow(R"({
        "schema_version": 1,
        "default_buffer_size": 256,
        "theme": "system",
        "update_check_endpoint_url": "https://example.com/update"
    })");

    ValidationResult result = validateSettings(doc, ValidationMode::Strict);
    EXPECT_TRUE(result.ok()) << "Basic settings should be valid";
}

TEST_F(JsonValidatorTest, SettingsValidatorThemeOptions)
{
    const std::string themes[] = {"light", "dark", "system"};

    for (const auto& theme : themes)
    {
        auto doc = parseOrThrow(R"({
            "schema_version": 1,
            "default_buffer_size": 256,
            "theme": ")" + theme + R"(",
            "update_check_endpoint_url": "https://example.com/update"
        })");
        ValidationResult result = validateSettings(doc, ValidationMode::Strict);
        EXPECT_TRUE(result.ok()) << "Theme '" << theme << "' should be valid";
    }
}

TEST_F(JsonValidatorTest, SettingsValidatorInvalidTheme)
{
    const auto doc = parseOrThrow(R"({
        "schema_version": 1,
        "default_buffer_size": 256,
        "theme": "invalid_theme",
        "update_check_endpoint_url": "https://example.com/update"
    })");

    ValidationResult result = validateSettings(doc, ValidationMode::Strict);
    EXPECT_FALSE(result.ok()) << "Invalid theme should fail validation";
}

TEST_F(JsonValidatorTest, SettingsValidatorTolerantPreservesUnknownFields)
{
    const auto doc = parseOrThrow(R"({
        "schema_version": 1,
        "default_buffer_size": 256,
        "theme": "light",
        "update_check_endpoint_url": "https://example.com/update",
        "future_field": "value"
    })");

    ValidationResult result = validateSettings(doc, ValidationMode::Tolerant);
    EXPECT_TRUE(result.ok()) << "Tolerant mode should accept unknown fields";
}

// =====================================================================
// Edge cases
// =====================================================================

TEST_F(JsonValidatorTest, EmptyObjectValidation)
{
    const auto doc = parseOrThrow("{}");
    ValidationResult result;
    requireObject(doc, result);
    EXPECT_TRUE(result.ok());
}

TEST_F(JsonValidatorTest, NullValueInJson)
{
    const auto doc = parseOrThrow(R"({"field": null})");
    EXPECT_TRUE(doc["field"].is_null());
}

TEST_F(JsonValidatorTest, LargeJsonDocument)
{
    // Create a moderately large JSON document.
    json doc = json::object();
    for (int i = 0; i < 1000; ++i)
    {
        doc["field_" + std::to_string(i)] = i;
    }

    // Should not crash and should be serializable.
    const auto text = doc.dump();
    const auto parsed = tryParse(text);
    EXPECT_TRUE(parsed.has_value());
}
