// src/audio-engine/routing/hardware_output.cpp
//
// T037 — HardwareOutputDevice implementation.
//
// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • Getters are RT-safe: single atomic reads of immutable strings/flags.
//   • Setters are UI thread only (called during device selection).
//   • No allocation in getter paths; setters may allocate (UI thread safe).
// =====================================================================

#include "hardware_output.h"

namespace jyglobalvst::engine {

// All functionality is header-only or inline.
// This .cpp file is a placeholder for future release-prep enhancements
// (e.g., direct WASAPI endpoint binding, RT-safe getters with atomic flags).

}  // namespace jyglobalvst::engine
