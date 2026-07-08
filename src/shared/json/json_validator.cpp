// src/shared/json/json_validator.cpp
// T017 — see header.

#include "json_validator.h"

namespace jyglobalvst::shared::json {

nlohmann::json parseOrThrow(std::string_view text)
{
    return nlohmann::json::parse(text);
}

std::optional<nlohmann::json> tryParse(std::string_view text)
{
    try
    {
        return nlohmann::json::parse(text);
    }
    catch (const nlohmann::json::parse_error&)
    {
        return std::nullopt;
    }
}

void requireObject(const nlohmann::json& doc, ValidationResult& out, const std::string& path)
{
    if (!doc.is_object())
    {
        out.addError(path.empty() ? "/" : path, "expected JSON object");
    }
}

void requireField(const nlohmann::json& doc, const std::string& key, ValidationResult& out,
                  const std::string& path)
{
    if (!doc.is_object() || !doc.contains(key))
    {
        out.addError(path + "/" + key, "required field missing");
    }
}

void rejectUnknownFields(const nlohmann::json& doc, const std::vector<std::string>& allowed,
                         ValidationResult& out, const std::string& path)
{
    if (!doc.is_object())
    {
        return;
    }
    for (auto it = doc.begin(); it != doc.end(); ++it)
    {
        bool found = false;
        for (const auto& a : allowed)
        {
            if (a == it.key())
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            out.addError(path + "/" + it.key(), "unknown field");
        }
    }
}

}  // namespace jyglobalvst::shared::json
