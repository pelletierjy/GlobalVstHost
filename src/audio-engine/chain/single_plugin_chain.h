// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • No allocation, no lock acquisition, no file I/O, no logging,
//     no GUI calls inside processBlock / audioDeviceIOCallbackWithContext.
// =====================================================================
// src/audio-engine/chain/single_plugin_chain.h
//
// T040 — SinglePluginChain adapter.
//
// Wraps a single PluginInstance to implement the IAudioProcessor interface.
// Acts as an adapter for US1 (single plugin) before the multi-plugin chain
// (US2, T058) replaces it. No plugin mutations needed in US1 phase.

#pragma once

#include "../../vst-host/plugin_instance.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

namespace jyglobalvst::engine {

class SinglePluginChain : public juce::AudioProcessor
{
public:
    SinglePluginChain();
    ~SinglePluginChain() override = default;

    // Plugin management (US1: single plugin only).
    void setPlugin(std::unique_ptr<PluginInstance> instance);
    PluginInstance* getPlugin() const noexcept { return plugin_.get(); }

    // AudioProcessor interface.
    void prepareToPlay(double sample_rate, int samples_per_block) override;
    void releaseResources() override;

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midi) override
    {
        juce::ignoreUnused(buffer, midi);
    }

    using AudioProcessor::processBlock;

    // Plugin information (JUCE contract).
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

private:
    std::unique_ptr<PluginInstance> plugin_;
};

}  // namespace jyglobalvst::engine
