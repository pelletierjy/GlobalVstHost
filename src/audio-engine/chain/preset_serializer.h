// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • This file is NOT reached from the audio callback in the current
//     build configuration; it runs exclusively on UI / control threads.
//   • Kept under src/audio-engine/ for cohesion; header satisfies T107.
// =====================================================================
// src/audio-engine/chain/preset_serializer.h
//
// T072 / T075 / T076 / T077 — Preset serialization, validation, and load/save helpers.

#pragma once

#include "plugin_chain.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace jyglobalvst::engine {

struct PresetValidationError
{
    std::string message;
};

struct PresetLoadResult
{
    bool ok {false};
    std::vector<PresetValidationError> errors;
    std::vector<MissingPluginInfo> missing;
};

// Strict validation per FR-022g-1.
// Returns empty errors vector if valid.
std::vector<PresetValidationError> validatePresetDocument(const nlohmann::json& doc,
                                                           std::size_t file_size_bytes);

// Build a preset JSON document from the current chain state.
nlohmann::json serializePreset(const PluginChain& chain,
                                const std::string& preset_name,
                                int target_buffer_size,
                                int target_sample_rate,
                                const std::string& target_device_friendly_name,
                                const std::string& input_endpoint_id,
                                const std::string& output_endpoint_id,
                                bool audio_running);

// Deserialize a preset and apply it to the chain via the provided callbacks.
// `resolvePlugin` should return a loaded PluginInstance (or nullptr if not found).
// `onPlaceholder` is called for unresolved slots so the caller can add a placeholder.
struct ResolvedPlugin
{
    std::shared_ptr<PluginInstance> instance;
    juce::MemoryBlock state_chunk; // decoded from base64
};

PresetLoadResult deserializePreset(const nlohmann::json& doc,
                                    const std::function<ResolvedPlugin(const PluginRef& ref)>& resolvePlugin,
                                    const std::function<void(std::shared_ptr<PlaceholderInstance> placeholder)>& onPlaceholder);

// Base64 helpers.
std::string base64Encode(const juce::MemoryBlock& data);
std::vector<std::uint8_t> base64Decode(const std::string& b64);

}  // namespace jyglobalvst::engine
