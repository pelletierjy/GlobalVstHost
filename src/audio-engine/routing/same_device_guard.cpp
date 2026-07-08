// src/audio-engine/routing/same_device_guard.cpp
//
// T009 — Conflict detection implementation.

#include "same_device_guard.h"
#include "../../shared/platform/audio_endpoints.h"

#if defined(_WIN32)
#    include <mmdeviceapi.h>
#    include <combaseapi.h>
#endif

#include <string>

namespace jyglobalvst::engine {

#if defined(_WIN32)

namespace {

std::wstring utf8ToWide(const std::string& s)
{
    if (s.empty())
        return {};
    const int needed = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (needed <= 0)
        return {};
    std::wstring out(static_cast<std::size_t>(needed - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), needed);
    return out;
}

// Resolve an endpoint ID, following "system default" semantics
EndpointId resolveEndpoint(const EndpointId& id)
{
    if (!id.empty() && id != "system-default")
        return id;

    // Resolve to system default render endpoint
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool com_init = (hr == S_OK || hr == S_FALSE);

    IMMDeviceEnumerator* enumerator = nullptr;
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr) || !enumerator)
    {
        if (com_init)
            CoUninitialize();
        return id;
    }

    IMMDevice* device = nullptr;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    enumerator->Release();

    if (FAILED(hr) || !device)
    {
        if (com_init)
            CoUninitialize();
        return id;
    }

    LPWSTR id_w = nullptr;
    hr = device->GetId(&id_w);
    device->Release();

    if (FAILED(hr) || !id_w)
    {
        if (com_init)
            CoUninitialize();
        return id;
    }

    // Convert wide string to UTF-8
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, id_w, -1, nullptr, 0, nullptr, nullptr);
    std::string resolved;
    if (utf8_len > 0)
    {
        resolved.resize(utf8_len - 1);
        WideCharToMultiByte(CP_UTF8, 0, id_w, -1, &resolved[0], utf8_len, nullptr, nullptr);
    }

    CoTaskMemFree(id_w);

    if (com_init)
        CoUninitialize();

    return resolved;
}

}  // namespace

#else

EndpointId resolveEndpoint(const EndpointId& id) { return id; }

#endif

struct SameDeviceGuard::Impl
{
    // Cached resolution for performance
};

SameDeviceGuard::SameDeviceGuard()
    : impl_(std::make_unique<Impl>())
{
}

SameDeviceGuard::~SameDeviceGuard() = default;

EndpointId SameDeviceGuard::checkConflict(
    const EndpointId& capture_id,
    const EndpointId& output_id) const
{
    if (capture_id.empty() || output_id.empty())
        return EndpointId{};

    // Resolve both IDs following "system default" semantics
    const auto resolved_capture = resolveEndpoint(capture_id);
    const auto resolved_output = resolveEndpoint(output_id);

    // If they resolve to the same device, return that device ID (conflict)
    if (!resolved_capture.empty() && resolved_capture == resolved_output)
        return resolved_capture;

    // No conflict
    return EndpointId{};
}

}  // namespace jyglobalvst::engine
