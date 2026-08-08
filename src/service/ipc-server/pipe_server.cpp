// src/service/ipc-server/pipe_server.cpp
//
// T115 — Named-pipe IPC server for tray ↔ service communication.

#include "pipe_server.h"

#include "session_auth.h"
#include "jyglobalvst/audio_engine.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <windows.h>

namespace jyglobalvst::service {

namespace {

// -------------------------------------------------------------------------
// Wire framing: 4-byte LE length prefix + UTF-8 JSON body.
// -------------------------------------------------------------------------

bool sendJson(HANDLE hPipe, const nlohmann::json& j)
{
    std::string body = j.dump();
    if (body.size() > 4 * 1024 * 1024)
        return false;  // Protocol limit.

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

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

std::wstring makePipeName(DWORD sessionId)
{
    return L"\\\\.\\pipe\\JyGlobalVST\\v1\" + std::to_wstring(sessionId);
}

nlohmann::json makeError(const std::string& code, const std::string& message)
{
    return nlohmann::json{
        {"error", {{"code", code}, {"message", message}}},
        {"result", nullptr}};
}

nlohmann::json deviceListToJson(const std::vector<HardwareOutputInfo>& devices)
{
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& d : devices)
    {
        arr.push_back({
            {"endpoint_id", d.endpoint_id},
            {"friendly_name", d.friendly_name},
            {"is_default", d.is_default},
            {"is_present", d.is_present},
            {"is_loopback", d.is_loopback}});
    }
    return arr;
}

nlohmann::json chainSnapshotToJson(const ChainSnapshot& snap)
{
    nlohmann::json slots = nlohmann::json::array();
    for (const auto& slot : snap.slots)
    {
        nlohmann::json j = {
            {"instance_id",
             jyglobalvst::PluginUidToHexString(slot.ref.plugin_uid)},
            {"kind",
             slot.kind == PluginSlotKind::Plugin ? "plugin" : "placeholder"},
            {"position", slot.position},
            {"is_bypassed", slot.is_bypassed},
            {"is_failed", slot.is_failed}};
        if (slot.kind == PluginSlotKind::Plugin)
        {
            j["plugin_uid"] = jyglobalvst::PluginUidToHexString(slot.ref.plugin_uid);
            j["plugin_vendor"] = slot.ref.vendor;
            j["plugin_name"] = slot.ref.name;
        }
        slots.push_back(j);
    }
    return {{"chain_revision", snap.chain_revision}, {"slots", slots}};
}

}  // namespace

// =============================================================================
// PipeServer
// =============================================================================

PipeServer::PipeServer(IAudioEngine* engine)
    : engine_(engine)
{
}

PipeServer::~PipeServer()
{
    stop();
}

bool PipeServer::start()
{
    DWORD sessionId = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &sessionId);
    pipe_name_ = makePipeName(sessionId);

    stop_flag_ = false;
    listener_thread_ = std::thread([this]() { runListener(); });
    return true;
}

void PipeServer::stop()
{
    stop_flag_ = true;

    // Close the listener pipe to unblock WaitNamedPipe / ConnectNamedPipe.
    HANDLE h = CreateFileW(
        pipe_name_.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);
    if (h != INVALID_HANDLE_VALUE)
        CloseHandle(h);

    if (listener_thread_.joinable())
        listener_thread_.join();
}

void PipeServer::runListener()
{
    while (!stop_flag_)
    {
        HANDLE hPipe = CreateNamedPipeW(
            pipe_name_.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            65536,
            65536,
            0,
            nullptr);

        if (hPipe == INVALID_HANDLE_VALUE)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        BOOL connected = ConnectNamedPipe(hPipe, nullptr)
            ? TRUE
            : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (!connected)
        {
            CloseHandle(hPipe);
            continue;
        }

        if (stop_flag_)
        {
            DisconnectNamedPipe(hPipe);
            CloseHandle(hPipe);
            break;
        }

        // Spawn a client thread.
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            client_threads_.emplace_back(
                [this, hPipe]() { handleClient(hPipe); });
        }

        // Reap finished threads.
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto it = client_threads_.begin(); it != client_threads_.end();)
        {
            if (it->joinable())
            {
                // We can't detect completion without a flag; just leave them.
                // On stop() we will detach or join all.
                ++it;
            }
            else
            {
                it = client_threads_.erase(it);
            }
        }
    }
}

void PipeServer::handleClient(HANDLE hPipe)
{
    // T116 — authenticate session.
    if (!authenticatePipeClient(hPipe))
    {
        nlohmann::json authError = {
            {"protocol_version", 1},
            {"command", "hello"},
            {"result", nullptr},
            {"error", {{"code", "auth.session_mismatch"},
                       {"message", "Client session does not match server session"}}}};
        sendJson(hPipe, authError);
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
        return;
    }

    bool subscribed_meters = false;

    while (!stop_flag_)
    {
        auto maybeMsg = recvJson(hPipe);
        if (!maybeMsg)
            break;

        const auto& msg = *maybeMsg;
        nlohmann::json response;
        response["protocol_version"] = 1;
        response["request_id"] = msg.value("request_id", "");
        response["command"] = msg.value("command", "");

        std::string cmd = msg.value("command", "");
        if (cmd == "hello")
        {
            DWORD sessionId = 0;
            ProcessIdToSessionId(GetCurrentProcessId(), &sessionId);
            response["result"] = {
                {"server_protocol_version", 1},
                {"session_id", static_cast<int>(sessionId)}};
            response["error"] = nullptr;
        }
        else if (cmd == "chain.snapshot")
        {
            response["result"] = chainSnapshotToJson(engine_->snapshotChain());
            response["error"] = nullptr;
        }
        else if (cmd == "chain.add")
        {
            auto payload = msg.value("payload", nlohmann::json::object());
            PluginRef ref;
            ref.plugin_uid = HexStringToPluginUid(payload.value("plugin_uid", ""));
            ref.vendor = payload.value("plugin_vendor", "");
            ref.name = payload.value("plugin_name", "");
            int pos = payload.value("position", 0);
            auto id = engine_->addPlugin(ref, pos);
            response["result"] = {
                {"chain_revision", engine_->snapshotChain().chain_revision},
                {"instance_id", id.isNull() ? "" : "ok"}};
            response["error"] = nullptr;
        }
        else if (cmd == "chain.remove")
        {
            auto payload = msg.value("payload", nlohmann::json::object());
            engine_->removeSlot(payload.value("position", 0));
            response["result"] = {
                {"chain_revision", engine_->snapshotChain().chain_revision}};
            response["error"] = nullptr;
        }
        else if (cmd == "chain.move")
        {
            auto payload = msg.value("payload", nlohmann::json::object());
            engine_->moveSlot(payload.value("from_position", 0),
                            payload.value("to_position", 0));
            response["result"] = {
                {"chain_revision", engine_->snapshotChain().chain_revision}};
            response["error"] = nullptr;
        }
        else if (cmd == "chain.set_bypass")
        {
            auto payload = msg.value("payload", nlohmann::json::object());
            engine_->setBypass(payload.value("position", 0),
                              payload.value("is_bypassed", false));
            response["result"] = {
                {"chain_revision", engine_->snapshotChain().chain_revision}};
            response["error"] = nullptr;
        }
        else if (cmd == "chain.set_parameter")
        {
            auto payload = msg.value("payload", nlohmann::json::object());
            engine_->setParameter(payload.value("position", 0),
                                  payload.value("parameter_id", 0),
                                  payload.value("value", 0.0f));
            response["result"] = {
                {"chain_revision", engine_->snapshotChain().chain_revision}};
            response["error"] = nullptr;
        }
        else if (cmd == "chain.repoint_placeholder")
        {
            auto payload = msg.value("payload", nlohmann::json::object());
            PluginRef ref;
            ref.plugin_uid = HexStringToPluginUid(payload.value("plugin_uid", ""));
            ref.vendor = payload.value("plugin_vendor", "");
            ref.name = payload.value("plugin_name", "");
            engine_->repointPlaceholder(payload.value("position", 0), ref);
            response["result"] = {
                {"chain_revision", engine_->snapshotChain().chain_revision}};
            response["error"] = nullptr;
        }
        else if (cmd == "device.list_outputs")
        {
            response["result"] = {
                {"outputs", deviceListToJson(engine_->listOutputs())}};
            response["error"] = nullptr;
        }
        else if (cmd == "device.select_output")
        {
            auto payload = msg.value("payload", nlohmann::json::object());
            engine_->selectOutput(payload.value("endpoint_id", ""));
            response["result"] = {
                {"negotiated_sample_rate", engine_->negotiatedSampleRate()},
                {"negotiated_bit_depth", "Float32"}};
            response["error"] = nullptr;
        }
        else if (cmd == "buffer.set_size")
        {
            auto payload = msg.value("payload", nlohmann::json::object());
            int size = payload.value("buffer_size", 256);
            engine_->setBufferSize(size);
            float latency_ms = (size * 1000.0f) /
                               static_cast<float>(engine_->negotiatedSampleRate());
            response["result"] = {
                {"applied", true},
                {"new_latency_ms", latency_ms}};
            response["error"] = nullptr;
        }
        else if (cmd == "preset.load")
        {
            auto payload = msg.value("payload", nlohmann::json::object());
            engine_->loadPreset(payload.value("file_path", ""));
            response["result"] = {
                {"chain_revision", engine_->snapshotChain().chain_revision},
                {"missing_plugins", nlohmann::json::array()}};
            response["error"] = nullptr;
        }
        else if (cmd == "subscribe.meters")
        {
            subscribed_meters = true;
            response["result"] = {{"subscribed", true}};
            response["error"] = nullptr;
        }
        else
        {
            response["result"] = nullptr;
            response["error"] = {
                {"code", "protocol.unknown_command"},
                {"message", "Unknown command: " + cmd}};
        }

        if (!sendJson(hPipe, response))
            break;
    }

    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
}

}  // namespace jyglobalvst::service
