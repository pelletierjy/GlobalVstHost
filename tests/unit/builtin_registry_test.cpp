#include <gtest/gtest.h>
#include "builtin-effects/builtin_effect_registry.h"
#include "builtin-effects/builtin_ids.h"

using namespace jyglobalvst;
using namespace jyglobalvst::engine;

TEST(BuiltinRegistryTest, EntriesContainAllEffects)
{
    BuiltinEffectRegistry registry;

    auto entries = registry.entries();
    EXPECT_EQ(entries.size(), 3);

    bool found_nighttime = false, found_eq = false, found_compressor = false;
    for (const auto& entry : entries)
    {
        if (entry.ref.name == "Volume Leveler")
        {
            found_nighttime = true;
            EXPECT_EQ(entry.ref.vendor, "JyGlobalVST");
            EXPECT_EQ(entry.file_path, "");
            EXPECT_TRUE(entry.has_editor);
        }
        if (entry.ref.name == "Equalizer")
        {
            found_eq = true;
            EXPECT_EQ(entry.ref.vendor, "JyGlobalVST");
            EXPECT_EQ(entry.file_path, "");
        }
        if (entry.ref.name == "Compressor")
        {
            found_compressor = true;
            EXPECT_EQ(entry.ref.vendor, "JyGlobalVST");
            EXPECT_EQ(entry.file_path, "");
        }
    }
    EXPECT_TRUE(found_nighttime && found_eq && found_compressor);
}

TEST(BuiltinRegistryTest, UidsAreDistinct)
{
    BuiltinEffectRegistry registry;
    auto entries = registry.entries();

    EXPECT_NE(entries[0].ref.plugin_uid, entries[1].ref.plugin_uid);
}

TEST(BuiltinRegistryTest, FindByRefWorks)
{
    BuiltinEffectRegistry registry;

    PluginRef ref;
    ref.vendor = "JyGlobalVST";
    ref.name = "Volume Leveler";

    EXPECT_TRUE(registry.isBuiltin(ref));
    auto entry = registry.findByRef(ref);
    EXPECT_NE(entry, nullptr);
    EXPECT_EQ(entry->ref.name, "Volume Leveler");
}

TEST(BuiltinRegistryTest, CreateWorks)
{
    BuiltinEffectRegistry registry;

    PluginRef ref;
    ref.vendor = "JyGlobalVST";
    ref.name = "Equalizer";

    auto proc = registry.create(ref);
    EXPECT_NE(proc, nullptr);
}
