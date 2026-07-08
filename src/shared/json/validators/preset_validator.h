// src/shared/json/validators/preset_validator.h
//
// T018 — Hand-rolled validator for the .jvst preset schema (preset-schema.json).
// Strict on import per FR-022g-1. UI / control thread only.

#pragma once

#include "../json_validator.h"

namespace jyglobalvst::shared::json::validators {

constexpr std::size_t kMaxPresetFileSize = 52'428'800;   // 50 MB
constexpr std::size_t kMaxStateChunkSize = 16'777'216;  // 16 MB decoded
constexpr int kCurrentPresetSchemaVersion = 1;

ValidationResult validatePreset(const nlohmann::json& doc, ValidationMode mode);

// Pre-parse helper. Returns false if size exceeds the FR-022g-1 cap.
[[nodiscard]] bool checkPresetFileSize(std::size_t bytes, ValidationResult& out);

}  // namespace jyglobalvst::shared::json::validators
