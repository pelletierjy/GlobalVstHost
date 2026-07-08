// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • This file is NOT reached from the audio callback in the current
//     build configuration; it runs exclusively on UI / control threads.
//   • Kept under src/audio-engine/ for cohesion; header satisfies T107.
// =====================================================================
// src/audio-engine/vst-host/vst3_loader.cpp
//
// T038 — VST3PluginLoader implementation.

#include "vst3_loader.h"

#include <juce_core/juce_core.h>

#include <cstdarg>
#include <iostream>
#include <windows.h>

namespace jyglobalvst::engine {

namespace {

void LogDebug(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char buffer[1024];
    vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, fmt, args);
    va_end(args);

    OutputDebugStringA("[JyGlobalVST] ");
    OutputDebugStringA(buffer);
    OutputDebugStringA("\n");

    std::cerr << "[JyGlobalVST] " << buffer << std::endl;
}

// JUCE's createIdentifierString() returns "VST3-<hex>-<hex>" which is NOT valid
// for HexStringToPluginUid (expects 32 pure hex chars). We hash it to a stable
// 128-bit value formatted as exactly 32 lowercase hex characters.
std::string makeHexUidFromIdentifier(const juce::String& identifier)
{
    const std::string input = identifier.toStdString();

    // Simple 128-bit hash: two FNV-1a 64-bit streams with different seeds.
    constexpr std::uint64_t kFnvOffset = 0xcbf29ce484222325;
    constexpr std::uint64_t kFnvPrime = 0x100000001b3;

    std::uint64_t h1 = kFnvOffset;
    std::uint64_t h2 = 0x84222325cbf29ce4; // alternate seed

    for (unsigned char c : input)
    {
        h1 ^= static_cast<std::uint64_t>(c);
        h1 *= kFnvPrime;
        h2 ^= static_cast<std::uint64_t>(c ^ 0x5A);
        h2 *= kFnvPrime;
    }

    auto u64ToHex = [](std::uint64_t v) {
        constexpr char digits[] = "0123456789abcdef";
        std::string out;
        out.reserve(16);
        for (int i = 60; i >= 0; i -= 4)
            out.push_back(digits[(v >> i) & 0xF]);
        return out;
    };

    return u64ToHex(h1) + u64ToHex(h2);
}

}  // namespace

VST3PluginLoader::VST3PluginLoader()
{
    // Format is ready to use; JUCE handles initialization.
}

VST3PluginLoader::LoadResult VST3PluginLoader::load(const std::filesystem::path& bundle_path)
{
    LoadResult result;

    LogDebug("VST3PluginLoader::load: bundle_path='%s'", bundle_path.string().c_str());

    if (!std::filesystem::exists(bundle_path))
    {
        result.error = "Bundle path does not exist: " + bundle_path.string();
        LogDebug("VST3PluginLoader::load: FAILED — path does not exist");
        return result;
    }

    bool is_bundle = std::filesystem::is_directory(bundle_path) && bundle_path.extension() == ".vst3";
    LogDebug("VST3PluginLoader::load: is_bundle=%s", is_bundle ? "true" : "false");

    try
    {
        // Step 1: Scan the file to get fully populated PluginDescription(s).
        // JUCE's VST3 format requires name + uid to match the correct class in the
        // plugin factory. A bare description with only fileOrIdentifier will fail.
        juce::VST3PluginFormat format;
        juce::OwnedArray<juce::PluginDescription> descriptions;
        const juce::String file_id(bundle_path.string());

        LogDebug("VST3PluginLoader::load: scanning file for plugin descriptions...");
        format.findAllTypesForFile(descriptions, file_id);

        LogDebug("VST3PluginLoader::load: found %d plugin description(s)", descriptions.size());

        if (descriptions.isEmpty())
        {
            result.error = "No VST3 plugins found in file: " + bundle_path.string();
            LogDebug("VST3PluginLoader::load: FAILED — no plugin descriptions found");
            return result;
        }

        juce::PluginDescription* desc = descriptions[0];
        LogDebug("VST3PluginLoader::load: selected description — name='%s', uid=0x%llX",
                 desc->name.toRawUTF8(),
                 static_cast<long long>(desc->uniqueId));

        // Step 2: Create the instance using the fully populated description.
        juce::AudioPluginFormatManager format_manager;
        format_manager.addFormat(new juce::VST3PluginFormat());

        juce::String error;
        auto instance = format_manager.createPluginInstance(*desc, 48000.0, 512, error);

        LogDebug("VST3PluginLoader::load: createPluginInstance returned instance=%p, error='%s'",
                 instance.get(),
                 error.toRawUTF8());

        if (!instance)
        {
            result.error = "Failed to create plugin instance. JUCE error: " + error.toStdString();
            LogDebug("VST3PluginLoader::load: FAILED — %s", result.error.c_str());
            return result;
        }

        if (!error.isEmpty())
        {
            LogDebug("VST3PluginLoader::load: WARNING from JUCE — '%s'", error.toRawUTF8());
        }

        // Create Plugin descriptor from the instance's properties.
        Plugin descriptor;
        descriptor.uid = makeHexUidFromIdentifier(instance->getPluginDescription().createIdentifierString());
        descriptor.vendor = instance->getPluginDescription().manufacturerName.toStdString();
        descriptor.name = instance->getPluginDescription().name.toStdString();
        descriptor.category = instance->getPluginDescription().category.toStdString();
        descriptor.version = instance->getPluginDescription().version.toStdString();
        descriptor.is_synth = instance->getPluginDescription().isInstrument;
        descriptor.file_path = bundle_path;

        LogDebug("VST3PluginLoader::load: extracted descriptor — uid='%s', vendor='%s', name='%s', "
                 "category='%s', version='%s', is_synth=%s",
                 descriptor.uid.c_str(),
                 descriptor.vendor.c_str(),
                 descriptor.name.c_str(),
                 descriptor.category.c_str(),
                 descriptor.version.c_str(),
                 descriptor.is_synth ? "true" : "false");

        // Wrap in PluginInstance.
        result.instance = std::make_unique<PluginInstance>(descriptor, std::move(instance));
        LogDebug("VST3PluginLoader::load: SUCCESS — created PluginInstance for '%s'", descriptor.name.c_str());
    }
    catch (const std::exception& e)
    {
        result.error = std::string("Exception during plugin load: ") + e.what();
        LogDebug("VST3PluginLoader::load: EXCEPTION — %s", result.error.c_str());
    }
    catch (...)
    {
        result.error = "Unknown exception during plugin load";
        LogDebug("VST3PluginLoader::load: UNKNOWN EXCEPTION");
    }

    return result;
}

}  // namespace jyglobalvst::engine
