// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • This file is NOT reached from the audio callback in the current
//     build configuration; it runs exclusively on UI / control threads.
//   • Kept under src/audio-engine/ for cohesion; header satisfies T107.
// =====================================================================
// src/audio-engine/vst-host/default_scan_paths.cpp
//
// T057 — Default scan paths implementation.

#include "default_scan_paths.h"

#include "../../shared/platform/known_folders.h"

#include <cstdlib>

namespace jyglobalvst::engine {

std::vector<std::filesystem::path> defaultVst3ScanPaths()
{
    // Test-only escape hatch: integration tests run rescanPlugins() against whatever
    // real, machine-specific VST3 plugins happen to be installed under these system
    // paths. That makes tests non-hermetic (catalog state depends on the dev machine)
    // and, worse, a misbehaving real plugin can hang the scan indefinitely — SEH only
    // catches structured exceptions, not hangs (see Constitution §V non-goals). Test
    // binaries set this env var so scans are fast, deterministic, and touch nothing
    // outside the test process. Never set in production/release builds.
    if (std::getenv("JYGLOBALVST_TEST_NO_DEFAULT_SCAN_PATHS") != nullptr)
    {
        return {};
    }

    std::vector<std::filesystem::path> paths;

    const auto program_files = shared::programFiles();
    if (!program_files.empty())
    {
        paths.push_back(program_files / "Common Files" / "VST3");
    }

    const auto local_app_data = shared::localAppData();
    if (!local_app_data.empty())
    {
        paths.push_back(local_app_data / "Programs" / "Common" / "VST3");
    }

    return paths;
}

}  // namespace jyglobalvst::engine
