// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • This file is NOT reached from the audio callback in the current
//     build configuration; it runs exclusively on UI / control threads.
//   • Kept under src/audio-engine/ for cohesion; header satisfies T107.
// =====================================================================
// src/audio-engine/vst-host/seh_wrapper.h
//
// T039 — SEH (Structured Exception Handling) wrapper for plugin processBlock.
//
// Per research.md §5: Wraps plugin processBlock in __try / __except + C++ try/catch
// to catch crashes. On failure, pre-allocated error notifications are queued for
// the UI thread. No allocation on the failure path (RT-safe).

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

namespace jyglobalvst::engine {

class SEHPluginWrapper
{
public:
    // Wrap a plugin's processBlock with exception handling.
    // Returns true if successful, false if an exception occurred.
    static bool processBlockSafe(juce::AudioPluginInstance* plugin,
                                 juce::AudioBuffer<float>& buffer,
                                 juce::MidiBuffer& midi) noexcept;

    // Wrap a plugin's prepareToPlay with SEH + C++ exception handling.
    // Returns true if successful, false if any exception occurred.
    static bool prepareToPlaySafe(juce::AudioPluginInstance* plugin,
                                  double sample_rate,
                                  int samples_per_block) noexcept;

    // Run an arbitrary callback under SEH + C++ exception guards. Used to isolate
    // plugin *loading* during a scan: a faulty VST3 can raise a Windows structured
    // exception (e.g. an access violation while its factory initialises) that a
    // plain C++ catch(...) cannot stop, which otherwise takes down the whole app.
    // `context` is passed straight through to `fn`. Returns true iff fn(context)
    // ran to completion without any C++ or structured exception.
    // NOTE: on a structured-exception escape, C++ objects constructed inside `fn`
    // are NOT unwound (SEH skips C++ destructors) — acceptable here because the
    // process would otherwise have crashed. Not for use on the audio thread.
    static bool invokeGuarded(void (*fn)(void*), void* context) noexcept;

    // Pre-allocate notification slots for error reporting.
    // Call once at initialization to ensure RT-path never allocates.
    static void prepareNotificationSlots(int num_slots);

private:
    static constexpr int kDefaultNotificationSlots = 16;
};

}  // namespace jyglobalvst::engine
