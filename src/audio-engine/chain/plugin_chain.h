// src/audio-engine/chain/plugin_chain.h
//
// T058 — PluginChain: multi-plugin chain using juce::AudioProcessorGraph wrapper.
// Ordered slots, chain_revision monotonic counter.
//
// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • processBlock is the only audio-thread entry point.
//   • It reads an atomic pointer to the active slot list — no mutex,
//     no allocation, no exception.
//   • Structural mutations (add/remove/move) are UI-thread only; they
//     build a new slot list and atomically publish it.
//   • The old slot list is retired lazily on the next UI-thread mutation.
// =====================================================================

#pragma once

#include "../vst-host/plugin_instance.h"
#include "placeholder_instance.h"

#include <jyglobalvst/types.h>

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace jyglobalvst::engine {

class PluginChain : public juce::AudioProcessor
{
public:
    struct Slot
    {
        InstanceId instance_id;
        std::shared_ptr<PluginInstance> instance;
        std::shared_ptr<PlaceholderInstance> placeholder;
        std::atomic<bool> is_bypassed {false};
        std::atomic<bool> is_failed {false};
        // Transient counter: incremented on each consecutive processBlock failure,
        // reset on success. Plugin is permanently marked failed only after
        // kMaxConsecutiveFailures, preventing spurious startup failures.
        std::atomic<int> failure_count {0};
        // Optional user-assigned label. UI-thread only: the audio thread never
        // reads it, so no atomic/lock is needed on the processBlock path.
        std::string tag;

        Slot() = default;
        Slot(const Slot& other)
            : instance_id(other.instance_id)
            , instance(other.instance)
            , placeholder(other.placeholder)
            , is_bypassed(other.is_bypassed.load(std::memory_order_relaxed))
            , is_failed(other.is_failed.load(std::memory_order_relaxed))
            , failure_count(0)
            , tag(other.tag)
        {
        }
        Slot(Slot&& other) noexcept
            : instance_id(other.instance_id)
            , instance(std::move(other.instance))
            , placeholder(std::move(other.placeholder))
            , is_bypassed(other.is_bypassed.load(std::memory_order_relaxed))
            , is_failed(other.is_failed.load(std::memory_order_relaxed))
            , failure_count(0)
            , tag(std::move(other.tag))
        {
        }
        Slot& operator=(const Slot& other)
        {
            if (this != &other)
            {
                instance_id = other.instance_id;
                instance = other.instance;
                placeholder = other.placeholder;
                is_bypassed.store(other.is_bypassed.load(std::memory_order_relaxed), std::memory_order_relaxed);
                is_failed.store(other.is_failed.load(std::memory_order_relaxed), std::memory_order_relaxed);
                failure_count.store(0, std::memory_order_relaxed);
                tag = other.tag;
            }
            return *this;
        }
        Slot& operator=(Slot&& other) noexcept
        {
            if (this != &other)
            {
                instance_id = other.instance_id;
                instance = std::move(other.instance);
                placeholder = std::move(other.placeholder);
                is_bypassed.store(other.is_bypassed.load(std::memory_order_relaxed), std::memory_order_relaxed);
                is_failed.store(other.is_failed.load(std::memory_order_relaxed), std::memory_order_relaxed);
                failure_count.store(0, std::memory_order_relaxed);
                tag = std::move(other.tag);
            }
            return *this;
        }

        PluginSlotKind kind() const noexcept
        {
            return placeholder ? PluginSlotKind::Placeholder : PluginSlotKind::Plugin;
        }
    };

    PluginChain();
    ~PluginChain() override;

    PluginChain(const PluginChain&) = delete;
    PluginChain& operator=(const PluginChain&) = delete;

    // --- Structural mutations (UI thread only) ---------------------------
    InstanceId addSlot(std::shared_ptr<PluginInstance> instance, int position);
    InstanceId addPlaceholderSlot(std::shared_ptr<PlaceholderInstance> placeholder, int position);
    void removeSlot(int position);
    void moveSlot(int from, int to);

    // --- Per-slot state mutations (can be called from audio thread) ------
    void setBypass(int position, bool bypassed);
    void setParameter(int position, ParamId param, float value);

    // --- Per-slot tag (UI thread only) -----------------------------------
    // Maximum stored tag length; longer input is truncated by setTag().
    static constexpr std::size_t kMaxTagLength = 32;
    // Trims surrounding whitespace and truncates to kMaxTagLength. An empty
    // (or whitespace-only) tag clears the slot's tag.
    void setTag(int position, const std::string& tag);

    // --- Snapshot for UI -------------------------------------------------
    std::vector<ChainSlotSnapshot> snapshot() const;
    std::shared_ptr<PluginInstance> getSlotInstance(int position) const;
    std::shared_ptr<PlaceholderInstance> getSlotPlaceholder(int position) const;
    int revision() const noexcept { return revision_.load(); }
    void bumpRevision() noexcept { revision_.fetch_add(1, std::memory_order_relaxed); }

    // Per-slot output metering (audio-thread writes, UI-thread reads).
    static constexpr int kMaxMeteredSlots = 32;
    void readSlotOutputLevels(std::vector<float>& peaks,
                              std::vector<float>& rms) const;

    // --- AudioProcessor interface ----------------------------------------
    void prepareToPlay(double sample_rate, int samples_per_block) override;
    void releaseResources() override;

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midi) override
    {
        juce::ignoreUnused(buffer, midi);
    }

    using AudioProcessor::processBlock;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    bool hasEditor() const override;
    juce::AudioProcessorEditor* createEditor() override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

private:
    void publishSlots(std::vector<Slot>&& new_slots);
    std::vector<Slot>* activeSlots() noexcept;
    const std::vector<Slot>* activeSlots() const noexcept;

    // Double-buffered slot storage.
    std::vector<Slot> slots_a_;
    std::vector<Slot> slots_b_;
    std::atomic<std::vector<Slot>*> active_slots_ {nullptr};

    std::atomic<int> revision_ {0};
    std::atomic<std::uint64_t> next_instance_id_ {1};

    // Per-slot output levels written by the audio thread during processBlock.
    std::array<std::atomic<float>, kMaxMeteredSlots> slot_output_peak_;
    std::array<std::atomic<float>, kMaxMeteredSlots> slot_output_rms_;

    double sample_rate_ {48000.0};
    int samples_per_block_ {256};
    bool is_prepared_ {false};
};

}  // namespace jyglobalvst::engine
