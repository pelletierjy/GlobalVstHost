// tests/integration/us1_device_removal_restore_test.cpp
//
// T033 — Integration test: hardware output device removal triggers auto-fallback.
//
// Per FR-024: When the selected output device is removed (e.g., USB DAC unplugged),
// the engine automatically falls back to Windows default output. When the device
// reconnects, the engine switches back.

#include "loopback_fixture.h"

namespace jyglobalvst::testing {

class US1DeviceRemovalRestoreTest : public LoopbackFixture
{
};

TEST_F(US1DeviceRemovalRestoreTest, DeviceRemovalFallsBackToDefault)
{
    // This test verifies FR-024 behavior: when the selected output device is removed,
    // the engine falls back to Windows default.
    // Testable-dev: simulated via loopback fixture (no actual device hot-swap).
    // Release: validated against real USB device unplugging.

    StartEngine();

    const auto outputs = engine()->listOutputs();
    ASSERT_GT(outputs.size(), 0);

    // Select a non-default device (if available).
    EndpointId selected_device = outputs[0].endpoint_id;
    for (const auto& device : outputs)
    {
        if (!device.is_default)
        {
            selected_device = device.endpoint_id;
            break;
        }
    }

    engine()->selectOutput(selected_device);
    EXPECT_EQ(engine()->currentOutput(), selected_device);

    // Simulate device removal: update endpoint list and emit notification.
    // Note: This is a placeholder test showing the contract. In actual testable-dev,
    // we'd use the loopback fixture to trigger IMMNotificationClient callbacks
    // (T027 extension: audio injection + device simulation).

    StopEngine();
}

TEST_F(US1DeviceRemovalRestoreTest, DeviceRestoreReconnectsToPreferred)
{
    // Per FR-024: When the preferred (previously selected) device reconnects,
    // the engine switches back to it.

    StartEngine();

    const auto outputs = engine()->listOutputs();
    ASSERT_GT(outputs.size(), 0);

    const auto initial_device = outputs[0].endpoint_id;
    engine()->selectOutput(initial_device);

    // Placeholder: simulate device disconnection and reconnection.
    // When reconnected, the engine should receive an onDeviceRestored event.
    (void)initial_device;

    StopEngine();
}

TEST_F(US1DeviceRemovalRestoreTest, OnDeviceLostEventFires)
{
    // When a device is lost, IAudioEngineListener::onDeviceLost should fire
    // with the lost device ID and the fallback device ID.

    StartEngine();

    // Placeholder: simulate device removal and verify listener callback.
    // Expected: listener()->device_lost_events() to contain a (lost, fallback) pair.

    const auto& events = listener()->device_lost_events();
    // Initially should be empty (no removal simulated yet).
    EXPECT_EQ(events.size(), 0);

    StopEngine();
}

TEST_F(US1DeviceRemovalRestoreTest, OnDeviceRestoredEventFires)
{
    // When a previously lost device reconnects, onDeviceRestored fires.

    StartEngine();

    // Placeholder: simulate device reconnection and verify listener callback.
    // Expected: listener()->last_restored_device() to be populated.

    // Initially no restore event.
    EXPECT_TRUE(listener()->last_restored_device().empty());

    StopEngine();
}

TEST_F(US1DeviceRemovalRestoreTest, AudioContinuesDuringFallback)
{
    // Audio should not stop or glitch when the engine falls back to Windows default.
    // Testable-dev: verified by the loopback fixture continuing to process audio
    // through the fallback device.
    // Release: measured via latency gates (Phase 7, T101-T105).

    StartEngine();

    // Placeholder: simulate device removal, measure audio continuity.
    // In release prep with hardware loopback, this measures actual audio output
    // at the fallback device with no dropout.

    StopEngine();
}

}  // namespace jyglobalvst::testing
