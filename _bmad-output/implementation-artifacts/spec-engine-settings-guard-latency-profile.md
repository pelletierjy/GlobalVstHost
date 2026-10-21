---
title: 'Engine settings guard + bridge-inclusive latency profile'
type: 'bugfix'
created: '2026-10-19'
status: 'in-progress'
baseline_commit: '7eecf3ffca5e296030bcdd10568d87d1dcb0a8e2'
context: []
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** (1) Windows occasionally crashes when switching WASAPI -> ASIO with the engine ON: settings mutations hit a live transport teardown. (2) The status-bar latency readout is structurally wrong: `plugin_chain_ms`/`resample_ms` are never assigned anywhere, the app's own capture/output bridge buffering is excluded, so "in" confuses the user and "out" never reacts to buffer-size or VST-rate changes even though the real latency does change.

**Approach:** (1) A UI-side RAII guard stops the engine before each audio settings mutation and restarts it afterwards only if it was running — the engine method then only mutates state. (2) Make `LatencyProfile()` bridge-inclusive: VST chain latency from the graph, capture/output ring-target bridges folded into `capture_ms`/`output_ms`, so the total responds to settings.

## Boundaries & Constraints

**Always:** Guard lives in `src/tray-app/` and uses only `IAudioEngine` (service mode / `IpcEngineProxy` must work unchanged, no build-mode branches). Engine methods keep their own semantics; guard is a no-op when the engine is stopped and safe under exceptions (RAII dtor restarts). Latency reads stay allocation-free/lock-free on audio-thread paths; `latencyProfile()` may take `control_mutex_` (the audio callback never takes it — verified). `/W4 /WX` clean; RT audit scripts pass.

**Ask First:** Any change inside `audioDeviceIOCallbackWithContext`; widening the guard to non-transport setters (volume, bypass, chain slot ops); adding/removing `LatencyProfile` fields (public contract — IPC, soak, diagnostics read it).

**Never:** No logging/telemetry. No tray-app access to engine internals. Do not wrap the audio ON/OFF toggle itself or chain mutations (they already serialize through the SPSC queue). Do not apply settings changes to the WASAPI streams (`WasapiOutput` period selection) — out of scope, indicator-only fix for that leg.

## I/O & Edge-Case Matrix

| Scenario | Input / State | Expected Output / Behavior | Error Handling |
|----------|--------------|---------------------------|----------------|
| Switch WASAPI -> ASIO output, engine ON | user picks ASIO device in output selector | guard stops engine -> handler mutates selection -> guard restarts; engine ends ON; no crash | mutation throws (e.g. `setBufferSize` invalid) -> guard dtor still restarts; existing handler catch reverts the selector |
| Settings change, engine OFF | guard observes not running | no stop/start; setting applied; engine stays OFF | n/a |
| Buffer size 512 -> 1024, pure WASAPI loopback ON | `latencyProfile()` after restart | `total_round_trip_ms` grows by ~one extra block of output bridge (3x block target) | n/a |
| VST rate change, ASIO ON | `latencyProfile()` | chain/resample component reflects the new bridge; total changes | n/a |
| Empty chain | `latencyProfile()` | `plugin_chain_ms` = 0; stream + bridge components still reported, total > 0 | n/a |

</frozen-after-approval>

## Code Map

- `src/tray-app/ui/main_window.cpp` -- all settings handlers (`comboBoxChanged` ~2505-2730; device-loss auto-switch 2573/2631; startup selects 1440/1456; ASIO control panel button -> `openAsioControlPanel`)
- `src/tray-app/ui/engine_settings_guard.h` -- NEW, header-only RAII guard
- `src/audio-engine/routing/audio_engine_impl.cpp:2040` -- `latencyProfile()`; `:72` `kCaptureTargetSeconds`; `:3134` engine-thread output target = 3*block; `:2620` `chain_out_fifo_`; `:425` `last_active_output_transport_kind_` re-sync in `start()`
- `src/audio-engine/routing/audio_engine_impl.h` -- ring members `:209-212`, `chain_out_fifo_ :253`, recursive `control_mutex_ :337`; NEW cached bridge-target atomics
- `src/audio-engine/chain/plugin_chain.h` -- graph wrapper (`:34`, `juce::AudioProcessor`); NEW `totalLatencySamples()`
- `third_party/juce-src/.../juce_AudioProcessor.h:843` -- `getLatencySamples()` accessor to sum per node
- `src/audio-engine/include/jyglobalvst/types.h:172` -- `LatencyProfile` (fields unchanged)
- `src/tray-app/ui/main_window.cpp:3436` -- status-bar readout; `src/tray-app/ui/about_diagnostics.cpp:45-49` -- component dump
- `tests/integration/us1_single_plugin_routing_test.cpp:107`, `tests/integration/us4_buffer_change_test.cpp:33` -- assertions to update/strengthen

## Tasks & Acceptance

**Execution:**
- [x] `src/tray-app/ui/engine_settings_guard.h` -- header-only RAII guard: ctor records `engine->isRunning()`, calls `stop()` if true; dtor calls `start()` only if it was running and still isn't.
- [x] `src/tray-app/ui/main_window.cpp` -- wrap every audio-settings handler with the guard: output/input device selection (incl. ASIO device + ASIO output pair), buffer size, VST rate, WASAPI exclusive toggle, ASIO control panel; leave volume/bypass/chain ops and the ON/OFF toggle untouched.
- [x] `src/audio-engine/chain/plugin_chain.h` / `.cpp` -- add `int totalLatencySamples() const` summing `getLatencySamples()` over the graph's active node processors.
- [x] `src/audio-engine/routing/audio_engine_impl.h` / `.cpp` -- add `std::atomic<int>` cached bridge targets: capture ring target (45 ms on JUCE/ASIO path; live `available()` when target is 0) and output bridge (`3 * chain_block` in pure-WASAPI mode; `chain_out_fifo_`/resampler-active flag otherwise), written in `start()` / `audioDeviceAboutToStart()` / `engineThreadLoop()`.
- [x] `src/audio-engine/routing/audio_engine_impl.cpp` -- `latencyProfile()`: take `control_mutex_`; `plugin_chain_ms` = chain latency / chain rate; `capture_ms` += capture bridge ms; `output_ms` += output bridge ms; total formula unchanged.
- [x] `tests/integration/us1_single_plugin_routing_test.cpp:107` -- assert `total == capture + resample + plugin_chain + output` (components, not just two).
- [x] `tests/integration/us4_buffer_change_test.cpp:33` -- strengthen: `profile_1024.total_round_trip_ms > profile_512.total_round_trip_ms` (buffer-responsive, test name already promises it).

**Acceptance Criteria:**
- Given the engine ON, when the user switches WASAPI -> ASIO output, then the engine restarts once and ends ON; 20 rapid switches produce no crash.
- Given the engine OFF, when any wrapped setting changes, then the engine stays OFF.
- Given pure WASAPI loopback ON, when buffer size doubles, then `total_round_trip_ms` increases by ~one extra block.
- Given a VST reporting `L` samples of latency, then `plugin_chain_ms` ~= L / chain_rate * 1000.
- Given `/W4 /WX`, when built, then zero warnings; affected test exes + `rt_audit.ps1` / `header_audit.ps1` / `no_i18n_audit.ps1` pass.

## Design Notes

The guard must stop the engine BEFORE the engine method: mutation methods self-restart only when `running_` (`setBufferSize` :879, `selectOutput` :771, `setWasapiExclusive` :2027), so a stopped engine just records state and the guard's single restart replaces N internal restarts. `selectOutput`'s transport-switch `reset()` branch is safely bypassed because `start()` re-syncs `last_active_output_transport_kind_` (:425). Bridges are folded into the existing `capture_ms`/`output_ms` fields (no struct change, IPC/soak/diagnostics keep working): "in" becomes "capture stream + capture bridge", "out" becomes "device/driver + output bridge" — exactly the settings-responsive number the user expects.

## Verification

**Commands:**
- `cmake --build build --config Release` -- expected: 0 warnings
- `.\build\tests\Release\us1_single_plugin_routing_test.exe`, `us4_buffer_change_test.exe`, `us1_empty_chain_passthrough_test.exe` -- expected: pass
- `.\tests\audit\rt_audit.ps1` + `header_audit.ps1` + `no_i18n_audit.ps1` -- expected: pass

**Manual checks:**
- Engine ON -> switch WASAPI output to ASIO x20 rapidly: no crash, engine ends ON.
- Status bar: change buffer size / VST rate while ON -> the reported latency visibly changes.
