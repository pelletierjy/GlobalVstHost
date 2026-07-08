// src/shared/json/validators/settings_validator.h
// T018 — Roaming settings.json validator. Tolerant by default (FR-022k:
// preserve unknown fields on round-trip).

#pragma once

#include "../json_validator.h"

namespace jyglobalvst::shared::json::validators {

constexpr int kCurrentSettingsSchemaVersion = 1;
constexpr std::size_t kMaxSettingsFileSize = 1'048'576;  // 1 MB

ValidationResult validateSettings(const nlohmann::json& doc, ValidationMode mode);

}  // namespace jyglobalvst::shared::json::validators
