#include <gtest/gtest.h>
#include "builtin-effects/builtin_effect_registry.h"
#include "builtin-effects/builtin_ids.h"

using namespace jyglobalvst;
using namespace jyglobalvst::engine;

TEST(BuiltinRegistryTest, EntriesContainBothEffects)
{
    BuiltinEffectRegistry registry;

    auto entries = registry.entries();
    EXPECT_EQ(entries.size(), 2);

    bool found_nighttime = false, found_eq = false;
    for (const auto& entry : entries)
    {
        if (entry.ref.name == "Night-time")
        {
            found_nighttime = true;
            EXPECT_EQ(entry.ref.vendor, "JyGlobalVST");
            EXPECT_EQ(entry.file_path, "");
            EXPECT_TRUE(entry.has_editor);
        }
        if (entry.ref.name == "EQ (Bass Boost)")
        {
            found_eq = true;
            EXPECT_EQ(entry.ref.vendor, "JyGlobalVST");
            EXPECT_EQ(entry.file_path, "");
        }
    }
    EXPECT_TRUE(found_nighttime && found_eq);
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
    ref.name = "Night-time";

    EXPECT_TRUE(registry.isBuiltin(ref));
    auto entry = registry.findByRef(ref);
    EXPECT_NE(entry, nullptr);
    EXPECT_EQ(entry->ref.name, "Night-time");
}

TEST(BuiltinRegistryTest, CreateWorks)
{
    BuiltinEffectRegistry registry;

    PluginRef ref;
    ref.vendor = "JyGlobalVST";
    ref.name = "EQ (Bass Boost)";

    auto proc = registry.create(ref);
    EXPECT_NE(proc, nullptr);
}
