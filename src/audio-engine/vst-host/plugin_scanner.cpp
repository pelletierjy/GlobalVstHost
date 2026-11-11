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

#include <juce_core/juce_core.h>

#include <algorithm>
#include <chrono>
#include <cwctype>
#include <set>

#include <windows.h>

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
            // FindFirstFileW("dir\\*") yields the "." and ".." entries first; they are
            // directories and do not end in .vst3, so without this filter they were
            // treated as vendor folders and <base>\\.\\*.vst3 re-enumerated every
            // top-level plugin a second time (each scanned and retained twice).
            if (name == L"." || name == L"..")
            {
                continue;
            }
            const bool is_bundle = name.size() > 5 && _wcsicmp(name.c_str() + name.size() - 5, L".vst3") == 0;
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

    // Safety net for the same class of bug the "."/".." filter above fixes: two
    // roots (or a root listed twice) can yield the same bundle; scanning it twice
    // would instantiate and retain it twice. Dedupe case-insensitively on the
    // normalized path — Windows paths are case-insensitive.
    std::vector<std::filesystem::path> unique;
    std::set<std::wstring> seen;
    unique.reserve(out.size());
    for (auto& p : out)
    {
        std::wstring key = p.lexically_normal().wstring();
        std::transform(key.begin(), key.end(), key.begin(),
                       [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
        if (seen.insert(key).second)
        {
            unique.push_back(std::move(p));
        }
    }
    return unique;
}

PluginScanner::PluginScanner() = default;

PluginScanner::~PluginScanner()
{
    cancel();
    // Bounded: the owning engine (~AudioEngineImpl) guarantees no scan thread
    // is still running here — it detaches and leaks the scanner instead.
    waitForScanExit(kScanJoinTimeout);
}

bool PluginScanner::start(const std::vector<std::filesystem::path>& paths,
                          IScanProgressListener* listener,
                          ScanCache* cache)
{
    std::unique_lock<std::mutex> lk(thread_mutex_);

    if (running_.load(std::memory_order_acquire) || thread_.joinable())
    {
        // A previous scan may be hung inside a misbehaving VST3 load; never
        // block the caller (a UI thread) forever, and never run two scans at
        // once — both would write scanned_instances_ concurrently.
        cancel_flag_.store(true);
        if (!scan_exited_cv_.wait_for(
                lk, kScanJoinTimeout, [this] { return !running_.load(std::memory_order_acquire); }))
        {
            return false;
        }
        if (thread_.joinable())
            thread_.join();
    }

    cancel_flag_.store(false);
    progress_listener_.store(listener, std::memory_order_release);
    running_.store(true, std::memory_order_release);
    current_paths_ = paths;

    try
    {
        thread_ = std::thread(&PluginScanner::runScan, this, current_paths_, cache);
    }
    catch (...)
    {
        running_.store(false, std::memory_order_release);
        progress_listener_.store(nullptr, std::memory_order_release);
        return false;
    }
    return true;
}

void PluginScanner::cancel()
{
    cancel_flag_.store(true);
}

bool PluginScanner::waitForScanExit(std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lk(thread_mutex_);
    if (!scan_exited_cv_.wait_for(lk, timeout, [this] { return !running_.load(std::memory_order_acquire); }))
    {
        return false;
    }
    if (thread_.joinable())
    {
        thread_.join();
    }
    return true;
}

void PluginScanner::detachRunningScan()
{
    std::lock_guard<std::mutex> lk(thread_mutex_);
    // Future callback sites load progress_listener_ afresh, so clearing it here
    // stops an already-running (detached) scan from touching a destroyed listener.
    progress_listener_.store(nullptr, std::memory_order_release);
    if (thread_.joinable())
    {
        thread_.detach();
    }
}

bool PluginScanner::isRunning() const noexcept
{
    return running_.load();
}

void PluginScanner::runScan(const std::vector<std::filesystem::path>& paths, ScanCache* cache)
{
    if (auto* listener = progress_listener_.load(std::memory_order_acquire))
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
                // Metadata-only scan: describe() reads the plugin's factory info
                // WITHOUT creating an AudioPluginInstance (no DSP buffers, no
                // editor, nothing that has to be retained after the scan — the
                // old full-instantiation path kept every scanned plugin alive for
                // the app lifetime, which is what drove RAM to 1.4GB+ per handful
                // of plugins and 5GB+ on large collections).
                //
                // Still guarded by SEH: a faulty VST3 can raise a structured
                // exception (access violation) while its factory initialises, which
                // a C++ catch(...) cannot stop. On such a failure we skip the
                // plugin and keep scanning.
                struct DescribeCtx
                {
                    VST3PluginLoader* loader;
                    const std::filesystem::path* file;
                    VST3PluginLoader::DescribeResult result;
                } ctx{loader.get(), &file, {}};

                const bool described_cleanly = SEHPluginWrapper::invokeGuarded(
                    [](void* p) {
                        auto* c = static_cast<DescribeCtx*>(p);
                        c->result = c->loader->describe(*c->file);
                    },
                    &ctx);

                if (!described_cleanly)
                {
                    // Plugin crashed while reading its factory; leave it out of
                    // the catalog.
                    continue;
                }

                auto& result = ctx.result;
                if (result)
                {
                    PluginCatalogEntry entry;
                    entry.ref.plugin_uid = HexStringToPluginUid(result.descriptor.uid);
                    entry.ref.vendor = result.descriptor.vendor;
                    entry.ref.name = result.descriptor.name;
                    entry.version = result.descriptor.version;
                    entry.file_path = file;
                    entry.category = result.descriptor.category;
                    entry.has_editor = true;  // Simplified; real check needs editor creation.
                    entry.scan_timestamp = std::chrono::system_clock::now();

                    discovered.push_back(std::move(entry));

                    if (auto* listener = progress_listener_.load(std::memory_order_acquire))
                    {
                        listener->onPluginDiscovered(discovered.back());
                    }
                    // No instance is retained: the catalog entry is pure metadata,
                    // and plugins for the chain are loaded on demand by addPlugin /
                    // loadChainFromJson when the user actually needs them.
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
        if (auto* listener = progress_listener_.load(std::memory_order_acquire))
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

        if (auto* listener = progress_listener_.load(std::memory_order_acquire))
        {
            listener->onScanFinished(static_cast<int>(discovered.size()));
        }
    }

    // Publish the exit while holding thread_mutex_ so a wait_for() predicate
    // check can never miss it, then wake any bounded waiter.
    {
        std::lock_guard<std::mutex> lk(thread_mutex_);
        running_.store(false, std::memory_order_release);
    }
    scan_exited_cv_.notify_all();
}

}  // namespace jyglobalvst::engine
