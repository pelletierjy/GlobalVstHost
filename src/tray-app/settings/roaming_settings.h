// src/tray-app/settings/roaming_settings.h
//
// T087 — Roaming settings entity (%AppData%\Roaming\JyGlobalVST\settings.json).
// Forward-compatible: unknown fields preserved on save per FR-022k.

#pragma once

#include <nlohmann/json.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace jyglobalvst::tray {

struct RoamingSettings
{
    int schema_version {1};
    std::vector<std::string> custom_scan_paths;
    std::vector<std::string> disabled_default_paths;
    int default_buffer_size {256};
    std::string theme {"system"};  // "light" | "dark" | "system"
    std::optional<std::string> default_hardware_device_friendly_name;
    std::string update_check_endpoint_url;
    bool start_minimized_to_tray {false};
    float master_volume {1.0f};  // Master output gain (0.0–1.0), persisted across sessions.
    bool energy_saver_enabled {false};  // Auto-suspend the engine during silence (off by default).

    // T013: Driverless audio capture device persistence
    std::optional<std::string> capture_endpoint_id;     // Loopback source endpoint
    std::optional<std::string> output_endpoint_id;      // Output device endpoint
    bool follow_default_capture {true};                 // When true, resolve capture to system default

    // Raw unknown fields preserved for forward compatibility.
    nlohmann::json unknown_fields;
};

class RoamingSettingsStore
{
public:
    RoamingSettingsStore();

    // Load from disk or return defaults if missing/corrupt.
    RoamingSettings load() const;

    // Save to disk, merging with existing unknown fields.
    void save(const RoamingSettings& settings) const;

    std::filesystem::path settingsPath() const;

private:
    std::filesystem::path path_;
};

}  // namespace jyglobalvst::tray
