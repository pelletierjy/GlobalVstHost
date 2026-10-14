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
    EXPECT_EQ(builtin_count, 6)
        << "Expected 6 built-in effects (Volume Leveler + EQ + Compressor + Noise Gate + "
           "Stereo Width + Loudness Normalizer)";
}

TEST(BuiltinEqTest, AddEqToChain)
{
    AudioEngineImpl engine;

    PluginRef ref;
    ref.vendor = "JyGlobalVST";
    ref.name = "Equalizer";

    auto id = engine.addPlugin(ref, 0);
    EXPECT_FALSE(id.isNull()) << "Failed to add EQ to chain";

    auto snapshot = engine.snapshotChain();
    EXPECT_EQ(snapshot.slots.size(), 1);
    EXPECT_EQ(snapshot.slots[0].ref.name, "Equalizer");
}

TEST(BuiltinEffectsTest, BothEffectsInChain)
{
    AudioEngineImpl engine;

    PluginRef nt_ref;
    nt_ref.vendor = "JyGlobalVST";
    nt_ref.name = "Volume Leveler";
    engine.addPlugin(nt_ref, 0);

    PluginRef eq_ref;
    eq_ref.vendor = "JyGlobalVST";
    eq_ref.name = "Equalizer";
    engine.addPlugin(eq_ref, 1);

    auto snapshot = engine.snapshotChain();
    EXPECT_EQ(snapshot.slots.size(), 2);
}
