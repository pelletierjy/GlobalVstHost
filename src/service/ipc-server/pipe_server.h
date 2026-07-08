// src/service/ipc-server/pipe_server.h
//
// T115 — Named-pipe IPC server for tray ↔ service communication.

#pragma once

#include <nlohmann/json.hpp>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>

namespace jyglobalvst {

class IAudioEngine;

namespace service {

class PipeServer
{
public:
    explicit PipeServer(IAudioEngine* engine);
    ~PipeServer();

    bool start();
    void stop();

private:
    void runListener();
    void handleClient(HANDLE hPipe);

    IAudioEngine* engine_ = nullptr;
    std::wstring pipe_name_;
    std::atomic<bool> stop_flag_ {false};
    std::thread listener_thread_;

    std::mutex clients_mutex_;
    std::vector<std::thread> client_threads_;
};

}  // namespace service
}  // namespace jyglobalvst
