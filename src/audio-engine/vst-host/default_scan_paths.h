// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • This file is NOT reached from the audio callback in the current
//     build configuration; it runs exclusively on UI / control threads.
//   • Kept under src/audio-engine/ for cohesion; header satisfies T107.
// =====================================================================
// src/audio-engine/vst-host/default_scan_paths.h
//
// T057 — Pre-seed default VST3 scan paths.
//
// Returns the two standard Windows VST3 directories per FR-005:
//   - %ProgramFiles%\Common Files\VST3
//   - %LocalAppData%\Programs\Common\VST3

#pragma once

#include <filesystem>
#include <vector>

namespace jyglobalvst::engine {

std::vector<std::filesystem::path> defaultVst3ScanPaths();

}  // namespace jyglobalvst::engine
