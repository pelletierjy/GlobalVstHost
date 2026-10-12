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
#include "seh_wrapper.h"
#include "vst3_loader.h"

#include <windows.h>

#include <juce_core/juce_core.h>

#include <algorithm>
#include <chrono>

namespace jyglobalvst::engine {

namespace {

void collectVst3FilesImpl(const std::filesystem::path& root, std::vector<std::filesystem::path>& out)
{
    // Absolute normalization: a relative scan root would otherwise resolve
    // against the process working directory, which differs between a normal
    // launch, a shortcut launch, and an MSIX packaged launch.
    std::error_code ec;
    const std::filesystem::path abs_root = std::filesystem::absolute(root, ec);
    const std::filesystem::path& base = ec ? root : abs_root;

    DWORD attr = GetFileAttributesW(base.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY) == 0)
    {
        return;
    }

    std::wstring pattern = base.c_str();
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
        out.emplace_back(base / data.cFileName);
    } while (FindNextFile(h, &data));

    FindClose(h);

    // One level of vendor subdirectories: <base>\<Vendor>\*.vst3 (the VST3.7
    // per-user layout). .vst3 directories are never descended into — their
    // Contents tree holds the module binary, not a separate plugin.
    const std::wstring sub_pattern = std::wstring(base.c_str()) + L"\\*";
    WIN32_FIND_DATAW sub_data;
    const HANDLE hs = FindFirstFileW(sub_pattern.c_str(), &sub_data);
    if (hs != INVALID_HANDLE_VALUE)
    {
        do
        {
            const std::wstring name = sub_data.cFileName;
            const bool is_bundle =
                name.size() > 5 && _wcsicmp(name.c_str() + name.size() - 5, L".vst3") == 0;
            if (is_bundle || (sub_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
            {
                continue;
            }
            const std::filesystem::path sub_dir = base / name;
            const std::wstring sub_vst3 = std::wstring(sub_dir.c_str()) + L"\\*.vst3";
            WIN32_FIND_DATAW f2;
            const HANDLE h2 = FindFirstFileW(sub_vst3.c_str(), &f2);
            if (h2 != INVALID_HANDLE_VALUE)
            {
                do
                {
                    out.emplace_back(sub_dir / f2.cFileName);
                } while (FindNextFile(h2, &f2));
                FindClose(h2);
            }
        } while (FindNextFile(hs, &sub_data));
        FindClose(hs);
    }
}

}  // namespace

std::vector<std::filesystem::path> collectVst3Candidates(const std::filesystem::path& root)
{
    std::vector<std::filesystem::path> out;
    collectVst3FilesImpl(root, out);
    return out;
}

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

        const std::vector<std::filesystem::path> files = collectVst3Candidates(path);

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
                // Load each plugin under an SEH guard: a faulty VST3 can raise a
                // structured exception (access violation) during instantiation
                // that a C++ catch(...) cannot stop, which would otherwise crash
                // the whole app mid-scan. On such a failure we skip the plugin and
                // keep scanning. (Matches the SEH isolation used for processBlock.)
                struct LoadCtx
                {
                    VST3PluginLoader* loader;
                    const std::filesystem::path* file;
                    VST3PluginLoader::LoadResult result;
                } ctx {loader.get(), &file, {}};

                const bool loaded_cleanly = SEHPluginWrapper::invokeGuarded(
                    [](void* p) {
                        auto* c = static_cast<LoadCtx*>(p);
                        c->result = c->loader->load(*c->file);
                    },
                    &ctx);

                if (!loaded_cleanly)
                {
                    // Plugin crashed while loading; leave it out of the catalog.
                    continue;
                }

                auto& result = ctx.result;
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
