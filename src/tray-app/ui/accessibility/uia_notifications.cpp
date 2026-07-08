// src/tray-app/ui/accessibility/uia_notifications.cpp
//
// T112 — UIA dynamic-event notifications for plugin/device/CPU state changes.
//
// NOTE: UiaRaiseNotificationEvent requires an IRawElementProviderSimple
// obtained from a JUCE component's AccessibilityHandler. In testable-dev
// these are no-ops; release wiring extracts the provider from the target
// component and calls the UIA API.

#include "uia_notifications.h"

namespace jyglobalvst::tray {

void notifyPluginAdded(HWND /*hwnd*/, const std::string& /*pluginName*/)
{
    // TODO(release): obtain IRawElementProviderSimple from the chain editor
    // component and call UiaRaiseNotificationEvent.
}

void notifyPluginRemoved(HWND /*hwnd*/, const std::string& /*pluginName*/)
{
    // TODO(release)
}

void notifyBypassToggled(HWND /*hwnd*/,
                         const std::string& /*pluginName*/,
                         bool /*bypassed*/)
{
    // TODO(release)
}

void notifyDeviceDisconnect(HWND /*hwnd*/, const std::string& /*deviceName*/)
{
    // TODO(release)
}

void notifyDeviceRestored(HWND /*hwnd*/, const std::string& /*deviceName*/)
{
    // TODO(release)
}

void notifyCpuWarning(HWND /*hwnd*/, bool /*active*/)
{
    // TODO(release)
}

void notifyPresetPartialLoad(HWND /*hwnd*/, int /*missingCount*/)
{
    // TODO(release)
}

}  // namespace jyglobalvst::tray
