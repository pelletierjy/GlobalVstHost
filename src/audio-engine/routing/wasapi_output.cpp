// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • open()/close()/start()/stop() run on the UI thread only.
//   • The internal render thread does NOT allocate after start().
//   • The audio callback drains from the ring buffer with no allocations.
// =====================================================================
// src/audio-engine/routing/wasapi_output.cpp
//
// T005 — WASAPI output client implementation.

#include "wasapi_output.h"
#include "format_convert.h"

#if defined(_WIN32)
#    include <windows.h>
#    include <avrt.h>
#    include <mmdeviceapi.h>
#    include <audioclient.h>
#    include <combaseapi.h>
#endif

#include <algorithm>
#include <cstring>
#include <thread>
#include <vector>

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

}  // namespace

struct WasapiOutput::Impl
{
    IMMDevice* device_ = nullptr;
    IAudioClient* audio_client_ = nullptr;
    IAudioRenderClient* render_client_ = nullptr;
    WAVEFORMATEX* mix_format_ = nullptr;
    HANDLE event_ = nullptr;
    std::thread render_thread_;
    std::atomic<bool> should_stop_{false};
    std::atomic<bool> running_{false};
    double negotiated_sample_rate_ = 0.0;
    double target_sample_rate_ = 0.0;
    float latency_ms_ = 0.0f;
    std::atomic<std::size_t> xrun_count_{0};
    shared::LockFreeAudioRingBuffer* ring_buffer_ = nullptr;
    UINT32 buffer_size_ = 0;
    std::vector<float> temp_buffer_;
};

WasapiOutput::WasapiOutput()
    : impl_(std::make_unique<Impl>())
{
}

WasapiOutput::~WasapiOutput()
{
    stop();
    close();
}

bool WasapiOutput::open(const EndpointId& endpoint_id, double target_sample_rate)
{
    close();

    impl_->target_sample_rate_ = target_sample_rate;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    (void)hr;  // COM may already be initialized on this thread

    IMMDeviceEnumerator* enumerator = nullptr;
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr) || enumerator == nullptr)
    {
        close();
        return false;
    }

    const std::wstring id_w = utf8ToWide(endpoint_id);
    hr = enumerator->GetDevice(id_w.c_str(), &impl_->device_);
    enumerator->Release();
    if (FAILED(hr) || impl_->device_ == nullptr)
    {
        close();
        return false;
    }

    hr = impl_->device_->Activate(
        __uuidof(IAudioClient), CLSCTX_ALL, nullptr,
        reinterpret_cast<void**>(&impl_->audio_client_));
    if (FAILED(hr) || impl_->audio_client_ == nullptr)
    {
        close();
        return false;
    }

    hr = impl_->audio_client_->GetMixFormat(&impl_->mix_format_);
    if (FAILED(hr) || impl_->mix_format_ == nullptr)
    {
        close();
        return false;
    }

    impl_->negotiated_sample_rate_ = static_cast<double>(impl_->mix_format_->nSamplesPerSec);

    REFERENCE_TIME default_period = 0;
    REFERENCE_TIME min_period = 0;
    impl_->audio_client_->GetDevicePeriod(&default_period, &min_period);
    if (default_period == 0)
        default_period = 100000LL;

    impl_->event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    const DWORD stream_flags = impl_->event_ != nullptr ? AUDCLNT_STREAMFLAGS_EVENTCALLBACK : 0;

    bool initialized = false;

    // Try IAudioClient3 shared mode first
    IAudioClient3* client3 = nullptr;
    if (SUCCEEDED(impl_->audio_client_->QueryInterface(
            __uuidof(IAudioClient3), reinterpret_cast<void**>(&client3))) &&
        client3 != nullptr)
    {
        UINT32 def_frames = 0;
        UINT32 fund_frames = 0;
        UINT32 min_frames = 0;
        UINT32 max_frames = 0;
        if (SUCCEEDED(client3->GetSharedModeEnginePeriod(
                impl_->mix_format_, &def_frames, &fund_frames, &min_frames, &max_frames)))
        {
            if (SUCCEEDED(client3->InitializeSharedAudioStream(
                    stream_flags, min_frames, impl_->mix_format_, nullptr)))
            {
                initialized = true;
            }
        }
        client3->Release();
    }

    // Fallback: classic shared mode
    if (!initialized)
    {
        hr = impl_->audio_client_->Initialize(
            AUDCLNT_SHAREMODE_SHARED, stream_flags, default_period, 0,
            impl_->mix_format_, nullptr);
        if (SUCCEEDED(hr))
            initialized = true;
    }

    if (!initialized)
    {
        close();
        return false;
    }

    // Bind the event handle so WASAPI signals us when buffer space is available.
    if (impl_->event_ != nullptr)
    {
        hr = impl_->audio_client_->SetEventHandle(impl_->event_);
        if (FAILED(hr))
        {
            close();
            return false;
        }
    }

    hr = impl_->audio_client_->GetBufferSize(&impl_->buffer_size_);
    if (FAILED(hr))
        impl_->buffer_size_ = 512;

    hr = impl_->audio_client_->GetService(
        __uuidof(IAudioRenderClient), reinterpret_cast<void**>(&impl_->render_client_));
    if (FAILED(hr) || impl_->render_client_ == nullptr)
    {
        close();
        return false;
    }

    // Calculate latency in milliseconds
    REFERENCE_TIME latency_100ns = 0;
    impl_->audio_client_->GetStreamLatency(&latency_100ns);
    impl_->latency_ms_ = static_cast<float>(latency_100ns) / 10000.0f;

    impl_->temp_buffer_.resize(impl_->buffer_size_ * 2);

    return true;
}

void WasapiOutput::close()
{
    if (impl_->render_client_)
    {
        impl_->render_client_->Release();
        impl_->render_client_ = nullptr;
    }
    if (impl_->audio_client_)
    {
        impl_->audio_client_->Stop();
        impl_->audio_client_->Release();
        impl_->audio_client_ = nullptr;
    }
    if (impl_->device_)
    {
        impl_->device_->Release();
        impl_->device_ = nullptr;
    }
    if (impl_->event_)
    {
        CloseHandle(impl_->event_);
        impl_->event_ = nullptr;
    }
    impl_->negotiated_sample_rate_ = 0.0;
}

bool WasapiOutput::start(shared::LockFreeAudioRingBuffer* ring_buffer)
{
    if (!impl_->audio_client_ || !impl_->render_client_ || ring_buffer == nullptr)
        return false;

    impl_->ring_buffer_ = ring_buffer;
    impl_->should_stop_ = false;

    HRESULT hr = impl_->audio_client_->Start();
    if (FAILED(hr))
        return false;

    impl_->running_ = true;
    impl_->render_thread_ = std::thread([this]() { renderThreadLoop(); });

    return true;
}

void WasapiOutput::renderThreadLoop()
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

#if defined(_WIN32)
    DWORD avrt_task_index = 0;
    HANDLE avrt_handle = AvSetMmThreadCharacteristics(L"Pro Audio", &avrt_task_index);
#endif

    const auto channels = static_cast<std::size_t>(impl_->mix_format_->nChannels);
    const auto sample_size = channels;

    while (!impl_->should_stop_.load(std::memory_order_acquire))
    {
        if (impl_->event_ != nullptr)
        {
            WaitForSingleObject(impl_->event_, 200);
        }

        UINT32 padding = 0;
        if (FAILED(impl_->audio_client_->GetCurrentPadding(&padding)))
            continue;

        const UINT32 available = impl_->buffer_size_ - padding;
        if (available == 0)
        {
            if (impl_->event_ == nullptr)
                Sleep(1);
            continue;
        }

        BYTE* buffer = nullptr;
        if (FAILED(impl_->render_client_->GetBuffer(available, &buffer)))
            continue;

        // Try to read from ring buffer into temporary planar buffers.
        if (impl_->ring_buffer_)
        {
            const std::size_t temp_frames = impl_->temp_buffer_.size() / 2;
            float* channels_ptrs[2] = {
                impl_->temp_buffer_.data(),
                impl_->temp_buffer_.data() + temp_frames};
            const std::size_t frames_read = impl_->ring_buffer_->tryRead(
                channels_ptrs, available);

            if (frames_read > 0 && buffer)
            {
                // Interleave float to the output format
                auto* out = reinterpret_cast<float*>(buffer);
                std::size_t out_idx = 0;

                for (std::size_t i = 0; i < frames_read; ++i)
                {
                    if (channels > 0)
                        out[out_idx++] = channels_ptrs[0][i];
                    if (channels > 1)
                        out[out_idx++] = channels_ptrs[1][i];
                }

                impl_->render_client_->ReleaseBuffer(static_cast<UINT32>(frames_read), 0);

                // Pad remaining with silence if needed
                if (frames_read < available)
                {
                    const UINT32 pad_frames = available - static_cast<UINT32>(frames_read);
                    if (SUCCEEDED(impl_->render_client_->GetBuffer(pad_frames, &buffer)))
                    {
                        std::fill(buffer, buffer + pad_frames * sample_size * 4, static_cast<BYTE>(0));
                        impl_->render_client_->ReleaseBuffer(pad_frames, 0);
                    }
                    impl_->xrun_count_.fetch_add(1, std::memory_order_relaxed);
                }
            }
            else
            {
                // No data available, fill with silence
                std::fill(buffer, buffer + available * sample_size * 4, static_cast<BYTE>(0));
                impl_->render_client_->ReleaseBuffer(available, 0);
                impl_->xrun_count_.fetch_add(1, std::memory_order_relaxed);
            }
        }
        else
        {
            // No ring buffer, fill with silence
            if (buffer)
            {
                std::fill(buffer, buffer + available * sample_size * 4, static_cast<BYTE>(0));
                impl_->render_client_->ReleaseBuffer(available, 0);
            }
        }

        if (impl_->event_ == nullptr)
            Sleep(1);
    }

#if defined(_WIN32)
    if (avrt_handle != nullptr)
        AvRevertMmThreadCharacteristics(avrt_handle);
#endif

    CoUninitialize();
}

void WasapiOutput::stop()
{
    impl_->should_stop_ = true;
    if (impl_->render_thread_.joinable())
    {
        impl_->render_thread_.join();
    }
    impl_->running_ = false;
}

bool WasapiOutput::isOpen() const noexcept
{
    return impl_->audio_client_ != nullptr;
}

bool WasapiOutput::isRunning() const noexcept
{
    return impl_->running_.load(std::memory_order_acquire);
}

double WasapiOutput::negotiatedSampleRate() const noexcept
{
    return impl_->negotiated_sample_rate_;
}

double WasapiOutput::targetSampleRate() const noexcept
{
    return impl_->target_sample_rate_;
}

float WasapiOutput::latencyMs() const noexcept
{
    return impl_->latency_ms_;
}

std::size_t WasapiOutput::xrunCount() const noexcept
{
    return impl_->xrun_count_;
}

#else

// Non-Windows stub
WasapiOutput::WasapiOutput() : impl_(std::make_unique<Impl>()) {}
WasapiOutput::~WasapiOutput() = default;
bool WasapiOutput::open(const EndpointId&, double) { return false; }
void WasapiOutput::close() {}
bool WasapiOutput::start(shared::LockFreeAudioRingBuffer*) { return false; }
void WasapiOutput::stop() {}
bool WasapiOutput::isOpen() const noexcept { return false; }
bool WasapiOutput::isRunning() const noexcept { return false; }
double WasapiOutput::negotiatedSampleRate() const noexcept { return 0.0; }
double WasapiOutput::targetSampleRate() const noexcept { return 0.0; }
float WasapiOutput::latencyMs() const noexcept { return 0.0f; }
std::size_t WasapiOutput::xrunCount() const noexcept { return 0; }

#endif

}  // namespace jyglobalvst::engine
