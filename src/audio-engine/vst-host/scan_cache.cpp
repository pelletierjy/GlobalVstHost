// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • This file is NOT reached from the audio callback in the current
//     build configuration; it runs exclusively on UI / control threads.
//   • Kept under src/audio-engine/ for cohesion; header satisfies T107.
// =====================================================================
// src/audio-engine/vst-host/scan_cache.cpp
//
// T056 — Scan-cache persistence implementation.

#include "scan_cache.h"

#include "../../shared/json/json_validator.h"
#include "../../shared/json/validators/scan_cache_validator.h"
#include "../../shared/platform/known_folders.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <fstream>
#include <iostream>

namespace jyglobalvst::engine {

namespace {

using json = nlohmann::json;

std::string uidToHex(const PluginUid& uid)
{
    constexpr char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(32);
    for (auto b : uid)
    {
        out.push_back(hex[b >> 4]);
        out.push_back(hex[b & 0x0F]);
    }
    return out;
}

PluginUid hexToUid(const std::string& s)
{
    PluginUid uid {};
    if (s.size() != 32)
        return uid;

    auto hexValue = [](char c) -> std::uint8_t {
        if (c >= '0' && c <= '9')
            return static_cast<std::uint8_t>(c - '0');
        if (c >= 'a' && c <= 'f')
            return static_cast<std::uint8_t>(10 + c - 'a');
        if (c >= 'A' && c <= 'F')
            return static_cast<std::uint8_t>(10 + c - 'A');
        return 0;
    };

    for (std::size_t i = 0; i < 16; ++i)
    {
        uid[i] = static_cast<std::uint8_t>((hexValue(s[i * 2]) << 4) | hexValue(s[i * 2 + 1]));
    }
    return uid;
}

json entryToJson(const PluginCatalogEntry& entry)
{
    json j;
    j["plugin_uid"] = uidToHex(entry.ref.plugin_uid);
    j["name"] = entry.ref.name;
    j["vendor"] = entry.ref.vendor;
    j["version"] = entry.version;
    j["file_path"] = entry.file_path.string();
    j["category"] = entry.category;
    j["supports_double_precision"] = entry.supports_double_precision;
    j["has_editor"] = entry.has_editor;

    const auto since_epoch = std::chrono::system_clock::to_time_t(entry.scan_timestamp);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&since_epoch), "%Y-%m-%dT%H:%M:%SZ");
    j["scan_timestamp"] = ss.str();

    j["is_blocklisted_by_user"] = entry.is_blocklisted_by_user;
    return j;
}

PluginCatalogEntry entryFromJson(const json& j)
{
    PluginCatalogEntry entry;
    if (j.contains("plugin_uid"))
        entry.ref.plugin_uid = hexToUid(j["plugin_uid"].get<std::string>());
    if (j.contains("name"))
        entry.ref.name = j["name"].get<std::string>();
    if (j.contains("vendor"))
        entry.ref.vendor = j["vendor"].get<std::string>();
    if (j.contains("version"))
        entry.version = j["version"].get<std::string>();
    if (j.contains("file_path"))
        entry.file_path = j["file_path"].get<std::string>();
    if (j.contains("category"))
        entry.category = j["category"].get<std::string>();
    if (j.contains("supports_double_precision"))
        entry.supports_double_precision = j["supports_double_precision"].get<bool>();
    if (j.contains("has_editor"))
        entry.has_editor = j["has_editor"].get<bool>();
    if (j.contains("scan_timestamp"))
    {
        std::tm tm = {};
        std::stringstream ss(j["scan_timestamp"].get<std::string>());
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        entry.scan_timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }
    if (j.contains("is_blocklisted_by_user"))
        entry.is_blocklisted_by_user = j["is_blocklisted_by_user"].get<bool>();
    return entry;
}

}  // namespace

ScanCache::ScanCache()
{
}

bool ScanCache::load()
{
    const auto path = cachePath();
    if (!std::filesystem::exists(path))
    {
        return false;
    }

    try
    {
        std::ifstream file(path);
        if (!file)
        {
            return false;
        }

        json doc;
        file >> doc;

        auto result = shared::json::validators::validateScanCache(doc, shared::json::ValidationMode::Tolerant);
        if (!result.ok())
        {
            return false;
        }

        std::lock_guard lk {mutex_};
        plugins_.clear();
        if (doc.contains("plugins") && doc["plugins"].is_array())
        {
            for (const auto& p : doc["plugins"])
            {
                plugins_.push_back(entryFromJson(p));
            }
        }
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool ScanCache::save() const
{
    try
    {
        const auto dir = cachePath().parent_path();
        std::filesystem::create_directories(dir);

        json doc;
        doc["schema_version"] = shared::json::validators::kCurrentScanCacheSchemaVersion;

        const auto now = std::chrono::system_clock::now();
        const auto since_epoch = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&since_epoch), "%Y-%m-%dT%H:%M:%SZ");
        doc["scan_completed_at"] = ss.str();

        json plugins = json::array();
        {
            std::lock_guard lk {mutex_};
            for (const auto& entry : plugins_)
            {
                plugins.push_back(entryToJson(entry));
            }
        }
        doc["plugins"] = std::move(plugins);

        std::ofstream file(cachePath());
        if (!file)
        {
            return false;
        }
        file << doc.dump(2);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void ScanCache::setPlugins(std::vector<PluginCatalogEntry> plugins)
{
    std::lock_guard lk {mutex_};
    plugins_ = std::move(plugins);
}

std::vector<PluginCatalogEntry> ScanCache::plugins() const
{
    std::lock_guard lk {mutex_};
    return plugins_;
}

std::optional<PluginCatalogEntry> ScanCache::findByRef(const PluginRef& ref) const
{
    std::lock_guard lk {mutex_};

    // Exact match (uid + vendor + name).
    for (const auto& entry : plugins_)
    {
        if (entry.ref.plugin_uid == ref.plugin_uid && entry.ref.vendor == ref.vendor
            && entry.ref.name == ref.name)
        {
            return entry;
        }
    }

    // Fallback: match by vendor + name only. This handles:
    //  - Old presets with all-zero UIDs (before uid hash fix)
    //  - Old scan caches with all-zero UIDs (before uid hash fix)
    //  - Any other uid mismatch where vendor+name is still valid.
    for (const auto& entry : plugins_)
    {
        if (entry.ref.vendor == ref.vendor && entry.ref.name == ref.name)
        {
            return entry;
        }
    }

    return std::nullopt;
}

void ScanCache::addEntry(const PluginCatalogEntry& entry)
{
    std::lock_guard lk {mutex_};
    plugins_.push_back(entry);
}

std::filesystem::path ScanCache::cachePath() const
{
    return shared::localStateDir() / "scan-cache.json";
}

}  // namespace jyglobalvst::engine
