// src/service/ipc-server/meter_pusher.cpp
//
// T118 — Service-side meter-frame pusher: forwards engine meter frames to
// all subscribed pipe clients.

#include "meter_pusher.h"

#include "pipe_server.h"
#include "jyglobalvst/audio_engine.h"

#include <nlohmann/json.hpp>

namespace jyglobalvst::service {

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

namespace {

bool sendJson(HANDLE hPipe, const nlohmann::json& j)
{
    std::string body = j.dump();
    uint32_t len = static_cast<uint32_t>(body.size());
    DWORD written = 0;
    if (!WriteFile(hPipe, &len, sizeof(len), &written, nullptr))
        return false;
    if (!WriteFile(hPipe, body.data(), len, &written, nullptr))
        return false;
    return true;
}

}  // namespace

// =============================================================================
// MeterPusher
// =============================================================================

MeterPusher::MeterPusher(IAudioEngine* engine)
    : engine_(engine)
{
}

MeterPusher::~MeterPusher()
{
    if (engine_)
        engine_->setListener(nullptr);
}

void MeterPusher::registerPipe(HANDLE hPipe)
{
    std::lock_guard<std::mutex> lock(clients_mutex_);
    clients_.push_back(hPipe);
}

void MeterPusher::unregisterPipe(HANDLE hPipe)
{
    std::lock_guard<std::mutex> lock(clients_mutex_);
    clients_.erase(
        std::remove(clients_.begin(), clients_.end(), hPipe),
        clients_.end());
}

void MeterPusher::onChainRevision(int new_revision)
{
    (void)new_revision;
}

void MeterPusher::onPluginFailed(const InstanceId& /*id*/,
                                  const std::string& /*reason*/)
{
}

void MeterPusher::onDeviceLost(const EndpointId& /*lost*/,
                                const EndpointId& /*fallback_to*/)
{
}

void MeterPusher::onDeviceRestored(const EndpointId& /*restored*/)
{
}

void MeterPusher::onCpuWarning(float rolling_1s_pct)
{
    nlohmann::json evt = {
        {"command", "event.notification"},
        {"payload",
         {{"severity", "warn"},
          {"code", "cpu.warning"},
          {"message", "CPU approaching limit; consider increasing buffer size"},
          {"context", {{"cpu_pct", rolling_1s_pct}}}}}};
    broadcast(evt);
}

void MeterPusher::onMeterFrame(const MeterFrame& frame)
{
    nlohmann::json evt = {
        {"command", "event.meter_frame"},
        {"payload",
         {{"timestamp_us", 0},
          {"input_peak_l", frame.input_peak_l},
          {"input_peak_r", frame.input_peak_r},
          {"output_peak_l", frame.output_peak_l},
          {"output_peak_r", frame.output_peak_r},
          {"cpu_pct", 0.0f},
          {"latency_ms", 0.0f}}}};
    broadcast(evt);
}

void MeterPusher::onPresetPartialLoad(
    const std::vector<MissingPluginInfo>& missing)
{
    (void)missing;
}

void MeterPusher::broadcast(const nlohmann::json& msg)
{
    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (auto it = clients_.begin(); it != clients_.end();)
    {
        if (!sendJson(*it, msg))
        {
            // Pipe broken; remove from list.
            it = clients_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

}  // namespace jyglobalvst::service
