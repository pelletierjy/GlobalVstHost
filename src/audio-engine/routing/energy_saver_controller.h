// src/audio-engine/routing/energy_saver_controller.h
//
// Pure, thread- and audio-agnostic decision logic for the Energy Saver feature.
// Kept free of atomics, threading, and audio concerns so it can be unit-tested
// deterministically with synthetic time and level samples. The engine drives it
// from a low-rate polling thread (see AudioEngineImpl::energySaverThreadLoop);
// the atomics that the audio callback reads live in the engine, not here.
//
// State machine:
//   • disabled          → never sleeps; idle timer held at "now".
//   • enabled, awake     → after `idle_ms` of input below the wake threshold,
//                          transitions to sleeping.
//   • enabled, sleeping  → the moment input rises above the wake threshold,
//                          transitions back to awake.

#pragma once

#include <cmath>

namespace jyglobalvst::engine {

class EnergySaverController
{
public:
    // idle_ms: sustained silence before sleeping. wake_db: input peak level (dBFS)
    // at or above which audio is considered "present".
    EnergySaverController(int idle_ms, float wake_db)
        : idle_ms_(idle_ms), wake_peak_(std::pow(10.0f, wake_db / 20.0f))
    {
    }

    // Toggling off can never leave the machine asleep.
    void setEnabled(bool enabled)
    {
        enabled_ = enabled;
        if (!enabled_)
            sleeping_ = false;
    }

    bool enabled() const { return enabled_; }
    bool sleeping() const { return sleeping_; }

    // Linear peak amplitude that counts as the silence/audio boundary (exposed
    // for tests that want to synthesise levels just above/below it).
    float wakePeak() const { return wake_peak_; }

    // Restart the idle timer and wake; call when monitoring (re)starts.
    void reset(long long now_ms)
    {
        last_active_ms_ = now_ms;
        sleeping_ = false;
    }

    // Advance the machine. `now_ms` is a monotonic timestamp in milliseconds;
    // `input_peak` is the current linear input peak amplitude. Returns true iff
    // the sleeping state changed as a result of this call.
    bool update(long long now_ms, float input_peak)
    {
        const bool audio_present = input_peak > wake_peak_;

        if (!enabled_)
        {
            last_active_ms_ = now_ms;
            return setSleeping(false);
        }

        if (sleeping_)
        {
            if (audio_present)
            {
                last_active_ms_ = now_ms;
                return setSleeping(false);
            }
            return false;
        }

        if (audio_present)
        {
            last_active_ms_ = now_ms;
            return false;
        }

        if (now_ms - last_active_ms_ >= idle_ms_)
            return setSleeping(true);

        return false;
    }

private:
    bool setSleeping(bool s)
    {
        if (sleeping_ == s)
            return false;
        sleeping_ = s;
        return true;
    }

    int idle_ms_;
    float wake_peak_;
    bool enabled_ {false};
    bool sleeping_ {false};
    long long last_active_ms_ {0};
};

}  // namespace jyglobalvst::engine
