// tests/integration/us3_device_resolution_test.cpp
//
// T070 — Integration test: device resolution priority (endpoint ID → friendly
// name → default) reported via tooltip per FR-022m.

#include "../integration/loopback_fixture.h"

namespace jyglobalvst::testing {

class US3DeviceResolutionTest : public LoopbackFixture
{
};

TEST_F(US3DeviceResolutionTest, ResolutionSourceStartsAsFallback)
{
    StartEngine();

    // Before any explicit selection, resolution source should be fallback
    // because no endpoint ID or friendly name has been persisted yet.
    EXPECT_EQ(engine()->currentResolutionSource(), DeviceResolutionSource::WindowsDefaultFallback);

    StopEngine();
}

TEST_F(US3DeviceResolutionTest, SelectingDeviceSetsEndpointIdMatch)
{
    StartEngine();

    const auto outputs = engine()->listOutputs();
    if (!outputs.empty())
    {
        engine()->selectOutput(outputs[0].endpoint_id);
        EXPECT_EQ(engine()->currentResolutionSource(), DeviceResolutionSource::EndpointIdMatch);
    }

    StopEngine();
}

}  // namespace jyglobalvst::testing
