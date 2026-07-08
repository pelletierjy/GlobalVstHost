// src/shared/json/validators/update_manifest_validator.cpp
// T018 — see header and contracts/update-manifest-schema.json.

#include "update_manifest_validator.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace jyglobalvst::shared::json::validators {

namespace {

constexpr std::array<const char*, 6> kAllowedTopLevel {"schema_version",      "latest_version", "minimum_supported",
                                                       "release_notes_url",   "download_url",   "published_at"};

bool looksLikeSemver(const std::string& s)
{
    // Pragmatic check: digits.digits.digits with optional -prerelease.
    int dots = 0;
    bool seen_digit_in_segment = false;
    bool in_prerelease = false;
    for (char c : s)
    {
        if (c == '-' && !in_prerelease && seen_digit_in_segment && dots == 2)
        {
            in_prerelease = true;
            continue;
        }
        if (in_prerelease)
        {
            if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-'))
            {
                return false;
            }
            continue;
        }
        if (c == '.')
        {
            if (!seen_digit_in_segment)
            {
                return false;
            }
            ++dots;
            seen_digit_in_segment = false;
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c)) == 0)
        {
            return false;
        }
        seen_digit_in_segment = true;
    }
    return dots == 2 && seen_digit_in_segment;
}

bool startsWithHttps(const std::string& s)
{
    return s.rfind("https://", 0) == 0;
}

}  // namespace

ValidationResult validateUpdateManifest(const nlohmann::json& doc, ValidationMode mode)
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

    for (const char* required : {"schema_version", "latest_version", "release_notes_url", "download_url"})
    {
        requireField(doc, required, out);
    }

    if (doc.contains("schema_version")
        && (!doc["schema_version"].is_number_integer()
            || doc["schema_version"].get<int>() != kCurrentUpdateManifestSchemaVersion))
    {
        out.addError("/schema_version", "expected version 1");
    }

    for (const char* semver_field : {"latest_version", "minimum_supported"})
    {
        if (doc.contains(semver_field) && !doc[semver_field].is_null())
        {
            if (!doc[semver_field].is_string() || !looksLikeSemver(doc[semver_field].get<std::string>()))
            {
                out.addError(std::string("/") + semver_field, "must be semantic version (e.g. 1.2.3 or 1.2.3-beta)");
            }
        }
    }

    for (const char* url_field : {"release_notes_url", "download_url"})
    {
        if (doc.contains(url_field))
        {
            if (!doc[url_field].is_string() || !startsWithHttps(doc[url_field].get<std::string>()))
            {
                out.addError(std::string("/") + url_field, "must be an https:// URL");
            }
        }
    }

    return out;
}

}  // namespace jyglobalvst::shared::json::validators
