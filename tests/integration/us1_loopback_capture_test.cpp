// tests/integration/us1_loopback_capture_test.cpp
//
// T014 — Integration test: loopback capture from render endpoint and output routing.
//
// Feature 005 (Driverless Audio Capture) enables capturing from a render endpoint
// (loopback mode) and routing the audio through the VST3 chain to a different
// output endpoint. This test verifies the basic loopback capture → chain → output
// flow with device selection and conflict detection.

#include "loopback_fixture.h"

#include <thread>
#include <chrono>
#include <gtest/gtest.h>

namespace jyglobalvst::testing {

class US1LoopbackCaptureTest : public LoopbackFixture
{
};

// T014: Verify loopback input list contains render endpoints
TEST_F(US1LoopbackCaptureTest, LoopbackInputListContainsRenderEndpoints)
{
    // In Feature 005 (testable-dev), listInputs() should return render endpoints
    // that can be opened in loopback mode (captured from).
    const auto inputs = engine()->listInputs();

    // Should have at least the default render endpoint available.
    ASSERT_GT(inputs.size(), 0) << "No input endpoints available for loopback capture";

    // Verify endpoints have valid IDs and friendly names.
    for (const auto& input : inputs)
    {
        EXPECT_FALSE(input.endpoint_id.empty());
        EXPECT_FALSE(input.friendly_name.empty());
    }
}

// T014: Verify output list contains available render endpoints
TEST_F(US1LoopbackCaptureTest, OutputListContainsRenderEndpoints)
{
    const auto outputs = engine()->listOutputs();

    // Should have at least one output endpoint.
    ASSERT_GT(outputs.size(), 0) << "No output endpoints available";

    // Verify endpoints have valid IDs and friendly names.
    for (const auto& output : outputs)
    {
        EXPECT_FALSE(output.endpoint_id.empty());
        EXPECT_FALSE(output.friendly_name.empty());
    }
}

// T014: Select different input and output endpoints
TEST_F(US1LoopbackCaptureTest, SelectDifferentInputAndOutput)
{
    const auto inputs = engine()->listInputs();
    const auto outputs = engine()->listOutputs();

    ASSERT_GT(inputs.size(), 0);
    ASSERT_GT(outputs.size(), 0);

    // Select first input and first output (if available, they should be different).
    engine()->selectInput(inputs[0].endpoint_id);
    engine()->selectOutput(outputs[0].endpoint_id);

    EXPECT_EQ(engine()->currentInput(), inputs[0].endpoint_id);
    EXPECT_EQ(engine()->currentOutput(), outputs[0].endpoint_id);
}

// T014: Verify device conflict detection when same device is selected
TEST_F(US1LoopbackCaptureTest, DeviceConflictDetection)
{
    const auto inputs = engine()->listInputs();
    const auto outputs = engine()->listOutputs();

    if (inputs.size() < 1 || outputs.size() < 1)
    {
        GTEST_SKIP() << "Need at least one input and one output for conflict test";
    }

    // Select same endpoint for both input and output.
    const EndpointId same_endpoint = inputs[0].endpoint_id;

    engine()->selectInput(same_endpoint);
    engine()->selectOutput(same_endpoint);

    // Attempt to start with same device should be refused.
    engine()->start();

    // Engine should not be running after a same-device conflict.
    EXPECT_FALSE(engine()->isRunning())
        << "Expected start() to be refused when same device is selected";
}

// T014: Verify mute activation on loopback input
TEST_F(US1LoopbackCaptureTest, LoopbackInputMuteActivation)
{
    const auto inputs = engine()->listInputs();
    const auto outputs = engine()->listOutputs();

    if (inputs.size() < 1 || outputs.size() < 1)
    {
        GTEST_SKIP() << "Need at least one input and one output";
    }

    // Select different input and output.
    EndpointId input_id = inputs[0].endpoint_id;
    EndpointId output_id = outputs[0].endpoint_id;

    // If outputs have more than one, ensure they're different.
    if (outputs.size() > 1 && input_id == output_id)
    {
        output_id = outputs[1].endpoint_id;
    }

    engine()->selectInput(input_id);
    engine()->selectOutput(output_id);

    // Start engine — should activate mute on input endpoint.
    engine()->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // isCaptureDeviceMuted() should return true if mute was activated.
    // Note: May return false if muting is unavailable on this endpoint,
    // in which case fallback callback should have fired.
    const bool is_muted = engine()->isCaptureDeviceMuted();
    const bool fallback_fired = !listener()->last_mute_fallback_endpoint().empty();

    EXPECT_TRUE(is_muted || fallback_fired)
        << "Expected capture device to be muted or fallback to fire";

    engine()->stop();
}

// T014: Verify latency measurement with loopback input
TEST_F(US1LoopbackCaptureTest, LoopbackLatencyMeasurement)
{
    const auto inputs = engine()->listInputs();
    const auto outputs = engine()->listOutputs();

    if (inputs.size() < 1 || outputs.size() < 1)
    {
        GTEST_SKIP() << "Need at least one input and one output";
    }

    // Ensure different devices.
    EndpointId input_id = inputs[0].endpoint_id;
    EndpointId output_id = outputs[0].endpoint_id;
    if (outputs.size() > 1 && input_id == output_id)
    {
        output_id = outputs[1].endpoint_id;
    }

    engine()->selectInput(input_id);
    engine()->selectOutput(output_id);

    engine()->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const auto latency = engine()->latencyProfile();

    // With loopback input and real output, should have measurable latencies.
    EXPECT_GE(latency.capture_ms, 0.0f);
    EXPECT_GE(latency.output_ms, 0.0f);
    EXPECT_GT(latency.total_round_trip_ms, 0.0f);

    // Total round-trip should be populated and non-negative.
    // Note: exact value depends on negotiated buffer size; constitutional ≤10ms
    // target is deferred for the driverless path (plan.md Complexity Tracking).
    EXPECT_GT(latency.total_round_trip_ms, 0.0f);

    engine()->stop();
}

// T014: Multiple start/stop cycles with loopback
TEST_F(US1LoopbackCaptureTest, MultipleStartStopWithLoopback)
{
    const auto inputs = engine()->listInputs();
    const auto outputs = engine()->listOutputs();

    if (inputs.size() < 1 || outputs.size() < 1)
    {
        GTEST_SKIP() << "Need at least one input and one output";
    }

    EndpointId input_id = inputs[0].endpoint_id;
    EndpointId output_id = outputs[0].endpoint_id;
    if (outputs.size() > 1 && input_id == output_id)
    {
        output_id = outputs[1].endpoint_id;
    }

    engine()->selectInput(input_id);
    engine()->selectOutput(output_id);

    // Repeatedly start and stop.
    for (int i = 0; i < 3; ++i)
    {
        engine()->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        engine()->stop();
    }

    // Should complete without crashes or hangs.
}

// T014: Verify chain is empty on initial load
TEST_F(US1LoopbackCaptureTest, EmptyChainBeforePluginLoad)
{
    const auto inputs = engine()->listInputs();
    const auto outputs = engine()->listOutputs();

    if (inputs.size() < 1 || outputs.size() < 1)
    {
        GTEST_SKIP();
    }

    engine()->selectInput(inputs[0].endpoint_id);
    engine()->selectOutput(outputs[0].endpoint_id);

    engine()->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // If start was refused (same device), skip the chain check.
    if (!engine()->isRunning())
    {
        GTEST_SKIP() << "Start refused (same device or no device available)";
    }

    const auto chain = engine()->snapshotChain();
    EXPECT_EQ(chain.slots.size(), 0)
        << "Chain should be empty until plugins are added";

    engine()->stop();
}

}  // namespace jyglobalvst::testing
