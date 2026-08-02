#include "builtin_effect_registry.h"
#include "builtin_ids.h"
#include "nighttime_processor.h"
#include "eq_processor.h"
#include "volume_leveler_processor.h"

namespace jyglobalvst::engine {

BuiltinEffectRegistry::BuiltinEffectRegistry()
{
    registerBuiltins();
}

BuiltinEffectRegistry::~BuiltinEffectRegistry() = default;

void BuiltinEffectRegistry::registerBuiltins()
{
    // Auto volume leveller / Compressor effect
    descriptors_.push_back(Descriptor{
        builtin::NIGHTTIME_UID,
        "Auto volume leveller / Compressor",
        "JyGlobalVST",
        "Fx",
        []() -> std::unique_ptr<juce::AudioPluginInstance> {
            return std::make_unique<NightTimeProcessor>();
        }
    });

    // EQ effect
    descriptors_.push_back(Descriptor{
        builtin::EQ_UID,
        "EQ (Bass Boost)",
        "JyGlobalVST",
        "Fx",
        []() -> std::unique_ptr<juce::AudioPluginInstance> {
            return std::make_unique<EqProcessor>();
        }
    });

    // Volume Leveler effect
    descriptors_.push_back(Descriptor{
        builtin::VOLUME_LEVELER_UID,
        "Volume Leveler",
        "JyGlobalVST",
        "Fx",
        []() -> std::unique_ptr<juce::AudioPluginInstance> {
            return std::make_unique<VolumeLevelerProcessor>();
        }
    });
}

const PluginCatalogEntry* BuiltinEffectRegistry::findByRef(const PluginRef& ref) const
{
    // Check by reserved UID first (primary)
    for (const auto& desc : descriptors_)
    {
        if (desc.uid == ref.plugin_uid)
        {
            // Build and return a temporary catalog entry (caller must not store pointer).
            // This is a helper for metadata lookup; the result is ephemeral.
            // In production, catalog entries should be cached if this becomes a hotpath.
            static PluginCatalogEntry temp;
            temp.ref.plugin_uid = desc.uid;
            temp.ref.vendor = desc.vendor;
            temp.ref.name = desc.name;
            temp.category = desc.category;
            temp.file_path = "";
            temp.has_editor = true;
            return &temp;
        }
    }

    // Fallback: check by (vendor, name) pair
    for (const auto& desc : descriptors_)
    {
        if (ref.vendor == desc.vendor && ref.name == desc.name)
        {
            static PluginCatalogEntry temp;
            temp.ref.plugin_uid = desc.uid;
            temp.ref.vendor = desc.vendor;
            temp.ref.name = desc.name;
            temp.category = desc.category;
            temp.file_path = "";
            temp.has_editor = true;
            return &temp;
        }
    }

    return nullptr;
}

std::vector<PluginCatalogEntry> BuiltinEffectRegistry::entries() const
{
    std::vector<PluginCatalogEntry> result;

    for (const auto& desc : descriptors_)
    {
        PluginCatalogEntry entry;
        entry.ref.plugin_uid = desc.uid;
        entry.ref.vendor = desc.vendor;
        entry.ref.name = desc.name;
        entry.category = desc.category;
        entry.file_path = "";  // Empty file_path marks built-ins
        entry.has_editor = true;

        result.push_back(entry);
    }

    return result;
}

bool BuiltinEffectRegistry::isBuiltin(const PluginRef& ref) const
{
    // Check by reserved UID first (primary)
    for (const auto& desc : descriptors_)
    {
        if (desc.uid == ref.plugin_uid)
        {
            return true;
        }
    }

    // Fallback: check by (vendor, name) pair
    for (const auto& desc : descriptors_)
    {
        if (ref.vendor == desc.vendor && ref.name == desc.name)
        {
            return true;
        }
    }

    return false;
}

std::unique_ptr<juce::AudioPluginInstance> BuiltinEffectRegistry::create(const PluginRef& ref) const
{
    // Find the descriptor by reserved UID (primary)
    for (const auto& desc : descriptors_)
    {
        if (desc.uid == ref.plugin_uid)
        {
            if (desc.factory)
            {
                return desc.factory();
            }
            return nullptr;
        }
    }

    // Fallback: find by (vendor, name) pair
    for (const auto& desc : descriptors_)
    {
        if (ref.vendor == desc.vendor && ref.name == desc.name)
        {
            if (desc.factory)
            {
                return desc.factory();
            }
            return nullptr;
        }
    }

    return nullptr;
}

}  // namespace jyglobalvst::engine
