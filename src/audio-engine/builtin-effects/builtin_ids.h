#pragma once

#include "jyglobalvst/types.h"
#include <array>
#include <cassert>

// REALTIME CONSTRAINTS HEADER
// ============================================================================
// This header defines reserved UID constants and parameter maps for built-in
// effects. Access is control-thread only (no allocation, no locking). Static
// assertions validate UID stability and uniqueness at compile time.
//
// Do not allocate, call malloc/new, acquire locks, perform I/O, or log from
// functions that reference these constants on the audio thread. All allocation
// and initialization must occur in prepareToPlay or earlier.
// ============================================================================

namespace jyglobalvst::engine::builtin {

// Reserved 16-byte ASCII seeds for built-in effect UIDs.
// These values MUST remain constant across releases for preset compatibility.
constexpr std::array<uint8_t, 16> NIGHTTIME_UID_SEED = {
    'J', 'Y', 'G', 'L', '-', 'N', 'I', 'G', 'H', 'T', 'I', 'M', 'E', '0', '1', '\0'
};

constexpr std::array<uint8_t, 16> EQ_UID_SEED = {
    'J', 'Y', 'G', 'L', '-', 'E', 'Q', '-', 'B', 'A', 'N', 'D', '0', '0', '1', '0'
};

constexpr std::array<uint8_t, 16> COMPRESSOR_UID_SEED = {
    'J', 'Y', 'G', 'L', '-', 'V', 'O', 'L', '-', 'L', 'V', 'L', 'R', '0', '0', '\0'
};

// Reserved PluginUid constants.
constexpr PluginUid NIGHTTIME_UID = PluginUid{
    0x4a, 0x59, 0x47, 0x4c, 0x2d, 0x4e, 0x49, 0x47,  // JYGL-NIG
    0x48, 0x54, 0x49, 0x4d, 0x45, 0x30, 0x31, 0x00   // HTIME01\0
};

constexpr PluginUid EQ_UID = PluginUid{
    0x4a, 0x59, 0x47, 0x4c, 0x2d, 0x45, 0x51, 0x2d,  // JYGL-EQ-
    0x42, 0x41, 0x4e, 0x44, 0x30, 0x30, 0x31, 0x30   // BAND0010
};

constexpr PluginUid COMPRESSOR_UID = PluginUid{
    0x4a, 0x59, 0x47, 0x4c, 0x2d, 0x56, 0x4f, 0x4c,  // JYGL-VOL
    0x2d, 0x4c, 0x56, 0x4c, 0x52, 0x30, 0x30, 0x00   // -LVLR00\0
};

// Compile-time validation: UID stability and uniqueness.
static_assert(NIGHTTIME_UID_SEED.size() == 16, "NIGHTTIME_UID_SEED must be exactly 16 bytes");
static_assert(EQ_UID_SEED.size() == 16, "EQ_UID_SEED must be exactly 16 bytes");
static_assert(COMPRESSOR_UID_SEED.size() == 16, "COMPRESSOR_UID_SEED must be exactly 16 bytes");
static_assert(NIGHTTIME_UID != EQ_UID, "Built-in UIDs must be distinct");
static_assert(NIGHTTIME_UID != COMPRESSOR_UID, "Built-in UIDs must be distinct");
static_assert(EQ_UID != COMPRESSOR_UID, "Built-in UIDs must be distinct");

// ============================================================================
// NIGHT-TIME PROCESSOR PARAMETER IDs
// ============================================================================

namespace nighttime {
    // Parameter indices.
    constexpr int PARAM_PRESET = 0;        // choice {0 Light, 1 Medium, 2 Strong}
    constexpr int PARAM_LOOKAHEAD_MS = 1;  // float 0.0 … 10.0 ms

    constexpr int NUM_PARAMETERS = 2;

    // Preset choices.
    enum class Preset : int
    {
        Light = 0,
        Medium = 1,
        Strong = 2
    };

    // Lookahead range (ms).
    constexpr float LOOKAHEAD_MIN_MS = 0.0f;
    constexpr float LOOKAHEAD_MAX_MS = 10.0f;
    constexpr float LOOKAHEAD_DEFAULT_MS = 0.0f;
}

// ============================================================================
// EQ PROCESSOR PARAMETER IDs
// ============================================================================

namespace eq {
    // Parameter indices for the 10 bands.
    constexpr int PARAM_BAND_0 = 0;
    constexpr int PARAM_BAND_1 = 1;
    constexpr int PARAM_BAND_2 = 2;
    constexpr int PARAM_BAND_3 = 3;
    constexpr int PARAM_BAND_4 = 4;
    constexpr int PARAM_BAND_5 = 5;
    constexpr int PARAM_BAND_6 = 6;
    constexpr int PARAM_BAND_7 = 7;
    constexpr int PARAM_BAND_8 = 8;
    constexpr int PARAM_BAND_9 = 9;
    constexpr int PARAM_BASS_BOOST = 10;
    constexpr int PARAM_INPUT_VOLUME = 11;

    constexpr int NUM_BANDS = 10;
    constexpr int NUM_PARAMETERS = 12;  // 10 bands + bass boost + input volume

    // Fixed band center frequencies (Hz).
    constexpr std::array<float, NUM_BANDS> BAND_CENTERS_HZ = {
        32.0f, 64.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
    };

    // Band gain range (dB).
    constexpr float BAND_GAIN_MIN_DB = -12.0f;
    constexpr float BAND_GAIN_MAX_DB = 12.0f;
    constexpr float BAND_GAIN_DEFAULT_DB = 0.0f;

    // Bass boost range (dB).
    constexpr float BASS_BOOST_MIN_DB = 0.0f;
    constexpr float BASS_BOOST_MAX_DB = 12.0f;
    constexpr float BASS_BOOST_DEFAULT_DB = 0.0f;

    // Input volume trim range (dB), applied before the band filters.
    constexpr float INPUT_VOLUME_MIN_DB = -12.0f;
    constexpr float INPUT_VOLUME_MAX_DB = 12.0f;
    constexpr float INPUT_VOLUME_DEFAULT_DB = 0.0f;

    // Output safety ceiling (dB, just below 0 dBFS).
    constexpr float OUTPUT_CEILING_DB = -0.1f;
}

// ============================================================================
// COMPRESSOR PROCESSOR PARAMETER IDs
// ============================================================================

namespace compressor {
    constexpr int PARAM_THRESHOLD = 0;   // float dB
    constexpr int PARAM_RATIO = 1;       // float 1.0 … 20.0
    constexpr int PARAM_ATTACK = 2;      // float ms
    constexpr int PARAM_RELEASE = 3;     // float ms
    constexpr int PARAM_MAKEUP = 4;      // float dB

    constexpr int NUM_PARAMETERS = 5;

    constexpr float THRESHOLD_MIN_DB = -60.0f;
    constexpr float THRESHOLD_MAX_DB = 0.0f;
    constexpr float THRESHOLD_DEFAULT_DB = -20.0f;

    constexpr float RATIO_MIN = 1.0f;
    constexpr float RATIO_MAX = 20.0f;
    constexpr float RATIO_DEFAULT = 4.0f;

    constexpr float ATTACK_MIN_MS = 0.1f;
    constexpr float ATTACK_MAX_MS = 100.0f;
    constexpr float ATTACK_DEFAULT_MS = 10.0f;

    constexpr float RELEASE_MIN_MS = 1.0f;
    constexpr float RELEASE_MAX_MS = 1000.0f;
    constexpr float RELEASE_DEFAULT_MS = 100.0f;

    constexpr float MAKEUP_MIN_DB = 0.0f;
    constexpr float MAKEUP_MAX_DB = 24.0f;
    constexpr float MAKEUP_DEFAULT_DB = 0.0f;
}

}  // namespace jyglobalvst::engine::builtin
