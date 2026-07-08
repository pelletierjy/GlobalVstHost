#include <gtest/gtest.h>
#include "routing/audio_engine_impl.h"
#include "builtin-effects/builtin_effect_registry.h"
#include <filesystem>

using namespace jyglobalvst;
using namespace jyglobalvst::engine;

TEST(BuiltinNightTimeTest, CatalogListsNightTime)
{
    AudioEngineImpl engine;

    auto catalog = engine.catalog();
    bool found = false;
    for (const auto& entry : catalog)
    {
        if (entry.ref.name == "Night-time" && entry.ref.vendor == "JyGlobalVST")
        {
            found = true;
            EXPECT_EQ(entry.file_path, "");  // Built-ins have empty file_path
            break;
        }
    }
    EXPECT_TRUE(found) << "Night-time not found in catalog";
}

TEST(BuiltinNightTimeTest, AddNightTimeToChain)
{
    AudioEngineImpl engine;

    PluginRef ref;
    ref.vendor = "JyGlobalVST";
    ref.name = "Night-time";

    auto id = engine.addPlugin(ref, 0);
    EXPECT_FALSE(id.isNull()) << "Failed to add Night-time to chain";

    auto snapshot = engine.snapshotChain();
    EXPECT_EQ(snapshot.slots.size(), 1);
    EXPECT_EQ(snapshot.slots[0].ref.name, "Night-time");
}

TEST(BuiltinNightTimeTest, PresetRoundTrip)
{
    AudioEngineImpl engine;

    PluginRef ref;
    ref.vendor = "JyGlobalVST";
    ref.name = "Night-time";
    engine.addPlugin(ref, 0);

    auto preset_path = std::filesystem::temp_directory_path() / "test_nighttime.jvst";
    engine.savePreset(preset_path, "Test Preset");

    AudioEngineImpl engine2;
    engine2.loadPreset(preset_path);

    auto snapshot = engine2.snapshotChain();
    EXPECT_EQ(snapshot.slots.size(), 1);
    EXPECT_EQ(snapshot.slots[0].ref.name, "Night-time");

    std::filesystem::remove(preset_path);
}
