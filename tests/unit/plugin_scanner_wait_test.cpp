// tests/unit/plugin_scanner_wait_test.cpp
//
// Bounded scan-join semantics. A VST3 whose load() hangs never returns and SEH
// cannot catch hangs, so every UI-thread-visible wait on the scan thread
// (waitForScanExit / a second start()) must be time-bounded, and a detached
// scan must remain observable until it actually exits — ~AudioEngineImpl relies
// on that to decide between joining and leaking the scanner.

#include "../../src/audio-engine/vst-host/plugin_scanner.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace jyglobalvst::testing {

namespace {

using namespace std::chrono_literals;

// Simulates a plugin whose load() never returns: runScan() delivers
// onScanStarted() on the scan thread before touching any path, so blocking
// there wedges the scan exactly like a hung DllLoad would.
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

    // Returns true once the scan thread is provably stuck inside onScanStarted().
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

TEST(PluginScannerWaitTest, WaitsAreBoundedWhileScanIsHung)
{
    engine::PluginScanner scanner;
    HangingScanListener listener;

    ASSERT_TRUE(scanner.start({}, &listener, nullptr));
    ASSERT_TRUE(listener.waitUntilHanging());
    EXPECT_TRUE(scanner.isRunning());

    // A bounded wait must give up — not hang — while the scan thread is stuck.
    EXPECT_FALSE(scanner.waitForScanExit(50ms));

    // A second scan must be refused rather than queue behind (or race) the hung one.
    EXPECT_FALSE(scanner.start({}, &listener, nullptr));

    listener.release();
    EXPECT_TRUE(scanner.waitForScanExit(5s));
    EXPECT_FALSE(scanner.isRunning());
}

TEST(PluginScannerWaitTest, DetachedScanExitRemainsObservable)
{
    engine::PluginScanner scanner;
    HangingScanListener listener;

    ASSERT_TRUE(scanner.start({}, &listener, nullptr));
    ASSERT_TRUE(listener.waitUntilHanging());
    ASSERT_FALSE(scanner.waitForScanExit(50ms));

    // cancelScan() detaches a scan that outlives its bounded wait; the engine
    // must still be able to observe its eventual exit before freeing anything.
    scanner.cancel();
    scanner.detachRunningScan();

    listener.release();
    EXPECT_TRUE(scanner.waitForScanExit(5s));
    EXPECT_FALSE(scanner.isRunning());
}

}  // namespace jyglobalvst::testing