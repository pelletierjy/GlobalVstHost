// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • This file is NOT reached from the audio callback in the current
//     build configuration; it runs exclusively on UI / control threads.
//   • Kept under src/audio-engine/ for cohesion; header satisfies T107.
// =====================================================================
// src/audio-engine/routing/wasapi_capture.cpp
//
// T025 — Direct WASAPI capture client via IAudioClient. Uses event-driven,
//        low-latency shared mode (IAudioClient3) when available, falling back
//        to default-period shared mode and/or polling.
//
// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • The capture thread does NOT allocate, lock, throw, or log.
//   • PCM → float conversion uses pre-allocated temp buffers.
//   • On ring-buffer overflow, frames are dropped and xrun_count_ bumps.
// =====================================================================

#include "wasapi_capture.h"
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
    {
        return {};
    }
    const int needed = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (needed <= 0)
    {
        return {};
    }
    std::wstring out(static_cast<std::size_t>(needed - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), needed);
    return out;
}

bool isFloatFormat(const WAVEFORMATEX* fmt)
{
    if (fmt == nullptr)
    {
        return false;
    }
    if (fmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
    {
        return true;
    }
    if (fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
    {
        auto* ex = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(fmt);
        if (ex->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
        {
            return true;
        }
    }
    return false;
}

bool isInt16Format(const WAVEFORMATEX* fmt)
{
    return fmt != nullptr && fmt->wFormatTag == WAVE_FORMAT_PCM && fmt->wBitsPerSample == 16;
}

bool isInt24Format(const WAVEFORMATEX* fmt)
{
    return fmt != nullptr && fmt->wFormatTag == WAVE_FORMAT_PCM && fmt->wBitsPerSample == 24;
}

}  // namespace

struct WasapiCapture::Impl
{
    IMMDevice* device_ = nullptr;
    IAudioClient* audio_client_ = nullptr;
    IAudioCaptureClient* capture_client_ = nullptr;
    WAVEFORMATEX* mix_format_ = nullptr;
    HANDLE event_ = nullptr;  // Wake event for event-driven capture; null = polling fallback.

    std::thread thread_;
    std::atomic<bool> running_ {false};
    std::atomic<bool> open_ {false};

    shared::LockFreeAudioRingBuffer* ring_buffer_ = nullptr;
    double negotiated_sample_rate_ = 48000.0;
    double target_sample_rate_ = 48000.0;
    int buffer_size_ = 0;
    std::size_t channels_ = 2;

    // Pre-allocated planar buffer (convert + deinterleave happen in one pass).
    std::vector<float> planar_;

    std::atomic<std::size_t> xrun_count_ {0};
    // Cleared when the stream is unrecoverable; the owner polls isHealthy() and
    // re-opens. Consecutive soft failures are counted so a single transient hiccup
    // doesn't tear down a working stream.
    std::atomic<bool> healthy_ {true};
    int consecutive_failures_ = 0;
    static constexpr int kMaxConsecutiveFailures = 20;

    // True for HRESULTs that mean the stream object is gone for good — no amount
    // of retrying on this client will bring it back, only a full re-open will.
    static bool isFatalStreamError(HRESULT hr)
    {
        return hr == AUDCLNT_E_DEVICE_INVALIDATED ||
               hr == AUDCLNT_E_SERVICE_NOT_RUNNING ||
               hr == AUDCLNT_E_NOT_INITIALIZED ||
               hr == AUDCLNT_E_RESOURCES_INVALIDATED;
    }

    // AUDCLNT_E_BUFFER_ERROR and friends are recoverable in place; a stop/reset/
    // start cycle usually resynchronises the client.
    bool tryRestartStream()
    {
        if (audio_client_ == nullptr)
            return false;
        audio_client_->Stop();
        if (FAILED(audio_client_->Reset()))
            return false;
        return SUCCEEDED(audio_client_->Start());
    }

    void threadLoop()
    {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);

#if defined(_WIN32)
        DWORD avrt_task_index = 0;
        HANDLE avrt_handle = AvSetMmThreadCharacteristics(L"Pro Audio", &avrt_task_index);
#endif

        while (running_.load(std::memory_order_acquire))
        {
            if (capture_client_ == nullptr || audio_client_ == nullptr || ring_buffer_ == nullptr)
            {
                Sleep(1);
                continue;
            }

            // Block until the capture client signals that data is ready. The
            // bounded timeout lets the thread re-check running_ and exit on stop().
            // event_ is null only when event-driven init was unavailable; in that
            // case we fall back to the polling Sleep(1) at the end of the loop.
            if (event_ != nullptr)
            {
                WaitForSingleObject(event_, 200);
            }

            // Drain all available packets before sleeping. Note that a packet
            // count of zero is NOT an error: a loopback endpoint delivers nothing
            // at all while no application is playing. Only real HRESULT failures
            // count against the stream's health.
            bool stream_failed = false;
            bool made_progress = false;
            for (;;)
            {
                UINT32 packet_size = 0;
                HRESULT hr = capture_client_->GetNextPacketSize(&packet_size);
                if (FAILED(hr))
                {
                    stream_failed = isFatalStreamError(hr) || !tryRestartStream();
                    break;
                }
                if (packet_size == 0)
                    break;

                BYTE* data = nullptr;
                UINT32 frames_read = 0;
                DWORD flags = 0;
                hr = capture_client_->GetBuffer(&data, &frames_read, &flags, nullptr, nullptr);
                if (FAILED(hr))
                {
                    stream_failed = isFatalStreamError(hr) || !tryRestartStream();
                    break;
                }

                if (data != nullptr && frames_read > 0 && (flags & AUDCLNT_BUFFERFLAGS_SILENT) == 0)
                    convertAndWrite(data, frames_read);
                else if (frames_read > 0)
                    writeSilence(frames_read);

                capture_client_->ReleaseBuffer(frames_read);
                made_progress = true;
            }

            if (stream_failed)
            {
                // A restart attempt may have succeeded; only a run of failures
                // condemns the stream. Without this the thread used to spin on the
                // 200 ms timeout forever, producing nothing while isRunning() kept
                // reporting healthy — the "sound stops until I toggle it" symptom.
                if (++consecutive_failures_ >= kMaxConsecutiveFailures)
                {
                    healthy_.store(false, std::memory_order_release);
                    break;
                }
                Sleep(5);
            }
            else if (made_progress)
            {
                consecutive_failures_ = 0;
            }

            if (event_ == nullptr)
            {
                Sleep(1);
            }
        }

#if defined(_WIN32)
        if (avrt_handle != nullptr)
            AvRevertMmThreadCharacteristics(avrt_handle);
#endif

        CoUninitialize();
    }

    void convertAndWrite(BYTE* data, UINT32 frames)
    {
        const auto n = static_cast<std::size_t>(frames);
        const auto ch = channels_;

        if (planar_.size() < n * ch)
        {
            planar_.resize(n * ch);
        }

        // Convert + deinterleave into the planar buffer in a single pass.
        if (isFloatFormat(mix_format_))
        {
            deinterleaveFloat32(reinterpret_cast<const float*>(data), planar_.data(), n, ch);
        }
        else if (isInt16Format(mix_format_))
        {
            int16ToFloat32Planar(reinterpret_cast<const std::int16_t*>(data), planar_.data(), n, ch);
        }
        else if (isInt24Format(mix_format_))
        {
            int24ToFloat32Planar(data, planar_.data(), n, ch);
        }
        else
        {
            // Unsupported format — write silence.
            writeSilence(frames);
            return;
        }

        const float* planar_ptrs[2] = {
            planar_.data(),
            ch > 1 ? planar_.data() + n : planar_.data()};

        const std::size_t written = ring_buffer_->tryWrite(planar_ptrs, n);
        if (written < n)
        {
            xrun_count_.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void writeSilence(UINT32 frames)
    {
        const auto n = static_cast<std::size_t>(frames);
        const auto ch = channels_;
        if (planar_.size() < n * ch)
        {
            planar_.resize(n * ch);
        }
        std::fill(planar_.begin(), planar_.begin() + n * ch, 0.0f);

        const float* planar_ptrs[2] = {
            planar_.data(),
            ch > 1 ? planar_.data() + n : planar_.data()};

        const std::size_t written = ring_buffer_->tryWrite(planar_ptrs, n);
        if (written < n)
        {
            xrun_count_.fetch_add(1, std::memory_order_relaxed);
        }
    }
};

#else

struct WasapiCapture::Impl
{
    shared::LockFreeAudioRingBuffer* ring_buffer_ = nullptr;
    double negotiated_sample_rate_ = 48000.0;
    double target_sample_rate_ = 48000.0;
    std::atomic<bool> open_ {false};
    std::atomic<bool> running_ {false};
    std::atomic<bool> healthy_ {true};
    std::atomic<std::size_t> xrun_count_ {0};
    std::thread thread_;
};

#endif  // _WIN32

WasapiCapture::WasapiCapture() = default;

WasapiCapture::~WasapiCapture()
{
    stop();
    close();
}

bool WasapiCapture::open(const EndpointId& endpoint_id,
                         double target_sample_rate,
                         shared::LockFreeAudioRingBuffer* ring_buffer,
                         bool loopback,
                         bool allow_exclusive)
{
    close();

    if (ring_buffer == nullptr || ring_buffer->channels() < 2)
    {
        return false;
    }

    impl_ = std::make_unique<Impl>();
    impl_->ring_buffer_ = ring_buffer;
    impl_->target_sample_rate_ = target_sample_rate;

    (void)endpoint_id;

#if defined(_WIN32)
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    (void)hr;  // COM may already be initialized on this thread

    IMMDeviceEnumerator* enumerator = nullptr;
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr) || enumerator == nullptr)
    {
        return false;
    }

    const std::wstring id_w = utf8ToWide(endpoint_id);
    hr = enumerator->GetDevice(id_w.c_str(), &impl_->device_);
    enumerator->Release();
    if (FAILED(hr) || impl_->device_ == nullptr)
    {
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
    impl_->channels_ = static_cast<std::size_t>(impl_->mix_format_->nChannels);

    REFERENCE_TIME default_period = 0;
    REFERENCE_TIME min_period = 0;
    hr = impl_->audio_client_->GetDevicePeriod(&default_period, &min_period);
    if (FAILED(hr))
    {
        default_period = 100000LL;  // 10 ms fallback.
    }

    // Event-driven capture removes the polling latency/jitter of the old
    // Sleep(1) loop. If the event can't be created we leave event_ null and
    // fall back to polling-mode initialise below.
    impl_->event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    DWORD stream_flags = impl_->event_ != nullptr ? AUDCLNT_STREAMFLAGS_EVENTCALLBACK : 0;

    // T004: Add loopback flag if this is a loopback capture (render endpoint)
    if (loopback)
    {
        stream_flags |= AUDCLNT_STREAMFLAGS_LOOPBACK;
    }

    bool initialized = false;

    // Best path: exclusive mode bypasses the Windows Audio Engine entirely,
    // giving direct hardware access at the minimum device period (typically
    // 1-3 ms vs. the shared-mode engine pipeline at ~5-15 ms total).
    // Uses a separate IAudioClient probe so the shared-mode fallback client
    // remains clean if exclusive initialisation fails (Initialize is one-shot).
    //
    // Skipped for loopback: AUDCLNT_STREAMFLAGS_LOOPBACK is only valid on a
    // render endpoint in shared mode, so the probe can only fail — and were it
    // ever to succeed it would take the render endpoint away from every other
    // application on the machine. Also skipped unless the user actually asked
    // for exclusive mode; probing it unconditionally meant an endpoint another
    // app held exclusively could make the open behave unpredictably.
    if (allow_exclusive && !loopback)
    {
        IAudioClient* excl_client = nullptr;
        hr = impl_->device_->Activate(
            __uuidof(IAudioClient), CLSCTX_ALL, nullptr,
            reinterpret_cast<void**>(&excl_client));
        if (SUCCEEDED(hr) && excl_client != nullptr)
        {
            hr = excl_client->IsFormatSupported(
                AUDCLNT_SHAREMODE_EXCLUSIVE, impl_->mix_format_, nullptr);
            if (hr == S_OK)
            {
                const REFERENCE_TIME excl_period = (min_period > 0) ? min_period : default_period;
                hr = excl_client->Initialize(
                    AUDCLNT_SHAREMODE_EXCLUSIVE,
                    stream_flags,
                    excl_period,
                    excl_period,
                    impl_->mix_format_,
                    nullptr);
                if (SUCCEEDED(hr))
                {
                    impl_->audio_client_->Release();
                    impl_->audio_client_ = excl_client;
                    excl_client = nullptr;
                    initialized = true;
                }
            }
            if (excl_client != nullptr)
                excl_client->Release();
        }
    }

    // Preferred shared-mode path: IAudioClient3 low-latency shared stream at the
    // minimum engine period (typically a few ms) instead of the ~10 ms default period.
    if (!initialized)
    {
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
    }

    // Fallback: classic shared-mode initialise at the default device period.
    if (!initialized)
    {
        hr = impl_->audio_client_->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            stream_flags,
            default_period,
            0,
            impl_->mix_format_,
            nullptr);
        if (FAILED(hr))
        {
            close();
            return false;
        }
    }

    // Bind the wake event so the capture thread can block instead of polling.
    // The stream was initialised with the event-callback flag, so a binding
    // failure here is fatal — drop the open rather than hang on a dead stream.
    if (impl_->event_ != nullptr)
    {
        hr = impl_->audio_client_->SetEventHandle(impl_->event_);
        if (FAILED(hr))
        {
            close();
            return false;
        }
    }

    UINT32 buffer_size = 0;
    hr = impl_->audio_client_->GetBufferSize(&buffer_size);
    if (FAILED(hr))
    {
        buffer_size = 0;
    }
    impl_->buffer_size_ = static_cast<int>(buffer_size);

    hr = impl_->audio_client_->GetService(
        __uuidof(IAudioCaptureClient),
        reinterpret_cast<void**>(&impl_->capture_client_));
    if (FAILED(hr) || impl_->capture_client_ == nullptr)
    {
        close();
        return false;
    }

    // Pre-allocate the planar buffer to the ring-buffer capacity.
    const std::size_t cap = ring_buffer->capacity();
    impl_->planar_.resize(cap * impl_->channels_);

    impl_->open_.store(true, std::memory_order_release);

#else
    (void)endpoint_id;
    (void)target_sample_rate;
    (void)loopback;
    (void)allow_exclusive;
#endif

    impl_->healthy_.store(true, std::memory_order_release);
    return true;
}

void WasapiCapture::close()
{
    stop();

#if defined(_WIN32)
    if (impl_ != nullptr)
    {
        if (impl_->capture_client_ != nullptr)
        {
            impl_->capture_client_->Release();
            impl_->capture_client_ = nullptr;
        }
        if (impl_->audio_client_ != nullptr)
        {
            impl_->audio_client_->Release();
            impl_->audio_client_ = nullptr;
        }
        if (impl_->mix_format_ != nullptr)
        {
            CoTaskMemFree(impl_->mix_format_);
            impl_->mix_format_ = nullptr;
        }
        if (impl_->device_ != nullptr)
        {
            impl_->device_->Release();
            impl_->device_ = nullptr;
        }
        if (impl_->event_ != nullptr)
        {
            CloseHandle(impl_->event_);
            impl_->event_ = nullptr;
        }
    }
#endif

    if (impl_ != nullptr)
    {
        impl_->open_.store(false, std::memory_order_release);
    }
}

bool WasapiCapture::start()
{
    if (impl_ == nullptr || !impl_->open_.load(std::memory_order_acquire))
    {
        return false;
    }

    if (impl_->running_.load(std::memory_order_acquire))
    {
        return true;
    }

#if defined(_WIN32)
    if (impl_->audio_client_ == nullptr)
    {
        return false;
    }

    const HRESULT hr = impl_->audio_client_->Start();
    if (FAILED(hr))
    {
        return false;
    }
#endif

    impl_->running_.store(true, std::memory_order_release);
    impl_->thread_ = std::thread(&Impl::threadLoop, impl_.get());

    return true;
}

void WasapiCapture::stop()
{
    if (impl_ == nullptr || !impl_->running_.load(std::memory_order_acquire))
    {
        return;
    }

    impl_->running_.store(false, std::memory_order_release);

#if defined(_WIN32)
    // Wake the capture thread immediately if it is blocked on the event wait.
    if (impl_->event_ != nullptr)
    {
        SetEvent(impl_->event_);
    }
#endif

    if (impl_->thread_.joinable())
    {
        impl_->thread_.join();
    }

#if defined(_WIN32)
    if (impl_->audio_client_ != nullptr)
    {
        impl_->audio_client_->Stop();
    }
#endif
}

bool WasapiCapture::isOpen() const noexcept
{
    return impl_ != nullptr && impl_->open_.load(std::memory_order_acquire);
}

bool WasapiCapture::isRunning() const noexcept
{
    return impl_ != nullptr && impl_->running_.load(std::memory_order_acquire);
}

bool WasapiCapture::isHealthy() const noexcept
{
    return impl_ != nullptr && impl_->healthy_.load(std::memory_order_acquire);
}

double WasapiCapture::negotiatedSampleRate() const noexcept
{
    return impl_ != nullptr ? impl_->negotiated_sample_rate_ : 48000.0;
}

double WasapiCapture::targetSampleRate() const noexcept
{
    return impl_ != nullptr ? impl_->target_sample_rate_ : 48000.0;
}

float WasapiCapture::latencyMs() const noexcept
{
#if defined(_WIN32)
    if (impl_ == nullptr || impl_->audio_client_ == nullptr)
    {
        return 0.0f;
    }
    REFERENCE_TIME latency = 0;
    if (FAILED(impl_->audio_client_->GetStreamLatency(&latency)))
    {
        return 0.0f;
    }
    return static_cast<float>(latency / 10000.0);  // 100 ns units → ms.
#else
    return 0.0f;
#endif
}

std::size_t WasapiCapture::xrunCount() const noexcept
{
    return impl_ != nullptr ? impl_->xrun_count_.load(std::memory_order_relaxed) : 0;
}

}  // namespace jyglobalvst::engine
