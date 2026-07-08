// src/audio-engine/vst-host/plugin_instance.h
//
// T036 — Plugin and PluginInstance entities.
//
// Per data-model.md §3, §4: a PluginInstance wraps a loaded JUCE
// AudioPluginInstance with metadata (instance ID, position in chain, bypass state).
// Plugin represents the static descriptor (UID, vendor, name, category).

#pragma once

#include <jyglobalvst/types.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>
#include <string>

namespace jyglobalvst::engine {

// Static metadata about a plugin (from plugin descriptor).
struct Plugin
{
    std::string uid;        // VST3 plugin UID (32-char hex string)
    std::string vendor;     // Plugin vendor name
    std::string name;       // Plugin display name
    std::string category;   // VST3 category (e.g., "Fx", "Instrument")
    std::string version;    // Plugin version string
    bool is_synth {false};  // True if plugin is an instrument

    Plugin() = default;
    explicit Plugin(const std::string& u) : uid(u) {}
};

// Runtime instance of a loaded plugin in the chain.
class PluginInstance
{
public:
    PluginInstance(const Plugin& descriptor, std::unique_ptr<juce::AudioPluginInstance> processor)
        : descriptor_(descriptor), processor_(std::move(processor))
    {
    }

    PluginInstance(const PluginInstance&) = delete;
    PluginInstance& operator=(const PluginInstance&) = delete;
    PluginInstance(PluginInstance&&) = default;
    PluginInstance& operator=(PluginInstance&&) = default;

    const Plugin& descriptor() const noexcept { return descriptor_; }
    juce::AudioPluginInstance* processor() const noexcept { return processor_.get(); }

    InstanceId id() const noexcept { return instance_id_; }
    void setId(const InstanceId& id) noexcept { instance_id_ = id; }

    bool isBypassed() const noexcept { return bypassed_; }
    void setBypass(bool bypassed) noexcept { bypassed_ = bypassed; }

    // Get plugin parameter by ID.
    float getParameter(ParamId param_id) const noexcept;
    void setParameter(ParamId param_id, float value) noexcept;

private:
    Plugin descriptor_;
    std::unique_ptr<juce::AudioPluginInstance> processor_;
    InstanceId instance_id_;
    bool bypassed_ {false};
};

}  // namespace jyglobalvst::engine
