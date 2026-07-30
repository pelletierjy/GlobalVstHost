// src/audio-engine/include/jyglobalvst/audio_engine.h
//
// T011 — IAudioEngine + IAudioEngineListener.
// Authoritative C++ surface that both the tray app (user-mode) and the Windows
// Service (release: service-mode) consume. The IPC layer (release) is a thin
// JSON dispatcher over this contract — no extra semantics.
//
// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • Every method on this interface is invoked from the UI / control
//     thread and MUST NOT block on the audio thread.
//   • Mutating calls (addPlugin, removeSlot, moveSlot, setBypass,
//     setParameter, repointPlaceholder) serialize into the engine's
//     internal lock-free SPSC command queue and return immediately.
//   • Effects become audible at the next audio buffer; observability is
//     via chain_revision bumps + IAudioEngineListener callbacks.
//   • Methods MAY block briefly on the message-pump thread (e.g.
//     savePreset's file I/O) — never on the audio thread.
//   • Failures of plugins or devices are NOT thrown; they are delivered
//     asynchronously through IAudioEngineListener.
// =====================================================================

#pragma once

#include "jyglobalvst/types.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace jyglobalvst {

class IAudioEngineListener
{
public:
    virtual ~IAudioEngineListener() = default;

    // Fired after a mutation has been applied on the audio thread.
    // chain_revision is monotonically increasing within a session.
    virtual void onChainRevision(int new_revision) = 0;

    // FR-023: plugin processBlock threw / SEH'd; engine bypassed it.
    virtual void onPluginFailed(const InstanceId& id, const std::string& reason) = 0;

    // FR-024: hardware output endpoint was removed; engine auto-fell-back.
    virtual void onDeviceLost(const EndpointId& lost, const EndpointId& fallback_to) = 0;

    // FR-024: a previously-lost preferred endpoint reappeared and was restored.
    virtual void onDeviceRestored(const EndpointId& restored) = 0;

    // FR-026: rolling 1-second CPU usage crossed the warning threshold.
    virtual void onCpuWarning(float rolling_1s_pct) = 0;

    // 30 Hz push of latest input/output meter sample (US4).
    virtual void onMeterFrame(const MeterFrame& frame) = 0;

    // US3: preset loaded but some slots could not resolve to scanned plugins.
    virtual void onPresetPartialLoad(const std::vector<MissingPluginInfo>& missing) = 0;

    // FR-005/FR-014: capture and output are the same device; hard block start().
    virtual void onSameDeviceConflict(const EndpointId& device) = 0;

    // FR-018: loopback capture muting is required but failed; fallback needed.
    virtual void onCaptureMuteFallbackRequired(const EndpointId& endpoint) = 0;

    // Energy Saver: the engine transitioned between active processing and the
    // low-power "sleeping" state (heavy DSP suspended while input is silent).
    // `sleeping == true` means processing is currently suspended. Non-pure so
    // existing listeners need not implement it.
    virtual void onEnergySaverStateChanged(bool /*sleeping*/) {}
};

class IAudioEngine
{
public:
    virtual ~IAudioEngine() = default;

    // --- Lifecycle ---------------------------------------------------------
    // Idempotent. start() binds capture + output and begins streaming. stop()
    // unbinds; per FR-022c, stop() persists the autosave unless the override
    // flag is set (set when a user explicitly loads a preset this session).
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    virtual void setListener(IAudioEngineListener* listener) = 0;
    virtual void setMasterVolume(float gain_linear) = 0;
    virtual void reset() = 0;

    // --- Energy Saver ------------------------------------------------------
    // When enabled, the engine watches the input level while running. After a
    // sustained silence it suspends the CPU-heavy processing path (VST chain)
    // and emits silence, keeping only a lightweight input probe alive so it can
    // auto-resume the instant audio returns. Toggling has no audible effect
    // while audio is not running; the preference is simply remembered.
    // State transitions are reported via IAudioEngineListener::onEnergySaverStateChanged.
    virtual void setEnergySaverEnabled(bool enabled) = 0;
    virtual bool isEnergySaverEnabled() const = 0;
    // True while the engine is currently in the suspended (sleeping) state.
    virtual bool isEnergySaverSleeping() const = 0;

    // --- Device selection --------------------------------------------------
    virtual std::vector<HardwareOutputInfo> listOutputs() const = 0;
    virtual void selectOutput(const EndpointId& id) = 0;
    virtual EndpointId currentOutput() const = 0;
    virtual DeviceResolutionSource currentResolutionSource() const = 0;

    // --- Buffer / sample-rate ---------------------------------------------
    // setBufferSize accepts {512, 1024} in WASAPI mode and
    // {32, 64, 128, 256, 512, 1024} in ASIO mode. Other values throw.
    virtual void setBufferSize(int samples) = 0;
    virtual int bufferSize() const = 0;
    virtual void setSampleRate(double rate) = 0;
    virtual double sampleRate() const = 0;
    virtual int negotiatedSampleRate() const = 0;
    // Actual sample rate of the selected output hardware endpoint (Hz), or 0 when
    // not running. The VST chain auto-follows this rate, so negotiatedSampleRate()
    // normally equals this value in normal operation.
    virtual int outputDeviceSampleRate() const = 0;
    // Actual sample rate of the selected input/capture hardware endpoint (Hz), or 0
    // when not running.
    virtual int inputDeviceSampleRate() const = 0;

    // --- Input source (testable-dev addition) -----------------------------
    // In testable-dev the engine captures from a user-selected existing input
    // device (WASAPI loopback on default render, or a virtual cable). In
    // release this becomes the JyGlobalVST virtual driver endpoint and the
    // input list collapses to a single hardcoded source. The contract surface
    // is otherwise identical — UI code does not branch on build mode.
    virtual std::vector<HardwareOutputInfo> listInputs() const = 0;
    virtual void selectInput(const EndpointId& id) = 0;
    virtual EndpointId currentInput() const = 0;

    // --- Device conflict detection (T007) ---------------------------------
    // Returns true if the capture device's mute state was successfully queried
    // and applied. False if muting is unavailable or failed (fallback required).
    virtual bool isCaptureDeviceMuted() const = 0;

    // --- ASIO-specific controls -------------------------------------------
    // setAsioOutputPair sets the first output channel to use (0-based).
    // The engine uses a stereo pair starting at this offset.
    virtual void setAsioOutputPair(int channel_offset) = 0;
    virtual int asioOutputPair() const = 0;
    // Opens the ASIO driver's native control panel (blocking call on UI
    // thread). Restarts the ASIO session on return to apply any changed
    // settings (e.g. buffer size). No-op if no ASIO device is open.
    virtual void openAsioControlPanel() = 0;

    // --- WASAPI exclusive mode -------------------------------------------
    // When enabled, uses "Windows Audio (Exclusive Mode)" device type which
    // bypasses the Windows Audio Engine mixer, reducing round-trip latency
    // by ~10 ms vs. shared mode. Exclusive mode prevents other apps from
    // using the same output device simultaneously. No-op in ASIO mode.
    virtual void setWasapiExclusive(bool exclusive) = 0;
    virtual bool wasapiExclusive() const = 0;

    // --- Plugin catalog ---------------------------------------------------
    virtual void rescanPlugins(IScanProgressListener* progress) = 0;
    virtual void cancelScan() = 0;
    virtual std::vector<PluginCatalogEntry> catalog() const = 0;

    // --- Chain ------------------------------------------------------------
    virtual ChainSnapshot snapshotChain() const = 0;
    virtual InstanceId addPlugin(const PluginRef& ref, int position) = 0;
    virtual InstanceId addPluginFromPath(const std::filesystem::path& vst3_path, int position) = 0;
    virtual void removeSlot(int position) = 0;
    virtual void moveSlot(int from, int to) = 0;
    virtual void setBypass(int position, bool bypassed) = 0;
    virtual void setParameter(int position, ParamId param, float value) = 0;
    virtual void openEditor(int position) = 0;
    virtual void closeEditor(int position) = 0;
    virtual void repointPlaceholder(int position, const PluginRef& ref) = 0;

    // --- Presets ----------------------------------------------------------
    virtual void loadPreset(const std::filesystem::path& path) = 0;
    virtual void savePreset(const std::filesystem::path& path, const std::string& name) = 0;
    // Like loadPreset but does NOT stop/start audio. Only safe when audio is not running.
    // Used by autosave restore to load chain+state before the first engine->start().
    virtual void restoreChain(const std::filesystem::path& path) = 0;

    // --- Monitoring -------------------------------------------------------
    virtual LatencyProfile latencyProfile() const = 0;
    virtual CpuStats cpuStats() const = 0;
    virtual MeterFrame latestMeterFrame() const = 0;
};

// Factory. The concrete implementation lives in src/audio-engine/routing/.
std::unique_ptr<IAudioEngine> createAudioEngine();

}  // namespace jyglobalvst
