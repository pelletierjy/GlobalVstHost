// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • No allocation, no lock acquisition, no file I/O, no logging,
//     no GUI calls inside processBlock / audioDeviceIOCallbackWithContext.
// =====================================================================
// src/audio-engine/vst-host/seh_wrapper.cpp
//
// T039 — SEH wrapper implementation.
//
// Catches both C++ exceptions and Windows structured exceptions (SEH) to
// prevent plugin crashes from bringing down the entire audio engine.

#include "seh_wrapper.h"

#include <exception>

#if defined(_WIN32)
#    include <windows.h>
#    include <excpt.h>
#endif

namespace jyglobalvst::engine {

namespace {

#if defined(_WIN32)
// SEH and C++ exception handling cannot coexist in one function (/EHsc).
bool processBlockWithSeh(juce::AudioPluginInstance* plugin,
                         juce::AudioBuffer<float>& buffer,
                         juce::MidiBuffer& midi) noexcept
{
    __try
    {
        plugin->processBlock(buffer, midi);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool prepareToPlayWithSeh(juce::AudioPluginInstance* plugin,
                           double sample_rate,
                           int samples_per_block) noexcept
{
    __try
    {
        plugin->prepareToPlay(sample_rate, samples_per_block);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
#endif

}  // namespace

bool SEHPluginWrapper::processBlockSafe(juce::AudioPluginInstance* plugin,
                                        juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& midi) noexcept
{
    if (!plugin)
        return false;

    try
    {
#if defined(_WIN32)
        return processBlockWithSeh(plugin, buffer, midi);
#else
        plugin->processBlock(buffer, midi);
        return true;
#endif
    }
    catch (const std::exception&)
    {
        // Caught a C++ exception. Return failure.
        return false;
    }
    catch (...)
    {
        // Catch-all for any other exception type.
        return false;
    }
}

bool SEHPluginWrapper::prepareToPlaySafe(juce::AudioPluginInstance* plugin,
                                          double sample_rate,
                                          int samples_per_block) noexcept
{
    if (!plugin)
        return false;

    try
    {
#if defined(_WIN32)
        return prepareToPlayWithSeh(plugin, sample_rate, samples_per_block);
#else
        plugin->prepareToPlay(sample_rate, samples_per_block);
        return true;
#endif
    }
    catch (const std::exception&)
    {
        return false;
    }
    catch (...)
    {
        return false;
    }
}

void SEHPluginWrapper::prepareNotificationSlots(int num_slots)
{
    // Pre-allocate notification ring buffer for error reporting.
    // Future: implement in conjunction with IAudioEngineListener::onPluginFailed.
    (void)num_slots;
}

}  // namespace jyglobalvst::engine
