// src/tray-app/presets/preset_folder_init.cpp
//
// T086 — Create %UserProfile%\Documents\JyGlobalVST\Presets\ on first launch if absent.

#include "platform/known_folders.h"

namespace jyglobalvst::tray {

void ensurePresetFolderExists()
{
    // known_folders.cpp already creates the directory in presetsDir().
    (void)jyglobalvst::shared::presetsDir();
}

}  // namespace jyglobalvst::tray
