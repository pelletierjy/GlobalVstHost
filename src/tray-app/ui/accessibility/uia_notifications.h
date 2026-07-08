// src/tray-app/ui/accessibility/uia_notifications.h
//
// T112 — UIA dynamic-event notifications.
// Wraps UiaRaiseNotificationEvent for screen readers (Narrator, NVDA).

#pragma once

#include <string>
#include <windows.h>

namespace jyglobalvst::tray {

enum class NotificationKind
{
    Info,
    Warn,
    Error,
};

void notifyPluginAdded(HWND hwnd, const std::string& pluginName);
void notifyPluginRemoved(HWND hwnd, const std::string& pluginName);
void notifyBypassToggled(HWND hwnd,
                         const std::string& pluginName,
                         bool bypassed);
void notifyDeviceDisconnect(HWND hwnd, const std::string& deviceName);
void notifyDeviceRestored(HWND hwnd, const std::string& deviceName);
void notifyCpuWarning(HWND hwnd, bool active);
void notifyPresetPartialLoad(HWND hwnd, int missingCount);

}  // namespace jyglobalvst::tray
