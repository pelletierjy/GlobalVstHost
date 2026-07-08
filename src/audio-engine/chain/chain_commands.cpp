// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • This file is NOT reached from the audio callback in the current
//     build configuration; it runs exclusively on UI / control threads.
//   • Kept under src/audio-engine/ for cohesion; header satisfies T107.
// =====================================================================
// src/audio-engine/chain/chain_commands.cpp
//
// T059 — Chain commands implementation (placeholder; logic lives in
// audio_engine_impl.cpp processBlock and PluginChain direct calls).

#include "chain_commands.h"

namespace jyglobalvst::engine {

// No standalone implementation required; commands are drained inline in
// AudioEngineImpl::audioDeviceIOCallbackWithContext and applied via
// PluginChain::setBypass / PluginChain::setParameter.

}  // namespace jyglobalvst::engine
