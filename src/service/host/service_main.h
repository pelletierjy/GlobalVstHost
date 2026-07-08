// src/service/host/service_main.h
//
// T114 — Windows Service host API.

#pragma once

#include <filesystem>

namespace jyglobalvst::service {

// Entry point for service-mode execution. Blocks until service stops.
// Returns 0 on clean exit, or Win32 error code.
int runAsService();

// SCM helpers (used by installer / command-line registration).
bool installService(const std::filesystem::path& exePath);
bool uninstallService();
bool startService();
bool stopService();

}  // namespace jyglobalvst::service
