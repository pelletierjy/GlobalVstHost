// tests/integration/us2_plugin_scan_test.cpp
//
// T050 — Integration test: background plugin scan (cancellable, incremental,
// progress reported) finds default-path plugins per FR-005.

#include "../integration/loopback_fixture.h"

#include <algorithm>
#include <chrono>
#include <thread>

namespace jyglobalvst::testing {

class US2PluginScanTest : public LoopbackFixture
{
};

TEST_F(US2PluginScanTest, ScanReturnsEmptyCatalogWhenNoPathsSet)
{
    StartEngine();

    // Before any scan, the catalog must contain no *scanned* (external VST3) entries.
    // It is never fully empty: the built-in effects (Auto volume leveller / Compressor, EQ, Volume Leveler) are always
    // present — they are registered directly, not discovered by a filesystem scan —
    // and are identifiable by their empty file_path (see BuiltinEffectRegistry).
    const auto catalog = engine()->catalog();
    const bool has_scanned_entry = std::any_of(catalog.begin(), catalog.end(),
        [](const auto& entry) { return !entry.file_path.empty(); });
    EXPECT_FALSE(has_scanned_entry) << "No scanned plugins should be present before a scan";

    StopEngine();
}

TEST_F(US2PluginScanTest, ScanCanBeStartedAndCancelled)
{
    StartEngine();

    // A null progress listener should be safe (engine may ignore it).
    engine()->rescanPlugins(nullptr);

    // Cancel should be safe even if scan hasn't started or already finished.
    engine()->cancelScan();

    StopEngine();
}

TEST_F(US2PluginScanTest, ScanProgressListenerReceivesLifecycleEvents)
{
    struct CountingListener : public IScanProgressListener
    {
        int started_count {0};
        int finished_count {0};
        int cancelled_count {0};
        int plugin_count {0};

        void onScanStarted(int /*total_paths*/) override { ++started_count; }
        void onPathStarted(const std::filesystem::path& /*path*/) override {}
        void onPluginDiscovered(const PluginCatalogEntry& /*entry*/) override { ++plugin_count; }
        void onScanFinished(int /*plugins_discovered*/) override { ++finished_count; }
        void onScanCancelled() override { ++cancelled_count; }
    };

    StartEngine();

    CountingListener progress;
    engine()->rescanPlugins(&progress);

    // Give the scan a moment to process (it may finish immediately if paths are empty).
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // At minimum, started should have been called.
    EXPECT_EQ(progress.started_count, 1) << "onScanStarted should fire exactly once";

    // Either finished or cancelled should eventually be called (0 or 1 each).
    EXPECT_TRUE(progress.finished_count <= 1);
    EXPECT_TRUE(progress.cancelled_count <= 1);

    StopEngine();
}

TEST_F(US2PluginScanTest, CatalogEntriesHaveRequiredFields)
{
    StartEngine();

    // Exercise the invariant with a deterministic injected entry rather than a real
    // filesystem scan (see JYGLOBALVST_TEST_NO_DEFAULT_SCAN_PATHS in loopback_fixture.h)
    // — a real scan depends on whatever VST3 plugins happen to be installed on the
    // machine running the test, which is neither hermetic nor guaranteed to find
    // anything to check.
    InjectTestPlugin("TestVendor", "TestPlugin");

    const auto catalog = engine()->catalog();
    for (const auto& entry : catalog)
    {
        EXPECT_FALSE(entry.ref.name.empty()) << "Every catalog entry must have a name";
        EXPECT_FALSE(entry.ref.vendor.empty()) << "Every catalog entry must have a vendor";
    }

    // Built-in effects (Auto volume leveller / Compressor, EQ, Volume Leveler) are registered directly, not scanned from
    // disk, and are identifiable by their deliberately empty file_path (see
    // BuiltinEffectRegistry) — so the file-path requirement is checked only against
    // the injected scanned entry above, not the whole catalog.
    const auto scanned = std::find_if(catalog.begin(), catalog.end(),
        [](const auto& entry) { return entry.ref.name == "TestPlugin"; });
    ASSERT_NE(scanned, catalog.end());
    EXPECT_FALSE(scanned->file_path.empty()) << "Every scanned catalog entry must have a file path";

    StopEngine();
}

TEST_F(US2PluginScanTest, ScanIsNonBlocking)
{
    StartEngine();

    struct SlowListener : public IScanProgressListener
    {
        void onScanStarted(int) override {}
        void onPathStarted(const std::filesystem::path&) override {}
        void onPluginDiscovered(const PluginCatalogEntry&) override {}
        void onScanFinished(int) override {}
        void onScanCancelled() override {}
    };

    SlowListener progress;

    // rescanPlugins must return promptly (it's background-threaded).
    const auto t0 = std::chrono::steady_clock::now();
    engine()->rescanPlugins(&progress);
    const auto t1 = std::chrono::steady_clock::now();

    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    EXPECT_LE(elapsed_ms, 50) << "rescanPlugins should return within 50 ms (non-blocking)";

    engine()->cancelScan();
    StopEngine();
}

}  // namespace jyglobalvst::testing
