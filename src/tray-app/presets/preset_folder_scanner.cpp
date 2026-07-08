// src/tray-app/presets/preset_folder_scanner.cpp
//
// T085 — OneDrive/Drive-tolerant preset-folder scanner.
// Scans %UserProfile%\Documents\JyGlobalVST\Presets\ for *.jvst files,
// tolerating eventual-consistency file appearances/disappearances.

#include "platform/known_folders.h"

#include <filesystem>
#include <vector>

namespace jyglobalvst::tray {

std::vector<std::filesystem::path> scanPresetFolder()
{
    std::vector<std::filesystem::path> out;
    auto dir = jyglobalvst::shared::presetsDir();
    if (!std::filesystem::exists(dir))
    {
        return out;
    }

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
    {
        if (ec)
        {
            // Tolerance: skip entries that disappear during iteration.
            ec.clear();
            continue;
        }
        if (entry.is_regular_file(ec) && entry.path().extension() == ".jvst")
        {
            out.push_back(entry.path());
        }
    }
    return out;
}

}  // namespace jyglobalvst::tray
