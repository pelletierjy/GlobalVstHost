// src/audio-engine/chain/single_plugin_chain.cpp
//
// T040 — SinglePluginChain implementation.
//
// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • processBlock is RT-safe: wrapped in SEH guards, no allocation.
//   • prepareToPlay and releaseResources are UI-thread only.
// =====================================================================

#include "single_plugin_chain.h"

#include "../vst-host/seh_wrapper.h"

namespace jyglobalvst::engine {

SinglePluginChain::SinglePluginChain()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

void SinglePluginChain::setPlugin(std::unique_ptr<PluginInstance> instance)
{
    plugin_ = std::move(instance);
}

void SinglePluginChain::prepareToPlay(double sample_rate, int samples_per_block)
{
    if (plugin_)
    {
        plugin_->processor()->prepareToPlay(sample_rate, samples_per_block);
    }
}

void SinglePluginChain::releaseResources()
{
    if (plugin_)
    {
        plugin_->processor()->releaseResources();
    }
}

void SinglePluginChain::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    if (!plugin_)
    {
        // No plugin: pass through (copy input to output).
        return;
    }

    // Wrap plugin processBlock in SEH guard.
    if (!SEHPluginWrapper::processBlockSafe(plugin_->processor(), buffer, midi))
    {
        // Plugin crashed: silence the output and notify listener.
        // (Listener notification happens in audio_engine_impl via onPluginFailed callback.)
        buffer.clear();
    }
}

const juce::String SinglePluginChain::getName() const
{
    return plugin_ ? juce::String(plugin_->descriptor().name) : juce::String("Empty Chain");
}

bool SinglePluginChain::acceptsMidi() const
{
    return plugin_ ? plugin_->processor()->acceptsMidi() : false;
}

bool SinglePluginChain::producesMidi() const
{
    return plugin_ ? plugin_->processor()->producesMidi() : false;
}

bool SinglePluginChain::isMidiEffect() const
{
    return plugin_ ? plugin_->processor()->isMidiEffect() : false;
}

double SinglePluginChain::getTailLengthSeconds() const
{
    return plugin_ ? plugin_->processor()->getTailLengthSeconds() : 0.0;
}

int SinglePluginChain::getNumPrograms()
{
    return plugin_ ? plugin_->processor()->getNumPrograms() : 1;
}

int SinglePluginChain::getCurrentProgram()
{
    return plugin_ ? plugin_->processor()->getCurrentProgram() : 0;
}

void SinglePluginChain::setCurrentProgram(int index)
{
    if (plugin_)
    {
        plugin_->processor()->setCurrentProgram(index);
    }
}

const juce::String SinglePluginChain::getProgramName(int index)
{
    return plugin_ ? plugin_->processor()->getProgramName(index) : juce::String();
}

void SinglePluginChain::changeProgramName(int index, const juce::String& newName)
{
    if (plugin_)
    {
        plugin_->processor()->changeProgramName(index, newName);
    }
}

void SinglePluginChain::getStateInformation(juce::MemoryBlock& destData)
{
    if (plugin_)
    {
        plugin_->processor()->getStateInformation(destData);
    }
}

void SinglePluginChain::setStateInformation(const void* data, int sizeInBytes)
{
    if (plugin_)
    {
        plugin_->processor()->setStateInformation(data, sizeInBytes);
    }
}

}  // namespace jyglobalvst::engine
