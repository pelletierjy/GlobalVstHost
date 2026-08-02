// src/tray-app/settings/roaming_settings.cpp
//
// T087 — Roaming settings persistence.

#include "roaming_settings.h"

#include "platform/known_folders.h"

#include <algorithm>
#include <fstream>
#include <iostream>

namespace jyglobalvst::tray {

RoamingSettingsStore::RoamingSettingsStore()
    : path_(jyglobalvst::shared::roamingSettingsDir() / "settings.json")
{
}

std::filesystem::path RoamingSettingsStore::settingsPath() const
{
    return path_;
}

RoamingSettings RoamingSettingsStore::load() const
{
    RoamingSettings s;
    s.schema_version = 1;
    s.default_buffer_size = 512;
    s.theme = "system";
    s.update_check_endpoint_url = "https://jyglobalvst.local/update-manifest.json";

    std::ifstream ifs(path_);
    if (!ifs)
    {
        return s;  // First launch — return defaults.
    }

    nlohmann::json doc;
    try
    {
        ifs >> doc;
    }
    catch (const std::exception&)
    {
        return s;  // Corrupt — return defaults.
    }

    if (doc.contains("schema_version") && doc["schema_version"].is_number_integer())
    {
        s.schema_version = doc["schema_version"].get<int>();
    }
    if (doc.contains("custom_scan_paths") && doc["custom_scan_paths"].is_array())
    {
        s.custom_scan_paths = doc["custom_scan_paths"].get<std::vector<std::string>>();
    }
    if (doc.contains("disabled_default_paths") && doc["disabled_default_paths"].is_array())
    {
        s.disabled_default_paths = doc["disabled_default_paths"].get<std::vector<std::string>>();
    }
    if (doc.contains("default_buffer_size") && doc["default_buffer_size"].is_number_integer())
    {
        int v = doc["default_buffer_size"].get<int>();
        if (v == 32 || v == 64 || v == 128 || v == 256 || v == 512 || v == 1024)
        {
            s.default_buffer_size = v;
        }
    }
    if (doc.contains("theme") && doc["theme"].is_string())
    {
        s.theme = doc["theme"].get<std::string>();
    }
    if (doc.contains("default_hardware_device_friendly_name"))
    {
        if (doc["default_hardware_device_friendly_name"].is_string())
        {
            s.default_hardware_device_friendly_name = doc["default_hardware_device_friendly_name"].get<std::string>();
        }
        else if (doc["default_hardware_device_friendly_name"].is_null())
        {
            s.default_hardware_device_friendly_name = std::nullopt;
        }
    }
    if (doc.contains("update_check_endpoint_url") && doc["update_check_endpoint_url"].is_string())
    {
        s.update_check_endpoint_url = doc["update_check_endpoint_url"].get<std::string>();
    }
    if (doc.contains("start_minimized_to_tray") && doc["start_minimized_to_tray"].is_boolean())
    {
        s.start_minimized_to_tray = doc["start_minimized_to_tray"].get<bool>();
    }
    if (doc.contains("master_volume") && doc["master_volume"].is_number())
    {
        float v = doc["master_volume"].get<float>();
        s.master_volume = std::max(0.0f, std::min(1.0f, v));
    }
    if (doc.contains("energy_saver_enabled") && doc["energy_saver_enabled"].is_boolean())
    {
        s.energy_saver_enabled = doc["energy_saver_enabled"].get<bool>();
    }
    if (doc.contains("tooltips_enabled") && doc["tooltips_enabled"].is_boolean())
    {
        s.tooltips_enabled = doc["tooltips_enabled"].get<bool>();
    }
    if (doc.contains("drift_compensation_enabled") && doc["drift_compensation_enabled"].is_boolean())
    {
        s.drift_compensation_enabled = doc["drift_compensation_enabled"].get<bool>();
    }

    if (doc.contains("capture_endpoint_id"))
    {
        if (doc["capture_endpoint_id"].is_string())
        {
            s.capture_endpoint_id = doc["capture_endpoint_id"].get<std::string>();
        }
        else if (doc["capture_endpoint_id"].is_null())
        {
            s.capture_endpoint_id = std::nullopt;
        }
    }
    if (doc.contains("output_endpoint_id"))
    {
        if (doc["output_endpoint_id"].is_string())
        {
            s.output_endpoint_id = doc["output_endpoint_id"].get<std::string>();
        }
        else if (doc["output_endpoint_id"].is_null())
        {
            s.output_endpoint_id = std::nullopt;
        }
    }
    if (doc.contains("follow_default_capture") && doc["follow_default_capture"].is_boolean())
    {
        s.follow_default_capture = doc["follow_default_capture"].get<bool>();
    }

    // Preserve unknown fields.
    static const char* kKnown[] = {"schema_version", "custom_scan_paths", "disabled_default_paths",
                                    "default_buffer_size", "theme", "default_hardware_device_friendly_name",
                                    "update_check_endpoint_url", "start_minimized_to_tray", "master_volume",
                                    "energy_saver_enabled", "tooltips_enabled", "drift_compensation_enabled",
                                    "capture_endpoint_id", "output_endpoint_id", "follow_default_capture"};
    for (auto it = doc.begin(); it != doc.end(); ++it)
    {
        bool known = false;
        for (const char* key : kKnown)
        {
            if (it.key() == key)
            {
                known = true;
                break;
            }
        }
        if (!known)
        {
            s.unknown_fields[it.key()] = it.value();
        }
    }

    return s;
}

void RoamingSettingsStore::save(const RoamingSettings& settings) const
{
    nlohmann::json doc;
    doc["schema_version"] = settings.schema_version;
    doc["custom_scan_paths"] = settings.custom_scan_paths;
    doc["disabled_default_paths"] = settings.disabled_default_paths;
    doc["default_buffer_size"] = settings.default_buffer_size;
    doc["theme"] = settings.theme;
    doc["default_hardware_device_friendly_name"] = settings.default_hardware_device_friendly_name
                                                       ? nlohmann::json(*settings.default_hardware_device_friendly_name)
                                                       : nlohmann::json(nullptr);
    doc["update_check_endpoint_url"] = settings.update_check_endpoint_url;
    doc["start_minimized_to_tray"] = settings.start_minimized_to_tray;
    doc["master_volume"] = settings.master_volume;
    doc["energy_saver_enabled"] = settings.energy_saver_enabled;
    doc["tooltips_enabled"] = settings.tooltips_enabled;
    doc["drift_compensation_enabled"] = settings.drift_compensation_enabled;
    doc["capture_endpoint_id"] = settings.capture_endpoint_id
                                     ? nlohmann::json(*settings.capture_endpoint_id)
                                     : nlohmann::json(nullptr);
    doc["output_endpoint_id"] = settings.output_endpoint_id
                                    ? nlohmann::json(*settings.output_endpoint_id)
                                    : nlohmann::json(nullptr);
    doc["follow_default_capture"] = settings.follow_default_capture;

    // Merge unknown fields.
    for (auto it = settings.unknown_fields.begin(); it != settings.unknown_fields.end(); ++it)
    {
        doc[it.key()] = it.value();
    }

    std::filesystem::create_directories(path_.parent_path());
    std::ofstream ofs(path_, std::ios::binary);
    if (ofs)
    {
        ofs << doc.dump(2);
    }
}

}  // namespace jyglobalvst::tray
