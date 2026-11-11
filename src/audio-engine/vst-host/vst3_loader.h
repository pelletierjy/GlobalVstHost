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

    // Read a plugin's metadata WITHOUT instantiating it (T-scan memory fix).
    // Uses VST3PluginFormat::findAllTypesForFile — the same call JUCE's own
    // plugin scanner uses — so no AudioPluginInstance is ever created: no DSP
    // buffers, no editor, nothing to retain after the scan.
    //
    // The returned descriptor.uid is derived exactly like the instance path does
    // (hash of PluginDescription::createIdentifierString()) so catalog entries,
    // presets and scan-cache refs keep matching; fileOrIdentifier is normalized
    // to the module file inside the bundle, mirroring createPluginInstance.
    struct DescribeResult
    {
        Plugin descriptor;
        std::string error;

        explicit operator bool() const noexcept { return error.empty(); }
    };

    DescribeResult describe(const std::filesystem::path& bundle_path);

private:
};

}  // namespace jyglobalvst::engine
