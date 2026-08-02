// src/tray-app/ipc-client/meter_listener.cpp
//
// T118 — Tray-side meter listener: receives meter frames from the IPC pipe
// and dispatches them to the UI thread.

#include "meter_listener.h"

#include "jyglobalvst/audio_engine.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace jyglobalvst::tray {

// =============================================================================
// MeterListener
// =============================================================================

MeterListener::MeterListener(IAudioEngineListener* ui_listener)
    : ui_listener_(ui_listener)
{
}

void MeterListener::onChainRevision(int new_revision)
{
    juce::MessageManager::callAsync([this, new_revision]() {
        if (ui_listener_)
            ui_listener_->onChainRevision(new_revision);
    });
}

void MeterListener::onPluginFailed(const InstanceId& id,
                                    const std::string& reason)
{
    juce::MessageManager::callAsync([this, id, reason]() {
        if (ui_listener_)
            ui_listener_->onPluginFailed(id, reason);
    });
}

void MeterListener::onDeviceLost(const EndpointId& lost,
                                  const EndpointId& fallback_to)
{
    juce::MessageManager::callAsync([this, lost, fallback_to]() {
        if (ui_listener_)
            ui_listener_->onDeviceLost(lost, fallback_to);
    });
}

void MeterListener::onDeviceRestored(const EndpointId& restored)
{
    juce::MessageManager::callAsync([this, restored]() {
        if (ui_listener_)
            ui_listener_->onDeviceRestored(restored);
    });
}

void MeterListener::onDeviceListChanged()
{
    juce::MessageManager::callAsync([this]() {
        if (ui_listener_)
            ui_listener_->onDeviceListChanged();
    });
}

void MeterListener::onCpuWarning(float rolling_1s_pct)
{
    juce::MessageManager::callAsync([this, rolling_1s_pct]() {
        if (ui_listener_)
            ui_listener_->onCpuWarning(rolling_1s_pct);
    });
}

void MeterListener::onMeterFrame(const MeterFrame& frame)
{
    juce::MessageManager::callAsync([this, frame]() {
        if (ui_listener_)
            ui_listener_->onMeterFrame(frame);
    });
}

void MeterListener::onPresetPartialLoad(
    const std::vector<MissingPluginInfo>& missing)
{
    juce::MessageManager::callAsync([this, missing]() {
        if (ui_listener_)
            ui_listener_->onPresetPartialLoad(missing);
    });
}

}  // namespace jyglobalvst::tray
