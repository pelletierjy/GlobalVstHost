#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace jyglobalvst {

// Device state JSON schema for persistence
struct DeviceStateSchema {
    std::string device_guid;
    std::string friendly_name;
    std::string device_state;
    uint32_t sample_rate = 48000;
    uint16_t bit_depth = 32;
    uint16_t channels = 2;
    bool is_float = true;
    int64_t timestamp_registered = 0;
    int64_t timestamp_last_active = 0;
    std::string last_error;
    
    // JSON serialization
    nlohmann::json ToJson() const {
        nlohmann::json j;
        j["device_guid"] = device_guid;
        j["friendly_name"] = friendly_name;
        j["device_state"] = device_state;
        j["sample_rate"] = sample_rate;
        j["bit_depth"] = bit_depth;
        j["channels"] = channels;
        j["is_float"] = is_float;
        j["timestamp_registered"] = timestamp_registered;
        j["timestamp_last_active"] = timestamp_last_active;
        j["last_error"] = last_error;
        return j;
    }
    
    static DeviceStateSchema FromJson(const nlohmann::json& j) {
        DeviceStateSchema schema;
        if (j.contains("device_guid")) schema.device_guid = j["device_guid"];
        if (j.contains("friendly_name")) schema.friendly_name = j["friendly_name"];
        if (j.contains("device_state")) schema.device_state = j["device_state"];
        if (j.contains("sample_rate")) schema.sample_rate = j["sample_rate"];
        if (j.contains("bit_depth")) schema.bit_depth = j["bit_depth"];
        if (j.contains("channels")) schema.channels = j["channels"];
        if (j.contains("is_float")) schema.is_float = j["is_float"];
        if (j.contains("timestamp_registered")) schema.timestamp_registered = j["timestamp_registered"];
        if (j.contains("timestamp_last_active")) schema.timestamp_last_active = j["timestamp_last_active"];
        if (j.contains("last_error")) schema.last_error = j["last_error"];
        return schema;
    }
    
    bool IsValid() const {
        return !device_guid.empty() && !friendly_name.empty();
    }
};

}  // namespace jyglobalvst
