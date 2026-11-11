// tests/contract/stop_scan_independence_test.cpp
//
// Regression test for the Store-build startup freeze: AudioEngineImpl::stop()
// used to join the plugin-scan thread while holding control_mutex_ on the UI
// message thread. A VST3 whose load() hangs (SEH catches faults, not hangs)
// therefore froze the whole window — WER logged MoAppHang and Windows closed
// the app while the "Scanning VST3 Plugins" dialog was on screen.
//
// Guarantee under test: stop() returns while a plugin scan is still in flight.

#include "../integration/loopback_fixture.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace jyglobalvst::testing {

class StopScanIndependenceTest : public LoopbackFixture
{};

namespace {

using namespace std::chrono_literals;

// Wedges the scan thread inside onScanStarted() (delivered before any path is
// visited), i.e. the scan behaves exactly like a plugin load that never returns.
class HangingScanListener final : public IScanProgressListener
{
public:
    void onScanStarted(int) override
    {
        entered_.store(true, std::memory_order_release);
        while (!release_.load(std::memory_order_acquire))
            std::this_thread::sleep_for(10ms);
    }

    void onPathStarted(const std::filesystem::path&) override {}

    void onPluginDiscovered(const PluginCatalogEntry&) override {}

    void onScanFinished(int) override {}

    void onScanCancelled() override {}

    bool waitUntilHanging() const
    {
        for (int i = 0; i < 200 && !entered_.load(std::memory_order_acquire); ++i)
            std::this_thread::sleep_for(10ms);
        return entered_.load(std::memory_order_acquire);
    }

    void release()
    {
        release_.store(true, std::memory_order_release);
    }

private:
    std::atomic<bool> entered_{false};
    std::atomic<bool> release_{false};
};

}  // namespace

TEST_F(StopScanIndependenceTest, StopDoesNotBlockOnHungPluginScan)
{
    StartEngine();

    // Absence of endpoints is a runner property (headless CI), not an API failure.
    if (engine()->listOutputs().empty())
    {
        StopEngine();
        GTEST_SKIP() << "No output endpoints available (headless CI runner)";
    }

    HangingScanListener scan;
    engine()->rescanPlugins(&scan);
    ASSERT_TRUE(scan.waitUntilHanging()) << "scan thread did not enter the blocking callback";

    std::atomic<bool> stop_returned{false};
    std::thread stopper([&] {
        engine()->stop();
        stop_returned.store(true, std::memory_order_release);
    });

    // stop() runs on the caller's thread; give it a generous 5 s — the pre-fix
    // implementation never returned at all (it joined the wedged scan thread).
    for (int i = 0; i < 250 && !stop_returned.load(std::memory_order_acquire); ++i)
        std::this_thread::sleep_for(20ms);
    const bool returned_in_time = stop_returned.load(std::memory_order_acquire);

    // Release the wedged "plugin load" so both worlds can finish cleanly: with
    // the bug the stuck stop() unblocks here and the EXPECT above reports the
    // failure instead of deadlocking the test binary.
    scan.release();
    stopper.join();

    EXPECT_TRUE(returned_in_time) << "stop() must not wait for the plugin-scan thread; it blocked >5 s with a "
                                     "hung scan in flight (this thread is the UI message thread — see the "
                                     "store-app-hang-on-startup investigation)";

    engine()->cancelScan();  // reap the scan thread before fixture teardown
    StopEngine();
}

}  // namespace jyglobalvst::testing