// src/shared/platform/endpoint_volume.cpp
//
// T008 — Mute/restore wrapper implementation.

#include "endpoint_volume.h"

#if defined(_WIN32)
#    include <windows.h>
#    include <mmdeviceapi.h>
#    include <endpointvolume.h>
#    include <combaseapi.h>
#    include <winerror.h>
#endif

namespace jyglobalvst::shared {

struct EndpointVolumeGuard::Impl
{
    IAudioEndpointVolume* endpoint_volume = nullptr;
    BOOL original_mute_state = FALSE;
    bool is_active = false;
    bool currently_muted = false;
};

EndpointVolumeGuard::EndpointVolumeGuard()
    : impl_(std::make_unique<Impl>())
{
}

EndpointVolumeGuard::~EndpointVolumeGuard()
{
    deactivate();
}

bool EndpointVolumeGuard::activate(const EndpointId& endpoint_id)
{
#if defined(_WIN32)
    deactivate();

    if (endpoint_id.empty())
        return false;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    (void)hr;  // COM may already be initialized on this thread

    IMMDeviceEnumerator* enumerator = nullptr;
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr) || !enumerator)
    {
        return false;
    }

    // Convert endpoint_id to wide string
    int needed = MultiByteToWideChar(CP_UTF8, 0, endpoint_id.c_str(), -1, nullptr, 0);
    if (needed <= 0)
    {
        enumerator->Release();
        return false;
    }

    std::wstring id_w(needed - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, endpoint_id.c_str(), -1, &id_w[0], needed);

    IMMDevice* device = nullptr;
    hr = enumerator->GetDevice(id_w.c_str(), &device);
    enumerator->Release();

    if (FAILED(hr) || !device)
    {
        return false;
    }

    hr = device->Activate(
        __uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
        reinterpret_cast<void**>(&impl_->endpoint_volume));
    device->Release();

    if (FAILED(hr) || !impl_->endpoint_volume)
    {
        return false;
    }

    // Record original mute state
    hr = impl_->endpoint_volume->GetMute(&impl_->original_mute_state);
    if (FAILED(hr))
    {
        impl_->endpoint_volume->Release();
        impl_->endpoint_volume = nullptr;
        return false;
    }

    impl_->is_active = true;

    return true;

#else
    (void)endpoint_id;
    return false;
#endif
}

bool EndpointVolumeGuard::mute()
{
    if (!impl_->is_active || !impl_->endpoint_volume)
        return false;

#if defined(_WIN32)
    HRESULT hr = impl_->endpoint_volume->SetMute(TRUE, nullptr);
    if (SUCCEEDED(hr))
        impl_->currently_muted = true;
    return SUCCEEDED(hr);
#else
    return false;
#endif
}

bool EndpointVolumeGuard::restore()
{
    if (!impl_->is_active || !impl_->endpoint_volume)
        return false;

#if defined(_WIN32)
    HRESULT hr = impl_->endpoint_volume->SetMute(impl_->original_mute_state, nullptr);
    if (SUCCEEDED(hr))
        impl_->currently_muted = false;
    return SUCCEEDED(hr);
#else
    return false;
#endif
}

void EndpointVolumeGuard::deactivate()
{
    if (impl_->endpoint_volume)
    {
        impl_->endpoint_volume->Release();
        impl_->endpoint_volume = nullptr;
    }
    impl_->is_active = false;
    impl_->currently_muted = false;
}

bool EndpointVolumeGuard::isActive() const noexcept
{
    return impl_->is_active;
}

bool EndpointVolumeGuard::isMuted() const noexcept
{
    return impl_->currently_muted;
}

}  // namespace jyglobalvst::shared
