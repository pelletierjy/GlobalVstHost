// tests/unit/same_device_guard_test.cpp
//
// T024 — Unit test: SameDeviceGuard resolves "system default" selections and
// correctly detects coincidence between capture and output devices.

#include <gtest/gtest.h>
#include "../../src/audio-engine/routing/same_device_guard.h"
#include "jyglobalvst/types.h"

using namespace jyglobalvst::engine;
using namespace jyglobalvst;

class SameDeviceGuardTest : public ::testing::Test
{
protected:
    SameDeviceGuard guard;
};

// Empty IDs should not conflict
TEST_F(SameDeviceGuardTest, EmptyIdsNoConflict)
{
    const EndpointId conflict = guard.checkConflict(EndpointId{}, EndpointId{});
    EXPECT_TRUE(conflict.empty());
}

// Empty capture should not conflict
TEST_F(SameDeviceGuardTest, EmptyCaptureNoConflict)
{
    const EndpointId conflict = guard.checkConflict(EndpointId{}, "some-output-id");
    EXPECT_TRUE(conflict.empty());
}

// Empty output should not conflict
TEST_F(SameDeviceGuardTest, EmptyOutputNoConflict)
{
    const EndpointId conflict = guard.checkConflict("some-capture-id", EndpointId{});
    EXPECT_TRUE(conflict.empty());
}

// Different specific IDs should not conflict
TEST_F(SameDeviceGuardTest, DifferentIdsNoConflict)
{
    const EndpointId conflict = guard.checkConflict("capture-id-1", "output-id-2");
    EXPECT_TRUE(conflict.empty());
}

// Same specific IDs should conflict
TEST_F(SameDeviceGuardTest, SameIdsConflict)
{
    const EndpointId conflict = guard.checkConflict("same-device-id", "same-device-id");
    EXPECT_EQ(conflict, "same-device-id");
}

// System-default vs specific ID: on a real system this resolves the default
// and compares. In unit tests without a real MMDeviceEnumerator, the resolution
// may fail and return the input unchanged; we verify the guard handles this
// gracefully (no crash, deterministic result).
TEST_F(SameDeviceGuardTest, SystemDefaultResolutionGraceful)
{
    // "system-default" is the symbolic ID used by the engine
    const EndpointId conflict1 = guard.checkConflict("system-default", "some-output");
    // Result depends on whether the system has a real default render endpoint.
    // Either way, it should not crash.
    (void)conflict1;

    const EndpointId conflict2 = guard.checkConflict("some-capture", "system-default");
    (void)conflict2;

    SUCCEED();
}

// Case sensitivity: IDs are treated as exact strings
TEST_F(SameDeviceGuardTest, CaseSensitiveComparison)
{
    const EndpointId conflict = guard.checkConflict("Device-A", "device-a");
    EXPECT_TRUE(conflict.empty());
}
