// src/audio-engine/routing/audio_engine_impl.cpp
//
// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • audioDeviceIOCallbackWithContext is the audio thread. No alloc,
//     no lock, no exception, no log, no UI call.
//   • UI-thread mutations enqueue commands; callback drains at top.
//   • CPU stats updated atomically from the callback (single producer);
//     listener notifications dispatch via JUCE MessageManager so they
//     never run on the audio thread.
//   • Testable-dev uses juce::AudioDeviceManager which abstracts WASAPI.
//     Release prep replaces this with direct IAudioClient3 (T025, T026).
// =====================================================================

#include "audio_engine_impl.h"
#include "device_watchdog.h"
#include "energy_saver_controller.h"

#include "../builtin-effects/builtin_effect_registry.h"
#include "../chain/preset_serializer.h"
#include "../vst-host/default_scan_paths.h"

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <fstream>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <thread>
#include <utility>
#include <iostream>
#include <windows.h>
#include <avrt.h>
#include <mmdeviceapi.h>

namespace jyglobalvst::engine {

namespace {

constexpr int kMeterThrottleDivisor = 3;  // ~30 Hz meter updates @ 48kHz 256-sample buffers

// Highest sample rate we size real-time buffers for. WASAPI shared mode ignores
// the requested rate and runs at the endpoint's mix-format rate, which can be up
// to 192 kHz. Ring buffers must give the drift controller the same real-time
// headroom (in milliseconds) regardless of that rate, so we size them from this
// ceiling rather than from desired_sample_rate_ (which defaults to 48 kHz and
// would leave the rings ~half their intended duration at 96 kHz → starvation and
// re-priming, audible as dropouts/clicks at correct pitch).
constexpr double kMaxSupportedSampleRate = 192000.0;

// --- Capture→ASIO clock-drift controller tuning -------------------------
// The WASAPI-capture → ASIO-output bridge crosses two independent clocks. A PI
// controller on the ring-buffer fill level continuously trims the resampling
// ratio: the integral term converges to the TRUE rate ratio, so even a
// persistent nominal mismatch (e.g. an ASIO device whose real rate differs from
// the rate it reports) is fully corrected rather than saturating the loop.
//
// Target ring fill — effectively the added bridge latency. Large enough to
// absorb WASAPI packet bursts and ASIO scheduling jitter without underrunning.
// Raised 30 → 45 ms: at 30 ms the WASAPI burst/jitter envelope could dip the ring
// below the re-prime threshold, and each re-prime emits a silence gap (audible
// cutout) that is NOT counted as an xrun. The extra 15 ms of cushion keeps the
// fill clear of that threshold. This is capture-bridge latency only; it is
// independent of the <10 ms plugin-chain processing budget.
constexpr double kCaptureTargetSeconds = 0.045;  // 45 ms

// Helper function to log debug messages
void LogDebug(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char buffer[1024];
    vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, fmt, args);
    va_end(args);

    OutputDebugStringA("[JyGlobalVST] ");
    OutputDebugStringA(buffer);
    OutputDebugStringA("\n");

    std::cerr << "[JyGlobalVST] " << buffer << std::endl;

    try
    {
        auto path = std::filesystem::path(std::getenv("LOCALAPPDATA")) / "JyGlobalVST" / "startup.log";
        std::filesystem::create_directories(path.parent_path());
        std::ofstream ofs(path, std::ios::app);
        if (ofs)
        {
            auto now = std::chrono::system_clock::now();
            auto t = std::chrono::system_clock::to_time_t(now);
            ofs << std::put_time(std::gmtime(&t), "%H:%M:%S") << " [Engine] " << buffer << "\n";
        }
    }
    catch (...)
    {
    }
}

// JUCE doesn't expose Windows endpoint IDs directly — it surfaces friendly
// names. The endpoint enumerator gives us both, so we map by friendly_name
// for testable-dev. Release prep does an exact endpoint-id match through
// direct WASAPI.
const std::string& nameForEndpoint(const std::vector<shared::EndpointDescriptor>& list,
                                   const EndpointId& id)
{
    static const std::string empty;
    for (const auto& d : list)
    {
        if (d.endpoint_id == id)
        {
            return d.friendly_name;
        }
    }
    return empty;
}

// Simple host window for a plugin's AudioProcessorEditor.
class PluginEditorHost : public juce::DocumentWindow
{
public:
    // Constructor only initialises the JUCE window; no plugin calls here so
    // standard C++ exception handling is sufficient.
    explicit PluginEditorHost(const juce::String& plugin_name)
        : juce::DocumentWindow(plugin_name + " — Editor",
                               juce::Colours::darkgrey,
                               juce::DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar(true);
    }

    // Called from attachEditorSafe() which is SEH-wrapped.
    void attachEditor(juce::AudioProcessorEditor* editor)
    {
        setResizable(editor->isResizable(), false);
        setContentOwned(editor, true);
        centreWithSize(editor->getWidth(), editor->getHeight());
        LogDebug("PluginEditorHost::attachEditor: window sized %dx%d", getWidth(), getHeight());
    }

    void closeButtonPressed() override
    {
        LogDebug("PluginEditorHost::closeButtonPressed: User closed editor window");
        setVisible(false);
    }
};

static DWORD g_editor_exception_code = 0;
static ULONG_PTR g_editor_exception_addr = 0;

// SEH wrapper for createEditor. Must contain no C++ objects with destructors.
// Captures exception code and faulting address for diagnostics.
static juce::AudioProcessorEditor* createEditorDRM(juce::AudioProcessor* proc)
{
    __try
    {
        return proc->createEditor();
    }
    __except ([](EXCEPTION_POINTERS* p) -> LONG {
        g_editor_exception_code = p->ExceptionRecord->ExceptionCode;
        g_editor_exception_addr = (ULONG_PTR)p->ExceptionRecord->ExceptionAddress;
        char buf[128];
        wsprintfA(buf, "[JyGlobalVST] createEditorDRM: exception 0x%08lX at 0x%llX\n",
                  (unsigned long)g_editor_exception_code,
                  (unsigned long long)g_editor_exception_addr);
        OutputDebugStringA(buf);
        return EXCEPTION_EXECUTE_HANDLER;
    }(GetExceptionInformation()))
    {
        return nullptr;
    }
}

// SEH wrapper for showing the editor window. A VST3 plugin's view attaches and
// first renders when the window becomes visible, which is another point where a
// faulty plugin can crash. Must contain no C++ objects with destructors (/EHsc).
bool showEditorSafe(juce::DocumentWindow* window)
{
    __try
    {
        window->setVisible(true);
        window->toFront(true);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// SEH wrapper for attaching a plugin editor to its host window. setContentOwned
// and isResizable() call into plugin code; /EHsc catch(...) does NOT catch SEH
// structured exceptions, so we need __try/__except here.
static bool attachEditorSafe(PluginEditorHost* window, juce::AudioProcessorEditor* editor)
{
    __try
    {
        window->attachEditor(editor);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

}  // namespace

AudioEngineImpl::AudioEngineImpl()
    : watchdog_(std::make_unique<DeviceWatchdog>(this))
    , plugin_chain_(std::make_unique<PluginChain>())
    , scanner_(std::make_unique<PluginScanner>())
    , scan_cache_(std::make_unique<ScanCache>())
    , plugin_loader_(std::make_unique<VST3PluginLoader>())
    , builtin_registry_(std::make_unique<BuiltinEffectRegistry>())
{
    device_manager_.initialiseWithDefaultDevices(2, 2);
    scan_cache_->load();
}

AudioEngineImpl::~AudioEngineImpl()
{
    stop();
    stopEnergySaver();  // defensive: guarantee the energy-saver thread is joined
}

void AudioEngineImpl::start()
{
    LogDebug("start() called");
    std::lock_guard lk {control_mutex_};
    if (running_.load())
    {
        LogDebug("start() already running, returning");
        return;
    }

    mixed_mode_active_.store(false, std::memory_order_release);
    bool wasapi_loopback_mode = false;

    if (desired_output_transport_kind_ == TransportKind::Asio &&
        desired_input_transport_kind_ == TransportKind::Wasapi &&
        !desired_input_id_.empty())
    {
        mixed_mode_active_.store(true, std::memory_order_release);
    }
    else if (desired_output_transport_kind_ == TransportKind::Wasapi &&
             desired_input_transport_kind_ == TransportKind::Wasapi &&
             !desired_input_id_.empty())
    {
        wasapi_loopback_mode = true;
    }

    // T025: Hard-block start if resolved capture and output are the same device.
    if (!desired_input_id_.empty() && !desired_output_id_.empty())
    {
        const EndpointId conflict = same_device_guard_.checkConflict(desired_input_id_, desired_output_id_);
        if (!conflict.empty())
        {
            notifyOnUiThread([conflict](IAudioEngineListener& l) {
                l.onSameDeviceConflict(conflict);
            });
            LogDebug("start() refused: same-device conflict on %s", conflict.c_str());
            return;
        }
    }

    LogDebug("start() transport setup: mixed_mode=%d, output_kind=%d (Wasapi=0), input_kind=%d (Wasapi=0), input_id='%s', wasapi_loopback=%d",
             mixed_mode_active_.load(std::memory_order_acquire) ? 1 : 0,
             static_cast<int>(desired_output_transport_kind_),
             static_cast<int>(desired_input_transport_kind_),
             desired_input_id_.c_str(),
             wasapi_loopback_mode ? 1 : 0);

    if (mixed_mode_active_.load(std::memory_order_acquire))
    {
        openWasapiCapture();
        applyAsioTransport();
    }
    else if (desired_output_transport_kind_ == TransportKind::Asio)
    {
        applyAsioTransport();
    }
    else if (wasapi_loopback_mode)
    {
        LogDebug("Using pure WASAPI loopback mode");
        openWasapiCapture();
        LogDebug("start(): wasapi_capture_ %s, capture_ring_buffer_ %s, rate=%.0f",
                 wasapi_capture_ ? "ok" : "null",
                 capture_ring_buffer_ ? "ok" : "null",
                 capture_wasapi_rate_);
        openWasapiOutput();
        LogDebug("start(): wasapi_output_ %s, output_ring_buffer_ %s, rate=%.0f",
                 wasapi_output_ ? "ok" : "null",
                 output_ring_buffer_ ? "ok" : "null",
                 output_wasapi_rate_);
    }
    else
    {
        applyDeviceSelection();
    }

    // T019: Protect loopback capture endpoint from feedback by muting/restoring
    if (!desired_input_id_.empty())
    {
        endpoint_volume_guard_.activate(desired_input_id_);
        if (!endpoint_volume_guard_.mute())
        {
            // Fallback: muting not available, notify UI to select different output
            notifyOnUiThread([id = desired_input_id_](IAudioEngineListener& l) {
                l.onCaptureMuteFallbackRequired(id);
            });
        }
    }

    if (wasapi_loopback_mode)
    {
        // Pure WASAPI mode: JUCE does not drive the callback.
        // Prepare work buffer and plugin chain manually.
        // The chain runs at the output device's negotiated rate so no output-side
        // resampler is needed. Capture is resampled only when its rate differs.
        double chain_rate = desired_sample_rate_;
        if (wasapi_output_ != nullptr && output_wasapi_rate_ > 0.0)
        {
            chain_rate = output_wasapi_rate_;
            LogDebug("Pure WASAPI mode: chain_rate = output_wasapi_rate_ = %.0f Hz", chain_rate);
        }
        else if (mixed_mode_active_.load(std::memory_order_acquire))
        {
            LogDebug("Mixed mode: chain_rate = desired_sample_rate_ = %.0f Hz", chain_rate);
        }
        work_buffer_.setSize(2, desired_buffer_size_, false, false, true);
        plugin_chain_->prepareToPlay(chain_rate, desired_buffer_size_);
        negotiated_sample_rate_.store(static_cast<int>(chain_rate));

        // Set up capture-side resampler when capture rate differs from chain rate.
        const double wasapi_rate = capture_wasapi_rate_ > 0.0 ? capture_wasapi_rate_ : chain_rate;
        capture_resampling_enabled_ = wasapi_rate > 0.0 && chain_rate > 0.0 &&
                                      std::abs(wasapi_rate - chain_rate) > 0.1;
        LogDebug("Capture resampling setup: capture_wasapi_rate_=%.0f, chain_rate=%.0f, wasapi_rate=%.0f, enabled=%d",
                 capture_wasapi_rate_, chain_rate, wasapi_rate, capture_resampling_enabled_ ? 1 : 0);
        if (capture_resampling_enabled_)
        {
            capture_nominal_ratio_ = wasapi_rate / chain_rate;
            LogDebug("Resampler ratio: %.4f (input/output rate ratio for %d->%d Hz)",
                     capture_nominal_ratio_, static_cast<int>(wasapi_rate), static_cast<int>(chain_rate));
            xrun_count_.store(0, std::memory_order_relaxed);

            const int raw_max = static_cast<int>(std::ceil(
                                    static_cast<double>(desired_buffer_size_) * capture_nominal_ratio_)) + 8;
            capture_resampler_.prepare(capture_nominal_ratio_, static_cast<std::size_t>(raw_max), 2);
            capture_raw_buffer_.setSize(2, raw_max, false, true, true);
        }
        else
        {
            capture_raw_buffer_.setSize(0, 0);
        }

        // No output-side resampler — chain rate matches output device rate.

        startEngineThread();
    }
    else
    {
        device_manager_.addAudioCallback(this);
        // JUCE's addAudioCallback calls audioDeviceAboutToStart only when
        // currentAudioDevice != null. After a WASAPI→ASIO device-type switch the
        // device may still be null at that point, leaving work_buffer_ at size 0.
        // Call it explicitly only in that case to avoid calling prepareToPlay twice,
        // which corrupts the internal state of some plugins (e.g. ARC X).
        if (work_buffer_.getNumSamples() == 0)
        {
            if (auto* device = device_manager_.getCurrentAudioDevice())
            {
                audioDeviceAboutToStart(device);
            }
        }
    }

    watchdog_->start(&endpoint_enum_);
    running_.store(true);
    last_active_output_transport_kind_ = desired_output_transport_kind_;
    if (mixed_mode_active_.load(std::memory_order_acquire) || wasapi_loopback_mode)
    {
    }
    startEnergySaver();
    LogDebug("start() completed");
}

void AudioEngineImpl::stop()
{
    LogDebug("stop() called");
    std::lock_guard lk {control_mutex_};
    if (!running_.exchange(false))
    {
        LogDebug("stop() not running, returning");
        return;
    }
    stopEnergySaver();
    scanner_->cancel();
    scanner_->waitUntilFinished();
    watchdog_->stop();
    stopEngineThread();
    device_manager_.removeAudioCallback(this);
    closeWasapiCapture();
    closeWasapiOutput();  // T017: Close WASAPI render output
    // T019: Restore captured endpoint mute state after stopping
    endpoint_volume_guard_.restore();
    endpoint_volume_guard_.deactivate();
    mixed_mode_active_.store(false, std::memory_order_release);
    work_buffer_.setSize(0, 0);
    plugin_chain_->releaseResources();
    LogDebug("stop() completed");
}

bool AudioEngineImpl::isRunning() const
{
    return running_.load();
}

void AudioEngineImpl::setListener(IAudioEngineListener* listener)
{
    std::lock_guard lk {control_mutex_};
    listener_ = listener;
}

void AudioEngineImpl::setMasterVolume(float gain_linear)
{
    const float clamped = std::max(0.0f, std::min(1.0f, gain_linear));
    EngineCommand cmd;
    cmd.kind = EngineCommand::Kind::SetMasterVolume;
    cmd.value = clamped;
    (void)command_queue_.tryPush(cmd);
}

void AudioEngineImpl::reset()
{
    // control_mutex_ is a recursive_mutex, so re-entering it from stop()/start()
    // on this thread is safe. Delegating to them (rather than hand-duplicating
    // their branch logic here) guarantees reset() can never drift out of sync
    // with the mixed/wasapi-loopback/plain-device branches start() handles —
    // which is what previously left reset() without a wasapi-loopback-mode
    // path, causing a stale sample rate after an ASIO -> WASAPI switch.
    std::lock_guard lk {control_mutex_};

    // plugin_chain_->releaseResources() (inside stop()) drops DSP resources but
    // preserves the loaded slots/state/ordering; start() re-prepares the chain
    // with the (possibly new) sample rate/block size.
    stop();
    start();

    chain_revision_.store(plugin_chain_->revision());
    notifyOnUiThread([this](IAudioEngineListener& l) {
        l.onChainRevision(chain_revision_.load());
    });
}

// =====================================================================
// Energy Saver
// =====================================================================

void AudioEngineImpl::setEnergySaverEnabled(bool enabled)
{
    const bool was = energy_saver_enabled_.exchange(enabled);
    if (was == enabled)
        return;

    LogDebug("setEnergySaverEnabled(%d)", enabled ? 1 : 0);

    // Turning the feature off must wake immediately so the chain resumes right
    // away; the polling thread would otherwise leave the engine silent for up to
    // one tick. The thread itself keeps running for the engine's lifetime.
    if (!enabled)
    {
        setEnergySaverSleeping(false);
    }
}

bool AudioEngineImpl::isEnergySaverEnabled() const
{
    return energy_saver_enabled_.load();
}

bool AudioEngineImpl::isEnergySaverSleeping() const
{
    return energy_saver_sleeping_.load();
}

void AudioEngineImpl::setEnergySaverSleeping(bool sleeping)
{
    if (energy_saver_sleeping_.exchange(sleeping) == sleeping)
        return;
    LogDebug("EnergySaver: %s", sleeping ? "sleeping" : "awake");
    notifyOnUiThread([sleeping](IAudioEngineListener& l) {
        l.onEnergySaverStateChanged(sleeping);
    });
}

void AudioEngineImpl::startEnergySaver()
{
    if (energy_saver_thread_running_.exchange(true))
        return;
    energy_saver_thread_ = std::thread([this]() { energySaverThreadLoop(); });
}

void AudioEngineImpl::stopEnergySaver()
{
    if (!energy_saver_thread_running_.exchange(false))
        return;
    if (energy_saver_thread_.joinable())
        energy_saver_thread_.join();
    // Never leave the audio path suspended once we are no longer watching it.
    setEnergySaverSleeping(false);
}

void AudioEngineImpl::energySaverThreadLoop()
{
    using clock = std::chrono::steady_clock;
    // All decision logic lives in EnergySaverController (unit-tested separately);
    // this thread just samples input level at ~10 Hz and syncs the result into
    // the atomics the audio callback reads.
    EnergySaverController controller(kEnergySaverIdleMs, kEnergySaverWakeDb);
    const auto t0 = clock::now();
    auto now_ms = [&t0]() {
        return static_cast<long long>(
            std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - t0).count());
    };
    controller.reset(now_ms());

    while (energy_saver_thread_running_.load(std::memory_order_acquire))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        controller.setEnabled(energy_saver_enabled_.load(std::memory_order_relaxed));
        const float peak = std::max(meter_input_peak_l_.load(std::memory_order_relaxed),
                                    meter_input_peak_r_.load(std::memory_order_relaxed));
        if (controller.update(now_ms(), peak))
        {
            setEnergySaverSleeping(controller.sleeping());
        }
    }
}

std::vector<HardwareOutputInfo> AudioEngineImpl::listOutputs() const
{
    std::vector<HardwareOutputInfo> out;
    for (const auto& d : endpoint_enum_.list(shared::EndpointFlow::Render))
    {
        HardwareOutputInfo info;
        info.endpoint_id = d.endpoint_id;
        info.friendly_name = d.friendly_name;
        info.is_default = d.is_default;
        info.is_present = d.is_present;
        info.transport_kind = TransportKind::Wasapi;
        out.push_back(std::move(info));
    }

    // Append ASIO devices (if ASIO support is compiled in).
    for (const auto& name : asio_transport_->listDevices())
    {
        HardwareOutputInfo info;
        info.endpoint_id = name;
        info.friendly_name = name;
        info.is_default = false;
        info.is_present = true;
        info.transport_kind = TransportKind::Asio;
        out.push_back(std::move(info));
    }

    // T011: Explicitly detect zero/one output edge cases for UI guidance.
    if (out.empty())
    {
        LogDebug("listOutputs(): no output devices available");
    }
    else if (out.size() == 1)
    {
        LogDebug("listOutputs(): only one output device available (%s)", out[0].friendly_name.c_str());
    }

    return out;
}

std::vector<HardwareOutputInfo> AudioEngineImpl::listInputs() const
{
    // T015: List render endpoints for loopback capture.
    // In testable-dev, "input sources" are render endpoints captured in loopback mode.
    std::vector<HardwareOutputInfo> out;
    for (const auto& d : endpoint_enum_.list(shared::EndpointFlow::Render))
    {
        HardwareOutputInfo info;
        info.endpoint_id = d.endpoint_id;
        info.friendly_name = d.friendly_name;
        info.is_default = d.is_default;
        info.is_present = d.is_present;
        out.push_back(std::move(info));
    }
    return out;
}

void AudioEngineImpl::selectOutput(const EndpointId& id)
{
    TransportKind new_transport_kind = TransportKind::Wasapi;

    {
        std::lock_guard lk {control_mutex_};
        if (desired_output_id_ == id && desired_asio_device_name_ == id)
        {
            return;
        }

        // Check if this is an ASIO device.
        const auto asio_devices = asio_transport_->listDevices();
        const bool is_asio = std::find(asio_devices.begin(), asio_devices.end(), id) != asio_devices.end();

        if (is_asio)
        {
            desired_asio_device_name_ = id;
            desired_output_id_.clear();
            desired_output_friendly_name_.clear();
            desired_output_transport_kind_ = TransportKind::Asio;
            new_transport_kind = TransportKind::Asio;
        }
        else
        {
            desired_output_id_ = id;
            desired_asio_device_name_.clear();
            desired_output_transport_kind_ = TransportKind::Wasapi;
            new_transport_kind = TransportKind::Wasapi;

            // Capture friendly name for resolution priority chain (FR-022m).
            const auto outputs = endpoint_enum_.list(shared::EndpointFlow::Render);
            for (const auto& d : outputs)
            {
                if (d.endpoint_id == id)
                {
                    desired_output_friendly_name_ = d.friendly_name;
                    break;
                }
            }
        }

        // Update mixed-mode flag.
        mixed_mode_active_.store(
            desired_output_transport_kind_ == TransportKind::Asio &&
                desired_input_transport_kind_ == TransportKind::Wasapi &&
                !desired_input_id_.empty(),
            std::memory_order_release);
    }
    if (running_.load())
    {
        std::lock_guard lk {control_mutex_};
        // If transport mode changed, use reset() for complete re-initialization
        if (new_transport_kind != last_active_output_transport_kind_)
        {
            reset();
            last_active_output_transport_kind_ = new_transport_kind;
        }
        else
        {
            stop();
            start();
        }
    }
}

void AudioEngineImpl::selectInput(const EndpointId& id)
{
    {
        std::lock_guard lk {control_mutex_};
        if (desired_input_id_ == id)
        {
            return;
        }
        desired_input_id_ = id;
        desired_input_transport_kind_ = TransportKind::Wasapi;

        // Update mixed-mode flag.
        mixed_mode_active_.store(
            desired_output_transport_kind_ == TransportKind::Asio &&
                desired_input_transport_kind_ == TransportKind::Wasapi &&
                !desired_input_id_.empty(),
            std::memory_order_release);
    }
    if (running_.load())
    {
        std::lock_guard lk {control_mutex_};
        stop();
        start();
    }
}

EndpointId AudioEngineImpl::currentOutput() const
{
    std::lock_guard lk {control_mutex_};
    if (desired_output_transport_kind_ == TransportKind::Asio)
    {
        return desired_asio_device_name_;
    }
    return desired_output_id_;
}

EndpointId AudioEngineImpl::currentInput() const
{
    std::lock_guard lk {control_mutex_};
    return desired_input_id_;
}

bool AudioEngineImpl::isCaptureDeviceMuted() const
{
    // T020: Return mute state from EndpointVolumeGuard
    return endpoint_volume_guard_.isMuted();
}

DeviceResolutionSource AudioEngineImpl::currentResolutionSource() const
{
    std::lock_guard lk {control_mutex_};
    return resolution_source_;
}

void AudioEngineImpl::setBufferSize(int samples)
{
    const bool is_asio = desired_output_transport_kind_ == TransportKind::Asio;
    constexpr int kAllowedWasapi[] = {512, 1024};
    constexpr int kAllowedAsio[] = {32, 64, 128, 256, 512, 1024};
    const auto* allowed = is_asio ? kAllowedAsio : kAllowedWasapi;
    const size_t count = is_asio ? std::size(kAllowedAsio) : std::size(kAllowedWasapi);
    if (std::find(allowed, allowed + count, samples) == allowed + count)
    {
        throw std::invalid_argument(is_asio
                                        ? "buffer size must be one of {32, 64, 128, 256, 512, 1024}"
                                        : "buffer size must be one of {512, 1024} in WASAPI mode");
    }
    {
        std::lock_guard lk {control_mutex_};
        if (desired_buffer_size_ == samples)
        {
            return;
        }
        desired_buffer_size_ = samples;
    }
    if (running_.load())
    {
        std::lock_guard lk {control_mutex_};
        stop();
        start();
    }
}

int AudioEngineImpl::bufferSize() const
{
    std::lock_guard lk {control_mutex_};
    return desired_buffer_size_;
}

void AudioEngineImpl::setSampleRate(double rate)
{
    {
        std::lock_guard lk {control_mutex_};
        if (desired_sample_rate_ == rate)
        {
            return;
        }
        desired_sample_rate_ = rate;
    }
    if (running_.load())
    {
        std::lock_guard lk {control_mutex_};
        stop();
        start();
    }
}

double AudioEngineImpl::sampleRate() const
{
    std::lock_guard lk {control_mutex_};
    return desired_sample_rate_;
}
int AudioEngineImpl::negotiatedSampleRate() const
{
    return negotiated_sample_rate_.load();
}
int AudioEngineImpl::outputDeviceSampleRate() const
{
    // Pure-WASAPI loopback mode: the render endpoint's negotiated rate.
    if (wasapi_output_ != nullptr && output_wasapi_rate_ > 0.0)
    {
        return static_cast<int>(output_wasapi_rate_);
    }
    // ASIO / JUCE-callback mode: the device drives both chain and output, so the
    // chain's negotiated rate is the output rate. Returns 0 when not running.
    if (running_.load())
    {
        return negotiated_sample_rate_.load();
    }
    return 0;
}

int AudioEngineImpl::inputDeviceSampleRate() const
{
    // Capture is always via WASAPI; report the endpoint's negotiated rate while
    // running, 0 otherwise.
    if (running_.load() && capture_wasapi_rate_ > 0.0)
    {
        return static_cast<int>(capture_wasapi_rate_);
    }
    return 0;
}

// --- Plugin catalog / chain / preset stubs ------------------------------

void AudioEngineImpl::rescanPlugins(IScanProgressListener* progress)
{
    std::lock_guard lk {control_mutex_};
    auto paths = defaultVst3ScanPaths();
    scanner_->start(paths, progress, scan_cache_.get());
}

void AudioEngineImpl::cancelScan()
{
    scanner_->cancel();
    scanner_->waitUntilFinished();
}

std::vector<PluginCatalogEntry> AudioEngineImpl::catalog() const
{
    std::lock_guard lk {control_mutex_};
    auto result = builtin_registry_->entries();
    auto scanned = scan_cache_->plugins();
    result.insert(result.end(), scanned.begin(), scanned.end());
    return result;
}

ChainSnapshot AudioEngineImpl::snapshotChain() const
{
    std::lock_guard lk {control_mutex_};
    ChainSnapshot s;
    s.chain_revision = plugin_chain_->revision();
    s.slots = plugin_chain_->snapshot();
    return s;
}

InstanceId AudioEngineImpl::addPlugin(const PluginRef& ref, int position)
{
    std::lock_guard lk {control_mutex_};

    LogDebug("addPlugin: position=%d, ref.vendor='%s', ref.name='%s'",
             position, ref.vendor.c_str(), ref.name.c_str());

    std::shared_ptr<PluginInstance> instance;

    // Try built-in registry first (FR-002: no disk access for built-ins).
    if (builtin_registry_ && builtin_registry_->isBuiltin(ref))
    {
        LogDebug("addPlugin: Resolving built-in effect");
        auto builtin_proc = builtin_registry_->create(ref);
        if (builtin_proc)
        {
            Plugin descriptor;
            auto entry = builtin_registry_->findByRef(ref);
            if (entry)
            {
                descriptor.uid = PluginUidToHexString(entry->ref.plugin_uid);
                descriptor.vendor = entry->ref.vendor;
                descriptor.name = entry->ref.name;
                descriptor.category = entry->category;
                descriptor.version = entry->version;
                descriptor.file_path.clear();
            }
            instance = std::make_shared<PluginInstance>(descriptor, std::move(builtin_proc));
            LogDebug("addPlugin: Built-in effect created successfully");
        }
        else
        {
            LogDebug("addPlugin: Built-in factory returned null");
        }
    }

    if (!instance)
    {
        const auto entry = scan_cache_->findByRef(ref);
        if (entry)
        {
            LogDebug("addPlugin: Found scan cache entry, file_path='%s'", entry->file_path.c_str());
            auto result = plugin_loader_->load(entry->file_path);
            if (result && result.instance)
            {
                LogDebug("addPlugin: Successfully loaded plugin instance");
                instance = std::shared_ptr<PluginInstance>(std::move(result.instance));
            }
            else
            {
                LogDebug("addPlugin: FAILED to load plugin - result invalid or instance null");
                if (!result)
                    LogDebug("addPlugin: plugin_loader_->load() returned false/null result");
                else if (!result.instance)
                    LogDebug("addPlugin: result exists but instance is null");
            }
        }
        else
        {
            LogDebug("addPlugin: FAILED - scan cache entry NOT FOUND for ref.name='%s'", ref.name.c_str());
        }
    }

    if (!instance)
    {
        LogDebug("addPlugin: FAILED — not adding placeholder slot");
        std::string reason = "Failed to load plugin: " + ref.name;
        notifyOnUiThread([reason](IAudioEngineListener& l) { l.onPluginFailed(InstanceId{}, reason); });
        return InstanceId{};
    }

    const auto id = plugin_chain_->addSlot(instance, position);
    chain_revision_.store(plugin_chain_->revision());
    notifyOnUiThread([this](IAudioEngineListener& l) { l.onChainRevision(chain_revision_.load()); });
    return id;
}

InstanceId AudioEngineImpl::addPluginFromPath(const std::filesystem::path& vst3_path, int position)
{
    std::lock_guard lk {control_mutex_};

    LogDebug("addPluginFromPath: position=%d, path='%s'", position, vst3_path.string().c_str());

    std::shared_ptr<PluginInstance> instance;
    auto result = plugin_loader_->load(vst3_path);
    if (result && result.instance)
    {
        LogDebug("addPluginFromPath: Successfully loaded plugin instance from file");
        instance = std::shared_ptr<PluginInstance>(std::move(result.instance));

        // T055-fix: ensure manually-loaded plugins are discoverable by preset restore.
        if (scan_cache_)
        {
            PluginCatalogEntry entry;
            entry.ref.plugin_uid = HexStringToPluginUid(instance->descriptor().uid);
            entry.ref.vendor = instance->descriptor().vendor;
            entry.ref.name = instance->descriptor().name;
            entry.version = instance->descriptor().version;
            entry.file_path = vst3_path;
            entry.category = instance->descriptor().category;
            entry.has_editor = true;  // Simplified.
            entry.scan_timestamp = std::chrono::system_clock::now();
            scan_cache_->addEntry(entry);
            scan_cache_->save();
        }
    }
    else
    {
        if (!result.error.empty())
            LogDebug("addPluginFromPath: FAILED - %s", result.error.c_str());
        else
            LogDebug("addPluginFromPath: FAILED - unknown error (no instance returned)");
    }

    if (!instance)
    {
        LogDebug("addPluginFromPath: FAILED — not adding placeholder slot");
        std::string reason = result.error.empty() ? "Unknown error loading plugin from path" : result.error;
        notifyOnUiThread([reason](IAudioEngineListener& l) { l.onPluginFailed(InstanceId{}, reason); });
        return InstanceId{};
    }

    const auto id = plugin_chain_->addSlot(instance, position);
    chain_revision_.store(plugin_chain_->revision());
    notifyOnUiThread([this](IAudioEngineListener& l) { l.onChainRevision(chain_revision_.load()); });
    return id;
}

void AudioEngineImpl::removeSlot(int position)
{
    LogDebug("removeSlot: Attempting to remove position %d", position);
    std::lock_guard lk {control_mutex_};

    if (!plugin_chain_)
    {
        LogDebug("removeSlot: plugin_chain_ is null");
        return;
    }

    auto snapshot = plugin_chain_->snapshot();
    if (position < 0 || position >= static_cast<int>(snapshot.size()))
    {
        LogDebug("removeSlot: Position %d out of bounds (chain size: %zu)", position, snapshot.size());
        return;
    }

    plugin_chain_->removeSlot(position);
    chain_revision_.store(plugin_chain_->revision());
    notifyOnUiThread([this](IAudioEngineListener& l) { l.onChainRevision(chain_revision_.load()); });
}

void AudioEngineImpl::moveSlot(int from, int to)
{
    LogDebug("moveSlot: Attempting to move from %d to %d", from, to);
    std::lock_guard lk {control_mutex_};

    if (!plugin_chain_)
    {
        LogDebug("moveSlot: plugin_chain_ is null");
        return;
    }

    auto snapshot = plugin_chain_->snapshot();
    if (from < 0 || from >= static_cast<int>(snapshot.size()) || to < 0 ||
        to >= static_cast<int>(snapshot.size()))
    {
        LogDebug("moveSlot: Invalid positions (from=%d, to=%d, chain_size=%zu)",
                 from, to, snapshot.size());
        return;
    }

    if (from == to)
    {
        LogDebug("moveSlot: from and to are the same, no-op");
        return;
    }

    plugin_chain_->moveSlot(from, to);
    chain_revision_.store(plugin_chain_->revision());
    notifyOnUiThread([this](IAudioEngineListener& l) { l.onChainRevision(chain_revision_.load()); });
}

void AudioEngineImpl::setBypass(int position, bool bypassed)
{
    if (position < 0)
    {
        LogDebug("setBypass: Invalid position %d (negative)", position);
        return;
    }

    if (!plugin_chain_)
    {
        LogDebug("setBypass: plugin_chain_ is null");
        return;
    }

    auto snapshot = plugin_chain_->snapshot();
    if (position >= static_cast<int>(snapshot.size()))
    {
        LogDebug("setBypass: Position %d out of bounds (chain size: %zu)", position,
                 snapshot.size());
        return;
    }

    plugin_chain_->setBypass(position, bypassed);
    rebumpChain();
}

void AudioEngineImpl::setParameter(int position, ParamId param, float value)
{
    if (position < 0)
    {
        LogDebug("setParameter: Invalid position %d (negative)", position);
        return;
    }

    if (!plugin_chain_)
    {
        LogDebug("setParameter: plugin_chain_ is null");
        return;
    }

    auto snapshot = plugin_chain_->snapshot();
    if (position >= static_cast<int>(snapshot.size()))
    {
        LogDebug("setParameter: Position %d out of bounds (chain size: %zu)", position,
                 snapshot.size());
        return;
    }

    EngineCommand cmd;
    cmd.kind = EngineCommand::Kind::SetParameter;
    cmd.position = position;
    cmd.param_id = param;
    cmd.value = value;
    if (!command_queue_.tryPush(cmd))
    {
        LogDebug("setParameter: Failed to push command to queue (queue full)");
    }
}

void AudioEngineImpl::openEditor(int position)
{
    LogDebug("openEditor: Starting for position %d", position);

    if (position < 0)
    {
        LogDebug("openEditor: Invalid position %d (negative)", position);
        return;
    }

    std::shared_ptr<PluginInstance> instance;
    juce::AudioProcessor* proc = nullptr;
    std::string plugin_name;
    {
        std::lock_guard lk {control_mutex_};

        if (!plugin_chain_)
        {
            LogDebug("openEditor: plugin_chain_ is null");
            return;
        }

        auto snapshot = plugin_chain_->snapshot();
        if (position >= static_cast<int>(snapshot.size()))
        {
            LogDebug("openEditor: Position %d out of bounds (chain size: %zu)", position,
                     snapshot.size());
            return;
        }

        instance = plugin_chain_->getSlotInstance(position);

        if (!instance)
        {
            LogDebug("openEditor: No plugin instance at position %d", position);
            return;
        }

        proc = instance->processor();
        if (!proc)
        {
            LogDebug("openEditor: Plugin instance has no processor at position %d", position);
            return;
        }

        plugin_name = instance->descriptor().name;
    }

    // Remove any existing window for this slot.
    closeEditor(position);

    // Hold control_mutex_ for the entire editor-creation sequence.
    // This blocks the audio thread's processBlock for the duration, which prevents
    // a crash in ARC X (and possibly other IK plugins) that occurs when createView()
    // and processBlock access shared state concurrently without synchronization.
    // Skip hasEditor() — ARC X faults there too; null return is the "no editor" signal.
    std::lock_guard lk {control_mutex_};

    // Re-validate after releasing and re-acquiring lock
    if (!proc)
    {
        LogDebug("openEditor: Processor became null after lock release");
        return;
    }

    LogDebug("openEditor: Creating editor for '%s', hasEditor=%d", plugin_name.c_str(), proc->hasEditor());
    juce::AudioProcessorEditor* editor = nullptr;

    try
    {
        LogDebug("openEditor: About to call createEditor()...");
        editor = createEditorDRM(proc);
        LogDebug("openEditor: createEditor() returned %p", (void*)editor);
    }
    catch (const std::exception& e)
    {
        LogDebug("openEditor: Exception during editor creation: %s", e.what());
        return;
    }
    catch (...)
    {
        LogDebug("openEditor: Unknown exception during editor creation — exception 0x%08lX at 0x%llX",
                 (unsigned long)g_editor_exception_code,
                 (unsigned long long)g_editor_exception_addr);
        return;
    }

    if (!editor)
    {
        LogDebug("openEditor: createEditor returned nullptr — exception 0x%08lX at 0x%llX",
                 (unsigned long)g_editor_exception_code,
                 (unsigned long long)g_editor_exception_addr);
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Editor Error",
                                               "Failed to create editor for " + juce::String(plugin_name) +
                                                   "\nException code: 0x" + juce::String::toHexString((int)g_editor_exception_code));
        return;
    }

    LogDebug("openEditor: Editor created successfully, dimensions: %dx%d",
             editor->getWidth(), editor->getHeight());

    auto host = std::make_unique<PluginEditorHost>(juce::String(plugin_name));

    if (!attachEditorSafe(host.get(), editor))
    {
        LogDebug("openEditor: Plugin faulted in attachEditor (setContentOwned/isResizable) — discarding window");
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Editor Error",
                                               "Failed to attach editor for " + juce::String(plugin_name));
        return;
    }

    if (!showEditorSafe(host.get()))
    {
        LogDebug("openEditor: Plugin faulted while showing its editor — discarding window");
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Editor Error",
                                               "Failed to show editor for " + juce::String(plugin_name));
        return;
    }

    LogDebug("openEditor: PluginEditorHost created, visible, and brought to front");

    editor_windows_[position] = std::move(host);

    LogDebug("openEditor: Editor window stored in map for position %d", position);
}

void AudioEngineImpl::closeEditor(int position)
{
    LogDebug("closeEditor: Closing editor for position %d", position);
    
    std::lock_guard lk {control_mutex_};
    auto it = editor_windows_.find(position);
    if (it != editor_windows_.end())
    {
        LogDebug("closeEditor: Found existing editor window, removing");
        editor_windows_.erase(it);
    }
    else
    {
        LogDebug("closeEditor: No editor window found for position %d", position);
    }
}

void AudioEngineImpl::repointPlaceholder(int position, const PluginRef& ref)
{
    LogDebug("repointPlaceholder: Starting for position %d", position);

    std::lock_guard lk {control_mutex_};

    if (position < 0)
    {
        LogDebug("repointPlaceholder: Invalid position %d (negative)", position);
        return;
    }

    if (!plugin_chain_)
    {
        LogDebug("repointPlaceholder: plugin_chain_ is null");
        return;
    }

    auto snapshot = plugin_chain_->snapshot();
    if (position >= static_cast<int>(snapshot.size()))
    {
        LogDebug("repointPlaceholder: Position %d out of bounds (chain size: %zu)", position,
                 snapshot.size());
        return;
    }

    auto placeholder = plugin_chain_->getSlotPlaceholder(position);
    if (!placeholder)
    {
        LogDebug("repointPlaceholder: No placeholder at position %d", position);
        return;
    }

    if (!scan_cache_)
    {
        LogDebug("repointPlaceholder: scan_cache_ is null");
        notifyOnUiThread([](IAudioEngineListener& l) {
            l.onPluginFailed(InstanceId{}, "Re-point failed: scan cache unavailable");
        });
        return;
    }

    if (!plugin_loader_)
    {
        LogDebug("repointPlaceholder: plugin_loader_ is null");
        notifyOnUiThread([](IAudioEngineListener& l) {
            l.onPluginFailed(InstanceId{}, "Re-point failed: plugin loader unavailable");
        });
        return;
    }

    const auto entry = scan_cache_->findByRef(ref);
    if (!entry)
    {
        LogDebug("repointPlaceholder: Plugin not found in scan cache");
        notifyOnUiThread([](IAudioEngineListener& l) {
            l.onPluginFailed(InstanceId{}, "Re-point failed: plugin not in scan cache");
        });
        return;
    }

    auto result = plugin_loader_->load(entry->file_path);
    if (!result || !result.instance)
    {
        LogDebug("repointPlaceholder: Plugin failed to load from %s", entry->file_path.string().c_str());
        notifyOnUiThread([](IAudioEngineListener& l) {
            l.onPluginFailed(InstanceId{}, "Re-point failed: plugin failed to load");
        });
        return;
    }

    auto instance = std::shared_ptr<PluginInstance>(std::move(result.instance));

    // Apply pending state chunk if present.
    if (!placeholder->pendingStateChunk().isEmpty())
    {
        auto* proc = instance->processor();
        if (proc)
        {
            try
            {
                proc->setStateInformation(placeholder->pendingStateChunk().getData(),
                                         static_cast<int>(placeholder->pendingStateChunk().getSize()));
            }
            catch (const std::exception& e)
            {
                LogDebug("repointPlaceholder: Failed to restore state: %s", e.what());
            }
            catch (...)
            {
                LogDebug("repointPlaceholder: Unknown exception restoring state");
            }
        }
    }

    instance->setBypass(placeholder->isBypassed());

    // Replace placeholder with plugin: remove placeholder, insert plugin at same position.
    plugin_chain_->removeSlot(position);
    plugin_chain_->addSlot(instance, position);
    chain_revision_.store(plugin_chain_->revision());
    notifyOnUiThread([this](IAudioEngineListener& l) { l.onChainRevision(chain_revision_.load()); });

    LogDebug("repointPlaceholder: Successfully replaced placeholder at position %d", position);
}

void AudioEngineImpl::loadPreset(const std::filesystem::path& path)
{
    // File size guard.
    {
        std::error_code ec;
        const auto size = std::filesystem::file_size(path, ec);
        if (ec)
        {
            notifyOnUiThread([](IAudioEngineListener& l) {
                l.onPluginFailed(InstanceId{}, "Preset load failed: cannot read file");
            });
            return;
        }
        if (size > 52428800)
        {
            notifyOnUiThread([](IAudioEngineListener& l) {
                l.onPluginFailed(InstanceId{}, "Preset load failed: file exceeds 50 MB");
            });
            return;
        }
    }

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs)
    {
        notifyOnUiThread([](IAudioEngineListener& l) {
            l.onPluginFailed(InstanceId{}, "Preset load failed: cannot open file");
        });
        return;
    }

    nlohmann::json doc;
    try
    {
        ifs >> doc;
    }
    catch (const std::exception& e)
    {
        notifyOnUiThread([&e](IAudioEngineListener& l) {
            l.onPluginFailed(InstanceId{}, std::string("Preset load failed: invalid JSON (") + e.what() + ")");
        });
        return;
    }

    auto errors = validatePresetDocument(doc, 0);
    if (!errors.empty())
    {
        notifyOnUiThread([errors](IAudioEngineListener& l) {
            l.onPluginFailed(InstanceId{}, "Preset load failed: " + errors[0].message);
        });
        return;
    }

    // Stop audio before rebuilding the chain so that setStateInformation (applied
    // via the pending-chunk mechanism in prepareToPlay) is never called concurrently
    // with processBlock. This also guarantees that prepareToPlay runs AFTER all
    // state chunks are stored — the correct VST3 activation order.
    stop();

    std::lock_guard lk {control_mutex_};

    // Restore buffer size from preset if present.
    if (doc.contains("target_buffer_size") && doc["target_buffer_size"].is_number_integer())
    {
        int bs = doc["target_buffer_size"].get<int>();
        if (bs == 32 || bs == 64 || bs == 128 || bs == 256 || bs == 512 || bs == 1024)
        {
            setBufferSize(bs);
        }
    }

    // Restore sample rate from preset if present.
    if (doc.contains("target_sample_rate") && !doc["target_sample_rate"].is_null())
    {
        if (doc["target_sample_rate"].is_number_integer())
        {
            int sr = doc["target_sample_rate"].get<int>();
            if (sr == 44100 || sr == 48000 || sr == 88200 || sr == 96000 || sr == 176400 || sr == 192000)
            {
                setSampleRate(static_cast<double>(sr));
            }
        }
    }

    // Restore endpoints if present.
    if (doc.contains("input_endpoint_id"))
    {
        auto id_json = doc["input_endpoint_id"];
        if (id_json.is_string())
        {
            std::string id = id_json.get<std::string>();
            if (!id.empty())
                selectInput(id);
        }
    }
    if (doc.contains("output_endpoint_id"))
    {
        auto id_json = doc["output_endpoint_id"];
        if (id_json.is_string())
        {
            std::string id = id_json.get<std::string>();
            if (!id.empty())
                selectOutput(id);
        }
    }

    // Clear existing chain.
    while (plugin_chain_->snapshot().size() > 0)
    {
        plugin_chain_->removeSlot(0);
    }

    std::vector<MissingPluginInfo> missing;
    loadChainFromJson(doc, missing);

    chain_revision_.store(plugin_chain_->revision());
    notifyOnUiThread([this](IAudioEngineListener& l) { l.onChainRevision(chain_revision_.load()); });
    if (!missing.empty())
    {
        notifyOnUiThread([missing](IAudioEngineListener& l) { l.onPresetPartialLoad(missing); });
    }

    // Restore audio running state from preset.
    if (doc.contains("audio_running") && doc["audio_running"].is_boolean())
    {
        if (doc["audio_running"].get<bool>())
        {
            start();
        }
    }
}

void AudioEngineImpl::loadChainFromJson(const nlohmann::json& doc,
                                         std::vector<MissingPluginInfo>& out_missing)
{
    LogDebug("loadChainFromJson() start");
    if (!doc.contains("slots") || !doc["slots"].is_array())
    {
        LogDebug("loadChainFromJson() no slots array");
        return;
    }

    const auto& slots = doc["slots"];
    for (std::size_t i = 0; i < slots.size(); ++i)
    {
        const auto& slot = slots[i];
        PluginRef ref;
        ref.plugin_uid = HexStringToPluginUid(slot.value("plugin_uid", ""));
        ref.vendor = slot.value("plugin_vendor", "");
        ref.name = slot.value("plugin_name", "");
        bool is_bypassed = slot.value("is_bypassed", false);
        std::string path_hint = slot.value("plugin_path_hint", "");
        std::string b64 = slot.value("state_chunk_b64", "");
        auto decoded = base64Decode(b64);
        juce::MemoryBlock chunk(decoded.data(), decoded.size());

        bool resolved = false;

        // 1) Try built-in registry first (data-model.md §6).
        if (builtin_registry_ && builtin_registry_->isBuiltin(ref))
        {
            auto builtin_proc = builtin_registry_->create(ref);
            if (builtin_proc)
            {
                Plugin descriptor;
                auto entry = builtin_registry_->findByRef(ref);
                if (entry)
                {
                    descriptor.uid = PluginUidToHexString(entry->ref.plugin_uid);
                    descriptor.vendor = entry->ref.vendor;
                    descriptor.name = entry->ref.name;
                    descriptor.category = entry->category;
                    descriptor.version = entry->version;
                    descriptor.file_path.clear();
                }
                auto instance = std::make_shared<PluginInstance>(descriptor, std::move(builtin_proc));
                if (!chunk.isEmpty())
                {
                    // Built-in processors keep their state in plain members that
                    // prepareToPlay does not reset, so apply the restored chunk
                    // immediately instead of deferring it to the next prepareToPlay
                    // (audio start). Deferring caused settings to be lost: the ~2s
                    // periodic autosave would read the still-default live state and
                    // overwrite the persisted chunk before audio ever started.
                    if (auto* proc = instance->processor())
                        proc->setStateInformation(chunk.getData(), static_cast<int>(chunk.getSize()));
                }
                instance->setBypass(is_bypassed);
                plugin_chain_->addSlot(instance, static_cast<int>(i));
                resolved = true;
                LogDebug("loadChainFromJson: resolved built-in '%s' at slot %zu", ref.name.c_str(), i);
            }
        }

        // 2) Fall back to scan cache / disk loader.
        if (!resolved)
        {
            const auto entry = scan_cache_->findByRef(ref);
            if (entry)
            {
                auto result = plugin_loader_->load(entry->file_path);
                if (result && result.instance)
                {
                    auto instance = std::shared_ptr<PluginInstance>(std::move(result.instance));
                    if (!chunk.isEmpty())
                        instance->setPendingStateChunk(chunk);
                    instance->setBypass(is_bypassed);
                    plugin_chain_->addSlot(instance, static_cast<int>(i));
                }
                else
                {
                    auto placeholder = std::make_shared<PlaceholderInstance>(ref, std::filesystem::path(path_hint));
                    placeholder->setBypass(is_bypassed);
                    if (!chunk.isEmpty())
                        placeholder->setPendingStateChunk(chunk);
                    plugin_chain_->addPlaceholderSlot(std::move(placeholder), static_cast<int>(i));
                    out_missing.push_back({ref, static_cast<int>(i)});
                }
            }
            else
            {
                bool loaded_from_hint = false;
                if (!path_hint.empty() && std::filesystem::exists(path_hint))
                {
                    auto hint_result = plugin_loader_->load(path_hint);
                    if (hint_result && hint_result.instance)
                    {
                        auto instance = std::shared_ptr<PluginInstance>(std::move(hint_result.instance));
                        if (!chunk.isEmpty())
                            instance->setPendingStateChunk(chunk);
                        instance->setBypass(is_bypassed);
                        plugin_chain_->addSlot(instance, static_cast<int>(i));
                        loaded_from_hint = true;

                        if (scan_cache_)
                        {
                            PluginCatalogEntry cache_entry;
                            cache_entry.ref.plugin_uid = HexStringToPluginUid(instance->descriptor().uid);
                            cache_entry.ref.vendor = instance->descriptor().vendor;
                            cache_entry.ref.name = instance->descriptor().name;
                            cache_entry.version = instance->descriptor().version;
                            cache_entry.file_path = path_hint;
                            cache_entry.category = instance->descriptor().category;
                            cache_entry.has_editor = true;
                            cache_entry.scan_timestamp = std::chrono::system_clock::now();
                            scan_cache_->addEntry(cache_entry);
                            scan_cache_->save();
                        }
                    }
                }

                if (!loaded_from_hint)
                {
                    auto placeholder = std::make_shared<PlaceholderInstance>(ref, std::filesystem::path(path_hint));
                    placeholder->setBypass(is_bypassed);
                    if (!chunk.isEmpty())
                        placeholder->setPendingStateChunk(chunk);
                    plugin_chain_->addPlaceholderSlot(std::move(placeholder), static_cast<int>(i));
                    out_missing.push_back({ref, static_cast<int>(i)});
                }
            }
        }
    }
    LogDebug("loadChainFromJson() done, restored_slots=%zu, missing=%zu", doc["slots"].size(), out_missing.size());
}

void AudioEngineImpl::restoreChain(const std::filesystem::path& path)
{
    LogDebug("restoreChain() called: %s", path.string().c_str());
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs)
    {
        LogDebug("restoreChain() failed: cannot open file");
        return;
    }

    nlohmann::json doc;
    try
    {
        ifs >> doc;
    }
    catch (const std::exception&)
    {
        return;
    }

    auto errors = validatePresetDocument(doc, 0);
    if (!errors.empty())
    {
        LogDebug("restoreChain() failed: validation errors");
        return;
    }

    LogDebug("restoreChain() stopping audio before restore");
    std::lock_guard lk{control_mutex_};

    while (plugin_chain_->snapshot().size() > 0)
        plugin_chain_->removeSlot(0);

    std::vector<MissingPluginInfo> missing;
    loadChainFromJson(doc, missing);

    chain_revision_.store(plugin_chain_->revision());
    notifyOnUiThread([this](IAudioEngineListener& l) { l.onChainRevision(chain_revision_.load()); });
    if (!missing.empty())
        notifyOnUiThread([missing](IAudioEngineListener& l) { l.onPresetPartialLoad(missing); });
    LogDebug("restoreChain() completed, slots=%zu, missing=%zu", plugin_chain_->snapshot().size(), missing.size());
}

void AudioEngineImpl::savePreset(const std::filesystem::path& path, const std::string& name)
{
    std::lock_guard lk {control_mutex_};

    int sr = negotiated_sample_rate_.load();
    std::string device_name = desired_output_friendly_name_;

    auto doc = serializePreset(*plugin_chain_, name, desired_buffer_size_, sr, device_name,
                               desired_input_id_, desired_output_id_, running_.load());

    std::ofstream ofs(path, std::ios::binary);
    if (ofs)
    {
        ofs << doc.dump(2);
    }
    else
    {
        notifyOnUiThread([](IAudioEngineListener& l) {
            l.onPluginFailed(InstanceId{}, "Preset save failed: cannot write file");
        });
    }
}

void AudioEngineImpl::setAsioOutputPair(int channel_offset)
{
    {
        std::lock_guard lk {control_mutex_};
        if (desired_asio_output_pair_ == channel_offset)
        {
            return;
        }
        desired_asio_output_pair_ = channel_offset;
    }
    if (running_.load())
    {
        std::lock_guard lk {control_mutex_};
        if (desired_output_transport_kind_ == TransportKind::Asio)
            applyAsioTransport();
    }
}

int AudioEngineImpl::asioOutputPair() const
{
    std::lock_guard lk {control_mutex_};
    return desired_asio_output_pair_;
}

void AudioEngineImpl::openAsioControlPanel()
{
    auto* device = device_manager_.getCurrentAudioDevice();
    if (device == nullptr)
        return;
    device->showControlPanel();
    if (running_.load())
    {
        stop();
        start();
    }
}

void AudioEngineImpl::setWasapiExclusive(bool exclusive)
{
    std::lock_guard lk {control_mutex_};
    if (wasapi_exclusive_ == exclusive)
        return;
    wasapi_exclusive_ = exclusive;
    if (desired_output_transport_kind_ != TransportKind::Wasapi)
        return;

    const bool wasapi_loopback_mode = desired_input_transport_kind_ == TransportKind::Wasapi &&
                                       !desired_input_id_.empty();
    if (running_.load() && wasapi_loopback_mode)
    {
        // Pure WASAPI-loopback mode opens capture/output directly (not via
        // AudioDeviceManager), so applyDeviceSelection() alone would not
        // reopen them. Go through a full stop/start instead.
        stop();
        start();
    }
    else
    {
        applyDeviceSelection();
    }
}

bool AudioEngineImpl::wasapiExclusive() const
{
    std::lock_guard lk {control_mutex_};
    return wasapi_exclusive_;
}

LatencyProfile AudioEngineImpl::latencyProfile() const
{
    LatencyProfile p;
    auto* device = device_manager_.getCurrentAudioDevice();

    // Prefer live WASAPI render latency when a real output stream is active
    // (pure WASAPI loopback mode or mixed mode), otherwise fall back to JUCE device.
    if (wasapi_output_ != nullptr)
    {
        p.output_ms = wasapi_output_->latencyMs();
    }
    else if (device != nullptr)
    {
        const double sr = device->getCurrentSampleRate();
        const int out_latency = device->getOutputLatencyInSamples();
        if (sr > 0.0)
        {
            p.output_ms = static_cast<float>(out_latency * 1000.0 / sr);
        }
    }

    // Prefer live WASAPI capture latency when a real capture stream is active,
    // otherwise fall back to JUCE device.
    if (wasapi_capture_ != nullptr)
    {
        p.capture_ms = wasapi_capture_->latencyMs();
    }
    else if (device != nullptr)
    {
        const double sr = device->getCurrentSampleRate();
        const int in_latency = device->getInputLatencyInSamples();
        if (sr > 0.0)
        {
            p.capture_ms = static_cast<float>(in_latency * 1000.0 / sr);
        }
    }

    // T021: Resampling latency is negligible for windowed sinc resampler (< 1 ms)
    // plugin_chain_ms is managed by plugin_chain_ on demand
    p.total_round_trip_ms = p.capture_ms + p.resample_ms + p.plugin_chain_ms + p.output_ms;
    p.last_updated = std::chrono::steady_clock::now();
    return p;
}

CpuStats AudioEngineImpl::cpuStats() const
{
    CpuStats s;
    s.instantaneous_pct = instantaneous_cpu_pct_.load();
    s.rolling_1s_pct = s.instantaneous_pct;
    s.xrun_count_session = xrun_count_.load();
    s.warning_active = s.rolling_1s_pct >= 5.0f;
    return s;
}

MeterFrame AudioEngineImpl::latestMeterFrame() const
{
    MeterFrame f;
    f.input_peak_l = meter_input_peak_l_.load();
    f.input_peak_r = meter_input_peak_r_.load();
    f.input_rms_l = meter_input_rms_l_.load();
    f.input_rms_r = meter_input_rms_r_.load();
    f.output_peak_l = meter_output_peak_l_.load();
    f.output_peak_r = meter_output_peak_r_.load();
    f.output_rms_l = meter_output_rms_l_.load();
    f.output_rms_r = meter_output_rms_r_.load();
    f.timestamp = std::chrono::steady_clock::now();
    return f;
}

std::vector<float> AudioEngineImpl::pluginOutputPeaks() const
{
    std::vector<float> peaks;
    std::vector<float> rms;
    if (plugin_chain_)
        plugin_chain_->readSlotOutputLevels(peaks, rms);
    return peaks;
}

std::vector<float> AudioEngineImpl::pluginOutputRms() const
{
    std::vector<float> peaks;
    std::vector<float> rms;
    if (plugin_chain_)
        plugin_chain_->readSlotOutputLevels(peaks, rms);
    return rms;
}

void AudioEngineImpl::injectTestCatalogEntry(const PluginCatalogEntry& entry)
{
    std::lock_guard lk {control_mutex_};
    scan_cache_->addEntry(entry);
}

juce::AudioProcessor* AudioEngineImpl::testGetProcessor(int position) const
{
    std::lock_guard lk {control_mutex_};
    if (!plugin_chain_)
        return nullptr;
    auto instance = plugin_chain_->getSlotInstance(position);
    return instance ? instance->processor() : nullptr;
}

InstanceId AudioEngineImpl::addPlaceholderSlot(int position)
{
    std::lock_guard lk {control_mutex_};
    auto placeholder = std::make_shared<PlaceholderInstance>(PluginRef{}, std::filesystem::path{});
    const auto id = plugin_chain_->addPlaceholderSlot(std::move(placeholder), position);
    chain_revision_.store(plugin_chain_->revision());
    notifyOnUiThread([this](IAudioEngineListener& l) { l.onChainRevision(chain_revision_.load()); });
    return id;
}

// =====================================================================
// Audio thread (juce::AudioIODeviceCallback)
// =====================================================================

void AudioEngineImpl::audioDeviceIOCallbackWithContext(const float* const* input_channel_data,
                                                      int num_input_channels,
                                                      float* const* output_channel_data,
                                                      int num_output_channels,
                                                      int num_samples,
                                                      const juce::AudioIODeviceCallbackContext& /*context*/)
{
    const auto t_start = rt_clock_.now();

    // 1. Drain pending parameter commands and apply to plugin chain.
    EngineCommand cmd;
    while (command_queue_.tryPop(cmd))
    {
        switch (cmd.kind)
        {
        case EngineCommand::Kind::SetParameter:
            plugin_chain_->setParameter(cmd.position, cmd.param_id, cmd.value);
            break;
        case EngineCommand::Kind::SetMasterVolume:
            master_volume_.store(cmd.value, std::memory_order_relaxed);
            break;
        default:
            break;
        }
    }

    // 2. Acquire input samples.
    // The chain always runs at the output device rate, so vst_samples equals
    // num_samples in all modes. Capture-side resampling handles any input mismatch.
    const int vst_samples = num_samples;
    const int samples = std::min(num_samples, work_buffer_.getNumSamples());
    if (wasapi_capture_ != nullptr && capture_ring_buffer_ != nullptr)
    {
        std::size_t avail = capture_ring_buffer_->available();

        // Fixed-ratio read from the WASAPI-capture ring. Resampling applies only
        // when capture and output rates differ.
        const std::size_t needed =
            static_cast<std::size_t>(std::ceil(static_cast<double>(vst_samples) * capture_nominal_ratio_)) + 2;
        const std::size_t raw_cap = static_cast<std::size_t>(capture_raw_buffer_.getNumSamples());
        const std::size_t peek_n = std::min({needed, avail, raw_cap});

        float* raw[2] = {
            capture_raw_buffer_.getWritePointer(0),
            capture_raw_buffer_.getWritePointer(1)};
        const std::size_t peeked = capture_ring_buffer_->peek(raw, peek_n);

        const float* src[2] = {raw[0], raw[1]};
        float* dst[2] = {
            work_buffer_.getWritePointer(0),
            work_buffer_.getNumChannels() > 1 ? work_buffer_.getWritePointer(1) : nullptr};

        std::size_t consumed = 0;
        if (capture_resampling_enabled_ && std::abs(capture_nominal_ratio_ - 1.0) > 0.001)
        {
            capture_resampler_.processAdaptive(capture_nominal_ratio_, src, dst,
                                               static_cast<std::size_t>(vst_samples), peeked, &consumed);
        }
        else
        {
            consumed = std::min(peeked, static_cast<std::size_t>(vst_samples));
            for (int c = 0; c < 2; ++c)
            {
                if (dst[c] != nullptr && src[c] != nullptr && consumed > 0)
                    std::memcpy(dst[c], src[c], consumed * sizeof(float));
                if (dst[c] != nullptr && consumed < static_cast<std::size_t>(vst_samples))
                    std::memset(dst[c] + consumed, 0, (vst_samples - consumed) * sizeof(float));
            }
        }
        capture_ring_buffer_->advanceRead(consumed);

        if (peeked < needed && capture_resampling_enabled_)
        {
            xrun_count_.fetch_add(1, std::memory_order_relaxed);
        }
    }
    else
    {
        const int channels = std::min(num_input_channels, work_buffer_.getNumChannels());
        for (int c = 0; c < channels; ++c)
        {
            if (input_channel_data[c] != nullptr)
            {
                std::memcpy(work_buffer_.getWritePointer(c), input_channel_data[c],
                            static_cast<std::size_t>(samples) * sizeof(float));
            }
        }
        for (int c = channels; c < work_buffer_.getNumChannels(); ++c)
        {
            work_buffer_.clear(c, 0, samples);
        }
    }

    // Meters only need to be current when we actually push a frame to the UI (~30 Hz).
    // Gate the per-block peak/RMS scans to those dispatch blocks so the audio thread
    // does ~1/kMeterThrottleDivisor of the metering work. getMagnitude() is SIMD-accelerated
    // (juce::FloatVectorOperations) which is why we prefer it over a hand-rolled loop.
    ++meter_callback_counter_;
    const bool do_meter = (meter_callback_counter_ % kMeterThrottleDivisor == 0);

    // 2a. Compute input peak / RMS (channels 0 and 1).
    if (do_meter)
    {
        if (work_buffer_.getNumChannels() > 0 && vst_samples > 0)
        {
            meter_input_peak_l_.store(work_buffer_.getMagnitude(0, 0, vst_samples));
            meter_input_rms_l_.store(work_buffer_.getRMSLevel(0, 0, vst_samples));
        }
        if (work_buffer_.getNumChannels() > 1 && vst_samples > 0)
        {
            meter_input_peak_r_.store(work_buffer_.getMagnitude(1, 0, vst_samples));
            meter_input_rms_r_.store(work_buffer_.getRMSLevel(1, 0, vst_samples));
        }
    }

    // 3. Process through plugin chain. Energy Saver: while sleeping we skip the
    //    CPU-heavy chain entirely and emit silence. The input peak/RMS above is
    //    still computed every metering block, so the energy-saver thread keeps
    //    seeing the true input level and resumes the instant audio returns.
    if (energy_saver_sleeping_.load(std::memory_order_relaxed))
    {
        work_buffer_.clear();
    }
    else
    {
        juce::MidiBuffer empty_midi;
        plugin_chain_->processBlock(work_buffer_, empty_midi);
    }

    // 3a. Apply master volume scalar.
    {
        const float gain = master_volume_.load(std::memory_order_relaxed);
        if (gain != 1.0f)
        {
            for (int c = 0; c < work_buffer_.getNumChannels(); ++c)
            {
                float* ptr = work_buffer_.getWritePointer(c);
                for (int s = 0; s < vst_samples; ++s)
                    ptr[s] *= gain;
            }
        }
    }

    // 4. Copy work buffer → output.
    if (wasapi_output_ == nullptr)
    {
        const int out_channels = std::min(num_output_channels, work_buffer_.getNumChannels());
        for (int c = 0; c < out_channels; ++c)
        {
            if (output_channel_data[c] != nullptr)
            {
                std::memcpy(output_channel_data[c], work_buffer_.getReadPointer(c),
                            static_cast<std::size_t>(num_samples) * sizeof(float));
            }
        }
        for (int c = out_channels; c < num_output_channels; ++c)
        {
            if (output_channel_data[c] != nullptr)
            {
                std::memset(output_channel_data[c], 0, static_cast<std::size_t>(num_samples) * sizeof(float));
            }
        }
    }

    // T017: Feed processed audio to WasapiOutput ring buffer.
    if (wasapi_output_ != nullptr && output_ring_buffer_ != nullptr)
    {
        const float* src_ptrs[2] = {
            work_buffer_.getReadPointer(0),
            work_buffer_.getNumChannels() > 1 ? work_buffer_.getReadPointer(1) : nullptr
        };
        const std::size_t written = output_ring_buffer_->tryWrite(
            src_ptrs, static_cast<std::size_t>(vst_samples));
        (void)written;
    }

    if (do_meter && vst_samples > 0)
    {
        const int out_channels = std::min(num_output_channels, work_buffer_.getNumChannels());
        if (out_channels > 0 && (wasapi_output_ != nullptr || output_channel_data[0] != nullptr))
        {
            meter_output_peak_l_.store(work_buffer_.getMagnitude(0, 0, vst_samples));
            meter_output_rms_l_.store(work_buffer_.getRMSLevel(0, 0, vst_samples));
        }
        if (out_channels > 1 && (wasapi_output_ != nullptr || output_channel_data[1] != nullptr))
        {
            meter_output_peak_r_.store(work_buffer_.getMagnitude(1, 0, vst_samples));
            meter_output_rms_r_.store(work_buffer_.getRMSLevel(1, 0, vst_samples));
        }
    }

    // 5. CPU utilization snapshot.
    const auto t_end = rt_clock_.now();
    const double elapsed_ms = rt_clock_.deltaToMs(t_start, t_end);
    const double sr = device_manager_.getCurrentAudioDevice() != nullptr
                          ? device_manager_.getCurrentAudioDevice()->getCurrentSampleRate()
                          : desired_sample_rate_;
    if (sr > 0.0 && num_samples > 0)
    {
        const double budget_ms = (static_cast<double>(num_samples) * 1000.0) / sr;
        if (budget_ms > 0.0)
        {
            instantaneous_cpu_pct_.store(static_cast<float>(elapsed_ms / budget_ms * 100.0));
        }
    }

    // 6. Throttled meter frame push to listener (~30 Hz). Uses the same cadence as the
    //    metering scans above (do_meter), so the frame reflects the just-computed values.
    if (do_meter)
    {
        MeterFrame frame{
            meter_input_peak_l_.load(),
            meter_input_peak_r_.load(),
            meter_input_rms_l_.load(),
            meter_input_rms_r_.load(),
            meter_output_peak_l_.load(),
            meter_output_peak_r_.load(),
            meter_output_rms_l_.load(),
            meter_output_rms_r_.load(),
            std::chrono::steady_clock::now()
        };
        notifyOnUiThread([frame](IAudioEngineListener& l) { l.onMeterFrame(frame); });
    }
}

void AudioEngineImpl::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    LogDebug("audioDeviceAboutToStart() called");
    if (device != nullptr)
    {
        const double device_rate = device->getCurrentSampleRate();
        juce_device_rate_ = device_rate;
        const int channels = std::max(2, device->getActiveOutputChannels().countNumberOfSetBits());
        const int block = device->getCurrentBufferSizeSamples();

        work_buffer_.setSize(channels, block, false, false, true);
        plugin_chain_->prepareToPlay(device_rate, block);
        negotiated_sample_rate_.store(static_cast<int>(device_rate));
        LogDebug("audioDeviceAboutToStart() device_sr=%.0f ch=%d block=%d", device_rate, channels, block);

        // Set up capture-side resampler when capture rate differs from device rate.
        const double wasapi_rate = capture_wasapi_rate_ > 0.0 ? capture_wasapi_rate_ : device_rate;
        capture_resampling_enabled_ = wasapi_rate > 0.0 && device_rate > 0.0 &&
                                      std::abs(wasapi_rate - device_rate) > 0.1;

        if (capture_resampling_enabled_)
        {
            capture_nominal_ratio_ = wasapi_rate / device_rate;
            xrun_count_.store(0, std::memory_order_relaxed);

            const int raw_max = static_cast<int>(std::ceil(
                                    static_cast<double>(block) * capture_nominal_ratio_)) + 8;
            capture_resampler_.prepare(capture_nominal_ratio_, static_cast<std::size_t>(raw_max), 2);
            capture_raw_buffer_.setSize(2, raw_max, false, true, true);

            LogDebug("audioDeviceAboutToStart() capture SRC: wasapi=%.0f device=%.0f ratio=%.6f",
                     wasapi_rate, device_rate, capture_nominal_ratio_);
        }
        else
        {
            capture_raw_buffer_.setSize(0, 0);
        }
    }
    LogDebug("audioDeviceAboutToStart() completed");
}

void AudioEngineImpl::audioDeviceStopped()
{
    work_buffer_.setSize(0, 0);
    plugin_chain_->releaseResources();
}

void AudioEngineImpl::audioDeviceError(const juce::String& /*error_message*/)
{
    xrun_count_.fetch_add(1);
}

// =====================================================================
// Internal helpers (UI thread)
// =====================================================================

void AudioEngineImpl::applyDeviceSelection()
{
    // Ensure we are on the correct WASAPI device type (shared vs. exclusive).
    device_manager_.setCurrentAudioDeviceType(
        wasapi_exclusive_ ? "Windows Audio (Exclusive Mode)" : "Windows Audio", true);

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    device_manager_.getAudioDeviceSetup(setup);

    if (!desired_input_id_.empty())
    {
        const auto input_list = endpoint_enum_.list(shared::EndpointFlow::Capture);
        const auto& name = nameForEndpoint(input_list, desired_input_id_);
        if (!name.empty())
        {
            setup.inputDeviceName = juce::String(name);
        }
    }

    // FR-022m: device resolution priority chain.
    // 1. endpoint ID match, 2. friendly name match, 3. Windows default.
    if (!desired_output_id_.empty())
    {
        const auto output_list = endpoint_enum_.list(shared::EndpointFlow::Render);
        const auto& name = nameForEndpoint(output_list, desired_output_id_);
        if (!name.empty())
        {
            setup.outputDeviceName = juce::String(name);
            resolution_source_ = DeviceResolutionSource::EndpointIdMatch;
        }
        else if (!desired_output_friendly_name_.empty())
        {
            bool matched = false;
            for (const auto& d : output_list)
            {
                if (d.friendly_name == desired_output_friendly_name_)
                {
                    setup.outputDeviceName = juce::String(d.friendly_name);
                    resolution_source_ = DeviceResolutionSource::FriendlyNameMatch;
                    matched = true;
                    break;
                }
            }
            if (!matched)
            {
                resolution_source_ = DeviceResolutionSource::WindowsDefaultFallback;
            }
        }
        else
        {
            resolution_source_ = DeviceResolutionSource::WindowsDefaultFallback;
        }
    }

    setup.bufferSize = desired_buffer_size_;
    // Explicitly (re-)request the rate rather than leaving whatever
    // getAudioDeviceSetup() inherited above: JUCE caches the last setup per
    // device *type* (lastDeviceTypeConfigs), so after a WASAPI -> ASIO -> WASAPI
    // round trip this can otherwise silently carry a now-stale sample rate
    // forward if that rate happens to still be in the device's supported list.
    setup.sampleRate = desired_sample_rate_ > 0.0 ? desired_sample_rate_ : 48000.0;
    setup.useDefaultInputChannels = true;
    setup.useDefaultOutputChannels = true;

    auto err = device_manager_.setAudioDeviceSetup(setup, true);

    // Safety net: WASAPI exclusive mode can fail to initialise on some devices/formats.
    // Rather than leaving the engine in a broken state, fall back to shared WASAPI and
    // retry once (keeping the resolved device names) so the app keeps producing audio.
    if (err.isNotEmpty() && wasapi_exclusive_)
    {
        wasapi_exclusive_ = false;
        device_manager_.setCurrentAudioDeviceType("Windows Audio", true);
        err = device_manager_.setAudioDeviceSetup(setup, true);
    }

    if (err.isNotEmpty())
    {
        // Bubble up to the listener on the UI thread.
        const juce::String captured = err;
        notifyOnUiThread([captured](IAudioEngineListener& l) {
            l.onPluginFailed(InstanceId {}, captured.toStdString());
        });
    }
}

void AudioEngineImpl::rebumpChain()
{
    const int new_rev = chain_revision_.fetch_add(1) + 1;
    notifyOnUiThread([new_rev](IAudioEngineListener& l) { l.onChainRevision(new_rev); });
}

void AudioEngineImpl::notifyOnUiThread(std::function<void(IAudioEngineListener&)> fn)
{
    if (listener_ == nullptr || !fn)
    {
        return;
    }
    auto* listener = listener_;
    // If no MessageManager exists yet, or we're already on the message thread,
    // call directly. This makes integration tests reliable without a running
    // dispatch loop, while still deferring to the async queue in a real GUI app.
    if (juce::MessageManager::getInstanceWithoutCreating() == nullptr ||
        juce::MessageManager::existsAndIsCurrentThread())
    {
        fn(*listener);
    }
    else
    {
        juce::MessageManager::callAsync([listener, f = std::move(fn)]() mutable { f(*listener); });
    }
}

void AudioEngineImpl::applyAsioTransport()
{
    if (desired_asio_device_name_.empty())
    {
        return;
    }

    SessionConfig asio_config;
    asio_config.buffer_size = desired_buffer_size_;
    asio_config.sample_rate = desired_sample_rate_;
    asio_config.input_channels = mixed_mode_active_.load(std::memory_order_acquire) ? 0 : 2;
    asio_config.output_channels = desired_asio_output_pair_ + 2;
    asio_transport_->setPreferred(asio_config);
    const auto report = asio_transport_->open(&device_manager_, desired_asio_device_name_);
    if (!report.error.empty())
    {
        fallback_reason_ = report.error;
        LogDebug("ASIO open failed for '%s': %s — falling back to WASAPI",
                 desired_asio_device_name_.c_str(), report.error.c_str());
        // Surface the fallback so the user knows they're on WASAPI, not ASIO.
        // The tray UI treats onDeviceLost as informational (status label +
        // device-list refresh); it does not auto-reconnect or restart.
        const EndpointId lost = desired_asio_device_name_;
        const EndpointId fallback_to = desired_output_id_;
        notifyOnUiThread([lost, fallback_to](IAudioEngineListener& l) {
            l.onDeviceLost(lost, fallback_to);
        });
        fallbackToWasapi();
        applyDeviceSelection();
        return;
    }

    transport_kind_ = TransportKind::Asio;
}

bool AudioEngineImpl::openAsioTransport(const EndpointId& id)
{
    const auto report = asio_transport_->open(&device_manager_, id);
    if (!report.error.empty())
    {
        fallback_reason_ = report.error;
        fallbackToWasapi();
        return false;
    }

    transport_kind_ = TransportKind::Asio;
    return true;
}

void AudioEngineImpl::fallbackToWasapi()
{
    asio_transport_->close();
    transport_kind_ = TransportKind::Wasapi;
}

void AudioEngineImpl::openWasapiCapture()
{
    if (desired_input_id_.empty())
    {
        return;
    }

    closeWasapiCapture();

    // Size for ~500 ms at the maximum supported rate so the drift controller keeps
    // the same real-time headroom for any WASAPI/ASIO rate combo (incl. 96/192 kHz).
    const std::size_t ring_capacity = static_cast<std::size_t>(kMaxSupportedSampleRate * 0.5) + 1;
    capture_ring_buffer_ = std::make_unique<shared::LockFreeAudioRingBuffer>(
        ring_capacity,
        2);

    wasapi_capture_ = std::make_unique<WasapiCapture>();
    // T016: Open in loopback mode to capture from render endpoints (testable-dev).
    if (!wasapi_capture_->open(desired_input_id_, desired_sample_rate_,
                                 capture_ring_buffer_.get(), true))  // loopback=true
    {
        // T012: Notify UI that loopback capture initialization failed.
        notifyOnUiThread([id = desired_input_id_](IAudioEngineListener& l) {
            l.onDeviceLost(id, EndpointId{});
        });
        wasapi_capture_.reset();
        capture_ring_buffer_.reset();
        return;
    }

    capture_wasapi_rate_ = wasapi_capture_->negotiatedSampleRate();

    if (!wasapi_capture_->start())
    {
        // T012: Notify UI that capture stream failed to start.
        notifyOnUiThread([id = desired_input_id_](IAudioEngineListener& l) {
            l.onDeviceLost(id, EndpointId{});
        });
        wasapi_capture_->close();
        wasapi_capture_.reset();
        capture_ring_buffer_.reset();
    }
}

void AudioEngineImpl::closeWasapiCapture()
{
    if (wasapi_capture_ != nullptr)
    {
        wasapi_capture_->stop();
        wasapi_capture_->close();
        wasapi_capture_.reset();
    }
    capture_ring_buffer_.reset();
}

void AudioEngineImpl::openWasapiOutput()
{
    // T017: Open real WASAPI render output
    closeWasapiOutput();

    if (desired_output_id_.empty())
        return;

    // Create ring buffer for audio to feed to output.
    // Size for ~500 ms at the maximum supported rate so the render thread keeps the
    // same real-time headroom for any output rate (incl. 96/192 kHz) — a fixed
    // 16384-frame ring is only ~170 ms at 96 kHz and starves under load.
    output_ring_buffer_ = std::make_unique<shared::LockFreeAudioRingBuffer>(
        static_cast<std::size_t>(kMaxSupportedSampleRate * 0.5) + 1,  // max frames
        2);                                                            // stereo

    if (!output_ring_buffer_)
        return;

    wasapi_output_ = std::make_unique<WasapiOutput>();
    if (!wasapi_output_->open(desired_output_id_, desired_sample_rate_))
    {
        notifyOnUiThread([id = desired_output_id_](IAudioEngineListener& l) {
            l.onDeviceLost(id, EndpointId{});
        });
        wasapi_output_.reset();
        output_ring_buffer_.reset();
        return;
    }

    output_wasapi_rate_ = wasapi_output_->negotiatedSampleRate();

    if (!wasapi_output_->start(output_ring_buffer_.get()))
    {
        notifyOnUiThread([id = desired_output_id_](IAudioEngineListener& l) {
            l.onDeviceLost(id, EndpointId{});
        });
        wasapi_output_->close();
        wasapi_output_.reset();
        output_ring_buffer_.reset();
        return;
    }
}

void AudioEngineImpl::closeWasapiOutput()
{
    if (wasapi_output_ != nullptr)
    {
        wasapi_output_->stop();
        wasapi_output_->close();
        wasapi_output_.reset();
    }
    output_ring_buffer_.reset();
}

void AudioEngineImpl::onDeviceStateChanged(const std::string& device_id, DWORD state)
{
    // T031: Handle device reappearance when a remembered device becomes active again.
    if (state != DEVICE_STATE_ACTIVE)
        return;

    std::lock_guard lk {control_mutex_};

    // If a previously-removed device that matches our desired input or output
    // becomes active again, notify the UI so it can offer resumption.
    if (device_id == desired_input_id_ || device_id == desired_output_id_)
    {
        LogDebug("onDeviceStateChanged: remembered device %s became active", device_id.c_str());
        notifyOnUiThread([device_id](IAudioEngineListener& l) {
            l.onDeviceRestored(device_id);
        });
    }

    // Notify UI that the available device list changed so it can refresh dropdowns.
    notifyOnUiThread([](IAudioEngineListener& l) {
        l.onDeviceListChanged();
    });
}

void AudioEngineImpl::onDeviceAdded(const std::string& device_id)
{
    // T031: If the added device matches a remembered selection, allow resuming.
    std::lock_guard lk {control_mutex_};

    if (device_id == desired_input_id_ || device_id == desired_output_id_)
    {
        LogDebug("onDeviceAdded: remembered device %s added", device_id.c_str());
        notifyOnUiThread([device_id](IAudioEngineListener& l) {
            l.onDeviceRestored(device_id);
        });
    }

    // A new endpoint appeared — refresh the UI lists.
    notifyOnUiThread([](IAudioEngineListener& l) {
        l.onDeviceListChanged();
    });
}

void AudioEngineImpl::onDeviceRemoved(const std::string& device_id)
{
    // T029: If the active output endpoint was removed, stop safely and prompt reselection.
    // T030: If the active capture endpoint (specific, non-default) was removed, fire a clear message.
    std::lock_guard lk {control_mutex_};
    if (!running_.load())
        return;

    if (device_id == desired_output_id_)
    {
        LogDebug("onDeviceRemoved: active output %s removed, stopping safely", device_id.c_str());
        notifyOnUiThread([id = desired_output_id_](IAudioEngineListener& l) {
            l.onDeviceLost(id, EndpointId{});
        });
        stop();
    }
    else if (device_id == desired_input_id_ && !follow_default_capture_)
    {
        LogDebug("onDeviceRemoved: active capture %s removed, stopping safely", device_id.c_str());
        notifyOnUiThread([id = desired_input_id_](IAudioEngineListener& l) {
            l.onDeviceLost(id, EndpointId{});
        });
        stop();
    }

    // An endpoint disappeared — refresh the UI lists.
    notifyOnUiThread([](IAudioEngineListener& l) {
        l.onDeviceListChanged();
    });
}

void AudioEngineImpl::onDefaultDeviceChanged(int flow, const std::string& device_id)
{
    if (flow != static_cast<int>(eRender))
        return;

    std::lock_guard lk {control_mutex_};
    if (!running_.load())
        return;

    // T026: If capture follows system default, re-resolve and check for same-device conflict.
    if (follow_default_capture_)
    {
        const EndpointId resolved_capture = same_device_guard_.checkConflict("system-default", desired_output_id_);
        if (!resolved_capture.empty())
        {
            LogDebug("onDefaultDeviceChanged: default changed to output device, conflict detected");
            notifyOnUiThread([id = resolved_capture](IAudioEngineListener& l) {
                l.onSameDeviceConflict(id);
            });
            stop();
            return;
        }

        // Re-open loopback capture on the new default endpoint.
        closeWasapiCapture();
        openWasapiCapture();
    }
    else if (device_id == desired_output_id_ && device_id == desired_input_id_)
    {
        // Specific endpoint selection now coincides with output due to some external change.
        notifyOnUiThread([id = device_id](IAudioEngineListener& l) {
            l.onSameDeviceConflict(id);
        });
        stop();
    }
}

void AudioEngineImpl::startEngineThread()
{
    if (engine_thread_running_.exchange(true))
        return;
    engine_thread_ = std::thread([this]() { engineThreadLoop(); });
}

void AudioEngineImpl::stopEngineThread()
{
    if (!engine_thread_running_.exchange(false))
        return;
    if (engine_thread_.joinable())
        engine_thread_.join();
}

void AudioEngineImpl::engineThreadLoop()
{
    const int block = desired_buffer_size_;
    // The chain runs at the auto-followed rate stored in negotiated_sample_rate_
    // (set in start() before this thread launches), not the user-requested rate.
    const double sr = static_cast<double>(negotiated_sample_rate_.load());
    if (block <= 0 || sr <= 0.0)
        return;

    // This thread is the processing stage: it drains the WASAPI-capture ring,
    // runs the plugin chain, and feeds the WASAPI-output ring. The capture and
    // output render threads both run at MMCSS "Pro Audio"; if this middle stage
    // runs at plain priority it gets preempted under load, starving the output
    // ring (dropouts) and jittering the capture-ring fill (which forces the
    // drift resampler to bend its ratio — audible as pitch/speed wobble). Join
    // the same real-time class so all three stages are scheduled equally.
    DWORD avrt_task_index = 0;
    HANDLE avrt_handle = AvSetMmThreadCharacteristics(L"Pro Audio", &avrt_task_index);

    // Dummy I/O buffers for the JUCE callback interface.
    std::vector<float> dummy_in_l(block, 0.0f);
    std::vector<float> dummy_in_r(block, 0.0f);
    std::vector<float> dummy_out_l(block, 0.0f);
    std::vector<float> dummy_out_r(block, 0.0f);
    const float* input_ptrs[2] = {dummy_in_l.data(), dummy_in_r.data()};
    float* output_ptrs[2] = {dummy_out_l.data(), dummy_out_r.data()};

    // The processing rate is governed by the OUTPUT HARDWARE CLOCK, not a
    // software timer. The output render thread drains output_ring_buffer_ at the
    // output device's true rate; we only produce another block when that ring has
    // drained below a small target fill. This backpressure phase-locks processing
    // to the output clock: no free-running steady_clock timer means no rate error
    // and no scheduling-jitter-driven ratio correction on the capture side.
    const std::size_t block_out = static_cast<std::size_t>(block);
    std::size_t target_fill = 3 * block_out;
    if (output_ring_buffer_ != nullptr)
    {
        target_fill = std::min<std::size_t>(target_fill, output_ring_buffer_->capacity() / 2);
    }

    // When the ring is full enough, wait for the render thread to drain. Poll at a
    // fraction of the block period so room is picked up promptly; the poll is only
    // a wait — it does NOT set the rate, so its jitter is harmless.
    const auto poll = std::chrono::microseconds(
        std::max<std::int64_t>(250, static_cast<std::int64_t>(block * 1'000'000.0 / sr / 4.0)));

    while (engine_thread_running_.load(std::memory_order_acquire))
    {
        if (output_ring_buffer_ != nullptr &&
            output_ring_buffer_->available() >= target_fill)
        {
            // Output ring has enough queued; let the hardware clock drain it.
            std::this_thread::sleep_for(poll);
            continue;
        }

        audioDeviceIOCallbackWithContext(input_ptrs, 2, output_ptrs, 2, block, {});

        if (output_ring_buffer_ == nullptr)
        {
            // No output ring to pace against (should not happen in loopback mode);
            // fall back to nominal block-period pacing to avoid a busy spin.
            std::this_thread::sleep_for(
                std::chrono::microseconds(static_cast<std::int64_t>(block * 1'000'000.0 / sr)));
        }
    }

    if (avrt_handle != nullptr)
        AvRevertMmThreadCharacteristics(avrt_handle);
}

// =====================================================================
// Factory
// =====================================================================

}  // namespace jyglobalvst::engine

namespace jyglobalvst {

std::unique_ptr<IAudioEngine> createAudioEngine()
{
    return std::make_unique<engine::AudioEngineImpl>();
}

}  // namespace jyglobalvst
