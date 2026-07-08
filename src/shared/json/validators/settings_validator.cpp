// src/shared/json/validators/settings_validator.cpp
// T018 — see header and contracts/settings-schema.json.

#include "settings_validator.h"

#include <algorithm>
#include <array>
#include <string>

namespace jyglobalvst::shared::json::validators {

namespace {

constexpr std::array<int, 6> kAllowedBufferSizes {32, 64, 128, 256, 512, 1024};
constexpr std::array<const char*, 3> kAllowedThemes {"light", "dark", "system"};

}  // namespace

ValidationResult validateSettings(const nlohmann::json& doc, ValidationMode mode)
{
    ValidationResult out;
    requireObject(doc, out);
    if (!doc.is_object())
    {
        return out;
    }

    // Unknown fields are explicitly allowed (FR-022k); we never reject them.
    // Strict mode here just promotes some warnings to errors.

    for (const char* required : {"schema_version", "default_buffer_size", "theme", "update_check_endpoint_url"})
    {
        requireField(doc, required, out);
    }

    if (doc.contains("schema_version")
        && (!doc["schema_version"].is_number_integer()
            || doc["schema_version"].get<int>() != kCurrentSettingsSchemaVersion))
    {
        (mode == ValidationMode::Strict ? out.addError("/schema_version", "expected version 1")
                                        : out.addWarning("/schema_version", "unrecognized version; using defaults"));
    }

    if (doc.contains("default_buffer_size"))
    {
        if (!doc["default_buffer_size"].is_number_integer()
            || std::find(kAllowedBufferSizes.begin(), kAllowedBufferSizes.end(),
                         doc["default_buffer_size"].get<int>())
                   == kAllowedBufferSizes.end())
        {
            // Per schema notes: invalid buffer falls back to 256 with a notification.
            out.addWarning("/default_buffer_size", "invalid value; falling back to 256");
        }
    }

    if (doc.contains("theme") && doc["theme"].is_string())
    {
        const auto& t = doc["theme"].get_ref<const std::string&>();
        if (std::find(kAllowedThemes.begin(), kAllowedThemes.end(), t) == kAllowedThemes.end())
        {
            if (mode == ValidationMode::Strict)
            {
                out.addError("/theme", "unknown theme; must be one of {light, dark, system}");
            }
            else
            {
                out.addWarning("/theme", "unknown theme; using 'system'");
            }
        }
    }

    if (doc.contains("update_check_endpoint_url"))
    {
        if (!doc["update_check_endpoint_url"].is_string()
            || doc["update_check_endpoint_url"].get<std::string>().rfind("https://", 0) != 0)
        {
            out.addError("/update_check_endpoint_url", "must be an https:// URL");
        }
    }

    if (doc.contains("custom_scan_paths"))
    {
        if (!doc["custom_scan_paths"].is_array())
        {
            out.addError("/custom_scan_paths", "must be an array of strings");
        }
        else
        {
            int i = 0;
            for (const auto& p : doc["custom_scan_paths"])
            {
                if (!p.is_string() || p.get<std::string>().size() > 4096)
                {
                    out.addError("/custom_scan_paths/" + std::to_string(i),
                                 "string ≤ 4096 chars required");
                }
                ++i;
            }
        }
    }

    return out;
}

}  // namespace jyglobalvst::shared::json::validators
