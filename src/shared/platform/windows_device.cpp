#include "windows_device.h"
#include <sstream>
#include <iomanip>
#include <random>
#include <memory>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>

#pragma comment(lib, "ole32.lib")  // For CoCreateGuid

namespace jyglobalvst {

constexpr wchar_t WindowsDeviceHelper::REGISTRY_KEY[];
constexpr wchar_t WindowsDeviceHelper::GUID_VALUE_NAME[];

std::string WindowsDeviceHelper::GenerateDeviceGuid() {
    GUID guid;
    HRESULT hr = CoCreateGuid(&guid);
    if (FAILED(hr)) {
        // Fallback: generate pseudo-random GUID
        // In production, this should log and handle failure
        guid = {0x00000000, 0x0000, 0x0000, {0, 0, 0, 0, 0, 0, 0, 0}};
    }
    
    // Format GUID as string: {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}
    wchar_t guid_str[40];
    swprintf_s(guid_str, 40,
        L"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
        guid.Data1, guid.Data2, guid.Data3,
        guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
        guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

    // Convert wide string to narrow string using proper conversion
    std::wstring wide_str(guid_str);
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wide_str.c_str(), (int)wide_str.length(), NULL, 0, NULL, NULL);
    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide_str.c_str(), (int)wide_str.length(), &result[0], size_needed, NULL, NULL);
    return result;
}

std::string WindowsDeviceHelper::GetOrCreateDeviceGuid() {
    std::string stored_guid;
    if (ReadDeviceGuidFromRegistry(stored_guid) && !stored_guid.empty()) {
        return stored_guid;
    }
    
    // Generate new GUID and persist it
    std::string new_guid = GenerateDeviceGuid();
    WriteDeviceGuidToRegistry(new_guid);
    return new_guid;
}

bool WindowsDeviceHelper::ReadDeviceGuidFromRegistry(std::string& out_guid) {
    HKEY hkey = nullptr;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, REGISTRY_KEY, 0, KEY_READ, &hkey);
    if (result != ERROR_SUCCESS) {
        return false;
    }
    
    wchar_t buffer[40] = {0};
    DWORD size = sizeof(buffer);
    result = RegQueryValueExW(hkey, GUID_VALUE_NAME, nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(buffer), &size);
    RegCloseKey(hkey);
    
    if (result == ERROR_SUCCESS && size > 0) {
        // Convert wide string to std::string using proper conversion
        std::wstring wide_guid(buffer);
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wide_guid.c_str(), (int)wide_guid.length(), NULL, 0, NULL, NULL);
        out_guid.resize(size_needed);
        WideCharToMultiByte(CP_UTF8, 0, wide_guid.c_str(), (int)wide_guid.length(), &out_guid[0], size_needed, NULL, NULL);
        return true;
    }
    
    return false;
}

bool WindowsDeviceHelper::WriteDeviceGuidToRegistry(const std::string& guid) {
    HKEY hkey = nullptr;
    LONG result = RegCreateKeyExW(HKEY_CURRENT_USER, REGISTRY_KEY, 0, nullptr,
                                  REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr,
                                  &hkey, nullptr);
    if (result != ERROR_SUCCESS) {
        return false;
    }
    
    // Convert std::string to wide string
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, guid.c_str(), (int)guid.length(), NULL, 0);
    std::wstring wide_guid(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, guid.c_str(), (int)guid.length(), &wide_guid[0], size_needed);

    result = RegSetValueExW(hkey, GUID_VALUE_NAME, 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(wide_guid.c_str()),
                           static_cast<DWORD>((wide_guid.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(hkey);
    
    return result == ERROR_SUCCESS;
}

bool WindowsDeviceHelper::DeleteDeviceFromRegistry() {
    HKEY hkey = nullptr;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, REGISTRY_KEY, 0, KEY_WRITE, &hkey);
    if (result != ERROR_SUCCESS) {
        return false;
    }
    
    result = RegDeleteValueW(hkey, GUID_VALUE_NAME);
    RegCloseKey(hkey);
    
    return result == ERROR_SUCCESS;
}

std::string WindowsDeviceHelper::GetSystemDefaultAudioFormat() {
    // Default fallback: 48 kHz, 32-bit float, stereo — the WASAPI shared-mode
    // mix format on most systems. Used only when the endpoint query fails.
    const std::string kFallback = "48000/32f/2";

    const HRESULT co_hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool do_uninit = (co_hr == S_OK);
    if (FAILED(co_hr) && co_hr != RPC_E_CHANGED_MODE) {
        return kFallback;
    }

    std::string result = kFallback;
    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator),
                                  reinterpret_cast<void**>(&enumerator));
    if (SUCCEEDED(hr)) {
        IMMDevice* device = nullptr;
        hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        if (SUCCEEDED(hr)) {
            IAudioClient* client = nullptr;
            hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                  reinterpret_cast<void**>(&client));
            if (SUCCEEDED(hr)) {
                WAVEFORMATEX* fmt = nullptr;
                hr = client->GetMixFormat(&fmt);
                if (SUCCEEDED(hr) && fmt != nullptr) {
                    bool is_float = (fmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);
                    if (fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                        fmt->cbSize >= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) {
                        const auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(fmt);
                        is_float = IsEqualGUID(ext->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) != 0;
                    }
                    std::ostringstream oss;
                    oss << fmt->nSamplesPerSec << "/"
                        << fmt->wBitsPerSample << (is_float ? "f" : "i")
                        << "/" << fmt->nChannels;
                    result = oss.str();
                    CoTaskMemFree(fmt);
                }
                client->Release();
            }
            device->Release();
        }
        enumerator->Release();
    }

    if (do_uninit) {
        CoUninitialize();
    }
    return result;
}

std::vector<std::string> WindowsDeviceHelper::EnumerateAudioDevices() {
    // Friendly names of active render endpoints (eConsole state), UTF-8.
    std::vector<std::string> devices;

    const HRESULT co_hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool do_uninit = (co_hr == S_OK);
    if (FAILED(co_hr) && co_hr != RPC_E_CHANGED_MODE) {
        return devices;
    }

    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator),
                                  reinterpret_cast<void**>(&enumerator));
    if (SUCCEEDED(hr)) {
        IMMDeviceCollection* collection = nullptr;
        hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
        if (SUCCEEDED(hr)) {
            UINT count = 0;
            collection->GetCount(&count);
            for (UINT i = 0; i < count; ++i) {
                IMMDevice* device = nullptr;
                if (FAILED(collection->Item(i, &device)) || device == nullptr) {
                    continue;
                }
                IPropertyStore* props = nullptr;
                if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props)) && props != nullptr) {
                    PROPVARIANT name;
                    PropVariantInit(&name);
                    if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &name)) &&
                        name.vt == VT_LPWSTR && name.pwszVal != nullptr) {
                        const std::wstring wide(name.pwszVal);
                        const int size_needed = WideCharToMultiByte(
                            CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.length()), nullptr, 0,
                            nullptr, nullptr);
                        std::string utf8(size_needed, 0);
                        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                                            static_cast<int>(wide.length()), &utf8[0], size_needed,
                                            nullptr, nullptr);
                        devices.push_back(utf8);
                    }
                    PropVariantClear(&name);
                    props->Release();
                }
                device->Release();
            }
            collection->Release();
        }
        enumerator->Release();
    }

    if (do_uninit) {
        CoUninitialize();
    }
    return devices;
}

std::string WindowsDeviceHelper::GetWASAPIErrorMessage(HRESULT hr) {
    switch (hr) {
        case S_OK:
            return "Success";
        case E_INVALIDARG:
            return "Invalid argument";
        case E_OUTOFMEMORY:
            return "Out of memory";
        case AUDCLNT_E_NOT_INITIALIZED:
            return "Audio client not initialized";
        case AUDCLNT_E_ALREADY_INITIALIZED:
            return "Audio client already initialized";
        case AUDCLNT_E_WRONG_ENDPOINT_TYPE:
            return "Wrong endpoint type";
        case AUDCLNT_E_DEVICE_INVALIDATED:
            return "Device invalidated";
        default:
            return "Unknown WASAPI error (0x" +
                   std::to_string(hr) + ")";
    }
}

}  // namespace jyglobalvst
