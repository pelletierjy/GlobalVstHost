// src/shared/json/validators/scan_cache_validator.cpp
// T018 — see header and contracts/scan-cache-schema.json.

#include "scan_cache_validator.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace jyglobalvst::shared::json::validators {

namespace {

constexpr std::array<const char*, 4> kAllowedTopLevel {"schema_version", "scan_completed_at", "scanned_paths",
                                                       "plugins"};

constexpr std::array<const char*, 10> kAllowedPluginKeys {
    "plugin_uid", "name",         "vendor",     "version",         "file_path",
    "category",   "supports_double_precision", "has_editor", "scan_timestamp", "is_blocklisted_by_user"};

bool isHexUid(const std::string& s)
{
    return s.size() == 32 && std::all_of(s.begin(), s.end(),
                                         [](char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; });
}

void validateScannedPlugin(const nlohmann::json& p, int idx, ValidationResult& out)
{
    const std::string path = "/plugins/" + std::to_string(idx);
    if (!p.is_object())
    {
        out.addError(path, "expected object");
        return;
    }
    rejectUnknownFields(p, {kAllowedPluginKeys.begin(), kAllowedPluginKeys.end()}, out, path);

    for (const char* required : {"plugin_uid", "name", "vendor", "file_path", "scan_timestamp"})
    {
        if (!p.contains(required))
        {
            out.addError(path + "/" + required, "required field missing");
        }
    }
    if (p.contains("plugin_uid") && (!p["plugin_uid"].is_string() || !isHexUid(p["plugin_uid"].get<std::string>())))
    {
        out.addError(path + "/plugin_uid", "must be a 32-character hex string");
    }
}

}  // namespace

ValidationResult validateScanCache(const nlohmann::json& doc, ValidationMode mode)
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

    for (const char* required : {"schema_version", "scan_completed_at", "plugins"})
    {
        requireField(doc, required, out);
    }

    if (doc.contains("schema_version")
        && (!doc["schema_version"].is_number_integer()
            || doc["schema_version"].get<int>() != kCurrentScanCacheSchemaVersion))
    {
        out.addError("/schema_version", "expected version 1");
    }

    if (doc.contains("plugins"))
    {
        if (!doc["plugins"].is_array())
        {
            out.addError("/plugins", "must be an array");
        }
        else
        {
            int idx = 0;
            for (const auto& p : doc["plugins"])
            {
                validateScannedPlugin(p, idx, out);
                ++idx;
            }
        }
    }

    return out;
}

}  // namespace jyglobalvst::shared::json::validators
