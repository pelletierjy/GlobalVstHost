// src/audio-engine/routing/audio_engine_impl.h
//
// T022 — Concrete IAudioEngine implementation. Pass-through in this revision
// (no chain). US1 (T040) extends with a single-plugin chain; US2 (T058–T060)
// extends to multi-plugin. The interface surface does not change.
//
// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • The audio callback (audioDeviceIOCallbackWithContext) is the only
//     audio-thread entry point. It MUST NOT allocate, lock, throw, log,
//     or call any IAudioEngine method directly.
//   • All control-thread mutations enqueue commands into command_queue_
//     (SpscCommandQueue from T012). The callback drains them at the top
//     of each block.
//   • Listener callbacks fire on the UI thread via JUCE's
//     AsyncUpdater / MessageManager — never from the audio callback.
//   • Testable-dev binds to a JUCE-managed WASAPI device. Release prep
//     swaps the device session for direct IAudioClient3 (T025, T026).
// =====================================================================

#pragma once

#include "format_convert.h"
#include "resampler.h"
#include "wasapi_capture.h"
#include "wasapi_output.h"  // T005: Real WASAPI render output

#include "../../shared/platform/endpoint_volume.h"  // T019: Mute/restore guard
#include "same_device_guard.h"  // T009: Conflict detection

#include "jyglobalvst/audio_engine.h"
#include "jyglobalvst/types.h"

#include <nlohmann/json.hpp>

#include "../chain/plugin_chain.h"
#include "../../shared/concurrency/spsc_queue.h"
#include "../../shared/concurrency/lockfree_ring_buffer.h"
#include "../../shared/platform/audio_endpoints.h"
#include "../../shared/platform/realtime_clock.h"

#include "../vst-host/plugin_scanner.h"
#include "../vst-host/scan_cache.h"
#include "../vst-host/vst3_loader.h"
#include "asio_transport.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace jyglobalvst::engine {
class BuiltinEffectRegistry;
class DeviceWatchdog;
}

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace jyglobalvst::engine {

// One slot per pending control-thread mutation. The variant kind is encoded
// in `kind`; payload fields are kind-specific.
struct EngineCommand
{
    enum class Kind : std::uint8_t
    {
        None,
        SetParameter,
        SetMasterVolume,
    };

    Kind kind {Kind::None};
    int position {0};
    ParamId param_id {0};
    float value {0.f};
    bool flag {false};
};

class AudioEngineImpl final
    : public IAudioEngine
    , private juce::AudioIODeviceCallback
{
public:
    AudioEngineImpl();
    ~AudioEngineImpl() override;

    AudioEngineImpl(const AudioEngineImpl&) = delete;
    AudioEngineImpl& operator=(const AudioEngineImpl&) = delete;

    // --- IAudioEngine ------------------------------------------------------
    void start() override;
    void stop() override;
    bool isRunning() const override;
    void setListener(IAudioEngineListener* listener) override;
    void setMasterVolume(float gain_linear) override;
    void reset() override;

    void setEnergySaverEnabled(bool enabled) override;
    bool isEnergySaverEnabled() const override;
    bool isEnergySaverSleeping() const override;

    std::vector<HardwareOutputInfo> listOutputs() const override;
    void selectOutput(const EndpointId& id) override;
    EndpointId currentOutput() const override;
    DeviceResolutionSource currentResolutionSource() const override;

    void setBufferSize(int samples) override;
    int bufferSize() const override;
    void setSampleRate(double rate) override;
    double sampleRate() const override;
    int negotiatedSampleRate() const override;
    int outputDeviceSampleRate() const override;
    int inputDeviceSampleRate() const override;

    std::vector<HardwareOutputInfo> listInputs() const override;
    void selectInput(const EndpointId& id) override;
    EndpointId currentInput() const override;

    bool isCaptureDeviceMuted() const override;

    void setAsioOutputPair(int channel_offset) override;
    int asioOutputPair() const override;
    void openAsioControlPanel() override;

    void setWasapiExclusive(bool exclusive) override;
    bool wasapiExclusive() const override;

    void rescanPlugins(IScanProgressListener* progress) override;
    void cancelScan() override;
    std::vector<PluginCatalogEntry> catalog() const override;

    ChainSnapshot snapshotChain() const override;
    InstanceId addPlugin(const PluginRef& ref, int position) override;
    InstanceId addPluginFromPath(const std::filesystem::path& vst3_path, int position) override;
    void removeSlot(int position) override;
    void moveSlot(int from, int to) override;
    void setBypass(int position, bool bypassed) override;
    void setParameter(int position, ParamId param, float value) override;
    void openEditor(int position) override;
    void closeEditor(int position) override;
    void repointPlaceholder(int position, const PluginRef& ref) override;

    void loadPreset(const std::filesystem::path& path) override;
    void savePreset(const std::filesystem::path& path, const std::string& name) override;
    void restoreChain(const std::filesystem::path& path) override;

    LatencyProfile latencyProfile() const override;
    CpuStats cpuStats() const override;
    MeterFrame latestMeterFrame() const override;
    std::vector<float> pluginOutputPeaks() const override;
    std::vector<float> pluginOutputRms() const override;

    // --- Test helpers ------------------------------------------------------
    void injectTestCatalogEntry(const PluginCatalogEntry& entry);
    InstanceId addPlaceholderSlot(int position);
    // Returns the live processor for the slot at `position`, or nullptr if the
    // slot is empty / a placeholder. Test-only; lets tests inspect built-in
    // effect state without starting audio.
    juce::AudioProcessor* testGetProcessor(int position) const;

private:
    // --- juce::AudioIODeviceCallback (audio thread) -----------------------
    void audioDeviceIOCallbackWithContext(const float* const* input_channel_data,
                                          int num_input_channels,
                                          float* const* output_channel_data,
                                          int num_output_channels,
                                          int num_samples,
                                          const juce::AudioIODeviceCallbackContext& context) override;

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceError(const juce::String& error_message) override;

    void applyDeviceSelection();
    void rebumpChain();
    // Loads slots+state from a preset JSON doc. Caller must hold control_mutex_.
    // Does not clear the chain or touch device/audio settings.
    void loadChainFromJson(const nlohmann::json& doc, std::vector<MissingPluginInfo>& out_missing);
    void notifyOnUiThread(std::function<void(IAudioEngineListener&)> fn);

    friend class DeviceWatchdog;

private:
    // --- Optional ASIO transport -----------------------------------------
    std::unique_ptr<ASIOTransport> asio_transport_ = std::make_unique<ASIOTransport>();
    TransportKind transport_kind_ {TransportKind::Wasapi};
    std::string fallback_reason_;

    void applyAsioTransport();
    bool openAsioTransport(const EndpointId& id);
    void fallbackToWasapi();

    // --- Input/Output (Driverless Audio Capture) ---------------------------
    std::unique_ptr<WasapiCapture> wasapi_capture_;       // T004: Loopback capture
    std::unique_ptr<WasapiOutput> wasapi_output_;         // T005: Real render output
    std::unique_ptr<shared::LockFreeAudioRingBuffer> capture_ring_buffer_;
    std::unique_ptr<shared::LockFreeAudioRingBuffer> output_ring_buffer_;  // T017
    std::atomic<bool> mixed_mode_active_ {false};

    // Capture-side resampler (UI-thread prepared, audio-thread used).
    // Active only when the capture endpoint rate differs from the output rate.
    WindowedSincResampler capture_resampler_;
    juce::AudioBuffer<float> capture_raw_buffer_;   // pre-allocated, raw WASAPI frames
    bool capture_resampling_enabled_ {false};
    double capture_wasapi_rate_ {0.0};
    double capture_nominal_ratio_ {1.0};      // wasapi_rate / chain_rate
    double output_wasapi_rate_ {0.0};         // T017: render endpoint negotiated rate

    // JUCE-callback device rate, stored in audioDeviceAboutToStart so the callback
    // can compute the VST-equivalent block size when the chain runs at a different
    // rate than the hardware.
    double juce_device_rate_ {0.0};

    // Output-side resampler (UI-thread prepared, audio-thread used) for the
    // ASIO / JUCE-callback path. The chain never follows an ASIO device's
    // negotiated rate (applyAsioTransport() deliberately never asks the driver
    // to change its clock) — instead it runs at the user-selected rate and
    // this resampler bridges chain audio to whatever the device actually
    // negotiated. Mirrors capture_resampler_ above, but source/target swapped.
    WindowedSincResampler output_resampler_;
    juce::AudioBuffer<float> output_resampled_buffer_;  // pre-allocated, device-rate frames
    bool output_resampling_enabled_ {false};
    double output_nominal_ratio_ {1.0};       // chain_rate / device_rate
    // True capacity work_buffer_ was allocated at (audioDeviceAboutToStart). The
    // callback shrinks work_buffer_'s logical size to chain_samples every block
    // (avoidReallocating, so its getNumSamples() no longer reflects the original
    // capacity) — this is the value to clamp chain_samples against instead.
    int chain_block_capacity_ {0};

    void openWasapiCapture();
    void closeWasapiCapture();
    void openWasapiOutput();      // T017: Open real WASAPI render output
    void closeWasapiOutput();     // T017: Close WASAPI render output

    // T017: Engine thread for pure WASAPI mode (no JUCE audio callback).
    void startEngineThread();
    void stopEngineThread();
    void engineThreadLoop();
    std::thread engine_thread_;
    std::atomic<bool> engine_thread_running_ {false};

    // --- Energy Saver ------------------------------------------------------
    // When enabled and running, energy_saver_thread_ polls the input peak at
    // ~10 Hz. After kEnergySaverIdleMs of silence (input below the wake
    // threshold) it sets energy_saver_sleeping_, which makes the audio callback
    // skip the VST chain and emit silence. The input peak is still computed
    // every metering block while sleeping, so the same thread wakes the engine
    // the moment audio returns. The thread never touches the audio path — it
    // only flips atomics and fires the listener notification.
    std::atomic<bool> energy_saver_enabled_ {false};
    std::atomic<bool> energy_saver_sleeping_ {false};
    std::thread energy_saver_thread_;
    std::atomic<bool> energy_saver_thread_running_ {false};
    static constexpr int kEnergySaverIdleMs = 2'000;         // silence before sleeping
    static constexpr float kEnergySaverWakeDb = -50.0f;      // input level counted as "audio"
    void startEnergySaver();
    void stopEnergySaver();
    void energySaverThreadLoop();
    void setEnergySaverSleeping(bool sleeping);

    // T026/T029-T031: Device watchdog callbacks (friend access from DeviceWatchdog).
    void onDeviceStateChanged(const std::string& device_id, DWORD state);
    void onDeviceAdded(const std::string& device_id);
    void onDeviceRemoved(const std::string& device_id);
    void onDefaultDeviceChanged(int flow, const std::string& device_id);

    // --- Member state -----------------------------------------------------
    mutable std::recursive_mutex control_mutex_;
    IAudioEngineListener* listener_ {nullptr};

    juce::AudioDeviceManager device_manager_;
    shared::AudioEndpointEnumerator endpoint_enum_;
    std::unique_ptr<DeviceWatchdog> watchdog_;

    // T008/T009: Device guards for loopback capture (Feature 005)
    shared::EndpointVolumeGuard endpoint_volume_guard_;
    SameDeviceGuard same_device_guard_;

    int desired_buffer_size_ {512};
    double desired_sample_rate_ {48000.0};
    int desired_asio_output_pair_ {0};
    EndpointId desired_input_id_;
    EndpointId desired_output_id_;
    std::string desired_output_friendly_name_;
    std::string desired_asio_device_name_;
    TransportKind desired_output_transport_kind_ {TransportKind::Wasapi};
    TransportKind desired_input_transport_kind_ {TransportKind::Wasapi};
    TransportKind last_active_output_transport_kind_ {TransportKind::Wasapi};
    bool desired_input_is_loopback_ {true};  // true for loopback (render), false for real capture
    bool wasapi_exclusive_ {false};
    bool follow_default_capture_ {false};  // T026: true when input follows system default render endpoint
    DeviceResolutionSource resolution_source_ {DeviceResolutionSource::WindowsDefaultFallback};

    std::atomic<bool> running_ {false};
    std::atomic<int> chain_revision_ {0};
    std::atomic<int> negotiated_sample_rate_ {48000};
    std::atomic<float> instantaneous_cpu_pct_ {0.f};
    std::atomic<float> master_volume_ {1.0f};
    std::atomic<std::uint64_t> xrun_count_ {0};

    // US4: atomic meter samples updated from audio callback, read by UI thread.
    std::atomic<float> meter_input_peak_l_ {0.f};
    std::atomic<float> meter_input_peak_r_ {0.f};
    std::atomic<float> meter_input_rms_l_ {0.f};
    std::atomic<float> meter_input_rms_r_ {0.f};
    std::atomic<float> meter_output_peak_l_ {0.f};
    std::atomic<float> meter_output_peak_r_ {0.f};
    std::atomic<float> meter_output_rms_l_ {0.f};
    std::atomic<float> meter_output_rms_r_ {0.f};

    // Audio-thread-owned scratch — sized in audioDeviceAboutToStart, never
    // reallocated within the callback.
    int meter_callback_counter_ {0};
    static constexpr int kMeterThrottleDivisor = 3;  // ~30 Hz at 48 kHz / 512

    juce::AudioBuffer<float> work_buffer_;
    shared::RealtimeClock rt_clock_;
    shared::SpscCommandQueue<EngineCommand, 256> command_queue_;

    std::unique_ptr<PluginChain> plugin_chain_;
    std::unique_ptr<ScanCache> scan_cache_;
    std::unique_ptr<VST3PluginLoader> plugin_loader_;
    std::unique_ptr<BuiltinEffectRegistry> builtin_registry_;
    std::unique_ptr<PluginScanner> scanner_;

    std::map<int, std::unique_ptr<juce::DocumentWindow>> editor_windows_;
};

}  // namespace jyglobalvst::engine
