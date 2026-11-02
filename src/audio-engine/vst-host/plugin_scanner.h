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

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <jyglobalvst/types.h>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace jyglobalvst::engine {

class VST3PluginLoader;
class ScanCache;
class PluginInstance;

// Enumerates .vst3 bundles under root: top level plus one level of vendor
// subdirectories (VST3.7 per-user layouts such as VST3\<Vendor>\Plugin.vst3).
// Never descends INTO a .vst3 bundle — its Contents\x86_64-win tree holds the
// module binary, not a separate plugin. The root is normalized to an absolute
// path so packaged (MSIX) launches, where the working directory is not the
// install directory, resolve identically. Exposed for unit tests.
std::vector<std::filesystem::path> collectVst3Candidates(const std::filesystem::path& root);

class PluginScanner
{
public:
    PluginScanner();
    ~PluginScanner();

    PluginScanner(const PluginScanner&) = delete;
    PluginScanner& operator=(const PluginScanner&) = delete;

    // Bounded join budget for every UI-thread-visible wait on the scan thread.
    // A VST3 load can hang forever (SEH catches faults, not hangs), so no caller
    // may ever block on the scan thread indefinitely.
    static constexpr std::chrono::milliseconds kScanJoinTimeout{2000};

    // Start a background scan of the given paths. Progress events are delivered
    // on the scan thread via the listener (which may post to the UI thread).
    // Returns false — without starting a scan — when a previous scan is still
    // running after kScanJoinTimeout (i.e. it is hung inside a plugin load).
    bool start(const std::vector<std::filesystem::path>& paths, IScanProgressListener* listener, ScanCache* cache);

    // Request cancellation. The scan thread will exit at the next safe point.
    void cancel();

    // Wait at most `timeout` for the scan thread to exit; returns false if it
    // is still running when the timeout expires. Joins the thread if it exited.
    // Safe to call from any thread.
    bool waitForScanExit(std::chrono::milliseconds timeout);

    // Sever the listener callbacks and detach a scan thread that outlived
    // waitForScanExit(). The scanner object (and the ScanCache passed to
    // start()) must then outlive the detached thread: the owner leaks them at
    // teardown instead of freeing memory the thread still writes through
    // (see ~AudioEngineImpl).
    void detachRunningScan();

    // Returns true if a scan is currently running.
    bool isRunning() const noexcept;

private:
    void runScan(const std::vector<std::filesystem::path>& paths, ScanCache* cache);

    std::atomic<bool> cancel_flag_{false};
    std::atomic<bool> running_{false};
    std::thread thread_;
    std::mutex thread_mutex_;
    std::condition_variable scan_exited_cv_;
    std::atomic<IScanProgressListener*> progress_listener_{nullptr};
    std::vector<std::filesystem::path> current_paths_;

    // Keep scanned plugin instances alive for the app lifetime.
    // Prevents VST3 modules from being unloaded, which avoids crashes
    // in plugins that leak callbacks / window hooks (e.g. IK Multimedia ARC 4).
    std::vector<std::unique_ptr<PluginInstance>> scanned_instances_;
};

}  // namespace jyglobalvst::engine
