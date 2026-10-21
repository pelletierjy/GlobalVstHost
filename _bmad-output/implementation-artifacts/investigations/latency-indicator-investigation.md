# Investigation: Latency indicator (in/out) misreporting

## Hand-off Brief

1. **What happened.** The status-bar latency readout (`Latency: %.2f ms (in %.2f + out %.2f)`) reports only two narrow, source-dependent quantities (WASAPI stream buffer latency + driver-reported output latency) and omits every bridge buffer the engine actually adds (45 ms capture ring, ~3-block output ring, resampler FIFO, VST chain latency) — so it neither explains the nonzero "input" figure nor responds to buffer-size / VST-rate settings.
2. **Where the case stands.** Root causes Confirmed at code level for the composition problem; the "ASIO latency doesn't move with buffer size" symptom is Deduced to a combination of (a) WASAPI streams ignoring `desired_buffer_size_` entirely and (b) output_ms being whatever `getLatencies()`/`GetStreamLatency()` returns, which excludes all app-side buffering. Needs runtime confirmation on hardware for the driver-dependent part.
3. **What's needed next.** Decide whether to fix the indicator (report true bridge-inclusive component latencies) or also fix the settings/latency coupling; then hand to `bmad-quick-dev`.

## Case Info

| Field            | Value                                                                                     |
| ---------------- | ----------------------------------------------------------------------------------------- |
| Ticket           | N/A (user report)                                                                          |
| Date opened      | 2026-06-09                                                                                 |
| Status           | Active                                                                                     |
| System           | Windows 10/11 x64, JYGlobalVST (JUCE 8.0.4, C++20, MSVC) — WASAPI + ASIO transports        |
| Evidence sources | Source code (engine, tray UI, JUCE third_party), no logs (app has no persistent logging)    |

## Problem Statement (user report, verbatim gist)

- Why does the indicator show an **input latency** when we only tap an already-existing digital signal (loopback)?
- The **output latency** does not reflect ASIO settings: changing buffer size or the VST sample rate does not move it, but it should.

## Evidence Inventory

| Source                          | Status    | Notes                                                                  |
| ------------------------------- | --------- | ---------------------------------------------------------------------- |
| `latencyProfile()` implementation | Available | `src/audio-engine/routing/audio_engine_impl.cpp:2040`                   |
| UI label formatting             | Available | `src/tray-app/ui/main_window.cpp:3431`                                 |
| WASAPI capture/output streams   | Available | `wasapi_capture.cpp`, `wasapi_output.cpp`                               |
| Bridge/ring buffering logic     | Available | `audio_engine_impl.cpp` engine thread + drift controller                |
| JUCE ASIO/WASAPI device latency | Available | `third_party/juce-src/.../juce_ASIO_windows.cpp`, `juce_WASAPI_Windows.cpp` |
| Runtime logs / hardware capture | Missing   | No persistent logging by design; ASIO driver-dependent values need hardware run |

## Investigation Backlog

| # | Path to Explore                                                     | Priority | Status  | Notes                                     |
| - | ------------------------------------------------------------------- | -------- | ------- | ----------------------------------------- |
| 1 | Confirm on hardware which value the ASIO driver's `getLatencies()` returns per buffer size | High     | Open    | Only refutes/confirms driver-dependency leg |
| 2 | Decide indicator spec: bridge-inclusive vs. driver-reported          | Medium   | Open    | Product decision after symptom confirmed  |

## Confirmed Findings

### Finding 1: The indicator only sums two raw driver queries; it never sees the engine's own buffering

**Evidence:** `src/audio-engine/routing/audio_engine_impl.cpp:2040-2081`
**Detail:** `latencyProfile()` computes:
- `output_ms` = `wasapi_output_->latencyMs()` (pure WASAPI) or `device->getOutputLatencyInSamples()` (ASIO/JUCE path) — `audio_engine_impl.cpp:2049`, `:2057`
- `capture_ms` = `wasapi_capture_->latencyMs()` or `device->getInputLatencyInSamples()` — `:2065`, `:2073`
- `resample_ms` and `plugin_chain_ms` are **never assigned anywhere in the repo** (repo-wide grep: only default initializers `src/audio-engine/include/jyglobalvst/types.h:175-176` and read-only consumers in UI/soak/tests). The comment "`plugin_chain_` manages plugin_chain_ms on demand" (`:2078`) is false — no such code exists.
- `total_round_trip_ms = capture + resample + plugin + output` (`:2079`) — omits the capture ring, output ring, and resampler FIFO.

### Finding 2: "Input latency" is nonzero because capture-side *buffering* is real — but the displayed number is the wrong buffering

**Evidence:** `src/audio-engine/routing/audio_engine_impl.cpp:72` (`kCaptureTargetSeconds = 0.045`), `:2660-2664` (capture ring target fill = 45 ms on the JUCE/ASIO path); `src/audio-engine/routing/wasapi_capture.cpp:669-686` (reported value = `GetStreamLatency()` or `buffer_size/rate`).
**Detail:** The WASAPI loopback stream does buffer: the Windows audio engine copies render data into a loopback capture buffer at engine-period granularity, and the app then holds a **45 ms target-fill cushion** in `capture_ring_buffer_` (drift controller). The user's premise ("pure digital tap ⇒ zero input latency") is refuted: there is genuine capture delay. However, the displayed `capture_ms` counts only the WASAPI stream buffer — the dominant 45 ms app-side bridge is **not** counted. So the premise is half-right: the label implies an ADC-style input latency that doesn't exist, and the value shown understates the real capture-side delay.
**Side note:** In mixed ASIO mode there can also be a genuine microphone input (`input_channels = 2` when not mixed, `audio_engine_impl.cpp:2822`).

### Finding 3 (WASAPI mode): `desired_buffer_size_` never reaches the WASAPI streams, so buffer size can't move output latency

**Evidence:** `src/audio-engine/routing/wasapi_output.cpp:110-159` — `WasapiOutput::open()` takes no buffer size; it initializes `InitializeSharedAudioStream(..., min_frames, ...)` (IAudioClient3 minimum engine period) or the classic default period. Same pattern in `wasapi_capture.cpp` (init at min/default period, `GetBufferSize` at `:509`).
**Detail:** `setBufferSize()` (`audio_engine_impl.cpp:859-881`) only stores `desired_buffer_size_` and restarts. In pure WASAPI mode that value sizes the **chain block** (`:366-369`, `:3117`), not the device streams. `WasapiOutput::latencyMs()` (`wasapi_output.cpp:383-396`) returns `GetStreamLatency()` (cached once at open, `wasapi_output.cpp:196-199`; engine-determined, app-buffer-independent) or `buffer_size_/negotiated_rate_` (engine period). → Displayed `output_ms` is structurally invariant to the buffer-size setting in WASAPI mode.

### Finding 4: The real output-side latency scales with buffer size but is invisible

**Evidence:** `src/audio-engine/routing/audio_engine_impl.cpp:3134-3140` — engine thread paces against `output_ring_buffer_` with `target_fill = 3 * block` where `block = desired_buffer_size_` (`:3117`).
**Deduction:** steady-state output-ring occupancy ≈ 3 blocks ⇒ app-side output delay ≈ `3 × buffer_size / rate` (32 ms @ 512/48 kHz, 64 ms @ 1024/48 kHz) + the WASAPI engine period — none of which appears in `output_ms`. Changing the buffer size *does* change real latency; the indicator just doesn't report that component.

### Finding 5 (ASIO mode): output latency = driver-reported `getLatencies()` only; VST-rate path never reopens the device

**Evidence:**
- ASIO: `output_ms` = JUCE `getOutputLatencyInSamples()` → `third_party/juce-src/modules/juce_audio_devices/native/juce_ASIO_windows.cpp:655`, populated only by `readLatencies()` (`:1021-1029`, called at `:561` during device open) from the driver's `asioObject->getLatencies()`.
- Buffer-size change in ASIO mode does reach the driver: `applyAsioTransport()` (`audio_engine_impl.cpp:2813`) → `setAudioDeviceSetup` compares `bufferSize` (`juce_AudioDeviceManager.cpp:55-63`) → device reopen → `readLatencies()` again. So the *refresh* path exists; whether the *value* moves depends on the driver (many report a fixed latency, some fail → JUCE resets to 0 at `:1023`).
- VST rate: `setSampleRate()` in ASIO mode calls `reconfigureChainInPlace()` (`audio_engine_impl.cpp:964-977`) which by design does **not** reopen the driver (`:902-915`). Device rate and driver latency are untouched by design; only the chain rate + output resampler + `chain_out_fifo_` change — and those contribute `resample_ms = 0` (Finding 1) plus an uncounted up-to-4-chain-block FIFO (`:2620-2623`).

## Deduced Conclusions

### Deduction 1: The indicator answers "what do the drivers report", not "how much delay does this app add"

Based on Findings 1, 3, 4, 5. The readout = (WASAPI loopback stream buffer) + (ASIO/WASAPI driver output latency). Everything the app itself queues — 45 ms capture cushion, 3-block output ring, resampler FIFO, VST plugin latency — is structurally absent. This simultaneously explains all three symptoms: confusing nonzero "in", static "out", and settings that don't move the numbers.

### Deduction 2: In WASAPI mode the buffer-size setting is largely cosmetic for latency

Based on Findings 3, 4: streams are pinned to the engine's min/default period; the setting only changes chain-block granularity (CPU headroom) and the 3-block output-ring occupancy (real latency, unreported).

## Hypothesized Paths

### Hypothesis 1: User premise — "input latency shouldn't exist for a digital tap"

**Status:** Refuted (partially)
**Resolution:** Findings 2 — genuine buffering exists (WASAPI loopback buffer + 45 ms bridge). The real issue is label semantics + under-reporting, not existence.

### Hypothesis 2: ASIO output latency never moves because the driver's `getLatencies()` returns a constant (or 0) per buffer size

**Status:** Open
**Theory:** JUCE refreshes latency on reopen (Confirmed, Finding 5), so a static display means the driver reports a fixed/failed value, or `chooseBestBufferSize` clamping neutralized the request.
**Would confirm:** Runtime check — change buffer 64→512 with ASIO control panel/log readout, or add a session-only debug readout of `getCurrentBufferSizeSamples()` vs `getOutputLatencyInSamples()`.
**Would refute:** Display moving on some other driver/buffer combination.
**Resolution:** Pending hardware run.

## Missing Evidence

| Gap                                              | Impact                                                        | How to Obtain                                              |
| ------------------------------------------------ | ------------------------------------------------------------- | ---------------------------------------------------------- |
| Runtime values per buffer size (ASIO)            | Settles Hypothesis 2 (driver-constant vs. app-side bug)       | Manual run with ASIO device; temporary in-session readout  |
| Actual steady-state ring fill at runtime         | Validates the Deduced 3-block / 45 ms contributions           | Meters or in-session diagnostic snapshot                    |

## Source Code Trace

| Element       | Detail                                                                                 |
| ------------- | -------------------------------------------------------------------------------------- |
| Error origin  | `AudioEngineImpl::latencyProfile()` — `src/audio-engine/routing/audio_engine_impl.cpp:2040` |
| Trigger       | UI 10 Hz timer → `MainWindow::refreshLatencyAndCpu()` — `src/tray-app/ui/main_window.cpp:3288`, `:3431` |
| Condition     | Fields `resample_ms`/`plugin_chain_ms` never written; capture/output rings excluded from sum; WASAPI streams ignore `desired_buffer_size_` |
| Related files | `wasapi_capture.cpp:669`, `wasapi_output.cpp:196,383`, `audio_engine_impl.cpp:72,2660,2813,3117`, `asio_transport.cpp:139`, `juce_ASIO_windows.cpp:561,655,1021` |

## Conclusion

**Confidence:** High for the composition/under-reporting root cause (Confirmed at code level, deterministic); Medium for the ASIO "doesn't move with buffer size" leg (Deduced — refresh path exists, actual driver value unverified without hardware).

The indicator is not lying so much as measuring the wrong thing: it shows driver stream latencies while the app's own bridge buffering (45 ms in, ~3 blocks out, resampler FIFO, VST latency) is absent, `plugin_chain_ms`/`resample_ms` are dead fields, and in WASAPI mode the buffer-size setting never reaches the streams at all.

## Recommended Next Steps

### Fix direction (categorised)

1. **Reporting mechanism:** make `latencyProfile()` sum bridge-inclusive components: capture stream latency + capture ring target fill (45 ms, or live occupancy), chain/VST latency (`AudioProcessorGraph::getLatencySamples()`), resampler latency + FIFO occupancy, output ring target fill (`3 × block`), output stream/driver latency.
2. **Settings coupling (WASAPI):** either apply `desired_buffer_size_` to `WasapiOutput`/`WasapiCapture` (e.g. `SetBufferSize` / period selection) or remove/annotate the buffer-size control in WASAPI mode — it currently cannot affect device latency.
3. **UX semantics:** rename "in" to "capture buffer" (loopback tap delay), since there is no ADC input latency in loopback mode.

### Diagnostic

Temporary in-session readout (no persistent logging, per constitution): display `getCurrentBufferSizeSamples()`, `getOutputLatencyInSamples()`, ring fills in the About/diagnostics dialog while changing ASIO buffer sizes — settles Hypothesis 2.

## Reproduction Plan

1. WASAPI mode: start engine, read `Latency: X (in a + out b)`; change buffer 512→1024 → `out` unchanged (Finding 3); actual output-ring delay doubles invisibly (Finding 4).
2. ASIO mode: change buffer → engine restarts, driver reopens; observe whether `out` moves (driver-dependent, Hypothesis 2).
3. Change VST rate in ASIO mode → chain reconfigures in place, driver untouched → `out` necessarily unchanged; `chain`/`resample` fields stay 0 (Finding 1).

## Side Findings

- In pure WASAPI loopback mode the chain rate is forced to the output endpoint's rate (`audio_engine_impl.cpp:345-349`), so the "VST sampling rate" selector is silently ignored there (documented only indirectly in `audio_engine.h:132-134`).
- `tests/integration/us1_single_plugin_routing_test.cpp:107` asserts `total == capture + output`, i.e. the tests encode the current (incomplete) composition — updating the indicator means updating these contract tests.
- `WasapiOutput::latency_ms_` is cached once at open (`wasapi_output.cpp:196-199`) — fine today because the stream is reopened on restart, but stale if `SetBufferSize` is ever added without reopen.
