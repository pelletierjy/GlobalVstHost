// src/audio-engine/chain/plugin_chain.cpp
//
// T058 — PluginChain implementation.
//
// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • processBlock reads active_slots_ (atomic load, acquire) and iterates
//     the pointed-to vector. No allocation, no mutex, no throw.
//   • setBypass / setParameter are called from the audio thread after
//     draining the SPSC command queue in audio_engine_impl.
//   • Structural mutations build a new vector on the UI thread and publish
//     with release semantics. The old vector is not destroyed until the
//     next mutation, ensuring the audio thread never dereferences freed
//     memory.
// =====================================================================

#include "plugin_chain.h"

#include "../vst-host/seh_wrapper.h"

#include <algorithm>
#include <windows.h>

namespace jyglobalvst::engine {

namespace {

// Number of consecutive processBlock failures before a slot is permanently marked
// failed. A grace period handles plugins (e.g. Guitar Rig) that transiently fail
// their first few callbacks while finishing startup initialisation.
constexpr int kMaxConsecutiveFailures = 5;

InstanceId makeInstanceId(std::uint64_t v)
{
    InstanceId id;
    id.high = v;
    id.low = 0;
    return id;
}

}  // namespace

PluginChain::PluginChain()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    active_slots_.store(&slots_a_, std::memory_order_relaxed);
    for (auto& a : slot_output_peak_)
        a.store(0.0f, std::memory_order_relaxed);
    for (auto& a : slot_output_rms_)
        a.store(0.0f, std::memory_order_relaxed);
}

PluginChain::~PluginChain() = default;

InstanceId PluginChain::addSlot(std::shared_ptr<PluginInstance> instance, int position)
{
    const auto id = makeInstanceId(next_instance_id_.fetch_add(1));
    if (instance)
    {
        instance->setId(id);
        if (is_prepared_)
        {
            auto* proc = instance->processor();
            if (proc)
            {
                proc->prepareToPlay(sample_rate_, samples_per_block_);
                // Apply pending state AFTER prepareToPlay — many VST3s reset state in prepareToPlay.
                if (!instance->pendingStateChunk().isEmpty())
                {
                    proc->setStateInformation(instance->pendingStateChunk().getData(),
                                              static_cast<int>(instance->pendingStateChunk().getSize()));
                    instance->clearPendingStateChunk();
                }
            }
        }
    }

    const auto* current = activeSlots();
    if (!current)
    {
        return id;
    }

    std::vector<Slot>& target = (current == &slots_a_) ? slots_b_ : slots_a_;

    target = *current;  // Copy current state.

    const int pos = std::clamp(position, 0, static_cast<int>(target.size()));
    Slot slot;
    slot.instance_id = id;
    slot.instance = std::move(instance);
    if (slot.instance)
    {
        slot.is_bypassed.store(slot.instance->isBypassed(), std::memory_order_relaxed);
    }
    target.insert(target.begin() + pos, std::move(slot));

    publishSlots(std::move(target));
    return id;
}

InstanceId PluginChain::addPlaceholderSlot(std::shared_ptr<PlaceholderInstance> placeholder, int position)
{
    const auto id = makeInstanceId(next_instance_id_.fetch_add(1));
    if (placeholder)
    {
        placeholder->setId(id);
    }

    const auto* current = activeSlots();
    std::vector<Slot>& target = (current == &slots_a_) ? slots_b_ : slots_a_;

    target = *current;

    const int pos = std::clamp(position, 0, static_cast<int>(target.size()));
    Slot slot;
    slot.instance_id = id;
    slot.placeholder = std::move(placeholder);
    if (slot.placeholder)
    {
        slot.is_bypassed.store(slot.placeholder->isBypassed(), std::memory_order_relaxed);
    }
    target.insert(target.begin() + pos, std::move(slot));

    publishSlots(std::move(target));
    return id;
}

void PluginChain::removeSlot(int position)
{
    const auto* current = activeSlots();
    if (!current || position < 0 || position >= static_cast<int>(current->size()))
    {
        return;
    }

    std::vector<Slot>& target = (current == &slots_a_) ? slots_b_ : slots_a_;
    target = *current;
    target.erase(target.begin() + position);

    publishSlots(std::move(target));
}

void PluginChain::moveSlot(int from, int to)
{
    const auto* current = activeSlots();
    if (!current || from < 0 || from >= static_cast<int>(current->size()))
    {
        return;
    }

    std::vector<Slot>& target = (current == &slots_a_) ? slots_b_ : slots_a_;
    target = *current;

    const int clamped_to = std::clamp(to, 0, static_cast<int>(target.size()) - 1);
    if (from == clamped_to)
    {
        return;
    }

    auto slot = std::move(target[from]);
    target.erase(target.begin() + from);
    target.insert(target.begin() + clamped_to, std::move(slot));

    publishSlots(std::move(target));
}

void PluginChain::setBypass(int position, bool bypassed)
{
    auto* slots = active_slots_.load(std::memory_order_acquire);
    if (!slots || position < 0 || position >= static_cast<int>(slots->size()))
    {
        return;
    }
    (*slots)[position].is_bypassed.store(bypassed, std::memory_order_relaxed);
}

void PluginChain::setParameter(int position, ParamId param, float value)
{
    auto* slots = active_slots_.load(std::memory_order_acquire);
    if (!slots || position < 0 || position >= static_cast<int>(slots->size()))
    {
        return;
    }
    auto& slot = (*slots)[position];
    if (slot.instance)
    {
        slot.instance->setParameter(param, value);
    }
}

std::vector<ChainSlotSnapshot> PluginChain::snapshot() const
{
    std::vector<ChainSlotSnapshot> out;
    const auto* slots = activeSlots();
    if (!slots)
    {
        return out;
    }

    // Copy size upfront to guard against concurrent mutations
    const int slot_count = static_cast<int>(slots->size());
    out.reserve(slot_count);

    for (int i = 0; i < slot_count; ++i)
    {
        // Double-check bounds in case the slots pointer changed
        if (i >= static_cast<int>(slots->size()))
        {
            break;
        }

        const auto& slot = (*slots)[i];
        ChainSlotSnapshot snap;
        snap.kind = slot.kind();
        snap.instance_id = slot.instance_id;
        snap.position = i;
        snap.is_bypassed = slot.is_bypassed.load(std::memory_order_relaxed);
        snap.is_failed = slot.is_failed.load(std::memory_order_relaxed);

        if (slot.instance)
        {
            try
            {
                snap.ref.plugin_uid = HexStringToPluginUid(slot.instance->descriptor().uid);
                snap.ref.vendor = slot.instance->descriptor().vendor;
                snap.ref.name = slot.instance->descriptor().name;
                snap.file_path = slot.instance->descriptor().file_path.string();
            }
            catch (const std::exception&)
            {
                // Descriptor access failed; use partial data
            }
        }
        else if (slot.placeholder)
        {
            try
            {
                snap.ref = slot.placeholder->recordedRef();
                snap.file_path = slot.placeholder->pathHint().string();
            }
            catch (const std::exception&)
            {
                // Placeholder access failed; use partial data
            }
        }
        out.push_back(std::move(snap));
    }
    return out;
}

std::shared_ptr<PluginInstance> PluginChain::getSlotInstance(int position) const
{
    const auto* slots = activeSlots();
    if (!slots || position < 0 || position >= static_cast<int>(slots->size()))
    {
        return nullptr;
    }
    return (*slots)[position].instance;
}

std::shared_ptr<PlaceholderInstance> PluginChain::getSlotPlaceholder(int position) const
{
    const auto* slots = activeSlots();
    if (!slots || position < 0 || position >= static_cast<int>(slots->size()))
    {
        return nullptr;
    }
    return (*slots)[position].placeholder;
}

void PluginChain::readSlotOutputLevels(std::vector<float>& peaks,
                                       std::vector<float>& rms) const
{
    const auto* slots = activeSlots();
    if (!slots)
    {
        peaks.clear();
        rms.clear();
        return;
    }
    const int n = static_cast<int>(slots->size());
    peaks.resize(n);
    rms.resize(n);
    for (int i = 0; i < n && i < kMaxMeteredSlots; ++i)
    {
        peaks[i] = slot_output_peak_[i].load(std::memory_order_relaxed);
        rms[i] = slot_output_rms_[i].load(std::memory_order_relaxed);
    }
    for (int i = kMaxMeteredSlots; i < n; ++i)
    {
        peaks[i] = 0.0f;
        rms[i] = 0.0f;
    }
}

void PluginChain::prepareToPlay(double sample_rate, int samples_per_block)
{
    OutputDebugStringA("[PluginChain] prepareToPlay: starting\n");
    sample_rate_ = sample_rate;
    samples_per_block_ = samples_per_block;

    char buf[256];
    sprintf_s(buf, sizeof(buf), "[PluginChain] prepareToPlay: sample_rate=%f, buffer_size=%d\n",
              sample_rate, samples_per_block);
    OutputDebugStringA(buf);

    const auto* slots = activeSlots();
    if (!slots)
    {
        return;
    }
    for (int i = 0; i < static_cast<int>(slots->size()); ++i)
    {
        auto& slot = (*slots)[i];
        if (slot.instance)
        {
            sprintf_s(buf, sizeof(buf), "[PluginChain] prepareToPlay: plugin %d - %s\n",
                      i, slot.instance->descriptor().name.c_str());
            OutputDebugStringA(buf);
            if (SEHPluginWrapper::prepareToPlaySafe(slot.instance->processor(), sample_rate, samples_per_block))
            {
                OutputDebugStringA("[PluginChain] prepareToPlay: success\n");
                // Apply pending state AFTER prepareToPlay — many VST3s reset state in prepareToPlay.
                if (!slot.instance->pendingStateChunk().isEmpty())
                {
                    slot.instance->processor()->setStateInformation(
                        slot.instance->pendingStateChunk().getData(),
                        static_cast<int>(slot.instance->pendingStateChunk().getSize()));
                    slot.instance->clearPendingStateChunk();
                }
            }
            else
            {
                sprintf_s(buf, sizeof(buf), "[PluginChain] prepareToPlay FAILED for plugin %d - %s\n",
                          i, slot.instance->descriptor().name.c_str());
                OutputDebugStringA(buf);
            }
        }
    }
    OutputDebugStringA("[PluginChain] prepareToPlay: done\n");
    is_prepared_ = true;
}

void PluginChain::releaseResources()
{
    const auto* slots = activeSlots();
    if (!slots)
    {
        return;
    }
    for (auto& slot : *slots)
    {
        if (slot.instance)
        {
            slot.instance->processor()->releaseResources();
        }
    }
    is_prepared_ = false;
}

void PluginChain::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    auto* slots = activeSlots();
    if (!slots)
    {
        return;
    }

    // Clear metering for all slots up front; active slots will overwrite.
    for (auto& a : slot_output_peak_)
        a.store(0.0f, std::memory_order_relaxed);
    for (auto& a : slot_output_rms_)
        a.store(0.0f, std::memory_order_relaxed);

    juce::MidiBuffer empty_midi;
    int slot_index = 0;
    for (auto& slot : *slots)
    {
        // Placeholders are audio-transparent (skipped).
        if (slot.placeholder || !slot.instance
            || slot.is_bypassed.load(std::memory_order_relaxed)
            || slot.is_failed.load(std::memory_order_relaxed))
        {
            ++slot_index;
            continue;
        }

        if (!SEHPluginWrapper::processBlockSafe(slot.instance->processor(), buffer, empty_midi))
        {
            // Allow a grace period for transient startup failures (e.g. plugins
            // that finish internal initialisation across the first few callbacks).
            const int count = slot.failure_count.fetch_add(1, std::memory_order_relaxed) + 1;
            if (count >= kMaxConsecutiveFailures)
            {
                slot.is_failed.store(true, std::memory_order_relaxed);
                // Audio continues; slot is now permanently bypassed until
                // the user resets it (US3 / UI action).
            }
        }
        else
        {
            // Reset the transient counter on any successful call.
            slot.failure_count.store(0, std::memory_order_relaxed);

            // Capture post-plugin output level for UI metering.
            if (slot_index < kMaxMeteredSlots)
            {
                const int n = buffer.getNumSamples();
                slot_output_peak_[slot_index].store(buffer.getMagnitude(0, 0, n),
                                                     std::memory_order_relaxed);
                slot_output_rms_[slot_index].store(buffer.getRMSLevel(0, 0, n),
                                                    std::memory_order_relaxed);
            }
        }
        ++slot_index;
    }

    juce::ignoreUnused(midi);
}

const juce::String PluginChain::getName() const
{
    return "JyGlobalVST Plugin Chain";
}

bool PluginChain::hasEditor() const
{
    return false;
}

juce::AudioProcessorEditor* PluginChain::createEditor()
{
    return nullptr;
}

bool PluginChain::acceptsMidi() const
{
    return false;
}

bool PluginChain::producesMidi() const
{
    return false;
}

bool PluginChain::isMidiEffect() const
{
    return false;
}

double PluginChain::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginChain::getNumPrograms()
{
    return 1;
}

int PluginChain::getCurrentProgram()
{
    return 0;
}

void PluginChain::setCurrentProgram(int /*index*/)
{
}

const juce::String PluginChain::getProgramName(int /*index*/)
{
    return {};
}

void PluginChain::changeProgramName(int /*index*/, const juce::String& /*newName*/)
{
}

void PluginChain::getStateInformation(juce::MemoryBlock& /*destData*/)
{
    // US3: serialize chain state.
}

void PluginChain::setStateInformation(const void* /*data*/, int /*sizeInBytes*/)
{
    // US3: deserialize chain state.
}

void PluginChain::publishSlots(std::vector<Slot>&& new_slots)
{
    std::vector<Slot>& target = (active_slots_.load(std::memory_order_relaxed) == &slots_a_) ? slots_b_ : slots_a_;
    if (&target != &new_slots)
        target = std::move(new_slots);
    active_slots_.store(&target, std::memory_order_release);
    revision_.fetch_add(1, std::memory_order_relaxed);
}

std::vector<PluginChain::Slot>* PluginChain::activeSlots() noexcept
{
    return active_slots_.load(std::memory_order_acquire);
}

const std::vector<PluginChain::Slot>* PluginChain::activeSlots() const noexcept
{
    return active_slots_.load(std::memory_order_acquire);
}

}  // namespace jyglobalvst::engine
