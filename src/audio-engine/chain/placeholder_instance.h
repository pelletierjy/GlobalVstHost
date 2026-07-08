// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • This file is NOT reached from the audio callback in the current
//     build configuration; it runs exclusively on UI / control threads.
//   • Kept under src/audio-engine/ for cohesion; header satisfies T107.
// =====================================================================
// src/audio-engine/chain/placeholder_instance.h
//
// T073 — PlaceholderInstance entity.
//
// A chain slot that points to an unresolved plugin (FR-022f).
// Audio-transparent; holds pending_state_chunk for re-instantiation.

#pragma once

#include <jyglobalvst/types.h>

#include <juce_core/juce_core.h>

#include <filesystem>
#include <memory>

namespace jyglobalvst::engine {

class PlaceholderInstance
{
public:
    PlaceholderInstance(const PluginRef& ref, std::filesystem::path path_hint);

    PlaceholderInstance(const PlaceholderInstance&) = delete;
    PlaceholderInstance& operator=(const PlaceholderInstance&) = delete;
    PlaceholderInstance(PlaceholderInstance&&) = default;
    PlaceholderInstance& operator=(PlaceholderInstance&&) = default;

    const PluginRef& recordedRef() const noexcept { return ref_; }
    const std::filesystem::path& pathHint() const noexcept { return path_hint_; }

    // State chunk held for re-instantiation when user re-points (FR-022f).
    void setPendingStateChunk(const juce::MemoryBlock& chunk);
    const juce::MemoryBlock& pendingStateChunk() const noexcept { return pending_state_chunk_; }

    InstanceId id() const noexcept { return instance_id_; }
    void setId(const InstanceId& id) noexcept { instance_id_ = id; }

    bool isBypassed() const noexcept { return bypassed_; }
    void setBypass(bool bypassed) noexcept { bypassed_ = bypassed; }

private:
    PluginRef ref_;
    std::filesystem::path path_hint_;
    juce::MemoryBlock pending_state_chunk_;
    InstanceId instance_id_;
    bool bypassed_ {false};
};

}  // namespace jyglobalvst::engine
