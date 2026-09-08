# Architecture Audit: FR-013a Single-Output Assumptions

**Task**: T126a
**Purpose**: Verify that v1's single-output design does not block future N-way fan-out.

## Audit Method

Grep `src/audio-engine/routing/` and `src/audio-engine/chain/` for:
1. Singleton output pointers (`HardwareOutputDevice* output_` treated as the only output).
2. Hardcoded channel-count `1` in output paths.
3. Contracts that assume one destination endpoint.

## Findings

### 1. `AudioEngineImpl` — single `desired_output_id_` and `desired_output_friendly_name_`

**File**: `src/audio-engine/routing/audio_engine_impl.cpp`, `audio_engine_impl.h`

The engine stores exactly one desired output:
```cpp
EndpointId desired_output_id_;
std::string desired_output_friendly_name_;
DeviceResolutionSource resolution_source_;
```

**Assessment**: ACCEPTABLE for v1. The `IAudioEngine` contract exposes `selectOutput(id)` which selects one endpoint. To support fan-out later, the contract can be extended with `addOutput(id)`, `removeOutput(id)`, and `listActiveOutputs()` without breaking existing callers. The internal state becomes a `std::vector<ActiveOutput>` rather than a single endpoint.

### 2. `juce::AudioDeviceManager` — single output device

**File**: `src/audio-engine/routing/audio_engine_impl.cpp`

The testable-dev implementation uses one `juce::AudioDeviceManager` which binds to exactly one input + one output device.

**Assessment**: EXPECTED. JUCE's `AudioDeviceManager` is inherently single-pair in shared mode. Future fan-out would likely require:
- One `AudioDeviceManager` per output (heavyweight), OR
- Direct `IAudioClient3` per endpoint with a manual mixer (preferred for latency control), OR
- A single WASAPI output + OS-level mirroring (simplest, least code change).

### 3. `LatencyProfile` — single `output_ms`

**File**: `src/audio-engine/include/jyglobalvst/types.h`

`LatencyProfile` has one `output_ms` field.

**Assessment**: ACCEPTABLE. Future fan-out can extend this to `std::vector<OutputLatency>` with per-endpoint breakdowns, or report the maximum/worst-case output latency.

### 4. `Preset` — single `target_device_friendly_name`

**File**: `contracts/preset-schema.json`, `src/audio-engine/chain/preset_serializer.cpp`

Preset stores one advisory device name.

**Assessment**: ACCEPTABLE. Future multi-output presets can replace `target_device_friendly_name` with `target_outputs: [ {friendly_name, endpoint_id} ]` and migrate v1 presets automatically.

### 5. No hardcoded `1` channel-count in output conversion paths

**File**: `src/audio-engine/routing/format_convert.cpp`, `resampler.cpp`

Format conversion and resampling operate on arbitrary channel counts derived from the buffer. No hardcoded mono assumption found.

**Assessment**: PASS — no blocker.

## Refactoring Recommendations (future fan-out)

1. **Introduce `OutputMixBus`**: a small class owning one `IAudioClient3`/JUCE device, format converter, and resampler. `AudioEngineImpl` holds a `std::vector<std::unique_ptr<OutputMixBus>>`.
2. **Move device selection into `OutputMixBus`**: resolution priority chain runs per-bus.
3. **Mixer stage after chain**: `plugin_chain_->processBlock()` produces one float buffer; a post-mix step copies it to each `OutputMixBus` (no per-bus re-processing).
4. **Latency profile per-bus**: `LatencyProfile` becomes a vector or aggregates min/max.

## Verdict

**PASS** — No v1 contract blocks N-way fan-out. All single-output assumptions are localized to internal engine state (not IPC schemas or preset formats) and are additive to refactor.
