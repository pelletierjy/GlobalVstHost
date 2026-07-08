// src/shared/platform/known_folders.cpp
// T014 — see header.

#include "known_folders.h"

#include <system_error>

#if defined(_WIN32)
#    include <windows.h>
#    include <shlobj.h>
#endif

namespace jyglobalvst::shared {

namespace {

#if defined(_WIN32)
std::filesystem::path knownFolder(REFKNOWNFOLDERID id)
{
    PWSTR raw = nullptr;
    const HRESULT hr = SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, nullptr, &raw);
    if (FAILED(hr) || raw == nullptr)
    {
        if (raw != nullptr)
        {
            CoTaskMemFree(raw);
        }
        return {};
    }
    std::filesystem::path p = raw;
    CoTaskMemFree(raw);
    return p;
}
#endif

std::filesystem::path ensureSubdir(std::filesystem::path base, const wchar_t* sub)
{
    if (base.empty())
    {
        return {};
    }
    auto p = base / sub;
    std::error_code ec;
    std::filesystem::create_directories(p, ec);
    return p;
}

}  // namespace

std::filesystem::path roamingAppData()
{
#if defined(_WIN32)
    return knownFolder(FOLDERID_RoamingAppData);
#else
    return {};
#endif
}

std::filesystem::path localAppData()
{
#if defined(_WIN32)
    return knownFolder(FOLDERID_LocalAppData);
#else
    return {};
#endif
}

std::filesystem::path userDocuments()
{
#if defined(_WIN32)
    return knownFolder(FOLDERID_Documents);
#else
    return {};
#endif
}

std::filesystem::path programFiles()
{
#if defined(_WIN32)
    return knownFolder(FOLDERID_ProgramFiles);
#else
    return {};
#endif
}

std::filesystem::path roamingSettingsDir()
{
    return ensureSubdir(roamingAppData(), L"JyGlobalVST");
}

std::filesystem::path localStateDir()
{
    return ensureSubdir(localAppData(), L"JyGlobalVST");
}

std::filesystem::path presetsDir()
{
    auto docs = userDocuments();
    if (docs.empty())
    {
        return {};
    }
    auto p = docs / L"JyGlobalVST" / L"Presets";
    std::error_code ec;
    std::filesystem::create_directories(p, ec);
    return p;
}

}  // namespace jyglobalvst::shared
