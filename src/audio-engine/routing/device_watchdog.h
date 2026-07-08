#pragma once

#include <windows.h>
#include <mmdeviceapi.h>
#include <memory>
#include <atomic>
#include <string>

namespace jyglobalvst::engine {

// Forward declaration to avoid circular include with audio_engine_impl.h
class AudioEngineImpl;

// REALTIME CONSTRAINTS: Watchdog detection is non-blocking; recovery uses lock-free queue.

// Device loss notification receiver
class IDeviceLossCallback {
public:
    virtual ~IDeviceLossCallback() = default;
    virtual void OnDeviceLoss() = 0;
};

// Device watchdog: detects system device changes via IMMNotificationClient
class DeviceWatchdog : public IMMNotificationClient {
public:
    static std::shared_ptr<DeviceWatchdog> Create(
        const std::string& device_guid,
        std::shared_ptr<IDeviceLossCallback> callback);
    
    // IMMNotificationClient implementation
    HRESULT __stdcall OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) override;
    HRESULT __stdcall OnDeviceAdded(LPCWSTR pwstrDeviceId) override;
    HRESULT __stdcall OnDeviceRemoved(LPCWSTR pwstrDeviceId) override;
    HRESULT __stdcall OnDefaultDeviceChanged(
        EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId) override;
    HRESULT __stdcall OnPropertyValueChanged(
        LPCWSTR pwstrDeviceId, const PROPERTYKEY key) override;
    
    // Watchdog lifecycle control
    // (endpoint parameter for future enhancement; not used in current implementation)
    void start(void* endpoint = nullptr);
    void stop();

    // COM reference counting
    HRESULT __stdcall QueryInterface(REFIID iid, void** ppunk) override;
    ULONG __stdcall AddRef() override;
    ULONG __stdcall Release() override;

    ~DeviceWatchdog();
    
    // Constructor for integration with AudioEngineImpl
    explicit DeviceWatchdog(AudioEngineImpl* engine_impl);

    // Constructor for device loss callback integration
    DeviceWatchdog(const std::string& device_guid,
                  std::shared_ptr<IDeviceLossCallback> callback);

private:

    std::string device_guid_;
    std::shared_ptr<IDeviceLossCallback> callback_;
    AudioEngineImpl* engine_impl_ = nullptr;
    std::atomic<ULONG> ref_count_{1};
};

}  // namespace jyglobalvst::engine
