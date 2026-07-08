// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • This file is NOT reached from the audio callback in the current
//     build configuration; it runs exclusively on UI / control threads.
//   • Kept under src/audio-engine/ for cohesion; header satisfies T107.
// =====================================================================
// src/audio-engine/chain/placeholder_instance.cpp
//
// T073 — PlaceholderInstance implementation.

#include "placeholder_instance.h"

namespace jyglobalvst::engine {

PlaceholderInstance::PlaceholderInstance(const PluginRef& ref, std::filesystem::path path_hint)
    : ref_(ref)
    , path_hint_(std::move(path_hint))
{
}

void PlaceholderInstance::setPendingStateChunk(const juce::MemoryBlock& chunk)
{
    pending_state_chunk_ = chunk;
}

}  // namespace jyglobalvst::engine
