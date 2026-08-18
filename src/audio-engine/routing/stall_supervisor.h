// src/audio-engine/routing/stall_supervisor.h
//
// Pure, thread- and audio-agnostic decision logic for audio-path stall
// detection. Kept free of atomics, threading, and audio concerns so it can be
// unit-tested deterministically with a synthetic clock and heartbeat counter.
// The engine drives it from the same low-rate polling thread as the Energy
// Saver (see AudioEngineImpl::energySaverThreadLoop); the atomics it samples
// live in the engine, not here.
//
// The signal is the audio callback's heartbeat counter. Both transports funnel
// through the same callback, so a counter that stops advancing while the engine
// believes it is running means no audio is flowing — the state a machine comes
// back in from standby when an ASIO driver never resumes streaming, or when a
// WASAPI render client was invalidated. Because the input meters are computed
// inside that callback, a dead callback also freezes the Energy Saver's wake
// detector, so nothing else notices.
//
// Recovery is a stop()/start() cycle on the control thread, which this class
// deliberately knows nothing about: update() only says "ask for a restart now".

#pragma once

#include <cstdint>

namespace jyglobalvst::engine {

class StallSupervisor
{
public:
    // stall_timeout_ms: heartbeat silence that counts as a stall. Must exceed the
    // longest legitimate gap between callbacks (a device starting up, a very large
    // buffer at a low rate). restart_backoff_ms: minimum spacing between restart
    // requests, so a transport that never recovers is not hammered.
    StallSupervisor(long long stall_timeout_ms, long long restart_backoff_ms)
        : stall_timeout_ms_(stall_timeout_ms), restart_backoff_ms_(restart_backoff_ms)
    {
    }

    // Arm the machine: the timeout window starts at `now_ms`, giving the
    // transport a full stall_timeout_ms to produce its first callback.
    void reset(long long now_ms, std::uint64_t heartbeat)
    {
        last_heartbeat_ = heartbeat;
        last_change_ms_ = now_ms;
        last_request_ms_ = now_ms - restart_backoff_ms_;
    }

    // Advance the machine. `heartbeat` is the audio callback's monotonic counter,
    // `running` is whether the engine believes it is streaming, and `suspended`
    // marks an intentional interruption during which a frozen heartbeat is
    // expected (e.g. the ASIO driver's modal control panel). Returns true iff a
    // restart should be requested right now.
    bool update(long long now_ms, std::uint64_t heartbeat, bool running, bool suspended)
    {
        if (heartbeat != last_heartbeat_)
        {
            last_heartbeat_ = heartbeat;
            last_change_ms_ = now_ms;
            return false;
        }

        if (!running || suspended)
        {
            // Hold the clock so the timeout is measured from the moment normal
            // operation resumes, not from before the interruption.
            last_change_ms_ = now_ms;
            return false;
        }

        if (now_ms - last_change_ms_ < stall_timeout_ms_)
            return false;

        if (now_ms - last_request_ms_ < restart_backoff_ms_)
            return false;

        last_request_ms_ = now_ms;
        // Re-arm the timeout as well, so a host that ignores the request is asked
        // again at the backoff rate rather than on every subsequent tick.
        last_change_ms_ = now_ms;
        return true;
    }

    // Milliseconds since the heartbeat last advanced (exposed for diagnostics).
    long long silentForMs(long long now_ms) const { return now_ms - last_change_ms_; }

private:
    long long stall_timeout_ms_;
    long long restart_backoff_ms_;
    std::uint64_t last_heartbeat_ {0};
    long long last_change_ms_ {0};
    long long last_request_ms_ {0};
};

}  // namespace jyglobalvst::engine
