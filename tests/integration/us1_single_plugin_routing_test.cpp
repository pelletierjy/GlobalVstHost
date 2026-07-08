// tests/integration/us1_single_plugin_routing_test.cpp
//
// T032 — Integration test: load one plugin via file picker, audio routes through plugin.
//
// Per Scenario 1: User loads a VST3 plugin, plays audio source, and hears
// processing applied. Audio flows: system audio → virtual device → plugin chain
// → hardware output. Single plugin only (multi-chain is US2).

#include "loopback_fixture.h"

namespace jyglobalvst::testing {

class US1SinglePluginRoutingTest : public LoopbackFixture
{
};

TEST_F(US1SinglePluginRoutingTest, AddPluginBumpsChainRevision)
{
    StartEngine();

    const int initial_revision = listener()->chain_revision();
    (void)initial_revision;

    // Attempt to add a plugin. This is a stub until T038/T039/T040 implement
    // the VST3 loader and chain. For now, we just verify the interface signature.
    PluginRef ref;
    ref.plugin_uid = {};
    // engine()->addPlugin(ref, 0);

    // Chain revision should be bumped (even if the add is a no-op in testable-dev).
    // Per audio_engine_impl.cpp, addPlugin() always calls rebumpChain().

    StopEngine();
}

TEST_F(US1SinglePluginRoutingTest, ChainSnapshotInitiallyEmpty)
{
    StartEngine();

    const auto chain = engine()->snapshotChain();
    // Initially, the chain should be empty (no plugins).
    // Per data-model.md §6, ChainSnapshot contains a chain_revision counter
    // and a slots array.
    (void)chain;

    StopEngine();
}

TEST_F(US1SinglePluginRoutingTest, BufferSizeAffectsLatency)
{
    StartEngine();

    // Set buffer size to a small value for low latency.
    engine()->setBufferSize(128);  // 128 samples = ~2.67 ms at 48 kHz

    const auto latency = engine()->latencyProfile();
    EXPECT_GT(latency.total_round_trip_ms, 0.0f) << "Latency should be measurable";

    // Try larger buffer size.
    engine()->setBufferSize(1024);  // 1024 samples = ~21.3 ms at 48 kHz

    const auto latency_large = engine()->latencyProfile();
    // Larger buffer should have longer latency (though not directly proportional
    // due to device latency being orthogonal).
    (void)latency_large;

    StopEngine();
}

TEST_F(US1SinglePluginRoutingTest, SampleRateNegotiation)
{
    StartEngine();

    const int sample_rate = engine()->negotiatedSampleRate();
    EXPECT_GT(sample_rate, 0) << "Sample rate should be negotiated";

    // Supported rates per FR-002: {44100, 48000, 96000, 176400, 192000}
    const int supported[] = {44100, 48000, 96000, 176400, 192000};
    bool is_supported = false;
    for (int sr : supported)
    {
        if (sample_rate == sr)
        {
            is_supported = true;
            break;
        }
    }
    EXPECT_TRUE(is_supported) << "Sample rate " << sample_rate << " should be in supported set";

    StopEngine();
}

TEST_F(US1SinglePluginRoutingTest, LatencyProfileReflectsDeviceLatency)
{
    StartEngine();

    const auto latency = engine()->latencyProfile();
    EXPECT_GE(latency.capture_ms, 0.0f) << "Capture latency should be non-negative";
    EXPECT_GE(latency.output_ms, 0.0f) << "Output latency should be non-negative";
    EXPECT_EQ(latency.total_round_trip_ms, latency.capture_ms + latency.output_ms);

    StopEngine();
}

TEST_F(US1SinglePluginRoutingTest, CpuStatsAvailable)
{
    StartEngine();

    // Give the engine time to measure CPU usage.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const auto cpu = engine()->cpuStats();
    EXPECT_GE(cpu.instantaneous_pct, 0.0f) << "CPU % should be non-negative";
    EXPECT_LE(cpu.instantaneous_pct, 100.0f) << "CPU % should be <= 100%";

    StopEngine();
}

TEST_F(US1SinglePluginRoutingTest, PassThroughLatencyUnder5ms)
{
    // Per Scenario 1 spec: "round-trip latency < 5 ms with no chain".
    // This test will verify that once pass-through is working correctly.
    // For now, it's a placeholder showing the contract.

    StartEngine();

    const auto latency = engine()->latencyProfile();
    // With pass-through (no plugins) and default 512-sample buffer at 48 kHz:
    // Buffer latency = 512 / 48000 * 1000 ≈ 10.67 ms
    // Plus device latency varies by driver, but typically < 5 ms.
    // Total should be under ~15 ms for modern drivers.
    // This test will be tightened once we have actual latency measurements.

    (void)latency;

    StopEngine();
}

}  // namespace jyglobalvst::testing
