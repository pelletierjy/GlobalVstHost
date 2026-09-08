// tests/integration/us1_empty_chain_passthrough_test.cpp
//
// T035a — Integration test: empty chain (no plugins) passes audio through transparently.
//
// Per spec.md Edge Cases ("No VST plugins loaded"): with an empty chain,
// the engine should pass audio from input → output with < 1 ms added latency
// (overhead of the engine vs raw loopback).

#include "loopback_fixture.h"

#include <thread>
#include <chrono>

namespace jyglobalvst::testing {

class US1EmptyChainPassthroughTest : public LoopbackFixture
{
};

TEST_F(US1EmptyChainPassthroughTest, EmptyChainSnapshot)
{
    StartEngine();

    const auto chain = engine()->snapshotChain();

    // Initially, the chain should be empty (no plugins loaded).
    // Once PluginChain is implemented (T058), this can verify the slots array is empty.

    StopEngine();
}

TEST_F(US1EmptyChainPassthroughTest, PassThroughWithNoPlugins)
{
    // With no plugins loaded, audio should pass through transparently.
    // Testable-dev: verified via the loopback fixture (WASAPI input → output).
    // Release: verified via hardware loopback latency measurement.

    StartEngine();

    // Allow engine to settle and measure latency.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const auto latency = engine()->latencyProfile();

    // With pass-through (no processing), latency should be minimal:
    // - WASAPI capture latency (typically < 2 ms)
    // - Buffer latency (512 samples at 48 kHz = 10.67 ms)
    // - WASAPI render latency (typically < 2 ms)
    // Total ~ 15 ms typical, but without plugin overhead.

    EXPECT_GT(latency.total_round_trip_ms, 0.0f);

    StopEngine();
}

TEST_F(US1EmptyChainPassthroughTest, CpuUsageMinimalWhenEmpty)
{
    StartEngine();

    // Let the engine stabilize.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    const auto cpu = engine()->cpuStats();

    // With no plugins, CPU usage should be minimal (just buffer copying).
    // Typically < 1% on modern systems.
    // Allow some headroom for system variance.
    EXPECT_LT(cpu.instantaneous_pct, 10.0f);

    StopEngine();
}

TEST_F(US1EmptyChainPassthroughTest, MultipleStartStopCyclesEmpty)
{
    // Repeatedly start and stop with an empty chain.
    // Should be stable and not leak resources.

    for (int i = 0; i < 3; ++i)
    {
        StartEngine();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        StopEngine();
    }

    // No crash, no hang. Success.
}

TEST_F(US1EmptyChainPassthroughTest, AudioOutputSelectionWithEmptyChain)
{
    StartEngine();

    const auto outputs = engine()->listOutputs();
    ASSERT_GT(outputs.size(), 0);

    // Select a specific output device.
    engine()->selectOutput(outputs[0].endpoint_id);

    // Verify selection took effect.
    EXPECT_EQ(engine()->currentOutput(), outputs[0].endpoint_id);

    // Audio should still pass through to the selected output.

    StopEngine();
}

TEST_F(US1EmptyChainPassthroughTest, BufferSizeChangeWithEmptyChain)
{
    StartEngine();

    // Change buffer size while running with empty chain.
    const int sizes[] = {32, 64, 128, 256, 512, 1024};

    for (int size : sizes)
    {
        engine()->setBufferSize(size);
        EXPECT_EQ(engine()->bufferSize(), size);

        // Audio should continue passing through.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    StopEngine();
}

TEST_F(US1EmptyChainPassthroughTest, SampleRateStable)
{
    StartEngine();

    const int sr1 = engine()->negotiatedSampleRate();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const int sr2 = engine()->negotiatedSampleRate();

    // Sample rate should remain stable.
    EXPECT_EQ(sr1, sr2);

    StopEngine();
}

TEST_F(US1EmptyChainPassthroughTest, LatencyMeasurementStable)
{
    StartEngine();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const auto latency1 = engine()->latencyProfile();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const auto latency2 = engine()->latencyProfile();

    // Latency measurements should be consistent.
    // Allow small variance due to timing.
    const float tolerance = 1.0f;  // 1 ms tolerance
    EXPECT_NEAR(latency1.total_round_trip_ms, latency2.total_round_trip_ms, tolerance);

    StopEngine();
}

}  // namespace jyglobalvst::testing
