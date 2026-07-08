// src/tray-app/settings/local_state.h
//
// T088 — LocalState file family (%LocalAppData%\JyGlobalVST\).
// window-state.json, endpoint-last.json. Tolerant of corruption per FR-022l.

#pragma once

#include <nlohmann/json.hpp>

#include <filesystem>
#include <optional>
#include <string>

namespace jyglobalvst::tray {

struct WindowState
{
    int x {0};
    int y {0};
    int width {640};
    int height {480};
    bool maximized {false};
};

struct EndpointSnapshot
{
    std::string endpoint_id;
    std::string friendly_name;
    std::string last_bound_at;  // ISO 8601 UTC.
};

struct DefaultDeviceSnapshot
{
    std::string prior_default_render_id;  // Endpoint ID before app changed it
    bool changed_by_app = false;           // true if app set virtual as default
    std::string set_timestamp;             // ISO 8601 UTC when changed
};

class LocalStateStore
{
public:
    LocalStateStore();

    std::filesystem::path localDir() const;

    // window-state.json
    WindowState loadWindowState() const;
    void saveWindowState(const WindowState& ws) const;

    // endpoint-last.json
    std::optional<EndpointSnapshot> loadEndpointSnapshot() const;
    void saveEndpointSnapshot(const EndpointSnapshot& snap) const;

    // last-preset.json — path of the last explicitly loaded or saved preset.
    std::optional<std::filesystem::path> loadLastPresetPath() const;
    void saveLastPresetPath(const std::filesystem::path& path) const;

    // default-device-snapshot.json — prior default device for crash recovery.
    std::optional<DefaultDeviceSnapshot> loadDefaultDeviceSnapshot() const;
    void saveDefaultDeviceSnapshot(const DefaultDeviceSnapshot& snap) const;

private:
    std::filesystem::path dir_;
};

}  // namespace jyglobalvst::tray
