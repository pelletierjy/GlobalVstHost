// src/service/ipc-server/meter_pusher.h
//
// T118 — Service-side meter-frame pusher.

#pragma once

#include "jyglobalvst/audio_engine.h"

#include <nlohmann/json.hpp>

#include <mutex>
#include <vector>
#include <windows.h>

namespace jyglobalvst::service {

// Forwards engine listener events (meter frames, CPU warnings) to all
// connected pipe clients that have subscribed.
class MeterPusher : public IAudioEngineListener
{
public:
    explicit MeterPusher(IAudioEngine* engine);
    ~MeterPusher() override;

    void registerPipe(HANDLE hPipe);
    void unregisterPipe(HANDLE hPipe);

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
    void broadcast(const nlohmann::json& msg);

    IAudioEngine* engine_ = nullptr;
    std::mutex clients_mutex_;
    std::vector<HANDLE> clients_;
};

}  // namespace jyglobalvst::service
