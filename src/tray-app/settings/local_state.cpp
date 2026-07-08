// src/tray-app/settings/local_state.cpp
//
// T088 — LocalState persistence implementation.

#include "local_state.h"

#include "platform/known_folders.h"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace jyglobalvst::tray {

LocalStateStore::LocalStateStore()
    : dir_(jyglobalvst::shared::localStateDir())
{
}

std::filesystem::path LocalStateStore::localDir() const
{
    return dir_;
}

// window-state.json ---------------------------------------------------------

WindowState LocalStateStore::loadWindowState() const
{
    WindowState ws;
    std::ifstream ifs(dir_ / "window-state.json");
    if (!ifs)
        return ws;

    nlohmann::json doc;
    try
    {
        ifs >> doc;
    }
    catch (const std::exception&)
    {
        return ws;
    }

    if (doc.contains("x") && doc["x"].is_number_integer())
        ws.x = doc["x"].get<int>();
    if (doc.contains("y") && doc["y"].is_number_integer())
        ws.y = doc["y"].get<int>();
    if (doc.contains("width") && doc["width"].is_number_integer())
        ws.width = doc["width"].get<int>();
    if (doc.contains("height") && doc["height"].is_number_integer())
        ws.height = doc["height"].get<int>();
    if (doc.contains("maximized") && doc["maximized"].is_boolean())
        ws.maximized = doc["maximized"].get<bool>();

    return ws;
}

void LocalStateStore::saveWindowState(const WindowState& ws) const
{
    nlohmann::json doc;
    doc["x"] = ws.x;
    doc["y"] = ws.y;
    doc["width"] = ws.width;
    doc["height"] = ws.height;
    doc["maximized"] = ws.maximized;

    std::filesystem::create_directories(dir_);
    std::ofstream ofs(dir_ / "window-state.json", std::ios::binary);
    if (ofs)
        ofs << doc.dump(2);
}

// endpoint-last.json --------------------------------------------------------

std::optional<EndpointSnapshot> LocalStateStore::loadEndpointSnapshot() const
{
    std::ifstream ifs(dir_ / "endpoint-last.json");
    if (!ifs)
        return std::nullopt;

    nlohmann::json doc;
    try
    {
        ifs >> doc;
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }

    EndpointSnapshot snap;
    if (doc.contains("endpoint_id") && doc["endpoint_id"].is_string())
        snap.endpoint_id = doc["endpoint_id"].get<std::string>();
    if (doc.contains("friendly_name") && doc["friendly_name"].is_string())
        snap.friendly_name = doc["friendly_name"].get<std::string>();
    if (doc.contains("last_bound_at") && doc["last_bound_at"].is_string())
        snap.last_bound_at = doc["last_bound_at"].get<std::string>();

    return snap;
}

void LocalStateStore::saveEndpointSnapshot(const EndpointSnapshot& snap) const
{
    nlohmann::json doc;
    doc["endpoint_id"] = snap.endpoint_id;
    doc["friendly_name"] = snap.friendly_name;
    doc["last_bound_at"] = snap.last_bound_at;

    std::filesystem::create_directories(dir_);
    std::ofstream ofs(dir_ / "endpoint-last.json", std::ios::binary);
    if (ofs)
        ofs << doc.dump(2);
}

// last-preset.json ------------------------------------------------------------

std::optional<std::filesystem::path> LocalStateStore::loadLastPresetPath() const
{
    std::ifstream ifs(dir_ / "last-preset.json");
    if (!ifs)
        return std::nullopt;

    nlohmann::json doc;
    try
    {
        ifs >> doc;
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }

    if (doc.contains("path") && doc["path"].is_string())
    {
        std::string p = doc["path"].get<std::string>();
        if (!p.empty())
            return std::filesystem::path(p);
    }
    return std::nullopt;
}

void LocalStateStore::saveLastPresetPath(const std::filesystem::path& path) const
{
    nlohmann::json doc;
    doc["path"] = path.string();

    std::filesystem::create_directories(dir_);
    std::ofstream ofs(dir_ / "last-preset.json", std::ios::binary);
    if (ofs)
        ofs << doc.dump(2);
}

// default-device-snapshot.json ------------------------------------------------

std::optional<DefaultDeviceSnapshot> LocalStateStore::loadDefaultDeviceSnapshot() const
{
    std::ifstream ifs(dir_ / "default-device-snapshot.json");
    if (!ifs)
        return std::nullopt;

    nlohmann::json doc;
    try
    {
        ifs >> doc;
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }

    DefaultDeviceSnapshot snap;
    if (doc.contains("prior_default_render_id") && doc["prior_default_render_id"].is_string())
        snap.prior_default_render_id = doc["prior_default_render_id"].get<std::string>();
    if (doc.contains("changed_by_app") && doc["changed_by_app"].is_boolean())
        snap.changed_by_app = doc["changed_by_app"].get<bool>();
    if (doc.contains("set_timestamp") && doc["set_timestamp"].is_string())
        snap.set_timestamp = doc["set_timestamp"].get<std::string>();

    return snap;
}

void LocalStateStore::saveDefaultDeviceSnapshot(const DefaultDeviceSnapshot& snap) const
{
    nlohmann::json doc;
    doc["prior_default_render_id"] = snap.prior_default_render_id;
    doc["changed_by_app"] = snap.changed_by_app;
    doc["set_timestamp"] = snap.set_timestamp;

    std::filesystem::create_directories(dir_);
    std::ofstream ofs(dir_ / "default-device-snapshot.json", std::ios::binary);
    if (ofs)
        ofs << doc.dump(2);
}

}  // namespace jyglobalvst::tray
