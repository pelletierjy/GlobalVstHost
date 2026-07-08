// src/shared/json/validators/update_manifest_validator.h
// T018 — update-manifest validator (Phase 8 release-only consumer, but the
// validator itself ships with shared infrastructure so the field is correctly
// rejected if a stray manifest is ever encountered).

#pragma once

#include "../json_validator.h"

namespace jyglobalvst::shared::json::validators {

constexpr int kCurrentUpdateManifestSchemaVersion = 1;

ValidationResult validateUpdateManifest(const nlohmann::json& doc, ValidationMode mode);

}  // namespace jyglobalvst::shared::json::validators
