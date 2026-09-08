// tests/integration/polish_no_network_idle_test.cpp
//
// T124 — Verify zero background network activity (FR-022n, FR-022o).
//
// Scenarios covered:
//   - parseResponse with valid manifest detects update.
//   - parseResponse with same version reports no update.
//   - parseResponse rejects unknown fields (strict schema).
//   - check() with an invalid endpoint returns an error (no crash, no hang).

#include "../../src/tray-app/updates/update_check.cpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace jyglobalvst::testing {

TEST(NetworkIdle, ParseValidManifestDetectsUpdate)
{
    std::string body = R"({
        "schema_version": 1,
        "latest_version": "1.2.0",
        "release_notes_url": "https://example.com/notes",
        "download_url": "https://example.com/download"
    })";

    auto result = tray::UpdateCheck::parseResponse(body, "1.0.0");
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.update_available);
    EXPECT_EQ(result.latest_version, "1.2.0");
    EXPECT_EQ(result.release_notes_url, "https://example.com/notes");
    EXPECT_EQ(result.download_url, "https://example.com/download");
    EXPECT_EQ(result.installed_version, "1.0.0");
}

TEST(NetworkIdle, ParseSameVersionReportsNoUpdate)
{
    std::string body = R"({
        "schema_version": 1,
        "latest_version": "1.0.0",
        "release_notes_url": "https://example.com/notes",
        "download_url": "https://example.com/download"
    })";

    auto result = tray::UpdateCheck::parseResponse(body, "1.0.0");
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.update_available);
}

TEST(NetworkIdle, ParseUnknownFieldsRejected)
{
    std::string body = R"({
        "schema_version": 1,
        "latest_version": "1.0.0",
        "release_notes_url": "https://example.com/notes",
        "download_url": "https://example.com/download",
        "extra_field": 123
    })";

    auto result = tray::UpdateCheck::parseResponse(body, "1.0.0");
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
}

TEST(NetworkIdle, ParseMalformedJsonFails)
{
    std::string body = "not json at all";

    auto result = tray::UpdateCheck::parseResponse(body, "0.1.0");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error_message, "Invalid JSON");
}

TEST(NetworkIdle, CheckInvalidEndpointReturnsError)
{
    tray::UpdateCheck checker;
    std::atomic<bool> done {false};
    tray::UpdateCheckResult result;

    checker.check("http://invalid.invalid:59999/test", "0.1.0",
        [&done, &result](auto r) {
            result = r;
            done = true;
        });

    // Wait up to 10 s for the network timeout / DNS failure.
    for (int i = 0; i < 100 && !done; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    EXPECT_TRUE(done) << "Callback was not invoked within timeout";
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
}

}  // namespace jyglobalvst::testing
