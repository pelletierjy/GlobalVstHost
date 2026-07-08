// src/shared/platform/audio_endpoints.h
//
// T016 — WASAPI endpoint enumeration wrapper (IMMDeviceEnumerator +
// IMMNotificationClient). UI / control thread only.

#pragma once

#include "jyglobalvst/types.h"

#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace jyglobalvst::shared {

enum class EndpointFlow : std::uint8_t
{
    Render,
    Capture,
};

struct EndpointDescriptor
{
    EndpointId endpoint_id;
    std::string friendly_name;
    EndpointFlow flow {EndpointFlow::Render};
    bool is_default {false};
    bool is_present {true};
};

class IAudioEndpointObserver
{
public:
    virtual ~IAudioEndpointObserver() = default;

    virtual void onEndpointAdded(const EndpointDescriptor& d) = 0;
    virtual void onEndpointRemoved(const EndpointId& id) = 0;
    virtual void onDefaultEndpointChanged(EndpointFlow flow, const EndpointId& id) = 0;
    virtual void onEndpointStateChanged(const EndpointId& id, bool is_present) = 0;
};

class AudioEndpointEnumerator
{
public:
    AudioEndpointEnumerator();
    ~AudioEndpointEnumerator();

    AudioEndpointEnumerator(const AudioEndpointEnumerator&) = delete;
    AudioEndpointEnumerator& operator=(const AudioEndpointEnumerator&) = delete;

    [[nodiscard]] std::vector<EndpointDescriptor> list(EndpointFlow flow) const;

    [[nodiscard]] EndpointId defaultEndpoint(EndpointFlow flow) const;

    // Returns true on successful subscription. Pass nullptr to unsubscribe.
    bool setObserver(IAudioEndpointObserver* observer);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace jyglobalvst::shared
