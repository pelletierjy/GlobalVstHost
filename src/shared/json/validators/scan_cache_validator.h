// src/shared/json/validators/scan_cache_validator.h
// T018 — scan-cache.json validator.

#pragma once

#include "../json_validator.h"

namespace jyglobalvst::shared::json::validators {

constexpr int kCurrentScanCacheSchemaVersion = 1;

ValidationResult validateScanCache(const nlohmann::json& doc, ValidationMode mode);

}  // namespace jyglobalvst::shared::json::validators
