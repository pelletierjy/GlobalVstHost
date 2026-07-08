#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace jyglobalvst {

// Forward declarations
class IAudioDeviceListener;
struct AudioFormat;

// Device state enum
enum class DeviceState {
    NotRegistered,  // Device not yet registered
    Registering,    // Registration in progress
    Registered,     // Registered in Windows but not active
    Active,         // Active and receiving audio
    Inactive,       // Registered but not in use
    Failed          // Registration or operation failed
};

// Audio format structure for device negotiation
struct AudioFormat {
    uint32_t sample_rate;      // 44100, 48000, 96000, 192000
    uint16_t bit_depth;        // 16, 24, 32 (for float)
    uint16_t channels;         // 1 (mono), 2 (stereo), etc.
    bool is_float;             // true for 32-bit float, false for PCM int
    
    // Validation
    bool IsValid() const;
};

// Device error types
struct DeviceError {
    enum class Type {
        None,
        RegistrationFailed,
        PermissionDenied,
        HardwareNotAvailable,
        FormatNotSupported,
        AlreadyRegistered,
        Unknown
    };
    
    Type type = Type::None;
    std::string message;
    uint32_t error_code = 0;
};

// Device listener interface for state change callbacks
class IAudioDeviceListener {
public:
    virtual ~IAudioDeviceListener() = default;
    
    virtual void OnDeviceStateChanged(DeviceState new_state) = 0;
    virtual void OnAudioFormatNegotiated(const AudioFormat& format) = 0;
    virtual void OnDeviceError(const DeviceError& error) = 0;
};

// Virtual audio device interface
class IAudioDevice {
public:
    virtual ~IAudioDevice() = default;
    
    // Device registration and lifecycle
    virtual DeviceError RegisterDevice() = 0;
    virtual DeviceError UnregisterDevice() = 0;
    virtual DeviceState GetDeviceState() const = 0;
    
    // Format negotiation
    virtual std::vector<AudioFormat> QuerySupportedFormats() const = 0;
    virtual DeviceError NegotiateFormat(const AudioFormat& requested_format) = 0;
    virtual AudioFormat GetNegotiatedFormat() const = 0;
    
    // Device identity and metrics
    virtual std::string GetDeviceGuid() const = 0;
    virtual std::string GetFriendlyName() const = 0;
    virtual uint32_t GetLatencyMs() const = 0;
    
    // Listener management
    virtual void RegisterListener(std::shared_ptr<IAudioDeviceListener> listener) = 0;
    virtual void UnregisterListener(std::shared_ptr<IAudioDeviceListener> listener) = 0;
};

}  // namespace jyglobalvst
