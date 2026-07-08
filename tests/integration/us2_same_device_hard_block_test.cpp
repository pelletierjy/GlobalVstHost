// tests/integration/us2_same_device_hard_block_test.cpp
//
// T023 — Integration test: same-device selection is hard-blocked at start with
// no override path (FR-005, SC-002).

#include "loopback_fixture.h"

#include <gtest/gtest.h>

namespace jyglobalvst::testing {

class US2SameDeviceHardBlockTest : public LoopbackFixture
{
};

// Attempting to start with the same device as both capture and output must be
// refused. The engine must not be running after the call.
TEST_F(US2SameDeviceHardBlockTest, SameDeviceHardBlockedAtStart)
{
    const auto inputs = engine()->listInputs();
    const auto outputs = engine()->listOutputs();

    if (inputs.empty() || outputs.empty())
    {
        GTEST_SKIP() << "Need at least one input and one output device";
    }

    // Pick the first input endpoint.
    const EndpointId same_endpoint = inputs[0].endpoint_id;

    // Select it as both capture source and output.
    engine()->selectInput(same_endpoint);
    engine()->selectOutput(same_endpoint);

    // Attempt to start.
    engine()->start();

    // Engine must NOT be running.
    EXPECT_FALSE(engine()->isRunning())
        << "start() must be hard-blocked when capture == output";

    // The listener should have received a same-device conflict callback.
    // (If notifyOnUiThread dispatches synchronously on the message thread, this
    // is immediate; otherwise it requires a running message loop.)
    EXPECT_EQ(listener()->last_conflict_device(), same_endpoint)
        << "onSameDeviceConflict must fire with the conflicting device ID";
}

// With processing active on distinct devices, changing the capture source to
// match the output (via selectInput) should not auto-stop, but the next start()
// must refuse.
TEST_F(US2SameDeviceHardBlockTest, DistinctDevicesStartOk)
{
    const auto inputs = engine()->listInputs();
    const auto outputs = engine()->listOutputs();

    if (inputs.size() < 1 || outputs.size() < 1)
    {
        GTEST_SKIP() << "Need at least one input and one output";
    }

    // Select different devices.
    EndpointId input_id = inputs[0].endpoint_id;
    EndpointId output_id = outputs[0].endpoint_id;

    // Ensure they are different; if only one device exists, skip.
    if (input_id == output_id)
    {
        GTEST_SKIP() << "Need at least two distinct devices";
    }

    engine()->selectInput(input_id);
    engine()->selectOutput(output_id);

    engine()->start();

    // Should be running successfully.
    EXPECT_TRUE(engine()->isRunning())
        << "start() should succeed with distinct capture and output";

    engine()->stop();
}

// When the UI selects the same device for both roles, selectInput/ selectOutput
// themselves do not hard-block (the block happens at start()), but the engine
// must reflect the selection so the UI can show the conflict before start.
TEST_F(US2SameDeviceHardBlockTest, SelectionAllowedButStartBlocked)
{
    const auto inputs = engine()->listInputs();

    if (inputs.empty())
    {
        GTEST_SKIP() << "Need at least one input device";
    }

    const EndpointId same = inputs[0].endpoint_id;

    engine()->selectInput(same);
    engine()->selectOutput(same);

    // Selections should be stored.
    EXPECT_EQ(engine()->currentInput(), same);
    EXPECT_EQ(engine()->currentOutput(), same);

    // But starting is blocked.
    engine()->start();
    EXPECT_FALSE(engine()->isRunning());
}

}  // namespace jyglobalvst::testing
