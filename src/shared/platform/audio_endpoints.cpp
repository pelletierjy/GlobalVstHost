// src/shared/platform/audio_endpoints.cpp
// T016 — see header.

#include "audio_endpoints.h"

#include <atomic>
#include <string>

#if defined(_WIN32)
#    include <windows.h>
#    include <mmdeviceapi.h>
#    include <Functiondiscoverykeys_devpkey.h>
#    include <comdef.h>
#endif

namespace jyglobalvst::shared {

namespace {

#if defined(_WIN32)

std::string wideToUtf8(LPCWSTR ws)
{
    if (ws == nullptr)
    {
        return {};
    }
    int needed = WideCharToMultiByte(CP_UTF8, 0, ws, -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 0)
    {
        return {};
    }
    std::string out(static_cast<std::size_t>(needed - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws, -1, out.data(), needed, nullptr, nullptr);
    return out;
}

EDataFlow toWinFlow(EndpointFlow f)
{
    return f == EndpointFlow::Render ? eRender : eCapture;
}

EndpointFlow fromWinFlow(EDataFlow f)
{
    return f == eRender ? EndpointFlow::Render : EndpointFlow::Capture;
}

class NotificationClient final : public IMMNotificationClient
{
public:
    explicit NotificationClient(IAudioEndpointObserver** observer_ptr) : observer_ptr_ {observer_ptr} {}

    ULONG STDMETHODCALLTYPE AddRef() override { return ++ref_count_; }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const auto c = --ref_count_;
        if (c == 0)
        {
            delete this;
        }
        return c;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override
    {
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IMMNotificationClient))
        {
            *out = static_cast<IMMNotificationClient*>(this);
            AddRef();
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR id) override
    {
        if (role != eConsole)
        {
            return S_OK;
        }
        if (auto* obs = observer())
        {
            obs->onDefaultEndpointChanged(fromWinFlow(flow), wideToUtf8(id));
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR id) override
    {
        if (auto* obs = observer())
        {
            EndpointDescriptor d;
            d.endpoint_id = wideToUtf8(id);
            d.is_present = true;
            obs->onEndpointAdded(d);
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR id) override
    {
        if (auto* obs = observer())
        {
            obs->onEndpointRemoved(wideToUtf8(id));
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR id, DWORD state) override
    {
        if (auto* obs = observer())
        {
            obs->onEndpointStateChanged(wideToUtf8(id), state == DEVICE_STATE_ACTIVE);
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }

private:
    IAudioEndpointObserver* observer() const noexcept { return observer_ptr_ != nullptr ? *observer_ptr_ : nullptr; }

    IAudioEndpointObserver** observer_ptr_;
    std::atomic<ULONG> ref_count_ {1};
};

#endif  // _WIN32

}  // namespace

struct AudioEndpointEnumerator::Impl
{
#if defined(_WIN32)
    bool com_initialized {false};
    IMMDeviceEnumerator* enumerator {nullptr};
    NotificationClient* notification_client {nullptr};
    IAudioEndpointObserver* observer {nullptr};
#endif
};

AudioEndpointEnumerator::AudioEndpointEnumerator() : impl_ {std::make_unique<Impl>()}
{
#if defined(_WIN32)
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (hr == S_OK || hr == S_FALSE)
    {
        impl_->com_initialized = true;
    }
    CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                     __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&impl_->enumerator));
#endif
}

AudioEndpointEnumerator::~AudioEndpointEnumerator()
{
#if defined(_WIN32)
    if (impl_->notification_client != nullptr && impl_->enumerator != nullptr)
    {
        impl_->enumerator->UnregisterEndpointNotificationCallback(impl_->notification_client);
        impl_->notification_client->Release();
        impl_->notification_client = nullptr;
    }
    if (impl_->enumerator != nullptr)
    {
        impl_->enumerator->Release();
        impl_->enumerator = nullptr;
    }
    if (impl_->com_initialized)
    {
        CoUninitialize();
        impl_->com_initialized = false;
    }
#endif
}

std::vector<EndpointDescriptor> AudioEndpointEnumerator::list(EndpointFlow flow) const
{
    std::vector<EndpointDescriptor> result;
#if defined(_WIN32)
    if (impl_->enumerator == nullptr)
    {
        return result;
    }

    IMMDeviceCollection* collection = nullptr;
    if (FAILED(impl_->enumerator->EnumAudioEndpoints(toWinFlow(flow), DEVICE_STATE_ACTIVE, &collection))
        || collection == nullptr)
    {
        return result;
    }

    EndpointId default_id = defaultEndpoint(flow);

    UINT count = 0;
    collection->GetCount(&count);
    for (UINT i = 0; i < count; ++i)
    {
        IMMDevice* dev = nullptr;
        if (FAILED(collection->Item(i, &dev)) || dev == nullptr)
        {
            continue;
        }

        EndpointDescriptor d;
        d.flow = flow;
        d.is_present = true;

        LPWSTR id_w = nullptr;
        if (SUCCEEDED(dev->GetId(&id_w)) && id_w != nullptr)
        {
            d.endpoint_id = wideToUtf8(id_w);
            CoTaskMemFree(id_w);
        }

        IPropertyStore* props = nullptr;
        if (SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &props)) && props != nullptr)
        {
            PROPVARIANT var;
            PropVariantInit(&var);
            if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &var)) && var.vt == VT_LPWSTR)
            {
                d.friendly_name = wideToUtf8(var.pwszVal);
            }
            PropVariantClear(&var);
            props->Release();
        }

        d.is_default = !d.endpoint_id.empty() && d.endpoint_id == default_id;
        dev->Release();
        result.push_back(std::move(d));
    }

    collection->Release();
#else
    (void)flow;
#endif
    return result;
}

EndpointId AudioEndpointEnumerator::defaultEndpoint(EndpointFlow flow) const
{
#if defined(_WIN32)
    if (impl_->enumerator == nullptr)
    {
        return {};
    }
    IMMDevice* dev = nullptr;
    if (FAILED(impl_->enumerator->GetDefaultAudioEndpoint(toWinFlow(flow), eConsole, &dev)) || dev == nullptr)
    {
        return {};
    }
    EndpointId id;
    LPWSTR id_w = nullptr;
    if (SUCCEEDED(dev->GetId(&id_w)) && id_w != nullptr)
    {
        id = wideToUtf8(id_w);
        CoTaskMemFree(id_w);
    }
    dev->Release();
    return id;
#else
    (void)flow;
    return {};
#endif
}

bool AudioEndpointEnumerator::setObserver(IAudioEndpointObserver* observer)
{
#if defined(_WIN32)
    if (impl_->enumerator == nullptr)
    {
        return false;
    }

    if (impl_->notification_client != nullptr)
    {
        impl_->enumerator->UnregisterEndpointNotificationCallback(impl_->notification_client);
        impl_->notification_client->Release();
        impl_->notification_client = nullptr;
    }

    impl_->observer = observer;
    if (observer == nullptr)
    {
        return true;
    }

    impl_->notification_client = new NotificationClient(&impl_->observer);
    return SUCCEEDED(impl_->enumerator->RegisterEndpointNotificationCallback(impl_->notification_client));
#else
    (void)observer;
    return false;
#endif
}

}  // namespace jyglobalvst::shared
