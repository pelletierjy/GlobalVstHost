#include <gtest/gtest.h>
#include "routing/audio_engine_impl.h"

using namespace jyglobalvst;
using namespace jyglobalvst::engine;

TEST(BuiltinEqTest, CatalogListsBothEffects)
{
    AudioEngineImpl engine;

    auto catalog = engine.catalog();
    int builtin_count = 0;
    for (const auto& entry : catalog)
    {
        if (entry.ref.vendor == "JyGlobalVST" && entry.file_path.empty())
            builtin_count++;
    }
    EXPECT_EQ(builtin_count, 3) << "Expected 3 built-in effects (Night-time + EQ + Volume Leveler)";
}

TEST(BuiltinEqTest, AddEqToChain)
{
    AudioEngineImpl engine;

    PluginRef ref;
    ref.vendor = "JyGlobalVST";
    ref.name = "EQ (Bass Boost)";

    auto id = engine.addPlugin(ref, 0);
    EXPECT_FALSE(id.isNull()) << "Failed to add EQ to chain";

    auto snapshot = engine.snapshotChain();
    EXPECT_EQ(snapshot.slots.size(), 1);
    EXPECT_EQ(snapshot.slots[0].ref.name, "EQ (Bass Boost)");
}

TEST(BuiltinEffectsTest, BothEffectsInChain)
{
    AudioEngineImpl engine;

    PluginRef nt_ref;
    nt_ref.vendor = "JyGlobalVST";
    nt_ref.name = "Night-time";
    engine.addPlugin(nt_ref, 0);

    PluginRef eq_ref;
    eq_ref.vendor = "JyGlobalVST";
    eq_ref.name = "EQ (Bass Boost)";
    engine.addPlugin(eq_ref, 1);

    auto snapshot = engine.snapshotChain();
    EXPECT_EQ(snapshot.slots.size(), 2);
}
