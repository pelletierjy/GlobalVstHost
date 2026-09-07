// src/tray-app/ipc-client/engine_proxy.cpp
//
// T117 — IPC client in tray app: IAudioEngine implementation over named pipe.

#include "engine_proxy.h"

#include <nlohmann/json.hpp>

#include <windows.h>

namespace jyglobalvst::tray {

namespace {

std::wstring makePipeName()
{
    DWORD sessionId = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &sessionId);
    return L"\\\\.\\pipe\\JyGlobalVST\\v1\\" + std::to_wstring(sessionId);
}

bool probePipe(const std::wstring& name)
{
    HANDLE h = CreateFileW(
        name.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);
    if (h != INVALID_HANDLE_VALUE)
    {
        CloseHandle(h);
        return true;
    }
    return false;
}

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

std::optional<nlohmann::json> recvJson(HANDLE hPipe)
{
    uint32_t len = 0;
    DWORD read = 0;
    if (!ReadFile(hPipe, &len, sizeof(len), &read, nullptr) || read != sizeof(len))
        return std::nullopt;
    if (len == 0 || len > 4 * 1024 * 1024)
        return std::nullopt;

    std::string body;
    body.resize(len);
    DWORD total = 0;
    while (total < len)
    {
        DWORD chunk = 0;
        if (!ReadFile(hPipe, body.data() + total, len - total, &chunk, nullptr))
            return std::nullopt;
        if (chunk == 0)
            return std::nullopt;
        total += chunk;
    }

    try
    {
        return nlohmann::json::parse(body);
    }
    catch (...)
    {
        return std::nullopt;
    }
}

}  // namespace

// =============================================================================
// IpcEngineProxy
// =============================================================================

IpcEngineProxy::IpcEngineProxy()
    : pipe_name_(makePipeName())
{
}

IpcEngineProxy::~IpcEngineProxy()
{
    disconnect();
}

bool IpcEngineProxy::isServiceModeAvailable()
{
    return probePipe(makePipeName());
}

bool IpcEngineProxy::connect()
{
    hPipe_ = CreateFileW(
        pipe_name_.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);

    if (hPipe_ == INVALID_HANDLE_VALUE)
    {
        hPipe_ = nullptr;
        return false;
    }

    DWORD mode = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(hPipe_, &mode, nullptr, nullptr);

    // Send hello.
    nlohmann::json hello = {
        {"protocol_version", 1},
        {"request_id", "hello-1"},
        {"command", "hello"},
        {"payload", {{"client_protocol_version", 1}}}};
    if (!sendJson(hPipe_, hello))
    {
        disconnect();
        return false;
    }

    auto resp = recvJson(hPipe_);
    if (!resp)
    {
        disconnect();
        return false;
    }

    if ((*resp).contains("error") && !(*resp)["error"].is_null())
    {
        disconnect();
        return false;
    }

    // Start background reader for async events (meter frames, notifications).
    reader_thread_ = std::thread([this]() { readLoop(); });
    return true;
}

void IpcEngineProxy::disconnect()
{
    stop_reader_ = true;
    if (reader_thread_.joinable())
        reader_thread_.join();

    if (hPipe_)
    {
        CloseHandle(hPipe_);
        hPipe_ = nullptr;
    }
}

void IpcEngineProxy::readLoop()
{
    while (!stop_reader_ && hPipe_)
    {
        auto msg = recvJson(hPipe_);
        if (!msg)
            continue;

        std::lock_guard<std::mutex> lock(listener_mutex_);
        if (!listener_)
            continue;

        const auto& j = *msg;
        std::string cmd = j.value("command", "");
        if (cmd == "event.meter_frame")
        {
            auto payload = j.value("payload", nlohmann::json::object());
            MeterFrame frame;
            frame.input_peak_l = payload.value("input_peak_l", 0.0f);
            frame.input_peak_r = payload.value("input_peak_r", 0.0f);
            frame.output_peak_l = payload.value("output_peak_l", 0.0f);
            frame.output_peak_r = payload.value("output_peak_r", 0.0f);
            listener_->onMeterFrame(frame);
        }
        else if (cmd == "event.notification")
        {
            // Map to appropriate listener callback based on code.
            auto payload = j.value("payload", nlohmann::json::object());
            std::string code = payload.value("code", "");
            if (code == "cpu.warning")
            {
                listener_->onCpuWarning(payload.value("cpu_pct", 0.0f));
            }
            else if (code == "device.lost")
            {
                listener_->onDeviceLost(
                    payload.value("endpoint_id", ""),
                    payload.value("fallback_endpoint_id", ""));
            }
            else if (code == "device.restored")
            {
                listener_->onDeviceRestored(payload.value("endpoint_id", ""));
            }
            else if (code == "device.list_changed")
            {
                listener_->onDeviceListChanged();
            }
        }
    }
}

nlohmann::json IpcEngineProxy::sendCommand(const std::string& cmd,
                                           const nlohmann::json& payload)
{
    if (!hPipe_)
        return {};

    static std::atomic<int> req_id_counter {1};
    nlohmann::json req = {
        {"protocol_version", 1},
        {"request_id", std::to_string(req_id_counter++)},
        {"command", cmd},
        {"payload", payload}};

    if (!sendJson(hPipe_, req))
        return {};

    auto resp = recvJson(hPipe_);
    if (!resp)
        return {};
    return *resp;
}

// -------------------------------------------------------------------------
// IAudioEngine implementation
// -------------------------------------------------------------------------

void IpcEngineProxy::start()
{
    sendCommand("engine.start", {});
}

void IpcEngineProxy::stop()
{
    sendCommand("engine.stop", {});
}

bool IpcEngineProxy::isRunning() const
{
    return false;
}

void IpcEngineProxy::setListener(IAudioEngineListener* listener)
{
    std::lock_guard<std::mutex> lock(listener_mutex_);
    listener_ = listener;
}

void IpcEngineProxy::setMasterVolume(float gain_linear)
{
    sendCommand("engine.set_master_volume", {{"gain_linear", gain_linear}});
}

void IpcEngineProxy::reset()
{
    sendCommand("engine.reset", {});
}

void IpcEngineProxy::setEnergySaverEnabled(bool enabled)
{
    sendCommand("engine.set_energy_saver_enabled", {{"enabled", enabled}});
}

bool IpcEngineProxy::isEnergySaverEnabled() const
{
    auto resp = const_cast<IpcEngineProxy*>(this)->sendCommand("engine.is_energy_saver_enabled", {});
    if (resp.contains("result") && resp["result"].contains("enabled"))
        return resp["result"]["enabled"].get<bool>();
    return false;
}

bool IpcEngineProxy::isEnergySaverSleeping() const
{
    auto resp = const_cast<IpcEngineProxy*>(this)->sendCommand("engine.is_energy_saver_sleeping", {});
    if (resp.contains("result") && resp["result"].contains("sleeping"))
        return resp["result"]["sleeping"].get<bool>();
    return false;
}

std::vector<HardwareOutputInfo> IpcEngineProxy::listOutputs() const
{
    auto resp = const_cast<IpcEngineProxy*>(this)->sendCommand("device.list_outputs", {});
    std::vector<HardwareOutputInfo> out;
    if (resp.contains("result") && resp["result"].contains("outputs"))
    {
        for (const auto& j : resp["result"]["outputs"])
        {
            HardwareOutputInfo info;
            info.endpoint_id = j.value("endpoint_id", "");
            info.friendly_name = j.value("friendly_name", "");
            info.is_default = j.value("is_default", false);
            info.is_present = j.value("is_present", true);
            info.is_loopback = j.value("is_loopback", false);
            out.push_back(info);
        }
    }
    return out;
}

void IpcEngineProxy::selectOutput(const EndpointId& id)
{
    sendCommand("device.select_output", {{"endpoint_id", id}});
}

EndpointId IpcEngineProxy::currentOutput() const
{
    // Not directly exposed over IPC; derive from listOutputs.
    auto outputs = const_cast<IpcEngineProxy*>(this)->listOutputs();
    for (const auto& o : outputs)
        if (o.is_default)
            return o.endpoint_id;
    return {};
}

DeviceResolutionSource IpcEngineProxy::currentResolutionSource() const
{
    return DeviceResolutionSource::EndpointIdMatch;
}

void IpcEngineProxy::setBufferSize(int samples)
{
    sendCommand("buffer.set_size", {{"buffer_size", samples}});
}

int IpcEngineProxy::bufferSize() const
{
    return 256;
}

void IpcEngineProxy::setSampleRate(double /*rate*/)
{
}

double IpcEngineProxy::sampleRate() const
{
    return 48000.0;
}

int IpcEngineProxy::negotiatedSampleRate() const
{
    return 48000;
}

int IpcEngineProxy::outputDeviceSampleRate() const
{
    // Not wired over IPC; the tray app hosts the engine in-process, so the real
    // value comes from AudioEngineImpl. Mirror the negotiated rate as a stub.
    return negotiatedSampleRate();
}

int IpcEngineProxy::inputDeviceSampleRate() const
{
    // Not wired over IPC (see outputDeviceSampleRate). Mirror the negotiated rate.
    return negotiatedSampleRate();
}

void IpcEngineProxy::setAsioOutputPair(int /*channel_offset*/)
{
}

int IpcEngineProxy::asioOutputPair() const
{
    return 0;
}

void IpcEngineProxy::openAsioControlPanel()
{
}

std::vector<HardwareOutputInfo> IpcEngineProxy::listInputs() const
{
    return {};
}

void IpcEngineProxy::selectInput(const EndpointId& /*id*/)
{
}

EndpointId IpcEngineProxy::currentInput() const
{
    return {};
}

void IpcEngineProxy::rescanPlugins(IScanProgressListener* /*progress*/)
{
}

void IpcEngineProxy::cancelScan()
{
}

std::vector<PluginCatalogEntry> IpcEngineProxy::catalog() const
{
    return {};
}

ChainSnapshot IpcEngineProxy::snapshotChain() const
{
    auto resp = const_cast<IpcEngineProxy*>(this)->sendCommand("chain.snapshot", {});
    ChainSnapshot snap;
    if (resp.contains("result"))
    {
        auto result = resp["result"];
        snap.chain_revision = result.value("chain_revision", 0);
    }
    return snap;
}

InstanceId IpcEngineProxy::addPlugin(const PluginRef& ref, int position)
{
    nlohmann::json payload = {
        {"plugin_uid", PluginUidToHexString(ref.plugin_uid)},
        {"plugin_vendor", ref.vendor},
        {"plugin_name", ref.name},
        {"position", position}};
    auto resp = sendCommand("chain.add", payload);
    InstanceId id;
    if (resp.contains("result") && resp["result"].contains("instance_id"))
    {
        // Simplified: treat non-empty as success.
        id.high = 1;
    }
    return id;
}

InstanceId IpcEngineProxy::addPluginFromPath(const std::filesystem::path& /*vst3_path*/,
                                              int /*position*/)
{
    return {};
}

void IpcEngineProxy::removeSlot(int position)
{
    sendCommand("chain.remove", {{"position", position}});
}

void IpcEngineProxy::moveSlot(int from, int to)
{
    sendCommand("chain.move", {{"from_position", from}, {"to_position", to}});
}

void IpcEngineProxy::setBypass(int position, bool bypassed)
{
    sendCommand("chain.set_bypass",
                {{"position", position}, {"is_bypassed", bypassed}});
}

void IpcEngineProxy::setSlotTag(int position, const std::string& tag)
{
    sendCommand("chain.set_tag", {{"position", position}, {"tag", tag}});
}

void IpcEngineProxy::setParameter(int position, ParamId param, float value)
{
    sendCommand("chain.set_parameter",
                {{"position", position},
                 {"parameter_id", param},
                 {"value", value}});
}

void IpcEngineProxy::openEditor(int /*position*/)
{
}

void IpcEngineProxy::closeEditor(int /*position*/)
{
}

void IpcEngineProxy::repointPlaceholder(int position, const PluginRef& ref)
{
    nlohmann::json payload = {
        {"position", position},
        {"plugin_uid", PluginUidToHexString(ref.plugin_uid)},
        {"plugin_vendor", ref.vendor},
        {"plugin_name", ref.name}};
    sendCommand("chain.repoint_placeholder", payload);
}

void IpcEngineProxy::loadPreset(const std::filesystem::path& path)
{
    sendCommand("preset.load", {{"file_path", path.string()}});
}

void IpcEngineProxy::savePreset(const std::filesystem::path& /*path*/,
                                const std::string& /*name*/)
{
}

void IpcEngineProxy::restoreChain(const std::filesystem::path& /*path*/)
{
}

void IpcEngineProxy::setWasapiExclusive(bool /*exclusive*/) {}

bool IpcEngineProxy::wasapiExclusive() const
{
    return false;
}

LatencyProfile IpcEngineProxy::latencyProfile() const
{
    return {};
}

CpuStats IpcEngineProxy::cpuStats() const
{
    return {};
}

MeterFrame IpcEngineProxy::latestMeterFrame() const
{
    return {};
}

std::vector<float> IpcEngineProxy::pluginOutputPeaks() const
{
    return {};
}

std::vector<float> IpcEngineProxy::pluginOutputRms() const
{
    return {};
}

}  // namespace jyglobalvst::tray
