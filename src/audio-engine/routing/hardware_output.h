// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • This file is NOT reached from the audio callback in the current
//     build configuration; it runs exclusively on UI / control threads.
//   • Kept under src/audio-engine/ for cohesion; header satisfies T107.
// =====================================================================
// src/audio-engine/routing/hardware_output.h
//
// T037 — HardwareOutputDevice entity.
//
// Per data-model.md §2: tracks the selected hardware output endpoint with
// endpoint ID, friendly name, and device state flags. RT-safe after preparation.

#pragma once

#include <jyglobalvst/types.h>

#include <string>

namespace jyglobalvst::engine {

// Describes the current selected hardware output device.
class HardwareOutputDevice
{
public:
    HardwareOutputDevice() = default;

    const EndpointId& endpoint_id() const noexcept { return endpoint_id_; }
    void set_endpoint_id(const EndpointId& id) noexcept { endpoint_id_ = id; }

    const std::string& friendly_name() const noexcept { return friendly_name_; }
    void set_friendly_name(const std::string& name) noexcept { friendly_name_ = name; }

    bool is_present() const noexcept { return is_present_; }
    void set_present(bool present) noexcept { is_present_ = present; }

    bool is_default() const noexcept { return is_default_; }
    void set_default(bool def) noexcept { is_default_ = def; }

private:
    EndpointId endpoint_id_;
    std::string friendly_name_;
    bool is_present_ {false};
    bool is_default_ {false};
};

}  // namespace jyglobalvst::engine
