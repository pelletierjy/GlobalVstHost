// src/tray-app/presets/autosave.h
//
// T081 / T082 / T083 — Auto-save state persistence.
//
// Written on app close (FR-022c). Read on launch (FR-022d).
// Silently discarded on corruption. Preset-override flag suppresses write (FR-022e).

#pragma once

#include "jyglobalvst/audio_engine.h"

#include <filesystem>
#include <string>

namespace jyglobalvst::tray {

class AutoSaveStore
{
public:
    AutoSaveStore();

    // Serialize current chain + device + buffer to autosave.json.
    // suppress_slots_due_to_preset_override (FR-022e) skips the write entirely: an
    // explicit preset load overrides the auto-save, so the next launch must restore
    // that preset rather than a stale auto-save file.
    // theme_id is the active ThemeId cast to int (1–6).
    void write(IAudioEngine* engine, bool suppress_slots_due_to_preset_override,
               int theme_id = 1);

    // Deserialize and apply to engine. Returns true if restored.
    // If out_audio_running is non-null, set to the persisted running state.
    // If out_theme_id is non-null, set to the persisted theme (1–6, default 1).
    bool restore(IAudioEngine* engine, bool* out_audio_running = nullptr,
                 int* out_theme_id = nullptr) const;

    std::filesystem::path autosavePath() const;

private:
    std::filesystem::path path_;
};

}  // namespace jyglobalvst::tray
