// src/audio-engine/vst-host/plugin_scanner.h
//
// T055 — PluginScanner.
//
// Background std::thread, atomic-cancellable, incremental enqueue to UI thread,
// progress reporting per FR-005 and research.md §4.
//
// Not touched from the audio thread; no REALTIME CONSTRAINTS header required.

#pragma once

#include "plugin_instance.h"

#include <jyglobalvst/types.h>

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace jyglobalvst::engine {

class VST3PluginLoader;
class ScanCache;
class PluginInstance;

class PluginScanner
{
public:
    PluginScanner();
    ~PluginScanner();

    PluginScanner(const PluginScanner&) = delete;
    PluginScanner& operator=(const PluginScanner&) = delete;

    // Start a background scan of the given paths. Progress events are delivered
    // on the calling thread via the listener (which may post to UI thread).
    void start(const std::vector<std::filesystem::path>& paths,
               IScanProgressListener* listener,
               ScanCache* cache);

    // Request cancellation. The scan thread will exit at the next safe point.
    void cancel();

    // Block until the scan thread has exited. Safe to call from any thread.
    void waitUntilFinished();

    // Returns true if a scan is currently running.
    bool isRunning() const noexcept;

private:
    void runScan(const std::vector<std::filesystem::path>& paths,
                 IScanProgressListener* listener,
                 ScanCache* cache);

    std::atomic<bool> cancel_flag_ {false};
    std::atomic<bool> running_ {false};
    std::thread thread_;
    std::mutex thread_mutex_;
    std::vector<std::filesystem::path> current_paths_;

    // Keep scanned plugin instances alive for the app lifetime.
    // Prevents VST3 modules from being unloaded, which avoids crashes
    // in plugins that leak callbacks / window hooks (e.g. IK Multimedia ARC 4).
    std::vector<std::unique_ptr<PluginInstance>> scanned_instances_;
};

}  // namespace jyglobalvst::engine
