#include <gtest/gtest.h>
#include "routing/audio_engine_impl.h"
#include "builtin-effects/eq_processor.h"
#include "builtin-effects/nighttime_processor.h"
#include <filesystem>

using namespace jyglobalvst;
using namespace jyglobalvst::engine;

namespace {

// Sets non-default state on the two built-in effects at slots 0 (Volume Leveler)
// and 1 (EQ). Values are chosen to differ from every constructor default.
void setNonDefaultState(AudioEngineImpl& engine)
{
    auto* nt = dynamic_cast<NightTimeProcessor*>(engine.testGetProcessor(0));
    ASSERT_NE(nt, nullptr);
    nt->setPresetIndex(2);       // default is 1
    // Look-ahead is intentionally fixed at 10 ms (see NightTimeProcessor) and is no
    // longer a persisted, user-adjustable value; setLookaheadMs() only affects the
    // live instance until the next state load/editor open, so it is not part of
    // this round-trip check.

    auto* eq = dynamic_cast<EqProcessor*>(engine.testGetProcessor(1));
    ASSERT_NE(eq, nullptr);
    eq->setBandGain(0, -6.0f);   // default is 0
    eq->setBandGain(4, 4.5f);
    eq->setBassBoost(3.0f);      // default is 0
}

void expectNonDefaultState(AudioEngineImpl& engine)
{
    auto* nt = dynamic_cast<NightTimeProcessor*>(engine.testGetProcessor(0));
    ASSERT_NE(nt, nullptr);
    EXPECT_EQ(nt->getPresetIndex(), 2);
    EXPECT_FLOAT_EQ(nt->getLookaheadMs(), 10.0f);

    auto* eq = dynamic_cast<EqProcessor*>(engine.testGetProcessor(1));
    ASSERT_NE(eq, nullptr);
    EXPECT_FLOAT_EQ(eq->getBandGain(0), -6.0f);
    EXPECT_FLOAT_EQ(eq->getBandGain(4), 4.5f);
    EXPECT_FLOAT_EQ(eq->getBassBoost(), 3.0f);
}

void addBuiltins(AudioEngineImpl& engine)
{
    PluginRef nt_ref;
    nt_ref.vendor = "JyGlobalVST";
    nt_ref.name = "Volume Leveler";
    engine.addPlugin(nt_ref, 0);

    PluginRef eq_ref;
    eq_ref.vendor = "JyGlobalVST";
    eq_ref.name = "EQ (Bass Boost)";
    engine.addPlugin(eq_ref, 1);
}

}  // namespace

TEST(BuiltinEffectsPersistenceTest, PresetRoundTrip)
{
    auto preset_path = std::filesystem::temp_directory_path() / "test_builtin_preset.jvst";

    {
        AudioEngineImpl engine;
        addBuiltins(engine);
        engine.savePreset(preset_path, "Test Builtin Preset");
    }

    {
        AudioEngineImpl engine2;
        engine2.loadPreset(preset_path);

        auto snapshot = engine2.snapshotChain();
        EXPECT_EQ(snapshot.slots.size(), 2);
        EXPECT_EQ(snapshot.slots[0].ref.name, "Volume Leveler");
        EXPECT_EQ(snapshot.slots[1].ref.name, "EQ (Bass Boost)");
    }

    std::filesystem::remove(preset_path);
}

TEST(BuiltinEffectsPersistenceTest, AutosaveRestores)
{
    auto autosave_path = std::filesystem::temp_directory_path() / "test_autosave.json";

    {
        AudioEngineImpl engine;

        PluginRef ref;
        ref.vendor = "JyGlobalVST";
        ref.name = "Volume Leveler";
        engine.addPlugin(ref, 0);

        engine.savePreset(autosave_path, "Autosave");
    }

    {
        AudioEngineImpl engine2;
        engine2.restoreChain(autosave_path);

        auto snapshot = engine2.snapshotChain();
        EXPECT_EQ(snapshot.slots.size(), 1);
        EXPECT_EQ(snapshot.slots[0].ref.name, "Volume Leveler");
    }

    std::filesystem::remove(autosave_path);
}

// Regression: built-in effect control values must survive a save/restore cycle.
// Previously the restored state was stashed as a pending chunk applied only on
// the next prepareToPlay (audio start), so a restore without audio yielded
// default settings.
TEST(BuiltinEffectsPersistenceTest, ControlValuesRoundTrip)
{
    auto preset_path = std::filesystem::temp_directory_path() / "test_builtin_values.jvst";

    {
        AudioEngineImpl engine;
        addBuiltins(engine);
        setNonDefaultState(engine);
        engine.savePreset(preset_path, "Values");
    }

    {
        AudioEngineImpl engine2;
        engine2.loadPreset(preset_path);
        // Values must be present immediately after load, WITHOUT starting audio.
        expectNonDefaultState(engine2);
    }

    std::filesystem::remove(preset_path);
}

// Regression: the periodic autosave (every ~2s) re-saves the live chain even
// when audio has never started. If restored state were only applied on
// prepareToPlay, the pending chunk would still hold defaults and this re-save
// would clobber the persisted settings. This reproduces that exact sequence:
// restore -> immediate re-save -> reload, and asserts the values survive.
TEST(BuiltinEffectsPersistenceTest, RestoreThenAutosaveDoesNotClobber)
{
    auto original = std::filesystem::temp_directory_path() / "test_autosave_orig.json";
    auto resaved = std::filesystem::temp_directory_path() / "test_autosave_resaved.json";

    // Session 1: user configures the built-ins; state is persisted.
    {
        AudioEngineImpl engine;
        addBuiltins(engine);
        setNonDefaultState(engine);
        engine.savePreset(original, "Autosave");
    }

    // Session 2: restore from autosave, then the periodic autosave fires before
    // audio ever starts (re-saving the live chain to a fresh file).
    {
        AudioEngineImpl engine;
        engine.restoreChain(original);
        expectNonDefaultState(engine);      // applied immediately on restore
        engine.savePreset(resaved, "Autosave");
    }

    // Session 3: the re-saved autosave must still carry the real settings.
    {
        AudioEngineImpl engine;
        engine.restoreChain(resaved);
        expectNonDefaultState(engine);
    }

    std::filesystem::remove(original);
    std::filesystem::remove(resaved);
}
