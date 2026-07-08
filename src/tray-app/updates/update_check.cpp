// src/tray-app/updates/update_check.cpp
//
// T122 — Explicit user-initiated update check.
//
// Blocking WinHTTP GET on a background std::thread.
// No JUCE dependency — caller dispatches to UI thread.

#include "update_check.h"

#include "json/validators/update_manifest_validator.h"
#include "json/json_validator.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

namespace jyglobalvst::tray {

namespace {

// Simple semver comparison: returns -1 if a < b, 0 if equal, 1 if a > b.
int compareSemverImpl(const std::string& a, const std::string& b)
{
    auto parse = [](const std::string& s, int& major, int& minor, int& patch,
                    std::string& pre) {
        size_t dash = s.find('-');
        std::string core =
            (dash == std::string::npos) ? s : s.substr(0, dash);
        pre = (dash == std::string::npos) ? "" : s.substr(dash + 1);
        major = minor = patch = 0;
        std::sscanf(core.c_str(), "%d.%d.%d", &major, &minor, &patch);
    };

    int ma {}, mi_a {}, pa {};
    int mb {}, mi_b {}, pb {};
    std::string prea, preb;
    parse(a, ma, mi_a, pa, prea);
    parse(b, mb, mi_b, pb, preb);

    if (ma != mb)
        return (ma > mb) ? 1 : -1;
    if (mi_a != mi_b)
        return (mi_a > mi_b) ? 1 : -1;
    if (pa != pb)
        return (pa > pb) ? 1 : -1;

    if (prea.empty() && !preb.empty())
        return 1;
    if (!prea.empty() && preb.empty())
        return -1;
    if (prea != preb)
        return (prea > preb) ? 1 : -1;
    return 0;
}

// Blocking HTTPS GET using WinHTTP. Returns empty body on any failure;
// error_out receives a human-readable reason.
std::string httpsGet(const std::string& url_str, std::string& error_out)
{
    std::wstring wurl(url_str.begin(), url_str.end());

    URL_COMPONENTS urlComp {};
    urlComp.dwStructSize = sizeof(urlComp);

    wchar_t hostName[256] = {};
    wchar_t urlPath[1024] = {};
    wchar_t scheme[16] = {};

    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = _countof(hostName);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = _countof(urlPath);
    urlComp.lpszScheme = scheme;
    urlComp.dwSchemeLength = _countof(scheme);

    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &urlComp))
    {
        error_out = "Invalid URL";
        return {};
    }

    HINTERNET hSession = WinHttpOpen(
        L"JyGlobalVST/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!hSession)
    {
        error_out = "Failed to open HTTP session";
        return {};
    }

    HINTERNET hConnect =
        WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
    if (!hConnect)
    {
        WinHttpCloseHandle(hSession);
        error_out = "Failed to connect";
        return {};
    }

    DWORD flags =
        (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        urlPath,
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags);
    if (!hRequest)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        error_out = "Failed to open request";
        return {};
    }

    if (!WinHttpSendRequest(
            hRequest,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0,
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0))
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        error_out = "Failed to send request";
        return {};
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr))
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        error_out = "Failed to receive response";
        return {};
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(
        hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &statusSize,
        WINHTTP_NO_HEADER_INDEX);

    if (statusCode != 200)
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        error_out = "HTTP " + std::to_string(statusCode);
        return {};
    }

    std::string response;
    DWORD available = 0;
    while (WinHttpQueryDataAvailable(hRequest, &available) && available > 0)
    {
        std::vector<char> buffer(available);
        DWORD read = 0;
        WinHttpReadData(hRequest, buffer.data(), available, &read);
        response.append(buffer.data(), read);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return response;
}

}  // namespace

// -------------------------------------------------------------------------
// UpdateCheck
// -------------------------------------------------------------------------

void UpdateCheck::check(const std::string& endpoint_url,
                        const std::string& current_version,
                        Callback callback)
{
    std::thread([endpoint_url, current_version, callback]() {
        std::string error;
        std::string body = httpsGet(endpoint_url, error);

        UpdateCheckResult result;
        if (body.empty())
        {
            result.success = false;
            result.error_message = error.empty() ? "Network error" : error;
            result.installed_version = current_version;
        }
        else
        {
            result = parseResponse(body, current_version);
        }

        callback(result);
    }).detach();
}

UpdateCheckResult UpdateCheck::parseResponse(const std::string& body,
                                            const std::string& current_version)
{
    UpdateCheckResult result;
    result.installed_version = current_version;

    try
    {
        auto doc = nlohmann::json::parse(body);
        auto validation =
            shared::json::validators::validateUpdateManifest(
                doc, shared::json::ValidationMode::Strict);

        if (!validation.ok())
        {
            result.success = false;
            result.error_message = "Invalid server response";
            return result;
        }

        result.success = true;
        result.latest_version = doc["latest_version"].get<std::string>();

        if (doc.contains("release_notes_url") && !doc["release_notes_url"].is_null())
        {
            result.release_notes_url =
                doc["release_notes_url"].get<std::string>();
        }
        if (doc.contains("download_url") && !doc["download_url"].is_null())
        {
            result.download_url = doc["download_url"].get<std::string>();
        }

        result.update_available =
            compareSemverImpl(result.latest_version, current_version) > 0;
    }
    catch (...)
    {
        result.success = false;
        result.error_message = "Invalid JSON";
    }

    return result;
}

int UpdateCheck::compareSemver(const std::string& a, const std::string& b)
{
    return compareSemverImpl(a, b);
}

}  // namespace jyglobalvst::tray
