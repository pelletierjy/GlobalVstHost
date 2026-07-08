// src/audio-engine/routing/wasapi_output.h
//
// T005 — Direct WASAPI render client via IAudioClient.
//
// Opens a user-selected render endpoint and implements the real IAudioClient
// render client for playback. Mirrors WasapiCapture's thread/no-alloc-after-open
// discipline.
//
// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • open()/close()/start()/stop() run on the UI thread only.
//   • The internal render thread does NOT allocate after start().
//   • The audio callback drains from the ring buffer with no allocations.
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

class WasapiOutput
{
public:
    WasapiOutput();
    ~WasapiOutput();

    WasapiOutput(const WasapiOutput&) = delete;
    WasapiOutput& operator=(const WasapiOutput&) = delete;
    WasapiOutput(WasapiOutput&&) = delete;
    WasapiOutput& operator=(WasapiOutput&&) = delete;

    // Open a render endpoint. Call from UI / control thread only.
    // target_sample_rate is the rate the audio thread will provide.
    // Returns true on success.
    bool open(const EndpointId& endpoint_id, double target_sample_rate);

    // Close and release all COM resources. Call from UI thread only.
    void close();

    // Start the render thread. Call from UI thread only.
    bool start(shared::LockFreeAudioRingBuffer* ring_buffer);

    // Stop the render thread. Call from UI thread only.
    void stop();

    [[nodiscard]] bool isOpen() const noexcept;
    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] double negotiatedSampleRate() const noexcept;
    [[nodiscard]] double targetSampleRate() const noexcept;
    [[nodiscard]] float latencyMs() const noexcept;
    [[nodiscard]] std::size_t xrunCount() const noexcept;

private:
    void renderThreadLoop();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace jyglobalvst::engine
