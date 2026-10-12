// src/tray-app/ui/accessibility/uia_notifications.cpp
//
// T112 — UIA dynamic-event notifications for plugin/device/CPU state changes.
//
// Raises UiaRaiseNotificationEvent using the host provider obtained from the
// target window (UiaHostProviderFromHwnd), so no JUCE-internal accessibility
// plumbing is required. Failures are silent by design: notifications must
// never interfere with audio or UI flows.

#include "uia_notifications.h"

#include <string>

#include <objbase.h>
#include <UIAutomation.h>

#pragma comment(lib, "uiautomationcore.lib")

namespace jyglobalvst::tray {

namespace {

std::wstring utf8ToWide(const std::string& text)
{
    if (text.empty())
    {
        return {};
    }
    const int needed = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.length()), nullptr, 0);
    std::wstring wide(needed, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.length()), &wide[0], needed);
    return wide;
}

// UIA's NotificationKind taxonomy has no severity levels; every notification
// is raised as NotificationKind_Other and the message text carries meaning
// (this also avoids the name clash between our tray::NotificationKind and the
// global UIA enum of the same name).
void raiseNotification(HWND hwnd, const std::string& message)
{
    if (hwnd == nullptr || !IsWindow(hwnd))
    {
        return;
    }

    IRawElementProviderSimple* provider = nullptr;
    if (FAILED(UiaHostProviderFromHwnd(hwnd, &provider)) || provider == nullptr)
    {
        return;
    }

    const std::wstring wide = utf8ToWide(message);
    BSTR bstr_message = SysAllocStringLen(wide.c_str(), static_cast<UINT>(wide.length()));
    BSTR bstr_activity = SysAllocString(L"JyGlobalVST");
    if (bstr_message != nullptr && bstr_activity != nullptr)
    {
        UiaRaiseNotificationEvent(
            provider, ::NotificationKind_Other, ::NotificationProcessing_ImportantAll, bstr_message,
            bstr_activity);
    }
    SysFreeString(bstr_message);
    SysFreeString(bstr_activity);
    provider->Release();
}

}  // namespace

void notifyPluginAdded(HWND hwnd, const std::string& pluginName)
{
    raiseNotification(hwnd, "Plugin added: " + pluginName);
}

void notifyPluginRemoved(HWND hwnd, const std::string& pluginName)
{
    raiseNotification(hwnd, "Plugin removed: " + pluginName);
}

void notifyBypassToggled(HWND hwnd, const std::string& pluginName, bool bypassed)
{
    raiseNotification(hwnd, pluginName + (bypassed ? " bypassed" : " enabled"));
}

void notifyDeviceDisconnect(HWND hwnd, const std::string& deviceName)
{
    raiseNotification(hwnd, "Output device disconnected: " + deviceName);
}

void notifyDeviceRestored(HWND hwnd, const std::string& deviceName)
{
    raiseNotification(hwnd, "Output device restored: " + deviceName);
}

void notifyCpuWarning(HWND hwnd, bool active)
{
    raiseNotification(hwnd, active ? "CPU warning active" : "CPU warning cleared");
}

void notifyPresetPartialLoad(HWND hwnd, int missingCount)
{
    raiseNotification(hwnd, "Preset loaded with " + std::to_string(missingCount) + " missing plugins");
}

}  // namespace jyglobalvst::tray
