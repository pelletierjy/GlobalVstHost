#include "device_watchdog.h"
#include "audio_engine_impl.h"
#include <comdef.h>

namespace jyglobalvst::engine {

namespace {

std::string wideToUtf8(LPCWSTR wstr)
{
    if (!wstr)
        return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0)
        return {};
    std::string out(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &out[0], len, nullptr, nullptr);
    return out;
}

}  // namespace

std::shared_ptr<DeviceWatchdog> DeviceWatchdog::Create(
    const std::string& device_guid,
    std::shared_ptr<IDeviceLossCallback> callback) {

    auto watchdog = std::make_shared<DeviceWatchdog>(device_guid, callback);

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != S_FALSE) {
        return nullptr;
    }

    IMMDeviceEnumerator* enumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                         CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                         reinterpret_cast<void**>(&enumerator));

    if (SUCCEEDED(hr) && enumerator) {
        enumerator->RegisterEndpointNotificationCallback(watchdog.get());
        enumerator->Release();
    }

    return watchdog;
}

void DeviceWatchdog::start(void* endpoint) {
    (void)endpoint;
}

void DeviceWatchdog::stop() {
}

HRESULT DeviceWatchdog::OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) {
    if (engine_impl_) {
        engine_impl_->onDeviceStateChanged(wideToUtf8(pwstrDeviceId), dwNewState);
    }
    else if (pwstrDeviceId && callback_) {
        if (dwNewState != DEVICE_STATE_ACTIVE) {
            callback_->OnDeviceLoss();
        }
    }
    return S_OK;
}

HRESULT DeviceWatchdog::OnDeviceAdded(LPCWSTR pwstrDeviceId) {
    if (engine_impl_) {
        engine_impl_->onDeviceAdded(wideToUtf8(pwstrDeviceId));
    }
    return S_OK;
}

HRESULT DeviceWatchdog::OnDeviceRemoved(LPCWSTR pwstrDeviceId) {
    if (engine_impl_) {
        engine_impl_->onDeviceRemoved(wideToUtf8(pwstrDeviceId));
    }
    else if (pwstrDeviceId && callback_) {
        callback_->OnDeviceLoss();
    }
    return S_OK;
}

HRESULT DeviceWatchdog::OnDefaultDeviceChanged(
    EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId) {
    (void)role;
    if (engine_impl_) {
        engine_impl_->onDefaultDeviceChanged(flow, wideToUtf8(pwstrDefaultDeviceId));
    }
    return S_OK;
}

HRESULT DeviceWatchdog::OnPropertyValueChanged(
    LPCWSTR pwstrDeviceId, const PROPERTYKEY key) {
    (void)pwstrDeviceId;
    (void)key;
    return S_OK;
}

HRESULT DeviceWatchdog::QueryInterface(REFIID iid, void** ppunk) {
    if (!ppunk) return E_POINTER;

    if (iid == IID_IUnknown || iid == __uuidof(IMMNotificationClient)) {
        AddRef();
        *ppunk = this;
        return S_OK;
    }

    *ppunk = nullptr;
    return E_NOINTERFACE;
}

ULONG DeviceWatchdog::AddRef() {
    return ref_count_.fetch_add(1, std::memory_order_relaxed) + 1;
}

ULONG DeviceWatchdog::Release() {
    ULONG new_count = ref_count_.fetch_sub(1, std::memory_order_release) - 1;
    if (new_count == 0) {
        delete this;
    }
    return new_count;
}

DeviceWatchdog::DeviceWatchdog(AudioEngineImpl* engine_impl)
    : device_guid_(""), callback_(nullptr), engine_impl_(engine_impl) {}

DeviceWatchdog::DeviceWatchdog(const std::string& device_guid,
                             std::shared_ptr<IDeviceLossCallback> callback)
    : device_guid_(device_guid), callback_(callback), engine_impl_(nullptr) {}

DeviceWatchdog::~DeviceWatchdog() {
    CoUninitialize();
}

}  // namespace jyglobalvst::engine
