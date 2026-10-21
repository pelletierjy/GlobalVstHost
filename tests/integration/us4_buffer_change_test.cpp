// tests/integration/us4_buffer_change_test.cpp
//
// T091 — Integration test: buffer-size change applies live; latency readout
// updates within one buffer.

#include "../integration/loopback_fixture.h"

namespace jyglobalvst::testing {

class US4BufferChangeTest : public LoopbackFixture
{};

TEST_F(US4BufferChangeTest, SetBufferSizeUpdatesValue)
{
    StartEngine();

    EXPECT_EQ(engine()->bufferSize(), 512);  // default

    engine()->setBufferSize(512);
    EXPECT_EQ(engine()->bufferSize(), 512);

    engine()->setBufferSize(1024);
    EXPECT_EQ(engine()->bufferSize(), 1024);

    // Invalid sizes throw.
    EXPECT_THROW(engine()->setBufferSize(999), std::invalid_argument);
    EXPECT_EQ(engine()->bufferSize(), 1024);  // unchanged

    StopEngine();
}

TEST_F(US4BufferChangeTest, LatencyProfileReflectsBufferSize)
{
    // The profile is populated from the live output stream; without an
    // output endpoint every field stays 0 (headless CI runners).
    if (engine()->listOutputs().empty())
    {
        GTEST_SKIP() << "No output endpoints available (headless CI runner)";
    }

    StartEngine();

    // Allow device to settle.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    engine()->setBufferSize(512);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto profile_512 = engine()->latencyProfile();
    EXPECT_GT(profile_512.total_round_trip_ms, 0.0f);

    // Buffer-responsive: doubling the buffer queues more frames in the output
    // bridge, so the reported round trip must grow with it.
    engine()->setBufferSize(1024);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto profile_1024 = engine()->latencyProfile();
    EXPECT_GT(profile_1024.total_round_trip_ms, 0.0f);
    EXPECT_GT(profile_1024.total_round_trip_ms, profile_512.total_round_trip_ms)
        << "Round-trip latency should grow when the buffer size doubles";

    // The profile was recently updated (not a stale cached readout).
    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::seconds>(now - profile_1024.last_updated).count();
    EXPECT_LE(std::abs(age), 5);

    StopEngine();
}

}  // namespace jyglobalvst::testing
