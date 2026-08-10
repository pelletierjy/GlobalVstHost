// src/audio-engine/routing/asio_transport.cpp
//
// T091 — JUCE ASIO transport wrapper for JyGlobalVST.
//
// Real implementation requires JUCE ASIO module on Windows. On other hosts
// this cpp compiles to harmless stubs so the engine still links.

#include "asio_transport.h"

#if JUCE_ASIO

#include "../vst-host/seh_wrapper.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_core/juce_core.h>

#endif

namespace jyglobalvst::engine {

ASIOTransport::ASIOTransport() = default;

ASIOTransport::~ASIOTransport() = default;

std::vector<std::string> ASIOTransport::listDevices() const
{
    if (devices_cached_)
        return cached_devices_;

#if JUCE_ASIO
    // Enumerate ASIO drivers into a fresh local list. Use a temporary
    // AudioDeviceManager rather than the engine's live one so this scan can never
    // disturb a running WASAPI/ASIO stream.
    std::vector<std::string> found;
    juce::AudioDeviceManager tempManager;
    for (auto* type : tempManager.getAvailableDeviceTypes())
    {
        if (type->getTypeName() == "ASIO")
        {
            type->scanForDevices();
            for (const auto& name : type->getDeviceNames())
            {
                found.emplace_back(name.toStdString());
            }
            break;
        }
    }

    // CRITICAL: only cache a SUCCESSFUL (non-empty) scan. An ASIO scan can
    // transiently return nothing — e.g. the driver is momentarily held by another
    // client, or was busy during startup. Caching that empty result would leave
    // ASIO permanently unselectable for the rest of the session (the user's
    // "ASIO isn't selectable" symptom). By only latching a non-empty result we
    // re-scan on the next call until the drivers actually enumerate.
    if (!found.empty())
    {
        cached_devices_ = std::move(found);
        devices_cached_ = true;
    }
    return cached_devices_;
#else
    devices_cached_ = true;
    return cached_devices_;
#endif
}

bool ASIOTransport::setPreferred(const SessionConfig& config)
{
    config_ = config;
    return true;
}

AsioSessionReport ASIOTransport::open(juce::AudioDeviceManager* manager,
                                      const std::string& device_name)
{
    AsioSessionReport report;

#if JUCE_ASIO
    if (manager == nullptr)
    {
        report.error = "AudioDeviceManager is null";
        report_ = report;
        return report_;
    }

    // Find the ASIO device type and verify the device exists.
    juce::AudioIODeviceType* asio_type = nullptr;
    for (auto* type : manager->getAvailableDeviceTypes())
    {
        if (type->getTypeName() == "ASIO")
        {
            asio_type = type;
            break;
        }
    }

    if (asio_type == nullptr)
    {
        report.error = "ASIO device type not available";
        report_ = report;
        return report_;
    }

    asio_type->scanForDevices();
    if (!asio_type->getDeviceNames().contains(juce::String(device_name)))
    {
        report.error = "ASIO device not found: " + device_name;
        report_ = report;
        return report_;
    }

    // Switch device type to ASIO.
    manager->setCurrentAudioDeviceType("ASIO", true);

    // Configure the ASIO device using a fresh setup struct.
    // CRITICAL: Do NOT call getAudioDeviceSetup() here. After
    // setCurrentAudioDeviceType(), JUCE's currentSetup still holds the
    // previous WASAPI session's device names. Using getAudioDeviceSetup()
    // would inherit those stale names, causing JUCE to skip the
    // reconfigure ("nothing changed") or open with wrong settings.
    // A default-constructed setup ensures every field is explicitly set,
    // so JUCE always sees a real change and performs a full reconfigure.
    //
    // Also CRITICAL: Explicitly set channel bitmasks. Relying on
    // useDefaultInputChannels with numInputChansNeeded==0 (JUCE default)
    // results in zero active input channels, so the callback receives
    // silence and the plugin chain sees no audio.
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    setup.outputDeviceName = juce::String(device_name);
    setup.inputDeviceName  = juce::String(device_name);
    setup.bufferSize       = config_.buffer_size;
    // 0.0 (the caller's normal request — see applyAsioTransport()) leaves the
    // driver at its current/native rate instead of forcing a hardware retune.
    setup.sampleRate       = config_.sample_rate > 0.0 ? config_.sample_rate : 0.0;
    setup.inputChannels.setRange(0, config_.input_channels, true);
    setup.outputChannels.setRange(0, config_.output_channels, true);
    setup.useDefaultInputChannels  = false;
    setup.useDefaultOutputChannels = false;

    juce::String err = manager->setAudioDeviceSetup(setup, true);

    // Many ASIO drivers are locked to the sample rate / buffer size configured in
    // their own control panel and reject a mismatched explicit request. Retry
    // granularly so a valid buffer-size change isn't lost just because the
    // sample-rate is locked, and vice-versa.
    if (err.isNotEmpty())
    {
        // Retry 1: keep requested buffer size, accept driver's sample rate.
        setup.sampleRate = 0.0;
        err = manager->setAudioDeviceSetup(setup, true);
    }
    if (err.isNotEmpty())
    {
        // Retry 2: keep requested sample rate, accept driver's buffer size.
        setup.sampleRate = config_.sample_rate > 0.0 ? config_.sample_rate : 48000.0;
        setup.bufferSize = 0;
        err = manager->setAudioDeviceSetup(setup, true);
    }
    if (err.isNotEmpty())
    {
        // Retry 3: accept both driver defaults.
        setup.sampleRate = 0.0;
        setup.bufferSize = 0;
        err = manager->setAudioDeviceSetup(setup, true);
    }

    if (err.isNotEmpty())
    {
        report.error = err.toStdString();
        report_ = report;
        negotiated_sample_rate_ = 0.0;
        return report_;
    }

    // Retrieve the actual negotiated sample rate from the opened device.
    if (auto* device = manager->getCurrentAudioDevice())
    {
        negotiated_sample_rate_ = device->getCurrentSampleRate();
    }
    else
    {
        negotiated_sample_rate_ = 0.0;
    }

    report.opened = true;
    report.device_name = device_name;
#else
    (void)manager;
    (void)device_name;
    report.error = "ASIO is unavailable in this build";
#endif

    report_ = report;
    return report_;
}

void ASIOTransport::close() noexcept
{
#if JUCE_ASIO
    (void)report_.device_name;
#endif

    report_.opened = false;
    report_.device_name.clear();
    report_.error.clear();
    negotiated_sample_rate_ = 0.0;
}

double ASIOTransport::getNegotiatedSampleRate() const noexcept
{
    return negotiated_sample_rate_;
}

}  // namespace jyglobalvst::engine
