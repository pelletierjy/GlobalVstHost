// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • This file is NOT reached from the audio callback in the current
//     build configuration; it runs exclusively on UI / control threads.
//   • Kept under src/audio-engine/ for cohesion; header satisfies T107.
// =====================================================================
// src/audio-engine/chain/chain_commands.h
//
// T059 — Chain mutation command definitions.
//
// Structural mutations (add/remove/move) are applied synchronously on the UI
// thread via PluginChain methods; the audio thread sees them through the
// atomic slot-list pointer.
//
// Per-slot mutations (bypass, parameter) are queued through the SPSC command
// queue and drained by the audio thread at the top of each processBlock.

#pragma once

#include <jyglobalvst/types.h>

#include <cstdint>

namespace jyglobalvst::engine {

// Command kinds delivered via SpscCommandQueue from UI thread to audio thread.
// The audio callback drains these and applies them to PluginChain.
enum class ChainCommandKind : std::uint8_t
{
    None,
    SetBypass,
    SetParameter,
};

struct ChainCommand
{
    ChainCommandKind kind {ChainCommandKind::None};
    int position {0};
    ParamId param_id {0};
    float value {0.f};
    bool flag {false};
};

}  // namespace jyglobalvst::engine
