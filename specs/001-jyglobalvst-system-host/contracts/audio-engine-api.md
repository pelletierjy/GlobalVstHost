# Contract: Audio Engine API (Internal)

**Status**: v1
**Scope**: C++ interface exposed by `src/audio-engine/` and consumed by `src/tray-app/` (user-mode install) and `src/service/` (service-mode install).
**Purpose**: Decouple UI from engine so the same UI binary works regardless of install mode. In service-mode, the tray app speaks `ipc-protocol.md` to the service, which forwards calls to this same interface in-process. UI code MUST NOT depend on which side of the IPC boundary it is on (FR-029).

## Header (conceptual)

```cpp
// src/audio-engine/include/jyglobalvst/audio_engine.h
//
// REALTIME CONSTRAINTS (Constitution §V):
// - All methods on this interface are called from the UI / control thread.
// - The audio thread runs internally; no method here is invoked from inside it.
// - Mutating methods serialize into the internal lock-free SPSC command queue
//   and return immediately. Effects become audible at the next processBlock.
// - "Apply" is observable via chain_revision bumps and event callbacks.

namespace jyglobalvst {

class IAudioEngineListener {
public:
    virtual ~IAudioEngineListener() = default;
    virtual void onChainRevision(int new_revision) = 0;
    virtual void onPluginFailed(const InstanceId& id, const std::string& reason) = 0;
    virtual void onDeviceLost(const EndpointId& lost,
                              const EndpointId& fallback_to) = 0;
    virtual void onDeviceRestored(const EndpointId& restored) = 0;
    virtual void onCpuWarning(float rolling_1s_pct) = 0;
    virtual void onMeterFrame(const MeterFrame& frame) = 0;
    virtual void onPresetPartialLoad(
        const std::vector<MissingPluginInfo>& missing) = 0;
};

class IAudioEngine {
public:
    virtual ~IAudioEngine() = default;

    // Lifecycle ---------------------------------------------------------
    virtual void start() = 0;                  // bind virtual + output, begin streaming
    virtual void stop() = 0;                   // unbind, persist autosave
    virtual void setListener(IAudioEngineListener*) = 0;

    // Device selection --------------------------------------------------
    virtual std::vector<HardwareOutputInfo> listOutputs() const = 0;
    virtual void selectOutput(const EndpointId&) = 0;
    virtual EndpointId currentOutput() const = 0;
    virtual DeviceResolutionSource currentResolutionSource() const = 0;

    // Buffer / sample-rate ---------------------------------------------
    virtual void setBufferSize(int samples) = 0;        // {128,256,512,1024}
    virtual int  bufferSize() const = 0;
    virtual int  negotiatedSampleRate() const = 0;

    // Plugin catalog ---------------------------------------------------
    virtual void rescanPlugins(IScanProgressListener*) = 0;
    virtual void cancelScan() = 0;
    virtual std::vector<PluginCatalogEntry> catalog() const = 0;

    // Chain ------------------------------------------------------------
    virtual ChainSnapshot snapshotChain() const = 0;
    virtual InstanceId addPlugin(const PluginRef&, int position) = 0;
    virtual void removeSlot(int position) = 0;
    virtual void moveSlot(int from, int to) = 0;
    virtual void setBypass(int position, bool bypassed) = 0;
    virtual void setParameter(int position, ParamId, float value) = 0;
    virtual void openEditor(int position) = 0;
    virtual void closeEditor(int position) = 0;
    virtual void repointPlaceholder(int position, const PluginRef&) = 0;

    // Presets ----------------------------------------------------------
    virtual void loadPreset(const std::filesystem::path&) = 0;
    virtual void savePreset(const std::filesystem::path&,
                            const std::string& name) = 0;

    // Monitoring -------------------------------------------------------
    virtual LatencyProfile latencyProfile() const = 0;
    virtual CpuStats cpuStats() const = 0;
};

std::unique_ptr<IAudioEngine> createAudioEngine();

} // namespace jyglobalvst
```

## Contract guarantees

1. **Real-time isolation**: No method on `IAudioEngine` blocks on the audio thread. Mutations enqueue commands; the next audio buffer applies them. The interface MAY block briefly on the message-pump thread (e.g., file I/O on `savePreset`), but never on the audio thread.

2. **Observability**: Every successful mutation bumps `chain_revision`. UI tracks `chain_revision` to know when its view is stale. `IAudioEngineListener::onChainRevision` fires on the UI thread.

3. **Error reporting**: Methods do not throw on plugin failures or device losses; those are asynchronous events delivered via the listener. Methods MAY throw `std::runtime_error` for programmer-error conditions (invalid position, unknown endpoint). The IPC adapter translates throws into error envelopes.

4. **Idempotency**: `start()` and `stop()` are idempotent. `selectOutput(currentOutput())` is a no-op. `setBufferSize(bufferSize())` is a no-op.

5. **Thread affinity**: All `IAudioEngine` methods MUST be called from the UI / control thread (i.e., serialized). The IPC adapter ensures this for service-mode.

6. **No persistent logging**: Per FR-022n / Constitution §V, the engine MUST NOT write logs to disk. All errors surface via the listener as in-session events.

7. **No quarantine, no crash detection**: `start()` always reads the auto-save unmodified; there is no sentinel file, no exit-cause record, no plugin-blocklist auto-population (clarifications #12, #22).

## Lifecycle

```
            createAudioEngine()
                   │
                   ▼
            [Constructed]
                   │
                   ▼  setListener(...)
            [ListenerAttached]
                   │
                   ▼  start()
            [Running]  ◄── all mutating methods valid here
                   │
                   ▼  stop()
            [Stopped]
                   │
                   ▼  (destructor)
            [Destroyed]
```

## Mapping to IPC

| IPC command | C++ method |
|---|---|
| `chain.snapshot` | `snapshotChain()` |
| `chain.add` | `addPlugin(...)` |
| `chain.remove` | `removeSlot(...)` |
| `chain.move` | `moveSlot(...)` |
| `chain.set_bypass` | `setBypass(...)` |
| `chain.set_parameter` | `setParameter(...)` |
| `chain.repoint_placeholder` | `repointPlaceholder(...)` |
| `device.list_outputs` | `listOutputs()` |
| `device.select_output` | `selectOutput(...)` |
| `buffer.set_size` | `setBufferSize(...)` |
| `preset.load` | `loadPreset(...)` |
| `subscribe.meters` | (server pushes via `onMeterFrame` listener at 30 Hz) |
| `event.notification` | (server pushes from listener callbacks) |

The IPC adapter is a thin JSON ↔ method dispatcher. It does not add semantics.

## Mixed-driver mode (FR-015a)

The engine internally supports ASIO output paired with a separate WASAPI input. This is transparent to the `IAudioEngine` contract surface:
- `selectOutput(id)` with an ASIO endpoint + `selectInput(id)` with a WASAPI endpoint automatically enters mixed mode.
- `listInputs()` continues to return only WASAPI capture endpoints; ASIO inputs are not independently selectable.
- `latencyProfile()` reports `capture_ms` from the WASAPI capture side and `output_ms` from the ASIO output side when mixed mode is active.
- No new public methods are required; the UI does not branch on mixed mode.

## Extension hooks (post-v1 forward compatibility)

- **N-way output (FR-013a)**: `selectOutput` takes a single `EndpointId` in v1; future versions will introduce `addOutput(...)` / `removeOutput(...)` and the routing graph already treats the single output as a degenerate fan-out. No interface change is required to v1 callers, only an addition.
- **MIDI routing** (deferred): not in v1; the chain plumbing accepts MIDI event buffers internally even though no v1 caller fills them — the data path is in place but the catalog method to bind a MIDI source is not exposed.
- **Mixed-driver input selection**: v1 only supports WASAPI input in mixed mode. Future versions may expose ASIO input paired with WASAPI output, but this requires additional architecture work (ASIO capture thread + WASAPI output callback synchronization).
