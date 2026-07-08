#pragma once

#include <string>
#include <vector>
#include <windows.h>
#include <mmdeviceapi.h>

namespace jyglobalvst {

// Windows-specific device registration and registry utilities
class WindowsDeviceHelper {
public:
    // Generate a deterministic GUID for the virtual device
    // Machine-specific and persistent across sessions
    static std::string GenerateDeviceGuid();
    
    // Retrieve stored GUID from registry or generate if not found
    static std::string GetOrCreateDeviceGuid();
    
    // Registry helpers for device persistence
    static bool ReadDeviceGuidFromRegistry(std::string& out_guid);
    static bool WriteDeviceGuidToRegistry(const std::string& guid);
    static bool DeleteDeviceFromRegistry();
    
    // Query Windows audio device information
    static std::string GetSystemDefaultAudioFormat();
    static std::vector<std::string> EnumerateAudioDevices();
    
    // HRESULT to human-readable error message
    static std::string GetWASAPIErrorMessage(HRESULT hr);
    
private:
    // Registry key for device storage
    static constexpr wchar_t REGISTRY_KEY[] = 
        L"Software\\JyGlobalVST\\AudioDevice";
    static constexpr wchar_t GUID_VALUE_NAME[] = L"DeviceGuid";
};

}  // namespace jyglobalvst
