// tests/unit/format_convert_test.cpp
//
// T030 — Unit tests for format converters (T024).
//
// Tests verify Int16 ↔ Float32, Int24 ↔ Float32 conversions, including
// round-trip fidelity, range clamping, and sign-extension correctness.

#include <gtest/gtest.h>
#include <routing/format_convert.h>

#include <cmath>
#include <cstring>
#include <vector>

using namespace jyglobalvst::engine;

class FormatConvertTest : public ::testing::Test
{
protected:
    static constexpr float kTolerance = 1e-5f;
};

TEST_F(FormatConvertTest, Int16ToFloat32MaxValue)
{
    const std::int16_t src[] = {32767};
    float dst[1];
    int16ToFloat32(src, dst, 1, 1);

    // 32767 / 32768 = 0.99997... (standard audio scaling)
    EXPECT_NEAR(dst[0], 32767.0f / 32768.0f, kTolerance);
}

TEST_F(FormatConvertTest, Int16ToFloat32MinValue)
{
    const std::int16_t src[] = {-32768};
    float dst[1];
    int16ToFloat32(src, dst, 1, 1);

    // -32768 / 32768 = -1.0
    EXPECT_NEAR(dst[0], -1.0f, kTolerance);
}

TEST_F(FormatConvertTest, Int16ToFloat32Zero)
{
    const std::int16_t src[] = {0};
    float dst[1];
    int16ToFloat32(src, dst, 1, 1);

    EXPECT_NEAR(dst[0], 0.0f, kTolerance);
}

TEST_F(FormatConvertTest, Int16ToFloat32MultiChannel)
{
    const std::int16_t src[] = {0, 16384, 32767, -32768};
    float dst[4];
    int16ToFloat32(src, dst, 2, 2);  // 2 frames, 2 channels

    EXPECT_NEAR(dst[0], 0.0f, kTolerance);
    EXPECT_NEAR(dst[1], 0.5f, kTolerance);
    EXPECT_NEAR(dst[2], 32767.0f / 32768.0f, kTolerance);
    EXPECT_NEAR(dst[3], -1.0f, kTolerance);
}

TEST_F(FormatConvertTest, Float32ToInt16MaxValue)
{
    const float src[] = {1.0f};
    std::int16_t dst[1];
    float32ToInt16(src, dst, 1, 1);

    EXPECT_EQ(dst[0], 32767);
}

TEST_F(FormatConvertTest, Float32ToInt16MinValue)
{
    const float src[] = {-1.0f};
    std::int16_t dst[1];
    float32ToInt16(src, dst, 1, 1);

    EXPECT_EQ(dst[0], -32768);
}

TEST_F(FormatConvertTest, Float32ToInt16Zero)
{
    const float src[] = {0.0f};
    std::int16_t dst[1];
    float32ToInt16(src, dst, 1, 1);

    EXPECT_EQ(dst[0], 0);
}

TEST_F(FormatConvertTest, Float32ToInt16Clamps)
{
    const float src[] = {2.0f, -2.0f, 0.5f};
    std::int16_t dst[3];
    float32ToInt16(src, dst, 3, 1);

    // 2.0 should clamp to 1.0 → 32767
    EXPECT_EQ(dst[0], 32767);
    // -2.0 should clamp to -1.0 → -32768
    EXPECT_EQ(dst[1], -32768);
    // 0.5 should map to 16383/16384
    EXPECT_NEAR(static_cast<float>(dst[2]) / 32767.0f, 0.5f, 0.002f);
}

TEST_F(FormatConvertTest, Int16RoundTrip)
{
    const std::int16_t original[] = {0, 1000, -1000, 32767, -32768};
    const auto count = 5;

    float temp[count];
    std::int16_t roundtrip[count];

    int16ToFloat32(original, temp, count, 1);
    float32ToInt16(temp, roundtrip, count, 1);

    for (int i = 0; i < count; ++i)
    {
        EXPECT_EQ(roundtrip[i], original[i]);
    }
}

TEST_F(FormatConvertTest, Int24ToFloat32MaxValue)
{
    const std::uint8_t src[] = {0xFF, 0xFF, 0x7F};  // 0x7FFFFF = 8388607
    float dst[1];
    int24ToFloat32(src, dst, 1, 1);

    // 8388607 / 8388608 = 0.99999988... (standard audio scaling)
    EXPECT_NEAR(dst[0], 8388607.0f / 8388608.0f, kTolerance);
}

TEST_F(FormatConvertTest, Int24ToFloat32MinValue)
{
    const std::uint8_t src[] = {0x00, 0x00, 0x80};  // 0x800000 = -8388608 (sign-extended)
    float dst[1];
    int24ToFloat32(src, dst, 1, 1);

    // -8388608 / 8388608 = -1.0
    EXPECT_NEAR(dst[0], -1.0f, kTolerance);
}

TEST_F(FormatConvertTest, Int24ToFloat32Zero)
{
    const std::uint8_t src[] = {0x00, 0x00, 0x00};
    float dst[1];
    int24ToFloat32(src, dst, 1, 1);

    EXPECT_NEAR(dst[0], 0.0f, kTolerance);
}

TEST_F(FormatConvertTest, Int24ToFloat32MultiChannel)
{
    // 2 frames, 2 channels = 4 samples
    // Frame 0 Ch0: 0x000000 = 0
    // Frame 0 Ch1: 0x400000 = 4194304 → 0.5
    // Frame 1 Ch0: 0xFFFFFF = -1 (0xFFFFFF sign-extends to -1)
    // Frame 1 Ch1: 0x000100 = 256
    const std::uint8_t src[] = {
        0x00, 0x00, 0x00,  // Sample 0
        0x00, 0x00, 0x40,  // Sample 1
        0xFF, 0xFF, 0xFF,  // Sample 2
        0x00, 0x01, 0x00   // Sample 3
    };
    float dst[4];
    int24ToFloat32(src, dst, 2, 2);

    EXPECT_NEAR(dst[0], 0.0f, kTolerance);
    EXPECT_NEAR(dst[1], 0.5f, kTolerance);
    EXPECT_NEAR(dst[2], -1.0f / 8388608.0f, kTolerance);
    EXPECT_NEAR(dst[3], 256.0f / 8388608.0f, kTolerance);
}

TEST_F(FormatConvertTest, Float32ToInt24MaxValue)
{
    const float src[] = {1.0f};
    std::uint8_t dst[3];
    float32ToInt24(src, dst, 1, 1);

    // 8388607 = 0x7FFFFF
    EXPECT_EQ(dst[0], 0xFF);
    EXPECT_EQ(dst[1], 0xFF);
    EXPECT_EQ(dst[2], 0x7F);
}

TEST_F(FormatConvertTest, Float32ToInt24MinValue)
{
    const float src[] = {-1.0f};
    std::uint8_t dst[3];
    float32ToInt24(src, dst, 1, 1);

    // -8388608 as 24-bit = 0x800000
    EXPECT_EQ(dst[0], 0x00);
    EXPECT_EQ(dst[1], 0x00);
    EXPECT_EQ(dst[2], 0x80);
}

TEST_F(FormatConvertTest, Float32ToInt24Zero)
{
    const float src[] = {0.0f};
    std::uint8_t dst[3];
    float32ToInt24(src, dst, 1, 1);

    EXPECT_EQ(dst[0], 0);
    EXPECT_EQ(dst[1], 0);
    EXPECT_EQ(dst[2], 0);
}

TEST_F(FormatConvertTest, Float32ToInt24Clamps)
{
    const float src[] = {2.0f, -2.0f};
    std::uint8_t dst[6];
    float32ToInt24(src, dst, 2, 1);

    // 2.0 clamps to 1.0 → 0x7FFFFF
    EXPECT_EQ(dst[0], 0xFF);
    EXPECT_EQ(dst[1], 0xFF);
    EXPECT_EQ(dst[2], 0x7F);

    // -2.0 clamps to -1.0 → 0x800000
    EXPECT_EQ(dst[3], 0x00);
    EXPECT_EQ(dst[4], 0x00);
    EXPECT_EQ(dst[5], 0x80);
}

TEST_F(FormatConvertTest, Int24RoundTrip)
{
    const std::uint8_t original[] = {
        0x00, 0x00, 0x00,  // 0
        0x00, 0x10, 0x00,  // 4096
        0xFF, 0xFF, 0xFF,  // -1
        0xFF, 0xFF, 0x7F   // 8388607
    };

    float temp[4];
    std::uint8_t roundtrip[12];

    int24ToFloat32(original, temp, 4, 1);
    float32ToInt24(temp, roundtrip, 4, 1);

    // Verify round-trip (with some tolerance due to float rounding).
    for (int i = 0; i < 4; ++i)
    {
        // Check byte-by-byte.
        bool match = (roundtrip[i * 3 + 0] == original[i * 3 + 0])
                  && (roundtrip[i * 3 + 1] == original[i * 3 + 1])
                  && (roundtrip[i * 3 + 2] == original[i * 3 + 2]);
        EXPECT_TRUE(match) << "Sample " << i << " round-trip mismatch";
    }
}

TEST_F(FormatConvertTest, MixedMultiChannel)
{
    // Test Int16 conversion with multiple channels.
    const std::int16_t src[] = {
        100, 200,   // Frame 0: L, R
        300, 400    // Frame 1: L, R
    };
    float dst[4];
    int16ToFloat32(src, dst, 2, 2);

    EXPECT_NEAR(dst[0], 100.0f / 32768.0f, kTolerance);
    EXPECT_NEAR(dst[1], 200.0f / 32768.0f, kTolerance);
    EXPECT_NEAR(dst[2], 300.0f / 32768.0f, kTolerance);
    EXPECT_NEAR(dst[3], 400.0f / 32768.0f, kTolerance);
}

// ---------------------------------------------------------------------------
// Planar (deinterleaving) variants. Output layout is channel-major:
// channel c occupies dst[c * frames .. c * frames + frames).
// ---------------------------------------------------------------------------

TEST_F(FormatConvertTest, Int16ToFloat32PlanarDeinterleaves)
{
    // 2 frames, 2 channels, interleaved L R L R.
    const std::int16_t src[] = {100, 200, 300, 400};
    float dst[4];
    int16ToFloat32Planar(src, dst, 2, 2);

    // Channel 0 (L): frames 0,1 → dst[0], dst[1].
    EXPECT_NEAR(dst[0], 100.0f / 32768.0f, kTolerance);
    EXPECT_NEAR(dst[1], 300.0f / 32768.0f, kTolerance);
    // Channel 1 (R): frames 0,1 → dst[2], dst[3].
    EXPECT_NEAR(dst[2], 200.0f / 32768.0f, kTolerance);
    EXPECT_NEAR(dst[3], 400.0f / 32768.0f, kTolerance);
}

TEST_F(FormatConvertTest, Int16ToFloat32PlanarMatchesInterleavedPlusManualDeinterleave)
{
    const std::int16_t src[] = {0, 16384, 32767, -32768};  // 2 frames, 2 channels
    float planar[4];
    int16ToFloat32Planar(src, planar, 2, 2);

    EXPECT_NEAR(planar[0], 0.0f, kTolerance);                 // L frame 0
    EXPECT_NEAR(planar[1], 32767.0f / 32768.0f, kTolerance);  // L frame 1
    EXPECT_NEAR(planar[2], 0.5f, kTolerance);                 // R frame 0
    EXPECT_NEAR(planar[3], -1.0f, kTolerance);                // R frame 1
}

TEST_F(FormatConvertTest, Int24ToFloat32PlanarDeinterleavesWithSignExtension)
{
    // 2 frames, 2 channels (same data as Int24ToFloat32MultiChannel, interleaved).
    const std::uint8_t src[] = {
        0x00, 0x00, 0x00,  // frame0 L: 0
        0x00, 0x00, 0x40,  // frame0 R: 0x400000 → 0.5
        0xFF, 0xFF, 0xFF,  // frame1 L: -1
        0x00, 0x01, 0x00   // frame1 R: 256
    };
    float dst[4];
    int24ToFloat32Planar(src, dst, 2, 2);

    // Channel 0 (L): frames 0,1.
    EXPECT_NEAR(dst[0], 0.0f, kTolerance);
    EXPECT_NEAR(dst[1], -1.0f / 8388608.0f, kTolerance);
    // Channel 1 (R): frames 0,1.
    EXPECT_NEAR(dst[2], 0.5f, kTolerance);
    EXPECT_NEAR(dst[3], 256.0f / 8388608.0f, kTolerance);
}

TEST_F(FormatConvertTest, DeinterleaveFloat32)
{
    // 3 frames, 2 channels, interleaved.
    const float src[] = {1.0f, -1.0f, 0.5f, -0.5f, 0.25f, -0.25f};
    float dst[6];
    deinterleaveFloat32(src, dst, 3, 2);

    // Channel 0.
    EXPECT_NEAR(dst[0], 1.0f, kTolerance);
    EXPECT_NEAR(dst[1], 0.5f, kTolerance);
    EXPECT_NEAR(dst[2], 0.25f, kTolerance);
    // Channel 1.
    EXPECT_NEAR(dst[3], -1.0f, kTolerance);
    EXPECT_NEAR(dst[4], -0.5f, kTolerance);
    EXPECT_NEAR(dst[5], -0.25f, kTolerance);
}

TEST_F(FormatConvertTest, DeinterleaveFloat32MonoIsIdentity)
{
    const float src[] = {0.1f, 0.2f, 0.3f};
    float dst[3];
    deinterleaveFloat32(src, dst, 3, 1);

    EXPECT_NEAR(dst[0], 0.1f, kTolerance);
    EXPECT_NEAR(dst[1], 0.2f, kTolerance);
    EXPECT_NEAR(dst[2], 0.3f, kTolerance);
}
