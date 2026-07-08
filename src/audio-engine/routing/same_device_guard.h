// src/audio-engine/routing/same_device_guard.h
//
// T009 — Conflict detection for capture and output device selection.
//
// Resolves capture and output EndpointIds (following "system default"
// where applicable) and reports whether they are the same device.
// Used to hard-block start() when capture == output (FR-005, FR-014).

#pragma once

#include "jyglobalvst/types.h"

#include <memory>

namespace jyglobalvst::engine {

class SameDeviceGuard
{
public:
    SameDeviceGuard();
    ~SameDeviceGuard();

    SameDeviceGuard(const SameDeviceGuard&) = delete;
    SameDeviceGuard& operator=(const SameDeviceGuard&) = delete;
    SameDeviceGuard(SameDeviceGuard&&) = delete;
    SameDeviceGuard& operator=(SameDeviceGuard&&) = delete;

    // Check if capture and output resolve to the same device.
    // Returns the device ID if they conflict, or empty EndpointId if distinct.
    [[nodiscard]] EndpointId checkConflict(
        const EndpointId& capture_id,
        const EndpointId& output_id) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace jyglobalvst::engine
