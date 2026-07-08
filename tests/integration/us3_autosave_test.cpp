// tests/integration/us3_autosave_test.cpp
//
// T068 — Integration test: auto-save on close → relaunch → state restored
// (FR-022c, FR-022d, FR-022e).

#include "../integration/loopback_fixture.h"

#include "../../src/tray-app/presets/autosave.cpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

namespace jyglobalvst::testing {

class US3AutoSaveTest : public LoopbackFixture
{
};

TEST_F(US3AutoSaveTest, WriteCreatesFile)
{
    StartEngine();

    jyglobalvst::tray::AutoSaveStore store;
    store.write(engine(), false);

    EXPECT_TRUE(std::filesystem::exists(store.autosavePath()));

    StopEngine();

    std::error_code ec;
    std::filesystem::remove(store.autosavePath(), ec);
}

TEST_F(US3AutoSaveTest, SuppressFlagPreventsWrite)
{
    StartEngine();

    jyglobalvst::tray::AutoSaveStore store;
    store.write(engine(), true);  // suppress due to preset override

    EXPECT_FALSE(std::filesystem::exists(store.autosavePath()));

    StopEngine();
}

TEST_F(US3AutoSaveTest, RestoreRecoversBufferSize)
{
    StartEngine();

    engine()->setBufferSize(512);

    jyglobalvst::tray::AutoSaveStore store;
    store.write(engine(), false);

    // Create a fresh engine to simulate relaunch.
    StopEngine();
    jyglobalvst::engine::AudioEngineImpl fresh_engine;
    TestAudioEngineListener fresh_listener;
    fresh_engine.setListener(&fresh_listener);

    EXPECT_EQ(fresh_engine.bufferSize(), 256);  // default before restore

    bool restored = store.restore(&fresh_engine);
    EXPECT_TRUE(restored);
    EXPECT_EQ(fresh_engine.bufferSize(), 512);

    std::error_code ec;
    std::filesystem::remove(store.autosavePath(), ec);
}

TEST_F(US3AutoSaveTest, RestoreReturnsFalseWhenFileMissing)
{
    jyglobalvst::tray::AutoSaveStore store;
    std::error_code ec;
    std::filesystem::remove(store.autosavePath(), ec);

    bool restored = store.restore(engine());
    EXPECT_FALSE(restored);
}

TEST_F(US3AutoSaveTest, RestoreSilentlyDiscardsCorruptFile)
{
    jyglobalvst::tray::AutoSaveStore store;
    std::filesystem::create_directories(store.autosavePath().parent_path());
    {
        std::ofstream ofs(store.autosavePath(), std::ios::binary);
        ofs << "not json at all";
    }

    bool restored = store.restore(engine());
    EXPECT_FALSE(restored);

    std::error_code ec;
    std::filesystem::remove(store.autosavePath(), ec);
}

}  // namespace jyglobalvst::testing
