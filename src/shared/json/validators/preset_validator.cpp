// src/shared/json/validators/preset_validator.cpp
// T018 — see header and contracts/preset-schema.json.

#include "preset_validator.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace jyglobalvst::shared::json::validators {

namespace {

constexpr std::array<int, 6> kAllowedSampleRates {44100, 48000, 88200, 96000, 176400, 192000};
constexpr std::array<int, 6> kAllowedBufferSizes {32, 64, 128, 256, 512, 1024};

constexpr std::array<const char*, 8> kAllowedTopLevel {
    "schema_version", "preset_name",          "created_at",
    "updated_at",     "target_sample_rate",   "target_buffer_size",
    "target_device_friendly_name",            "slots"};

constexpr std::array<const char*, 8> kAllowedSlotKeys {
    "position",       "plugin_uid",    "plugin_vendor",   "plugin_name",
    "plugin_path_hint", "is_bypassed", "state_chunk_b64", "tag"};

bool isHexUid(const std::string& s)
{
    if (s.size() != 32)
    {
        return false;
    }
    return std::all_of(s.begin(), s.end(),
                       [](char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; });
}

std::size_t decodedBase64Size(std::size_t encoded_len, std::size_t pad)
{
    if (encoded_len == 0)
    {
        return 0;
    }
    return (encoded_len / 4) * 3 - pad;
}

std::size_t base64Padding(const std::string& s)
{
    std::size_t pad = 0;
    if (!s.empty() && s.back() == '=')
    {
        ++pad;
        if (s.size() >= 2 && s[s.size() - 2] == '=')
        {
            ++pad;
        }
    }
    return pad;
}

void validateSlot(const nlohmann::json& slot, int expected_position, ValidationResult& out)
{
    const std::string path = "/slots/" + std::to_string(expected_position);

    if (!slot.is_object())
    {
        out.addError(path, "slot must be an object");
        return;
    }

    rejectUnknownFields(slot, {kAllowedSlotKeys.begin(), kAllowedSlotKeys.end()}, out, path);

    for (const char* required : {"position", "plugin_uid", "plugin_vendor", "plugin_name",
                                 "is_bypassed", "state_chunk_b64"})
    {
        if (!slot.contains(required))
        {
            out.addError(path + "/" + required, "required field missing");
        }
    }

    if (slot.contains("position"))
    {
        if (!slot["position"].is_number_integer() || slot["position"].get<int>() != expected_position)
        {
            out.addError(path + "/position",
                         "position must equal slot index (" + std::to_string(expected_position) + ")");
        }
    }
    if (slot.contains("plugin_uid"))
    {
        if (!slot["plugin_uid"].is_string() || !isHexUid(slot["plugin_uid"].get<std::string>()))
        {
            out.addError(path + "/plugin_uid", "must be a 32-character hex string");
        }
    }
    if (slot.contains("plugin_vendor") && (!slot["plugin_vendor"].is_string()
        || slot["plugin_vendor"].get<std::string>().empty()
        || slot["plugin_vendor"].get<std::string>().size() > 256))
    {
        out.addError(path + "/plugin_vendor", "non-empty string ≤ 256 chars required");
    }
    if (slot.contains("plugin_name") && (!slot["plugin_name"].is_string()
        || slot["plugin_name"].get<std::string>().empty()
        || slot["plugin_name"].get<std::string>().size() > 256))
    {
        out.addError(path + "/plugin_name", "non-empty string ≤ 256 chars required");
    }
    if (slot.contains("plugin_path_hint") && !slot["plugin_path_hint"].is_string())
    {
        out.addError(path + "/plugin_path_hint", "must be a string");
    }
    if (slot.contains("is_bypassed") && !slot["is_bypassed"].is_boolean())
    {
        out.addError(path + "/is_bypassed", "must be boolean");
    }
    if (slot.contains("tag") && !slot["tag"].is_string())
    {
        out.addError(path + "/tag", "must be a string");
    }
    if (slot.contains("state_chunk_b64"))
    {
        if (!slot["state_chunk_b64"].is_string())
        {
            out.addError(path + "/state_chunk_b64", "must be base64 string");
        }
        else
        {
            const auto& s = slot["state_chunk_b64"].get_ref<const std::string&>();
            const auto decoded = decodedBase64Size(s.size(), base64Padding(s));
            if (decoded > kMaxStateChunkSize)
            {
                out.addError(path + "/state_chunk_b64",
                             "decoded size exceeds 16 MB (FR-022g-1)");
            }
        }
    }
}

}  // namespace

bool checkPresetFileSize(std::size_t bytes, ValidationResult& out)
{
    if (bytes > kMaxPresetFileSize)
    {
        out.addError("/", "preset file exceeds 50 MB cap (FR-022g-1)");
        return false;
    }
    return true;
}

ValidationResult validatePreset(const nlohmann::json& doc, ValidationMode mode)
{
    ValidationResult out;
    requireObject(doc, out);
    if (!doc.is_object())
    {
        return out;
    }

    if (mode == ValidationMode::Strict)
    {
        rejectUnknownFields(doc, {kAllowedTopLevel.begin(), kAllowedTopLevel.end()}, out);
    }

    for (const char* required : {"schema_version", "preset_name", "created_at", "updated_at",
                                 "target_buffer_size", "slots"})
    {
        requireField(doc, required, out);
    }

    if (doc.contains("schema_version"))
    {
        if (!doc["schema_version"].is_number_integer()
            || doc["schema_version"].get<int>() != kCurrentPresetSchemaVersion)
        {
            out.addError("/schema_version",
                         "expected " + std::to_string(kCurrentPresetSchemaVersion)
                             + " (FR-022b migration handled separately)");
        }
    }
    if (doc.contains("preset_name") && (!doc["preset_name"].is_string()
        || doc["preset_name"].get<std::string>().empty()
        || doc["preset_name"].get<std::string>().size() > 128))
    {
        out.addError("/preset_name", "non-empty string ≤ 128 chars required");
    }
    if (doc.contains("target_buffer_size"))
    {
        if (!doc["target_buffer_size"].is_number_integer()
            || std::find(kAllowedBufferSizes.begin(), kAllowedBufferSizes.end(),
                         doc["target_buffer_size"].get<int>())
                   == kAllowedBufferSizes.end())
        {
            out.addError("/target_buffer_size", "must be one of {32, 64, 128, 256, 512, 1024}");
        }
    }
    if (doc.contains("target_sample_rate") && !doc["target_sample_rate"].is_null())
    {
        if (!doc["target_sample_rate"].is_number_integer()
            || std::find(kAllowedSampleRates.begin(), kAllowedSampleRates.end(),
                         doc["target_sample_rate"].get<int>())
                   == kAllowedSampleRates.end())
        {
            out.addError("/target_sample_rate", "must be one of allowed sample rates or null");
        }
    }
    if (doc.contains("slots"))
    {
        if (!doc["slots"].is_array())
        {
            out.addError("/slots", "must be an array");
        }
        else
        {
            int idx = 0;
            for (const auto& slot : doc["slots"])
            {
                validateSlot(slot, idx, out);
                ++idx;
            }
        }
    }

    return out;
}

}  // namespace jyglobalvst::shared::json::validators
