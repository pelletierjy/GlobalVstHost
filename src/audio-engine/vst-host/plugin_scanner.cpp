// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • This file is NOT reached from the audio callback in the current
//     build configuration; it runs exclusively on UI / control threads.
//   • Kept under src/audio-engine/ for cohesion; header satisfies T107.
// =====================================================================
// src/audio-engine/vst-host/plugin_scanner.cpp
//
// T055 — PluginScanner implementation.

#include "plugin_scanner.h"

#include "scan_cache.h"
#include "vst3_loader.h"

#include <windows.h>

#include <juce_core/juce_core.h>

#include <algorithm>
#include <chrono>

namespace jyglobalvst::engine {

namespace {

void collectVst3Files(const std::filesystem::path& root, std::vector<std::filesystem::path>& out)
{
    DWORD attr = GetFileAttributesW(root.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY) == 0)
    {
        return;
    }

    std::wstring pattern = root.c_str();
    if (!pattern.empty() && pattern.back() != L'\\' && pattern.back() != L'/')
    {
        pattern += L'\\';
    }
    pattern += L"*.vst3";

    WIN32_FIND_DATAW data;
    HANDLE h = FindFirstFileW(pattern.c_str(), &data);
    if (h == INVALID_HANDLE_VALUE)
    {
        return;
    }

    do
    {
        out.emplace_back(root / data.cFileName);
    } while (FindNextFile(h, &data));

    FindClose(h);
}

}  // namespace

PluginScanner::PluginScanner() = default;

PluginScanner::~PluginScanner()
{
    cancel();
    waitUntilFinished();
}

void PluginScanner::start(const std::vector<std::filesystem::path>& paths,
                          IScanProgressListener* listener,
                          ScanCache* cache)
{
    std::lock_guard<std::mutex> lk(thread_mutex_);

    if (thread_.joinable())
    {
        cancel_flag_.store(true);
        thread_.join();
    }

    cancel_flag_.store(false);
    running_.store(true);
    current_paths_ = paths;

    thread_ = std::thread(&PluginScanner::runScan, this, current_paths_, listener, cache);
}

void PluginScanner::cancel()
{
    cancel_flag_.store(true);
}

void PluginScanner::waitUntilFinished()
{
    std::lock_guard<std::mutex> lk(thread_mutex_);
    if (thread_.joinable())
    {
        thread_.join();
    }
}

bool PluginScanner::isRunning() const noexcept
{
    return running_.load();
}

void PluginScanner::runScan(const std::vector<std::filesystem::path>& paths,
                            IScanProgressListener* listener,
                            ScanCache* cache)
{
    if (listener)
    {
        listener->onScanStarted(static_cast<int>(paths.size()));
    }

    std::unique_ptr<VST3PluginLoader> loader;
    std::vector<PluginCatalogEntry> discovered;

    for (const auto& path : paths)
    {
        if (cancel_flag_.load())
        {
            break;
        }

        std::vector<std::filesystem::path> files;
        collectVst3Files(path, files);

        if (!files.empty() && !loader)
        {
            loader = std::make_unique<VST3PluginLoader>();
        }

        for (const auto& file : files)
        {
            if (cancel_flag_.load())
            {
                break;
            }

            try
            {
                auto result = loader->load(file);
                if (result && result.instance)
                {
                    PluginCatalogEntry entry;
                    entry.ref.plugin_uid = HexStringToPluginUid(result.instance->descriptor().uid);
                    entry.ref.vendor = result.instance->descriptor().vendor;
                    entry.ref.name = result.instance->descriptor().name;
                    entry.version = result.instance->descriptor().version;
                    entry.file_path = file;
                    entry.category = result.instance->descriptor().category;
                    entry.has_editor = true;  // Simplified; real check needs editor creation.
                    entry.scan_timestamp = std::chrono::system_clock::now();

                    discovered.push_back(std::move(entry));

                    if (listener)
                    {
                        listener->onPluginDiscovered(discovered.back());
                    }

                    // Keep the instance alive for the app lifetime.
                    // Some plugins (e.g. IK Multimedia ARC 4) register callbacks
                    // that are not cleaned up before the VST3 module is unloaded.
                    // When Windows later calls the dangling callback the app crashes
                    // with 0xc000041d. Keeping the module loaded prevents this.
                    scanned_instances_.push_back(std::move(result.instance));
                }
            }
            catch (const std::exception&)
            {
                // Defensive: loader->load() already catches internally, but if an
                // unexpected exception leaks from JUCE internals we must not crash
                // the scan thread.
            }
            catch (...)
            {
                // Same for non-standard exceptions.
            }
        }
    }

    if (cancel_flag_.load())
    {
        if (listener)
        {
            listener->onScanCancelled();
        }
    }
    else
    {
        if (cache)
        {
            cache->setPlugins(discovered);
            cache->save();
        }

        if (listener)
        {
            listener->onScanFinished(static_cast<int>(discovered.size()));
        }
    }
    running_.store(false);
}

}  // namespace jyglobalvst::engine
