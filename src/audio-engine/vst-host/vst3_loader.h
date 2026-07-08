// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • This file is NOT reached from the audio callback in the current
//     build configuration; it runs exclusively on UI / control threads.
//   • Kept under src/audio-engine/ for cohesion; header satisfies T107.
// =====================================================================
// src/audio-engine/vst-host/vst3_loader.h
//
// T038 — VST3PluginLoader.
//
// Wraps JUCE VST3PluginFormat to instantiate AudioPluginInstance from a .vst3
// bundle path. Used by the plugin chain to load plugins on demand.

#pragma once

#include "plugin_instance.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <filesystem>
#include <memory>
#include <string>

namespace jyglobalvst::engine {

class VST3PluginLoader
{
public:
    VST3PluginLoader();
    ~VST3PluginLoader() = default;

    // Load a VST3 plugin from a bundle path (e.g., "C:\...\MyPlugin.vst3").
    // Returns a PluginInstance on success, or nullptr + error string on failure.
    struct LoadResult
    {
        std::unique_ptr<PluginInstance> instance;
        std::string error;

        explicit operator bool() const noexcept { return instance != nullptr; }
    };

    LoadResult load(const std::filesystem::path& bundle_path);

private:
};

}  // namespace jyglobalvst::engine
