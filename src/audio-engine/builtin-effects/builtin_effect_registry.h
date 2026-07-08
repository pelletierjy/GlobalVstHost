#pragma once

#include "jyglobalvst/types.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>

namespace jyglobalvst::engine {

// ============================================================================
// BuiltinEffectRegistry: Catalog and factory for built-in audio effects.
//
// REALTIME CONSTRAINTS: All public methods are control-thread only.
// No allocation, locking, I/O, or logging occurs on the audio thread.
// ============================================================================

class BuiltinEffectRegistry
{
public:
    BuiltinEffectRegistry();
    ~BuiltinEffectRegistry();

    // Returns a stable list of catalog entries for all built-in effects.
    // Each entry has: reserved UID, "JyGlobalVST" vendor, empty file_path,
    // has_editor = true, category = "Fx". Order is deterministic.
    std::vector<PluginCatalogEntry> entries() const;

    // Returns true if the given ref matches a built-in effect by reserved UID
    // (primary) or fallback (vendor, name) pair.
    bool isBuiltin(const PluginRef& ref) const;

    // Constructs a fresh instance of a built-in effect if `ref` matches;
    // otherwise returns nullptr. The returned instance is an AudioPluginInstance
    // suitable for insertion into the existing PluginInstance/PluginChain slot
    // machinery. Allocates on control thread only.
    std::unique_ptr<juce::AudioPluginInstance> create(const PluginRef& ref) const;

    // Returns a catalog entry for a built-in effect, or nullptr if not found.
    // Used for descriptor/metadata lookup without instantiation.
    const PluginCatalogEntry* findByRef(const PluginRef& ref) const;

private:
    // Descriptor for a single built-in effect.
    struct Descriptor
    {
        PluginUid uid;
        std::string name;
        std::string vendor;
        std::string category;
        std::function<std::unique_ptr<juce::AudioPluginInstance>()> factory;
    };

    std::vector<Descriptor> descriptors_;

    void registerBuiltins();
};

}  // namespace jyglobalvst::engine
