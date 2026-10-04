// src/tray-app/ui/local_dir_resolver.h
//
// Resolves the shared local application-data directory used to materialise
// files that must be reachable from outside the process — presently the user
// guide HTML handed to the default browser.
//
// Factoring this out of main_window.cpp keeps the tray-app's MSIX-aware path
// logic unit-testable without dragging in JUCE, ASIO, or binary resources.

#pragma once

#include <filesystem>
#include <string>

namespace jyglobalvst::tray {

// Reads the wide environment variable named `name`. Returns an empty string
// when unset or empty. Virtualised behind a function pointer so unit tests can
// substitute a controlled implementation without mutating the real process
// environment (which is unsafe under parallel test execution).
using EnvGetter = const wchar_t* (*)(const wchar_t* name);

// Default implementation: delegates to ::_wgetenv.
const wchar_t* defaultEnvGetter(const wchar_t* name);

// Returns the package family name of the current MSIX package, or an empty
// string when not running under MSIX. Virtualised behind a function pointer
// for the same reason as EnvGetter.
using PfnGetter = std::wstring (*)();

// Default implementation: delegates to ::GetCurrentPackageFamilyName.
std::wstring defaultPfnGetter();

// Builds the shared local directory path:
//   - When LOCALAPPDATA is set (normal / MSIX case): %LOCALAPPDATA%\JyGlobalVST
//     (with the MSIX LocalCache redirect applied under a packaged identity).
//   - Falls back to %USERPROFILE%\AppData\Local\JyGlobalVST when LOCALAPPDATA is
//     absent, since that is the canonical definition of the local app-data folder.
//   - Returns an empty path when neither variable is available.
//
// `env` and `pfn` allow injecting test doubles; pass `defaultEnvGetter` and
// `defaultPfnGetter` for production behaviour.
std::filesystem::path sharedLocalDir(EnvGetter env = defaultEnvGetter,
                                     PfnGetter pfn = defaultPfnGetter);

}  // namespace jyglobalvst::tray
