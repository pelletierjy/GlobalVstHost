// src/audio-engine/vst-host/scan_cache.h
//
// T056 — Scan-cache persistence (%LocalAppData%\JyGlobalVST\scan-cache.json).
//
// Not touched from the audio thread; no REALTIME CONSTRAINTS header required.

#pragma once

#include <jyglobalvst/types.h>

#include <filesystem>
#include <mutex>
#include <vector>

namespace jyglobalvst::engine {

class ScanCache
{
public:
    ScanCache();

    // Load from disk. Returns true if a valid cache was read.
    bool load();

    // Save to disk. Returns true on success.
    bool save() const;

    // Replace in-memory catalog.
    void setPlugins(std::vector<PluginCatalogEntry> plugins);

    // Access current catalog (thread-safe copy).
    std::vector<PluginCatalogEntry> plugins() const;

    // Search by (uid, vendor, name) resolution tuple per FR-022g-2.
    std::optional<PluginCatalogEntry> findByRef(const PluginRef& ref) const;

    // Append a single entry (test helper).
    void addEntry(const PluginCatalogEntry& entry);

private:
    std::filesystem::path cachePath() const;

    mutable std::mutex mutex_;
    std::vector<PluginCatalogEntry> plugins_;
};

}  // namespace jyglobalvst::engine
