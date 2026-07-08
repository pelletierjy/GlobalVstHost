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
    // Create the temp manager only once — ASIO drivers must not be loaded and
    // unloaded repeatedly within the same process or the driver state corrupts.
    juce::AudioDeviceManager tempManager;
    for (auto* type : tempManager.getAvailableDeviceTypes())
    {
        if (type->getTypeName() == "ASIO")
        {
            type->scanForDevices();
            for (const auto& name : type->getDeviceNames())
            {
                cached_devices_.emplace_back(name.toStdString());
            }
            break;
        }
    }
#endif

    devices_cached_ = true;
    return cached_devices_;
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
    setup.sampleRate       = config_.sample_rate > 0.0 ? config_.sample_rate : 48000.0;
    setup.inputChannels.setRange(0, config_.input_channels, true);
    setup.outputChannels.setRange(0, config_.output_channels, true);
    setup.useDefaultInputChannels  = false;
    setup.useDefaultOutputChannels = false;

    const juce::String err = manager->setAudioDeviceSetup(setup, true);
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
