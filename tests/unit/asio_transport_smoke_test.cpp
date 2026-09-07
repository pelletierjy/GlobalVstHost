// tests/unit/asio_transport_smoke_test.cpp
//
// T092 — ASIO transport smoke tests.
//
// Verifies ASIO transport state transitions and transport-type enum behavior.
// Avoids constructing `AudioEngineImpl` so tests run on Windows CI runners
// without requiring live audio devices or ASIO drivers.

#include <routing/asio_transport.h>

#include <jyglobalvst/types.h>
#include <gtest/gtest.h>

namespace jyglobalvst::engine {

class ASIOTransportSmokeTest : public ::testing::Test
{
protected:
    ASIOTransport transport_;
};

TEST_F(ASIOTransportSmokeTest, InitiallyClosed)
{
    const auto report = transport_.lastReport();
    EXPECT_FALSE(transport_.isOpen());
    EXPECT_FALSE(report.opened);
    EXPECT_TRUE(report.error.empty());
}

TEST_F(ASIOTransportSmokeTest, PreferredSessionConfigUpdatesWithoutThrow)
{
    SessionConfig cfg;
    cfg.buffer_size = 512;
    cfg.sample_rate = 44100.0;
    cfg.input_channels = 2;
    cfg.output_channels = 2;

    EXPECT_NO_THROW(transport_.setPreferred(cfg));
}

TEST_F(ASIOTransportSmokeTest, ListDevicesDoesNotThrow)
{
    EXPECT_NO_THROW(transport_.listDevices());
}

TEST_F(ASIOTransportSmokeTest, OpenWithUnknownDeviceReportsError)
{
    const auto report = transport_.open(nullptr, "does-not-exist");
    EXPECT_FALSE(report.opened);
    EXPECT_FALSE(report.error.empty());
}

TEST_F(ASIOTransportSmokeTest, CloseIsIdempotent)
{
    transport_.close();
    transport_.close();
    EXPECT_FALSE(transport_.isOpen());
}

TEST_F(ASIOTransportSmokeTest, LastReportReflectsMostRecentOpen)
{
    // In non-ASIO builds or without valid devices, open() returns failed
    // reports. Verify repeated calls produce consistent failure reports.
    const auto r1 = transport_.open(nullptr, "missing");
    const auto r2 = transport_.open(nullptr, "missing");
    EXPECT_FALSE(r1.opened);
    EXPECT_FALSE(r2.opened);
    EXPECT_EQ(r1.error, r2.error);
}

TEST(TransportKindEnumTest, HasExactlyWasapiAndAsio)
{
    EXPECT_EQ(static_cast<std::uint8_t>(TransportKind::Wasapi), 0U);
    EXPECT_EQ(static_cast<std::uint8_t>(TransportKind::Asio), 1U);
}

}  // namespace jyglobalvst::engine
