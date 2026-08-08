// src/audio-engine/include/jyglobalvst/types.h
//
// Shared value types used by both the audio engine and its callers. Per the
// data-model.md, these mirror entities the engine exposes through IAudioEngine.

#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace jyglobalvst {

using EndpointId = std::string;  // Opaque Windows endpoint identifier.

enum class TransportKind : std::uint8_t
{
    Wasapi,
    Asio,
};

struct InstanceId
{
    std::uint64_t high {0};
    std::uint64_t low {0};

    bool operator==(const InstanceId&) const = default;
    bool isNull() const noexcept { return high == 0 && low == 0; }
};

using PluginUid = std::array<std::uint8_t, 16>;  // Steinberg VST3 TUID.

// Convert a 32-char hex string (e.g. "ABCDEF0123456789ABCDEF0123456789") to PluginUid.
// Returns all-zeroes on invalid input (never throws).
inline PluginUid HexStringToPluginUid(const std::string& hex)
{
    PluginUid uid {};
    if (hex.size() < 32)
        return uid;

    auto hexValue = [](char c) -> std::uint8_t {
        if (c >= '0' && c <= '9')
            return static_cast<std::uint8_t>(c - '0');
        if (c >= 'a' && c <= 'f')
            return static_cast<std::uint8_t>(10 + c - 'a');
        if (c >= 'A' && c <= 'F')
            return static_cast<std::uint8_t>(10 + c - 'A');
        return 0xFF; // sentinel for invalid
    };

    for (std::size_t i = 0; i < 16; ++i)
    {
        auto hi = hexValue(hex[i * 2]);
        auto lo = hexValue(hex[i * 2 + 1]);
        if (hi > 0x0F || lo > 0x0F)
            return uid; // invalid character → return zeroes
        uid[i] = static_cast<std::uint8_t>((hi << 4) | lo);
    }
    return uid;
}

// Convert PluginUid to a 32-char lowercase hex string.
inline std::string PluginUidToHexString(const PluginUid& uid)
{
    std::string hex;
    hex.reserve(32);
    for (auto b : uid)
    {
        constexpr char digits[] = "0123456789abcdef";
        hex.push_back(digits[b >> 4]);
        hex.push_back(digits[b & 0x0F]);
    }
    return hex;
}

using ParamId = std::uint32_t;

enum class DeviceResolutionSource : std::uint8_t
{
    EndpointIdMatch,
    FriendlyNameMatch,
    WindowsDefaultFallback,
};

enum class PluginSlotKind : std::uint8_t
{
    Plugin,
    Placeholder,
};

enum class BitDepth : std::uint8_t
{
    Int16,
    Int24,
    Float32,
};

struct HardwareOutputInfo
{
    EndpointId endpoint_id;
    std::string friendly_name;
    bool is_default {false};
    bool is_present {true};
    TransportKind transport_kind {TransportKind::Wasapi};
    // True when this input source is a render endpoint captured in loopback mode
    // (system audio). False for real capture devices (microphone, line-in, etc.).
    bool is_loopback {false};
};

struct PluginRef
{
    PluginUid plugin_uid {};
    std::string vendor;
    std::string name;
};

struct PluginCatalogEntry
{
    PluginRef ref;
    std::string version;
    std::string category;
    std::filesystem::path file_path;
    bool supports_double_precision {false};
    bool has_editor {false};
    std::chrono::system_clock::time_point scan_timestamp {};
    bool is_blocklisted_by_user {false};
};

struct ChainSlotSnapshot
{
    PluginSlotKind kind {PluginSlotKind::Plugin};
    InstanceId instance_id {};
    PluginRef ref {};
    int position {0};
    bool is_bypassed {false};
    bool is_failed {false};
    bool editor_open {false};
    std::string file_path; // Source .vst3 path (for preset fallback loading)
};

struct ChainSnapshot
{
    int chain_revision {0};
    std::vector<ChainSlotSnapshot> slots;
};

struct MissingPluginInfo
{
    PluginRef ref;
    int position {0};
};

struct MeterFrame
{
    float input_peak_l {0.f};
    float input_peak_r {0.f};
    float input_rms_l {0.f};
    float input_rms_r {0.f};
    float output_peak_l {0.f};
    float output_peak_r {0.f};
    float output_rms_l {0.f};
    float output_rms_r {0.f};
    std::chrono::steady_clock::time_point timestamp {};
};

struct LatencyProfile
{
    float capture_ms {0.f};
    float resample_ms {0.f};
    float plugin_chain_ms {0.f};
    float output_ms {0.f};
    float total_round_trip_ms {0.f};
    std::chrono::steady_clock::time_point last_updated {};
};

struct CpuStats
{
    float instantaneous_pct {0.f};
    float rolling_1s_pct {0.f};
    std::uint64_t xrun_count_session {0};
    bool warning_active {false};
};

class IScanProgressListener
{
public:
    virtual ~IScanProgressListener() = default;

    virtual void onScanStarted(int total_paths) = 0;
    virtual void onPathStarted(const std::filesystem::path& path) = 0;
    virtual void onPluginDiscovered(const PluginCatalogEntry& entry) = 0;
    virtual void onScanFinished(int plugins_discovered) = 0;
    virtual void onScanCancelled() = 0;
};

}  // namespace jyglobalvst
