// src/shared/json/json_validator.h
//
// T017 — nlohmann/json wrapper with strict / tolerant validation modes.
//
// Strict mode (used on preset import per FR-022g-1): rejects unknown fields,
// out-of-range values, size limits.
// Tolerant mode (used on roaming settings read per FR-022k): accepts and
// preserves unknown fields, falls back on out-of-range with a notification.
//
// UI / control thread only. Never invoked from the audio thread.

#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

namespace jyglobalvst::shared::json {

enum class ValidationMode : std::uint8_t
{
    Strict,
    Tolerant,
};

struct ValidationError
{
    std::string path;     // JSON pointer to the offending value.
    std::string message;  // Human-readable explanation.
};

class ValidationResult
{
public:
    bool ok() const noexcept { return errors_.empty(); }
    const std::vector<ValidationError>& errors() const noexcept { return errors_; }
    const std::vector<ValidationError>& warnings() const noexcept { return warnings_; }

    void addError(std::string path, std::string message)
    {
        errors_.push_back({std::move(path), std::move(message)});
    }

    void addWarning(std::string path, std::string message)
    {
        warnings_.push_back({std::move(path), std::move(message)});
    }

    void merge(ValidationResult other)
    {
        for (auto& e : other.errors_)
        {
            errors_.push_back(std::move(e));
        }
        for (auto& w : other.warnings_)
        {
            warnings_.push_back(std::move(w));
        }
    }

private:
    std::vector<ValidationError> errors_;
    std::vector<ValidationError> warnings_;
};

// Parse helpers. Strict parse throws nlohmann::json::parse_error on syntax issues.
nlohmann::json parseOrThrow(std::string_view text);

// Returns std::nullopt on syntax error.
std::optional<nlohmann::json> tryParse(std::string_view text);

// Common field helpers used by all four schema validators.
void requireObject(const nlohmann::json& doc, ValidationResult& out, const std::string& path = "");
void requireField(const nlohmann::json& doc, const std::string& key, ValidationResult& out,
                  const std::string& path = "");
void rejectUnknownFields(const nlohmann::json& doc, const std::vector<std::string>& allowed,
                         ValidationResult& out, const std::string& path = "");

}  // namespace jyglobalvst::shared::json
