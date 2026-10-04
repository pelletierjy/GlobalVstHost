// src/tray-app/ui/local_dir_resolver.cpp
//
// Extracted from main_window.cpp so the MSIX-aware local-directory resolution
// can be unit-tested without linking the full tray-app (JUCE GUI, ASIO, icons).

#include "local_dir_resolver.h"

#include <cstdlib>
#include <windows.h>
#include <appmodel.h>

namespace jyglobalvst::tray {

// Wide, not std::getenv: the path runs through a user profile name, which need
// not be representable in the process ANSI code page.
const wchar_t* defaultEnvGetter(const wchar_t* name)
{
    return ::_wgetenv(name);
}

// Empty unless this process runs with MSIX package identity (the Store build).
std::wstring defaultPfnGetter()
{
    UINT32 length = 0;
    if (::GetCurrentPackageFamilyName(&length, nullptr) != ERROR_INSUFFICIENT_BUFFER)
    {
        return {};
    }

    std::wstring name(length, L'\0');
    if (::GetCurrentPackageFamilyName(&length, name.data()) != ERROR_SUCCESS)
    {
        return {};
    }

    name.resize(length > 0 ? length - 1 : 0);
    return name;
}

std::filesystem::path sharedLocalDir(EnvGetter env, PfnGetter pfn)
{
    const wchar_t* local = env(L"LOCALAPPDATA");
    if (local == nullptr || *local == L'\0')
    {
        const wchar_t* profile = env(L"USERPROFILE");
        if (profile == nullptr || *profile == L'\0')
        {
            return {};
        }

        std::filesystem::path base(profile);
        const auto family = pfn();
        if (!family.empty())
        {
            base = base / L"Packages" / family / L"LocalCache" / L"Local";
        }
        return base / L"AppData" / L"Local" / "JyGlobalVST";
    }

    std::filesystem::path base(local);
    const auto family = pfn();
    if (!family.empty())
    {
        base = base / L"Packages" / family / L"LocalCache" / L"Local";
    }

    return base / "JyGlobalVST";
}

}  // namespace jyglobalvst::tray
