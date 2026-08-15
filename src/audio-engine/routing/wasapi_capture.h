// src/audio-engine/routing/wasapi_capture.h
//
// T025 — Direct WASAPI capture client via IAudioClient.
//
// Opens a user-selected capture endpoint, starts a dedicated thread that waits
// on the capture client's event (event-driven, low-latency shared mode via
// IAudioClient3 when available; polling fallback otherwise), converts to
// 32-bit float, and writes into a LockFreeAudioRingBuffer for consumption by
// the ASIO callback thread.
//
// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • open()/close()/start()/stop() run on the UI thread only.
//   • The internal capture thread does NOT allocate after start().
//   • PCM → float conversion uses pre-allocated temp buffers.
// =====================================================================

#pragma once

#include "jyglobalvst/types.h"

#include "../../shared/concurrency/lockfree_ring_buffer.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace jyglobalvst::engine {

class WasapiCapture
{
public:
    WasapiCapture();
    ~WasapiCapture();

    WasapiCapture(const WasapiCapture&) = delete;
    WasapiCapture& operator=(const WasapiCapture&) = delete;
    WasapiCapture(WasapiCapture&&) = delete;
    WasapiCapture& operator=(WasapiCapture&&) = delete;

    // Open a capture endpoint. Call from UI / control thread only.
    // target_sample_rate is the rate the consumer expects (e.g. ASIO output
    // rate). The actual negotiated capture rate is exposed via
    // negotiatedSampleRate(). loopback=true opens a render endpoint in
    // loopback mode instead of a normal capture endpoint (FR-001, T004).
    // allow_exclusive additionally permits the low-latency exclusive-mode probe.
    // It is ignored when loopback is true: AUDCLNT_STREAMFLAGS_LOOPBACK is only
    // valid in shared mode, and probing exclusive on a render endpoint risks
    // seizing it from every other application.
    bool open(const EndpointId& endpoint_id,
              double target_sample_rate,
              shared::LockFreeAudioRingBuffer* ring_buffer,
              bool loopback = false,
              bool allow_exclusive = false);

    // Close and release all COM resources. Call from UI thread only.
    void close();

    // Start the capture thread. Call from UI thread only.
    bool start();

    // Stop the capture thread. Call from UI thread only.
    void stop();

    [[nodiscard]] bool isOpen() const noexcept;
    [[nodiscard]] bool isRunning() const noexcept;

    // False once the capture thread has given up on the stream — the endpoint was
    // invalidated, the audio service went away, or the client kept failing. The
    // thread stays alive but produces nothing, so isRunning() alone cannot tell
    // a healthy idle stream (a loopback endpoint with nothing playing legitimately
    // delivers no packets) from a dead one. The owner polls this and re-opens.
    [[nodiscard]] bool isHealthy() const noexcept;
    [[nodiscard]] double negotiatedSampleRate() const noexcept;
    [[nodiscard]] double targetSampleRate() const noexcept;
    [[nodiscard]] float latencyMs() const noexcept;
    [[nodiscard]] std::size_t xrunCount() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace jyglobalvst::engine
