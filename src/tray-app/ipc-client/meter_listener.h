// src/tray-app/ipc-client/meter_listener.h
//
// T118 — Tray-side meter listener.
// Receives async events from the IPC reader thread and forwards them to the
// UI thread via JUCE MessageManager::callAsync.

#pragma once

#include "jyglobalvst/audio_engine.h"

namespace jyglobalvst::tray {

class MeterListener : public IAudioEngineListener
{
public:
    explicit MeterListener(IAudioEngineListener* ui_listener);

    // IAudioEngineListener --------------------------------------------------
    void onChainRevision(int new_revision) override;
    void onPluginFailed(const InstanceId& id,
                        const std::string& reason) override;
    void onDeviceLost(const EndpointId& lost,
                      const EndpointId& fallback_to) override;
    void onDeviceRestored(const EndpointId& restored) override;
    void onCpuWarning(float rolling_1s_pct) override;
    void onMeterFrame(const MeterFrame& frame) override;
    void onPresetPartialLoad(
        const std::vector<MissingPluginInfo>& missing) override;

private:
    IAudioEngineListener* ui_listener_ = nullptr;
};

}  // namespace jyglobalvst::tray
