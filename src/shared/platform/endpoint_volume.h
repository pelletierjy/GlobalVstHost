// src/shared/platform/endpoint_volume.h
//
// T008 — Mute/restore wrapper for IAudioEndpointVolume.
//
// Wraps the Windows IAudioEndpointVolume COM interface to record
// the mute state on activation and suppress/restore it on close.
// Used to prevent feedback and double-audio from loopback capture
// (FR-018).

#pragma once

#include "jyglobalvst/types.h"

#include <memory>
#include <string>
#include <vector>

namespace jyglobalvst::shared {

class EndpointVolumeGuard
{
public:
    EndpointVolumeGuard();
    ~EndpointVolumeGuard();

    EndpointVolumeGuard(const EndpointVolumeGuard&) = delete;
    EndpointVolumeGuard& operator=(const EndpointVolumeGuard&) = delete;
    EndpointVolumeGuard(EndpointVolumeGuard&&) = delete;
    EndpointVolumeGuard& operator=(EndpointVolumeGuard&&) = delete;

    // Activate: record mute state on the given endpoint.
    // Returns true if muting is supported; false if fallback is required.
    bool activate(const EndpointId& endpoint_id);

    // Mute the endpoint (only if activate() succeeded).
    bool mute();

    // Restore the original mute state.
    bool restore();

    // Deactivate: release COM resources.
    void deactivate();

    [[nodiscard]] bool isActive() const noexcept;
    [[nodiscard]] bool isMuted() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace jyglobalvst::shared
