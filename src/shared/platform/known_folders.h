// src/shared/platform/known_folders.h
//
// T014 — Windows known-folder resolvers. UI-thread only; never invoked from
// the audio thread.

#pragma once

#include <filesystem>

namespace jyglobalvst::shared {

// %AppData%\Roaming
std::filesystem::path roamingAppData();

// %LocalAppData%
std::filesystem::path localAppData();

// %UserProfile%\Documents
std::filesystem::path userDocuments();

// %ProgramFiles%
std::filesystem::path programFiles();

// Convenience helpers — return the JyGlobalVST subfolder, creating it if absent.
std::filesystem::path roamingSettingsDir();    // %AppData%\Roaming\JyGlobalVST
std::filesystem::path localStateDir();         // %LocalAppData%\JyGlobalVST
std::filesystem::path presetsDir();            // Documents\JyGlobalVST\Presets

}  // namespace jyglobalvst::shared
