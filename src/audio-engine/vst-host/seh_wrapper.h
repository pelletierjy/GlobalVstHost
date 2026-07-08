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

    // Pre-allocate notification slots for error reporting.
    // Call once at initialization to ensure RT-path never allocates.
    static void prepareNotificationSlots(int num_slots);

private:
    static constexpr int kDefaultNotificationSlots = 16;
};

}  // namespace jyglobalvst::engine
