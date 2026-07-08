// src/audio-engine/routing/asio_transport.h
//
// T091 — JUCE ASIO transport wrapper for JyGlobalVST.
//
// Thin, RT-safe wrapper around JUCE ASIO device session. This class is
// owned and called only from the UI/control thread; the audio callback
// remains AudioEngineImpl juce::AudioIODeviceCallback-style, unchanged.
// Fallback to WASAPI happens at the AudioEngineImpl selection layer when
// ASIO init fails.
//
// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • configure() and setPreferred* run on the control thread only.
//   • open()/close() manage the underlying JUCE AudioDeviceManager device
//     setup and MUST NOT run from the audio thread.
// =====================================================================

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace juce { class AudioDeviceManager; }

namespace jyglobalvst::engine {

struct SessionConfig
{
    int buffer_size {256};
    double sample_rate {48000.0};
    int input_channels {2};
    int output_channels {2};
};

struct AsioSessionReport
{
    bool opened {false};
    std::string device_name;
    std::string error;
};

class ASIOTransport
{
public:
    ASIOTransport();
    ~ASIOTransport();

    ASIOTransport(const ASIOTransport&) = delete;
    ASIOTransport& operator=(const ASIOTransport&) = delete;
    ASIOTransport(ASIOTransport&&) = delete;
    ASIOTransport& operator=(ASIOTransport&&) = delete;

    // Enumerate ASIO output device names. Returns empty list if ASIO is
    // unavailable in this build or no ASIO drivers are installed.
    std::vector<std::string> listDevices() const;

    // Configure the preferred ASIO session parameters. Returns false if
    // the requested settings are unsupported by the driver.
    bool setPreferred(const SessionConfig& config);

    // Open a named ASIO device on the supplied AudioDeviceManager.
    AsioSessionReport open(class juce::AudioDeviceManager* manager,
                           const std::string& device_name);

    // Close the current ASIO session.
    void close() noexcept;

    // Whether an ASIO device is currently open.
    bool isOpen() const noexcept { return report_.opened; }

    // Last opened device report.
    AsioSessionReport lastReport() const noexcept { return report_; }

    // Get the actual negotiated sample rate of the current ASIO device.
    // Returns 0 if no device is open or sample rate is unknown.
    double getNegotiatedSampleRate() const noexcept;

private:
    SessionConfig config_;
    AsioSessionReport report_;
    double negotiated_sample_rate_ {0.0};

    // Cached once on first call; ASIO drivers must not be enumerated repeatedly.
    mutable std::vector<std::string> cached_devices_;
    mutable bool devices_cached_ {false};
};

}  // namespace jyglobalvst::engine
