---
description: "Task list for JyGlobalVST (System Host) — feature 001"
---

# Tasks: JyGlobalVST (System Host)

**Input**: Design documents from `/specs/001-jyglobalvst-system-host/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: Included. Per Constitution §Testing Discipline, this project requires unit tests for DSP, integration tests for WASAPI + VST, latency soak tests, and a plugin-compatibility matrix.

**Organization**: Tasks are grouped by user story (US1–US4) so each can be implemented, tested, and demoed independently. Phase 1 (Setup) and Phase 2 (Foundational) are blocking prerequisites for all stories. Phase 7 (Audio Validation) gates the NON-NEGOTIABLE performance principles. Phase 8 (Polish) covers cross-cutting concerns deferred from the MVP path.

---

## Testable Build Scope (active development filter)

See `plan.md` → **Build Modes**. The active development target is **testable-dev**: full audio engine + VST chain + UI, no driver / no installer / no signing / no service / no hardware-loopback gates. Tasks listed below are **DEFERRED until release prep** — skip them during testable-dev work. Everything not listed here is IN SCOPE for testable-dev.

**Audio source substitution**: the driver-dependent capture endpoint is replaced with a user-selected existing Windows input (WASAPI loopback on the default render device, or a user-installed virtual cable such as Voicemeeter / VB-Cable). T025 keeps the same `WASAPI capture client` code path; only the device-binding step picks a different endpoint until the real driver lands.

### Deferred tasks (release-only)

**Setup phase**:
- **T008** — signing-scripts skeleton *(release: needs EV cert + Authenticode infra)*
- **T009** — WiX 4 MSI placeholder *(release: installer comes with signing)*

**Foundational phase**:
- **T019** — WaveRT virtual driver skeleton *(release: needs WDK + WHQL submission)*
- **T020** — APO companion DLL *(release: paired with driver)*
- **T021** — driver test-signing flag *(release: needs WDK)*

**User Story 1 (MVP)**:
- **T047** — auto-launch via HKCU Run key *(release: installed by MSI)*
- **T048** — WiX 4 MSI installer authoring *(release)*
- **T049** — Authenticode signing wiring *(release)*

**Phase 7 — Audio Validation** (release gate; testable-dev keeps the *script* audits T106 / T107 / T107a but skips hardware-loopback measurement reports):
- **T101** — Scenario 4 single-plugin loopback latency *(release: hardware loopback rig)*
- **T102** — Scenario 4 heavy-chain loopback latency *(release: hardware loopback rig)*
- **T103** — Scenario 8 CPU profile on reference hardware *(release: reference machines)*
- **T104** — Scenario 14 12-hour soak *(release: dedicated runner)*
- **T105** — Scenario 7 sample-rate transitions *(release: hardware verification)*
- **T108** — Pluginval release matrix run *(release: full plugin set; CI smoke-run is in scope for testable-dev under T029)*

**Phase 8 — Polish**:
- **T109..T113** — Accessibility tier (focus rings, accelerators, UIA, NVDA/Narrator script)
- **T114..T121** — Service mode (host, pipe server, per-session auth, IPC client, meter push, installer flag, Scenario 10)
- **T122..T124** — Update-check endpoint, About menu wiring, idle-network test
- **T125** — Multi-session / RDP / Fast-User-Switching integration test
- **T126** — Release-engineering build/sign/stage script
- **T127** — WHQL submission package preparation
- **T129** — Quickstart Scenarios 1–14 VM end-to-end run
- **T130** — Per-PR CI gate workflow
- **T131..T136** — ASIO multi-client driver (research, SDK integration, driver host, format negotiation, Windows registration, DAW validation)

### Kept under testable-dev (do not skip)

- **T010** — WDK detection: the `FindWDK.cmake` no-op fallback is exactly what lets testable-dev compile without WDK installed. Keep.
- **T106 / T107 / T107a** — RT-safety / header-presence / no-i18n audit scripts: run on every commit regardless of build mode. Keep.
- **T126a** — single-output architecture audit: pure static review, no hardware needed. Keep.
- **T128** — README: terse user docs, useful for any tester. Keep.

### Effective testable-dev task count: ~107 / 133 (26 deferred)

---

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3, US4)
- All paths are repository-relative; `src/` and `tests/` per `plan.md` Project Structure.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project skeleton, toolchain, dependency pinning, signing scaffolding.

- [X] T001 Create top-level source directory tree per plan.md: `src/{driver,audio-engine,tray-app,service,shared,installer}/` and `tests/{unit,integration,latency,compat}/`
- [X] T002 Add root `CMakeLists.txt` declaring C++20, MSVC toolchain, x64-only, warnings-as-errors (`/W4 /WX`), in `CMakeLists.txt`
- [X] T003 [P] Vendor JUCE 8.x via CMake `FetchContent` or git submodule under `third_party/juce/`; expose `juce::juce_audio_devices`, `juce::juce_audio_processors`, `juce::juce_gui_basics` in `CMakeLists.txt`
- [X] T004 [P] Vendor `nlohmann/json` 3.11+ via `FetchContent` under `third_party/json/` and expose as `nlohmann_json::nlohmann_json` in `CMakeLists.txt`
- [X] T005 [P] Vendor GoogleTest 1.14+ under `third_party/googletest/` and enable `CTest` in `tests/CMakeLists.txt`
- [X] T006 [P] Add `.clang-format` at repo root with project style (4-space indent, 120-col limit, namespace `jyglobalvst`) in `.clang-format`
- [X] T007 [P] Add `.editorconfig` enforcing CRLF on Windows, UTF-8, trim trailing whitespace in `.editorconfig`
- [ ] T008 [P] Create signing-scripts skeleton (`signtool` invocation reading `JYGLOBALVST_CERT_THUMBPRINT` env var) in `src/installer/signing/sign-binary.ps1` and `src/installer/signing/sign-msi.ps1` *(DEFERRED — release-only)*
- [ ] T009 [P] Add WiX 4 toolset bootstrap (NuGet restore step) and a placeholder `Product.wxs` declaring the upgrade code GUID in `src/installer/wix/Product.wxs` *(DEFERRED — release-only)*
- [X] T010 [P] Configure WDK 10 path detection (`FindWDK.cmake`) and gate driver targets so the build still succeeds without WDK (driver target becomes a no-op) in `cmake/FindWDK.cmake`

**Checkpoint**: `cmake -B build` succeeds on a fresh Windows + MSVC environment with no driver targets enabled by default.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Cross-cutting infrastructure that every user story depends on — driver scaffolding, the `IAudioEngine` contract, the lock-free command queue, JSON validators, the loopback test harness.

**⚠️ CRITICAL**: No user-story work can begin until this phase is complete.

### Shared infrastructure

- [X] T011 [P] Define `IAudioEngine` and `IAudioEngineListener` interfaces per `contracts/audio-engine-api.md` in `src/audio-engine/include/jyglobalvst/audio_engine.h`. Include the REALTIME CONSTRAINTS comment block (Constitution §V).
- [X] T012 [P] Implement lock-free single-producer / single-consumer command queue (`SpscCommandQueue<T, Capacity>`) using `std::atomic` indices in `src/shared/concurrency/spsc_queue.h`
- [X] T013 [P] Implement `RealtimeClock` (`QueryPerformanceCounter` wrapper, RT-safe) in `src/shared/platform/realtime_clock.h`
- [X] T014 [P] Implement Windows path resolvers (`%AppData%`, `%LocalAppData%`, `%UserProfile%\Documents`, `%ProgramFiles%`) in `src/shared/platform/known_folders.cpp`
- [X] T015 [P] Implement session-scoped named-mutex helper (`Local\JyGlobalVST.<name>.<SessionId>`) in `src/shared/platform/session_mutex.cpp`
- [X] T016 [P] Implement WASAPI endpoint enumeration wrapper (`IMMDeviceEnumerator` + `IMMNotificationClient`) in `src/shared/platform/audio_endpoints.cpp`
- [X] T017 [P] Implement nlohmann/json wrapper with strict / tolerant validation modes in `src/shared/json/json_validator.h` and `.cpp`
- [X] T018 [P] Implement hand-rolled validators for all four schemas (preset, settings, scan-cache, update-manifest) using contracts/*.json as reference in `src/shared/json/validators/{preset,settings,scan_cache,update_manifest}_validator.cpp`

### Driver scaffolding

- [X] T019 Create Sysvad-derived WaveRT virtual endpoint driver skeleton (`.inf`, `.sys` build target, channel-count locked to 2 per FR-003, supported sample rates {44100, 48000, 96000, 176400, 192000} per FR-002) in `src/driver/waveRT/jyglobalvst-driver.inf` and `src/driver/waveRT/main.cpp`
- [X] T020 [P] Create Stream Effect APO companion DLL for format negotiation in `src/driver/apo/jyglobalvst_apo.cpp`
- [X] T021 Implement test-signing build flag (`JYGLOBALVST_TEST_SIGNED=ON`) that signs the driver `.cat` with a development cert and emits a `bcdedit /set testsigning on` reminder; production builds reject this flag in `cmake/DriverSigning.cmake`

### Audio engine skeleton (pass-through, no plugins)

- [X] T022 Implement pass-through `AudioEngine` (capture from virtual endpoint → 32-bit float → output to hardware endpoint, no chain) in `src/audio-engine/routing/audio_engine_impl.cpp` and `src/audio-engine/routing/audio_engine_impl.h`. REALTIME CONSTRAINTS header MANDATORY (Constitution §V).
- [X] T023 Implement `WindowedSinc` resampler binding (JUCE `Interpolators::WindowedSinc`) wrapped in a pre-allocated state object in `src/audio-engine/routing/resampler.cpp`
- [X] T024 Implement format converter (Int16 ↔ Float32, Int24 packed ↔ Float32) with no allocation on hot path in `src/audio-engine/routing/format_convert.cpp`
- [X] T025 Implement WASAPI capture client (`IAudioClient3::InitializeSharedAudioStream` from the JyGlobalVST virtual endpoint) in `src/audio-engine/routing/wasapi_capture.cpp` *(implemented as direct `IAudioClient` shared-mode capture thread with `IAudioCaptureClient` polling; supports mixed-driver mode where ASIO output is paired with WASAPI input)*
- [X] T026 Implement WASAPI output client (`IAudioClient3::InitializeSharedAudioStream` to user-selected hardware endpoint, sample-rate negotiation per FR-014) in `src/audio-engine/routing/wasapi_output.cpp` *(testable-dev: delegated to `juce::AudioDeviceManager`; release prep replaces with direct `IAudioClient3`)*

### Test harness

- [X] T027 [P] Create virtual-loopback test fixture for CI (second WaveRT instance in loop mode) in `tests/integration/loopback_fixture.cpp` *(testable-dev: WASAPI loopback on default render device + GTest fixture; release prep adds actual virtual driver loopback)*
- [ ] T028 [P] Create hardware-loopback latency harness scaffolding (USB interface + TRS cable; documented setup; runs locally not in CI) in `tests/latency/hardware_loopback_runner.cpp` and `tests/latency/README.md` *(DEFERRED — release-only)*
- [X] T029 [P] Wire Pluginval CLI into `tests/compat/run_pluginval.ps1` with a configurable plugin path list; CI gates on PASS for ARC X, Sonarworks Reference, ReaEQ, FabFilter Pro-Q 3
- [X] T030 [P] Unit tests for: `SpscCommandQueue` (T012), `RealtimeClock` (T013), format converters Int16/Int24↔Float32 (T024), WindowedSinc resampler accuracy at all five sample rates with round-trip THD measurement (T023), chain-mutation correctness (insert/remove/reorder semantic equivalence — fixture only, no engine), and all four JSON validators (T017, T018) in `tests/unit/{spsc_queue_test,realtime_clock_test,format_convert_test,resampler_quality_test,chain_mutation_semantics_test,json_validator_test}.cpp`
- [X] T030a [P] Unit tests for `LockFreeAudioRingBuffer` (mixed-driver cross-thread audio buffer) in `tests/unit/lockfree_ring_buffer_test.cpp`

**Checkpoint**: Pass-through audio works end-to-end (system audio → virtual device → engine → hardware), measured at < 5 ms with no chain. All shared infrastructure components compile and unit-test green. User-story implementation can now begin in parallel.

---

## Phase 3: User Story 1 — Apply VST processing to all system audio (Priority: P1) 🎯 MVP

**Goal**: A user can install JyGlobalVST, set it as Windows default output, load **one** VST3 plugin, select a hardware output, and hear processed audio. Single-plugin chain only.

**Independent Test** (Scenario 1 from `quickstart.md`): After install, set JyGlobalVST as default output, load one VST plugin via a file picker (full scan deferred to US2), play any audio source, confirm processing audible on selected hardware. Total time install → audio under 2 minutes (SC-001). Round-trip latency ≤ 10 ms with default buffer (AUDIO-001).

### Tests for User Story 1 ⚠️

> Write FIRST, ensure FAIL before implementation. Tests use the virtual-loopback fixture from T027.

- [X] T031 [P] [US1] Integration test: virtual device registered + selectable as Windows default in `tests/integration/us1_virtual_device_registered_test.cpp`
- [X] T032 [P] [US1] Integration test: load one plugin via file picker, audio routes through plugin to hardware output in `tests/integration/us1_single_plugin_routing_test.cpp`
- [X] T033 [P] [US1] Integration test: hardware output device removal triggers auto-fallback to Windows default + restore on reconnect (FR-024) in `tests/integration/us1_device_removal_restore_test.cpp`
- [X] T034 [P] [US1] Integration test: system sleep/wake reinitializes audio path without user intervention (FR-025) in `tests/integration/us1_sleep_wake_test.cpp`
- [X] T035 [P] [US1] Contract test: `IAudioEngine.start/stop/selectOutput/setBufferSize` idempotency and listener events in `tests/contract/us1_audio_engine_api_test.cpp`
- [X] T035a [P] [US1] Integration test: empty chain (no plugins) passes audio through transparently with measured added latency < 1 ms vs. raw loopback per spec.md Edge Cases ("No VST plugins loaded") in `tests/integration/us1_empty_chain_passthrough_test.cpp`

### Implementation for User Story 1

- [X] T036 [P] [US1] Implement `Plugin` and `PluginInstance` entities per `data-model.md` §3, §4 in `src/audio-engine/vst-host/plugin_instance.h/.cpp`
- [X] T037 [P] [US1] Implement `HardwareOutputDevice` entity + endpoint-ID/friendly-name tracking per `data-model.md` §2 in `src/audio-engine/routing/hardware_output.h/.cpp`. REALTIME CONSTRAINTS header MANDATORY for the .cpp (Constitution §V).
- [X] T038 [US1] Implement `VST3PluginLoader` (JUCE `VST3PluginFormat` + `AudioPluginInstance` instantiation by `.vst3` bundle path) in `src/audio-engine/vst-host/vst3_loader.cpp`
- [X] T039 [US1] Wrap plugin `processBlock` in SEH `__try / __except` + C++ `try / catch(...)` per `research.md` §5; bypass-on-failure with no allocation on failure path; pre-allocated UI notification slots in `src/audio-engine/vst-host/seh_wrapper.cpp` (depends on T038)
- [X] T040 [US1] Extend `AudioEngine` from T022 to a single-plugin chain (`SinglePluginChain` adapter implementing `IAudioProcessor`) in `src/audio-engine/chain/single_plugin_chain.cpp` (depends on T038, T039)
- [X] T025a Mixed-driver audio support: allow ASIO output + WASAPI input simultaneously. Implements `LockFreeAudioRingBuffer`, `WasapiCapture` (direct `IAudioClient` capture thread), and modifies `AudioEngineImpl` to read input from ring buffer in ASIO callback when `mixed_mode_active_` is true. Extends `DeviceWatchdog` to monitor capture endpoint loss. in `src/audio-engine/routing/{wasapi_capture.cpp,audio_engine_impl.cpp,device_watchdog.cpp}`
- [X] T041 [US1] Implement device-removal detection (`IMMNotificationClient::OnDeviceStateChanged`) and auto-fallback to Windows default output per FR-024 in `src/audio-engine/routing/device_watchdog.cpp` (depends on T026, T037)
- [X] T042 [US1] Implement device-restore: when preferred endpoint reconnects, switch back automatically and emit `IAudioEngineListener::onDeviceRestored` per FR-024 in `src/audio-engine/routing/device_watchdog.cpp`
- [X] T043 [US1] Implement sleep/wake reinitialization (`WM_POWERBROADCAST` handler in tray app forwards `PBT_APMSUSPEND`/`PBT_APMRESUMEAUTOMATIC` to engine; engine releases + reacquires WASAPI clients) per FR-025 in `src/audio-engine/routing/power_handler.cpp` and `src/tray-app/ui/main_window.cpp`
- [X] T044 [P] [US1] Implement single-instance enforcement using `T015` session mutex; second launch focuses existing window via well-known class name and `WM_BRINGTOFRONT` message per FR-022j in `src/tray-app/single-instance/single_instance.cpp`
- [X] T045 [US1] Implement minimal tray UI: tray icon, main window with hardware-output dropdown (populated from `IAudioEngine::listOutputs`), "Load plugin…" file picker (`.vst3` only), one plugin slot, audio on/off toggle in `src/tray-app/ui/main_window.cpp` (depends on T040 for finalized chain shape per Constitution §III)
- [X] T046 [P] [US1] Implement device-resolution priority chain per FR-022m (endpoint ID → friendly name → Windows default) and surface resolution source in device-selector tooltip in `src/audio-engine/routing/device_resolver.cpp` (depends on T037)
- [ ] T047 [US1] Wire tray app to auto-launch on user login via HKCU `Run` key set by the MSI custom action; default install mode is user-mode (FR-027) in `src/installer/wix/RunKey.wxs`
- [ ] T048 [US1] Author MSI installer (WiX 4): bundles driver `.cat`/`.inf`/`.sys`, APO DLL, tray app binary, JUCE runtime; single UAC prompt; no reboot; full uninstall per FR-030; Authenticode-signed per FR-031 in `src/installer/wix/Product.wxs`, `src/installer/wix/DriverPackage.wxs`, `src/installer/wix/TrayApp.wxs`
- [ ] T049 [US1] Wire Authenticode signing into the MSI build (T008 scripts invoked from CMake `add_custom_command` post-link) in `cmake/SigningTargets.cmake`

**Checkpoint** (US1 MVP complete): Install MSI → JyGlobalVST appears in Windows Sound → load one plugin via file picker → audio processed end-to-end → device removal/restore handled → sleep/wake handled → second launch focuses existing window. Round-trip latency ≤ 10 ms (validated in Phase 7).

---

## Phase 4: User Story 2 — Manage a chain of VST plugins (Priority: P2)

**Goal**: User can scan VST3 directories, build an ordered chain of multiple plugins, bypass individual plugins, reorder without audio dropout, and remove plugins.

**Independent Test** (Scenario 2): Scan plugins, load three into a chain in specified order, play audio, verify processing order. Toggle bypass on middle plugin; reorder live; remove a plugin. No audio dropouts during any mutation (FR-010).

### Tests for User Story 2 ⚠️

- [X] T050 [P] [US2] Integration test: background plugin scan (cancellable, incremental, progress reported) finds default-path plugins per FR-005 in `tests/integration/us2_plugin_scan_test.cpp`
- [X] T051 [P] [US2] Integration test: build a 3-plugin chain; verify processing order; toggle bypass on middle plugin; remove and re-add in `tests/integration/us2_chain_mutation_test.cpp`
- [X] T052 [P] [US2] Integration test: live reorder during playback — measure for audio dropout (zero allowed) per FR-010 in `tests/integration/us2_live_reorder_test.cpp`
- [X] T053 [P] [US2] Integration test: plugin GUI open/close while audio plays; GUI crash does NOT stop audio in `tests/integration/us2_plugin_editor_test.cpp`
- [X] T054 [P] [US2] Contract test: `chain.add/remove/move/set_bypass/repoint_placeholder` IPC commands round-trip correctly through the in-process adapter in `tests/contract/us2_chain_commands_test.cpp`

### Implementation for User Story 2

- [X] T055 [P] [US2] Implement `PluginScanner` (background `std::thread`, atomic-cancellable, incremental enqueue to UI thread, progress reporting) per FR-005 and `research.md` §4 in `src/audio-engine/vst-host/plugin_scanner.cpp`
- [X] T056 [P] [US2] Implement scan-cache persistence (`scan-cache.json` per `contracts/scan-cache-schema.json`) in `src/audio-engine/vst-host/scan_cache.cpp`. Note: scan-cache is NOT touched from the audio thread; no REALTIME CONSTRAINTS header required.
- [X] T057 [US2] Pre-seed default scan paths (`%ProgramFiles%\Common Files\VST3`, `%LocalAppData%\Programs\Common\VST3`) per FR-005 in `src/audio-engine/vst-host/default_scan_paths.cpp` (depends on T014, T055)
- [X] T058 [US2] Implement `PluginChain` (`juce::AudioProcessorGraph` wrapper, ordered slots, `chain_revision` monotonic counter) per `data-model.md` §6 in `src/audio-engine/chain/plugin_chain.cpp`
- [X] T059 [US2] Implement chain mutation commands (`addPlugin`, `removeSlot`, `moveSlot`, `setBypass`, `setParameter`) routed through the SPSC queue (T012); audio thread drains commands at top of each `processBlock` in `src/audio-engine/chain/chain_commands.cpp` (depends on T012, T058)
- [X] T060 [US2] Replace the single-plugin chain (T040) with the multi-plugin chain (T058) in `IAudioEngine` implementation; preserve API surface so US1 callers are unaffected in `src/audio-engine/routing/audio_engine_impl.cpp`
- [X] T061 [P] [US2] Implement plugin editor window (JUCE `AudioProcessorEditor` host window, show/hide via `IAudioEngine::openEditor/closeEditor`) with separate SEH guard around editor message-pump calls per `research.md` §5 follow-up in `src/tray-app/ui/plugin_editor_window.cpp`
- [X] T062 [US2] Implement chain editor UI: scrollable list with depth indicator, drag-to-reorder, per-slot bypass + remove buttons, "Add plugin" from scanned-plugin chooser, no hard chain-length limit per FR-008 in `src/tray-app/ui/chain_editor.cpp`
- [X] T063 [P] [US2] Implement scan UI: progress indicator showing current path + plugin count, cancel button, incremental plugin appearance in chooser per FR-005 in `src/tray-app/ui/scan_dialog.cpp`
- [X] T064 [US2] Wire user-managed scan paths (add / remove / disable additional directories beyond defaults) and persist to `settings.json` (T091) per FR-005 in `src/tray-app/ui/settings_dialog_scan_paths.cpp`

**Checkpoint** (US2 complete): Multi-plugin chains work, scan is non-blocking and cancellable, mutations are dropout-free. Latency under heavy chain validated in Phase 7.

> **Bug fix during US2 completion** (2026-06-06): `VST3PluginLoader::load()` passed `&format_` (a stack member) to `juce::AudioPluginFormatManager::addFormat()`, which takes ownership via `OwnedArray`. The manager's destructor then `delete`d the stack member, causing `STATUS_HEAP_CORRUPTION` (0xc0000374) when loading real VST3 plugins. Fixed by allocating a fresh `juce::VST3PluginFormat` on the heap with `new` for each `load()` call, letting the local `AudioPluginFormatManager` own it. This also allowed restoring the intended background `std::thread` in `PluginScanner` (T055), which had been temporarily made synchronous as a misdiagnosed workaround. All 140 tests now pass.

---

## Phase 5: User Story 3 — Save and recall presets (Priority: P3)

**Goal**: User saves named chain configurations and restores them. Includes preset import/export, drag-and-drop, missing-plugin placeholder handling, auto-save on close, schema migration, and the roaming/local settings split.

**Independent Test** (Scenario 3): Configure a chain, save as "Gaming". Clear chain. Load "Gaming" — chain rebuilds with order/params/bypass preserved. Remove a plugin's `.vst3` from disk; reload "Gaming" — placeholder appears in middle slot; non-modal notification lists missing plugin; re-point or remove the placeholder. Edit a preset to add an unknown field — import rejected per FR-022g-1.

### Tests for User Story 3 ⚠️

- [X] T065 [P] [US3] Integration test: save chain → quit → relaunch → load preset → chain matches original (FR-021, FR-022a) in `tests/integration/us3_preset_round_trip_test.cpp`
- [X] T066 [P] [US3] Integration test: load preset with missing plugin → placeholder appears, audio bypasses it, re-point works (FR-022f, FR-022g-2) in `tests/integration/us3_placeholder_test.cpp`
- [X] T067 [P] [US3] Integration test: import malformed preset (unknown field, > 50 MB file, > 16 MB state chunk) → rejected with no partial state per FR-022g-1 in `tests/integration/us3_import_validation_test.cpp`
- [X] T068 [P] [US3] Integration test: auto-save on close → relaunch → state restored (FR-022c, FR-022d, FR-022e) in `tests/integration/us3_autosave_test.cpp`
- [X] T069 [P] [US3] Integration test: schema-version migration (v1 + future v2 stub) preserves unknown fields on round-trip (FR-022b) in `tests/integration/us3_schema_migration_test.cpp`
- [X] T070 [P] [US3] Integration test: device resolution priority (endpoint ID → friendly name → default) reported via tooltip per FR-022m in `tests/integration/us3_device_resolution_test.cpp`
- [X] T071 [P] [US3] Unit test: preset JSON schema validator rejects all invalid documents in the negative-test corpus in `tests/unit/preset_validator_test.cpp`

### Implementation for User Story 3

- [X] T072 [P] [US3] Implement `Preset` and `PresetSlot` entities per `data-model.md` §7 + serialization to `contracts/preset-schema.json` in `src/tray-app/presets/preset.h/.cpp`
- [X] T073 [P] [US3] Implement `PlaceholderInstance` entity per `data-model.md` §5 (audio-transparent slot holding `pending_state_chunk`) in `src/audio-engine/chain/placeholder_instance.h/.cpp`
- [X] T074 [US3] Extend `PluginChain` to support mixed `PluginInstance` + `PlaceholderInstance` slots; chain skips placeholders in audio path in `src/audio-engine/chain/plugin_chain.cpp` (extends T058)
- [X] T075 [US3] Implement preset save (serialize chain + buffer/device metadata, base64-encode `state_chunk` via VST3 `getStateInformation`) in `src/tray-app/presets/preset_writer.cpp` (depends on T072)
- [X] T076 [US3] Implement preset load with resolution by `(plugin_uid, vendor, name)` against scan cache per FR-022g-2; unresolved slots become placeholders per FR-022f in `src/tray-app/presets/preset_loader.cpp` (depends on T056, T072, T073, T074)
- [X] T077 [US3] Implement strict import validator (unknown-fields rejection, file ≤ 50 MB, state chunk ≤ 16 MB decoded, schema version recognized) per FR-022g-1 in `src/tray-app/presets/preset_import_validator.cpp` (depends on T017, T018)
- [X] T078 [P] [US3] Implement drag-and-drop import handler on main window + Import Preset… file picker (filter to `*.jvst` files only); on filename collision, prompt overwrite/rename/cancel per FR-022g in `src/tray-app/ui/preset_import_handler.cpp`
- [X] T079 [P] [US3] Implement "Export Preset…" action (file picker) and "Reveal in Explorer" action (`ShellExecuteW`) per FR-022h in `src/tray-app/ui/preset_export_actions.cpp`
- [X] T080 [P] [US3] Implement schema-version migration framework (`migratePreset(json, fromVersion, toVersion)`) preserving unknown fields per FR-022b in `src/tray-app/presets/preset_migrator.cpp`
- [X] T081 [US3] Implement auto-save: serialize current chain + device + buffer to `%LocalAppData%\JyGlobalVST\autosave.json` on app close per FR-022c; not visible in preset list in `src/tray-app/presets/autosave.cpp`
- [X] T082 [US3] Implement auto-restore on launch: read `autosave.json` if present + parseable; on corruption silently discard + start blank per FR-022d; NO crash detection / no sentinel file per clarification #22 in `src/tray-app/presets/autosave.cpp`
- [X] T083 [US3] Implement preset-override-of-autosave flag (FR-022e): when user explicitly loads a preset, set an in-memory flag that suppresses the auto-save write at exit in `src/tray-app/presets/autosave.cpp`
- [X] T084 [P] [US3] Implement "Re-point placeholder" UI action: opens scanned-plugin chooser filtered to compatible plugins; on selection, calls `IAudioEngine::repointPlaceholder` and applies `pending_state_chunk` in `src/tray-app/ui/placeholder_repoint_dialog.cpp`
- [X] T085 [P] [US3] Implement OneDrive/Drive-tolerant preset-folder scanner (handle eventual-consistency file appearances/disappearances without crashing) per FR-022i in `src/tray-app/presets/preset_folder_scanner.cpp`
- [X] T086 [US3] Create `%UserProfile%\Documents\JyGlobalVST\Presets\` on first launch if absent per FR-022 in `src/tray-app/presets/preset_folder_init.cpp`
- [X] T087 [P] [US3] Implement `Settings` entity (roaming `settings.json`) per `data-model.md` §9 + `contracts/settings-schema.json`; tolerant of unknown fields on read, preserves them on write per FR-022k in `src/tray-app/settings/roaming_settings.cpp`
- [X] T088 [P] [US3] Implement `LocalState` file family (`window-state.json`, `endpoint-last.json`) per `data-model.md` §10 + FR-022l in `src/tray-app/settings/local_state.cpp`
- [X] T089 [US3] Wire device resolution priority chain (T046) to read endpoint ID from `endpoint-last.json` (priority 1) and friendly-name from `settings.json` (priority 2) per FR-022m in `src/audio-engine/routing/device_resolver.cpp`

**Checkpoint** (US3 complete): Presets save/load round-trip with full fidelity. Placeholders handle missing plugins gracefully. Strict import validation rejects malformed files. Auto-save preserves state across restarts. Roaming vs local state split honored.

---

## Phase 6: User Story 4 — Monitor audio levels and latency in real time (Priority: P3)

**Goal**: Real-time visibility into input/output levels, current round-trip latency, and CPU usage. Buffer-size selector lets the user trade latency for CPU headroom.

**Independent Test** (Scenario 4 + Scenario 8): Audio playing — input + output meters respond, latency readout displays current ms value. Change buffer size — latency display updates. Heavy chain triggers CPU warning at 5% threshold (FR-026).

### Tests for User Story 4 ⚠️

- [X] T090 [P] [US4] Integration test: meter values track playback levels within ±1 dB on synthetic test signal in `tests/integration/us4_meters_test.cpp`
- [X] T091 [P] [US4] Integration test: buffer-size change applies live; latency readout updates within one buffer in `tests/integration/us4_buffer_change_test.cpp`
- [X] T092 [P] [US4] Integration test: synthetic heavy chain pushes CPU > 5%; warning appears within 1 s; warning clears within 1 s after load drops (FR-026) in `tests/integration/us4_cpu_warning_test.cpp`

### Implementation for User Story 4

- [X] T093 [P] [US4] Implement `LatencyProfile` entity (capture / resample / chain / output components) per `data-model.md` §12 in `src/audio-engine/monitoring/latency_profile.cpp`. REALTIME CONSTRAINTS header MANDATORY (Constitution §V).
- [X] T094 [P] [US4] Implement `CPUMonitor` (per-callback `QueryPerformanceCounter` ratio, rolling 1-second mean, xrun counter) per `research.md` §8 + `data-model.md` §13 in `src/audio-engine/monitoring/cpu_monitor.cpp`. REALTIME CONSTRAINTS header MANDATORY (Constitution §V).
- [X] T095 [US4] Wire CPU monitor to listener event `onCpuWarning` when `rolling_1s_pct ≥ 5%`; warn-only (no auto-bypass, no auto-buffer-increase) per FR-026 in `src/audio-engine/monitoring/cpu_monitor.cpp`
- [X] T096 [P] [US4] Implement level-meter taps (peak + RMS, RT-safe ring buffer of 30 Hz samples) at engine input and output in `src/audio-engine/monitoring/level_meters.cpp`. REALTIME CONSTRAINTS header MANDATORY (Constitution §V).
- [X] T097 [US4] Implement buffer-size selector dropdown ({32, 64, 128, 256, 512, 1024}); selection calls `IAudioEngine::setBufferSize`, persists to roaming `settings.json` as default per FR-015 + FR-022k in `src/tray-app/ui/buffer_size_dropdown.cpp`
- [X] T098 [US4] Implement meters UI (input L/R + output L/R) with peak-hold + clip indicator in `src/tray-app/ui/meters_panel.cpp`
- [X] T099 [US4] Implement latency readout display (FR-019) showing total + per-component breakdown in tooltip in `src/tray-app/ui/latency_readout.cpp`
- [X] T100 [US4] Implement persistent CPU-warning banner ("CPU approaching limit; consider increasing buffer size") visible until rolling-1s drops below threshold per FR-026 in `src/tray-app/ui/cpu_warning_banner.cpp`

**Checkpoint** (US4 complete): All meters, latency readout, buffer dropdown, and CPU warning are wired and visually correct.

---

## Phase 7: Audio Validation (NON-NEGOTIABLE per Constitution §II)

**Purpose**: Validate the four NON-NEGOTIABLE performance metrics — latency, CPU, stability, sample-rate handling — against the SUCCESS criteria in `spec.md`.

**⚠️ CRITICAL**: Audio features MUST NOT advance to release until every task in this phase passes. Failures here BLOCK release independently of any user story status.

- [ ] T101 [AUDIO/LATENCY] Run Scenario 4 (single-plugin loopback via hardware USB interface): measure round-trip latency at 256-sample buffer; record CPU model, sample rate, plugin used, measured ms; gate: ≤ **10 ms** per AUDIO-001 in `tests/latency/scenario4_single_plugin.md` (report)
- [ ] T102 [AUDIO/LATENCY] Run Scenario 4 (5-plugin chain via hardware loopback): measure worst-case; gate: ≤ **20 ms** per AUDIO-005 in `tests/latency/scenario4_heavy_chain.md` (report)
- [ ] T103 [AUDIO/LATENCY] Run Scenario 8 (CPU profiling): typical 3-plugin chain on Intel i5 8th gen / Ryzen 5 3000-class; gate: ≤ **5%** rolling-1s per AUDIO-002 in `tests/latency/scenario8_cpu_profile.md` (report)
- [ ] T104 [AUDIO/LATENCY] Run Scenario 14 (12-hour soak): typical chain, looping playlist; gate: **0 xruns**, no drift, memory growth ≤ 200 MB per AUDIO-003 + SC-005 in `tests/latency/scenario14_soak.md` (report)
- [ ] T105 [AUDIO] Run Scenario 7 (sample-rate transitions): 44.1 ↔ 48 ↔ 96 ↔ 176.4 ↔ 192 kHz; gate: no audible artifacts per AUDIO-004 in `tests/latency/scenario7_sample_rate.md` (report)
- [X] T106 [AUDIO] Real-time-constraint audit: grep all `.cpp/.h` files for `malloc/new/std::vector::push_back/std::mutex` inside any function reachable from `processBlock`; gate: zero hits, or each hit annotated with a `// RT-SAFE: reason` comment that survives review per Constitution §V in `tests/audit/rt_audit.ps1`
- [X] T107 [AUDIO] REALTIME CONSTRAINTS header presence audit: every `.cpp/.h` file under `src/audio-engine/routing/`, `src/audio-engine/chain/`, `src/audio-engine/vst-host/`, `src/audio-engine/monitoring/` MUST carry the Constitution §V header; CI gate in `tests/audit/header_audit.ps1`
- [X] T107a English-only audit per FR-019c: grep the repo for string-externalization artifacts (`*.po`, `*.pot`, `*.mo`, `*.resx`, `*.resw`, `gettext`, `wxLocale`, `juce::TRANS`); gate: zero hits, or each hit annotated with a justification comment. Reject any introduced i18n framework in `tests/audit/no_i18n_audit.ps1`
- [ ] T108 [AUDIO] Plugin compatibility matrix: Pluginval run against the full release matrix (ARC X, Sonarworks Reference, ReaEQ, FabFilter Pro-Q 3, plus the negative-test crashing plugin from T039); document results in `tests/compat/release_matrix.md`

**Checkpoint**: All NON-NEGOTIABLE gates pass. No regression vs. previous release. Reports archived for audit trail.

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Service-mode install, accessibility, single-instance multi-session edge cases, update check, CI gating, and documentation. None of these block the MVP (US1) but every one is in the v1 scope.

### Accessibility (FR-019a, FR-019b)

- [X] T109 [P] Implement full keyboard navigation: tab order across main window, chain editor, device dropdown, meters, About dialog; visible focus indicator (2 px ring overlay) per FR-019a in `src/tray-app/ui/accessibility/focus_handling.cpp`
- [X] T110 [P] Implement keyboard accelerators: load preset (Ctrl+O), save preset (Ctrl+S), bypass plugin (Ctrl+B with chain focus), open plugin GUI (Enter on chain slot), scan plugins (Ctrl+R) per FR-019a in `src/tray-app/ui/accessibility/keyboard_accelerators.cpp`
- [X] T111 Implement UIA accessible names/roles/values via JUCE `AccessibilityHandler` for every interactive control + meters per FR-019b in `src/tray-app/ui/accessibility/uia_handlers.cpp`
- [X] T112 Implement UIA dynamic-event notifications (`UiaRaiseNotificationEvent`) for plugin added/removed, bypass toggled, device disconnect, CPU warning per FR-019b in `src/tray-app/ui/accessibility/uia_notifications.cpp`
- [X] T113 [P] Integration test: NVDA / Narrator script verifies all primary actions reachable by keyboard and announced (Scenario 12) in `tests/integration/polish_accessibility_test.cpp`

### Service mode (FR-027, FR-028, FR-028a, FR-028b, FR-029)

- [X] T114 [P] Implement Windows Service host (`SCM`-registered) that owns the same `AudioEngine` as the in-process build; lifecycle: install / start / stop / uninstall in `src/service/host/service_main.cpp`
- [X] T115 [P] Implement named-pipe IPC server: pipe name `\\.\pipe\JyGlobalVST\v1\<session_id>`, ACL restricted to interactive user session, JSON envelope framing per `contracts/ipc-protocol.md` in `src/service/ipc-server/pipe_server.cpp`
- [X] T116 Implement per-session authentication: `GetNamedPipeClientProcessId` → `OpenProcessToken` → `TokenSessionId` match check; reject mismatched sessions per FR-028a in `src/service/ipc-server/session_auth.cpp` (depends on T115)
- [X] T117 [P] Implement IPC client in the tray app: connect to service pipe; protocol negotiation via `hello`; dispatch `IAudioEngine` calls as IPC envelopes when service mode is detected in `src/tray-app/ipc-client/engine_proxy.cpp`
- [X] T118 [P] Implement IPC meter-frame push (30 Hz) from service to tray via the same pipe; client drains and dispatches to UI thread; FUTURE: migrate to shared-memory ring if profiling shows JSON overhead matters in `src/service/ipc-server/meter_pusher.cpp` and `src/tray-app/ipc-client/meter_listener.cpp`
- [X] T119 Extend installer to support `INSTALL_AS_SERVICE=1` advanced flag: installs service binary + sets startup type Automatic; mutual-exclusion with user-mode per FR-028b; mode switch preserves auto-save + presets per FR-028b in `src/installer/wix/ServiceMode.wxs`
- [X] T120 Implement About → Diagnostics surface showing engine host mode ("In-Process" vs "Windows Service") per FR-029 in `src/tray-app/ui/about_diagnostics.cpp`
- [X] T121 Integration test: Scenario 10 (service-mode pre-login audio via lock-screen sound + tray UI reconnection at login) in `tests/integration/polish_service_mode_test.cpp`

### Update check (FR-022n, FR-022o)

- [X] T122 Implement explicit user-initiated update check: HTTPS GET against `update_check_endpoint_url` from settings; parse response per `contracts/update-manifest-schema.json`; display dialog with version + release notes + download link; no auto-poll, no auto-download per FR-022n + FR-022o in `src/tray-app/updates/update_check.cpp`
- [X] T123 [P] Implement About → Check for updates… menu item bound to T122 in `src/tray-app/ui/about_menu.cpp`
- [X] T124 [P] Verify zero background network activity: `netstat`/Wireshark check during a 5-minute idle session (Scenario 13) in `tests/integration/polish_no_network_idle_test.cpp`

### Multi-session edge cases (FR-022j extended)

- [ ] T125 Integration test: Fast-User-Switching produces independent tray-app instances per session; RDP produces an independent instance; service-mode service is single-instance machine-wide but multiple tray clients coexist (Scenario 11) in `tests/integration/polish_multi_session_test.cpp`

### Release prep

- [X] T126 [P] Author release-engineering script that builds, signs, and stages MSI artifacts; embeds version into manifest; outputs SHA256 in `tools/release/build_release.ps1`
- [X] T126a Architecture audit for FR-013a: grep `src/audio-engine/routing/` and `src/audio-engine/chain/` for single-output singleton assumptions (e.g., `HardwareOutputDevice* output_` member treated as the only output, hardcoded `1` channel-count in output paths, contracts that assume one destination endpoint). Document findings; refactor any singleton assumptions into a 1-element collection so future N-way fan-out is additive. Report in `tests/audit/fr_013a_singleoutput_audit.md`
- [ ] T127 [P] WHQL submission package preparation: driver `.cab`, HLK test results placeholder, attestation manifest in `src/installer/whql/submission_package.md`
- [X] T128 [P] User documentation: README describing install, first-run setup, hardware-output selection, common troubleshooting — keep terse, no marketing copy in `docs/README.md`
- [ ] T129 Run `quickstart.md` Scenarios 1–14 end-to-end on a clean Windows 11 VM and a clean Windows 10 1909 VM; capture pass/fail in `tests/release/quickstart_run_<date>.md`

### CI gating (Constitution §"Code Review Gate")

- [X] T130 Per-PR CI workflow for audio-component PRs: runs 30-min soak (xrun count) + virtualized-loopback latency measurement + CPU usage report; gates merge per Constitution "Code Review Gate" in `.github/workflows/audio-pr-gate.yml`

### ASIO multi-client driver (FR-013b, FR-032)

- [ ] T131 [P] Research/design: document ASIO driver architecture, shared-memory ring buffer layout, event signaling protocol, and multi-client open/close state machine. Review Steinberg ASIO SDK 2.3 license terms and confirm redistribution rights for compiled driver binary in `specs/001-jyglobalvst-system-host/research.md` §16 *(DEFERRED — release-only: needs Steinberg SDK + legal review)*
- [ ] T132 [P] Integrate Steinberg ASIO SDK 2.3 as a build-only dependency: download from Steinberg developer site, consume headers in `src/driver/asio/`, do NOT redistribute SDK source in repository. Add CMake target `jyglobalvst_asio_driver` gated by `JYGLOBALVST_BUILD_ASIO=ON` in `src/driver/asio/CMakeLists.txt` *(DEFERRED — release-only)*
- [ ] T133 Implement ASIO driver host (`jyglobalvst_asio.dll`): implement `ASIOInit`, `ASIOCreateBuffers`, `ASIODisposeBuffers`, `ASIOStart`, `ASIOStop`, `ASIOGetLatencies`, `ASIOGetBufferSize`, `ASIOGetSampleRate`, `ASIOCanSampleRate`, `ASIOSetSampleRate`, `ASIOGetChannels`, `bufferSwitch`. Reads from shared-memory ring buffer (`Global\JyGlobalVST_AsioRing_v1`) signaled by `Global\JyGlobalVST_AsioEvent_v1`. Supports stereo (2-channel) output only. Reports correct latency via `ASIOGetLatencies` (AUDIO-006) in `src/driver/asio/asio_driver.cpp` *(DEFERRED — release-only)*
- [ ] T134 Implement ASIO format negotiation: enumerate supported sample rates {44.1, 48, 96, 176.4, 192} kHz and buffer sizes {32, 64, 128, 256, 512, 1024}. Reject mismatched open attempts when another client already holds the driver with different parameters. Return `ASE_FormatNotSupported` with descriptive error string in `src/driver/asio/asio_driver.cpp` *(DEFERRED — release-only)*
- [ ] T135 Implement ASIO driver registration/unregistration in Windows: write/remove `HKLM\SOFTWARE\ASIO\JyGlobalVST ASIO` registry key (driver DLL path, CLSID placeholder) during MSI install/uninstall. No reboot required. Add custom action in `src/installer/wix/AsioDriver.wxs` *(DEFERRED — release-only)*
- [ ] T136 Integration test: ASIO multi-client validation — open driver in two DAW-like host processes with matching parameters, verify both receive identical audio stream; open with mismatched parameters, verify second open fails gracefully. Run against Ableton Live 12, Reaper 7, and a synthetic ASIO host in `tests/asio/multi_client_test.cpp` and `tests/asio/daw_compatibility_test.cpp` *(DEFERRED — release-only)*

**Checkpoint**: Release candidate ready — all v1 functional requirements satisfied; all Audio Validation gates green; accessibility, service mode, update check, and ASIO multi-client driver complete.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately.
- **Foundational (Phase 2)**: Depends on Setup. BLOCKS all user stories.
- **US1 (Phase 3)**: Depends on Foundational complete. MVP target.
- **US2 (Phase 4)**: Depends on Foundational. Independent of US1 in architecture but in practice extends `AudioEngine` (T060), so US1 ships first.
- **US3 (Phase 5)**: Depends on Foundational + US2 chain shape. Logically independent UI-wise.
- **US4 (Phase 6)**: Depends on Foundational. Architecturally independent of US2/US3 — can be developed in parallel with them.
- **Audio Validation (Phase 7)**: Depends on US1+US2+US3+US4 being feature-complete. Gates release.
- **Polish (Phase 8)**: Depends on Foundational. Most tasks can be done in parallel with US3/US4. Service-mode, accessibility, and ASIO multi-client are NOT MVP-blocking but ARE v1-required.
- **ASIO multi-client (T131..T136)**: Depends on Foundational (shared-memory ring buffer interface) and Audio Routing Engine output buffer contract. Can be developed in parallel with US3/US4 once the engine output buffer is stable. Deferred to release prep.

### User Story Dependencies (architectural)

- **US1**: depends on Phase 2 only.
- **US2**: depends on Phase 2; in practice replaces T040 with T058–T060, so it runs after US1 reaches the MVP checkpoint.
- **US3**: depends on US2's `PluginChain` shape (T058) to thread placeholders correctly, and on the scan cache (T056) for re-resolution by `(uid, vendor, name)`.
- **US4**: depends on Phase 2 only. Architecturally orthogonal to US2/US3 (the monitoring taps observe the chain but don't mutate it).

### Within Each User Story

- Tests written first (per phase test block) and made to fail before implementation. *Skip tests-first only if a task explicitly extends an existing tested entity.*
- Entities and value types first (e.g., T036 before T038).
- Engine / service code before UI code that consumes it.
- Each `Checkpoint` line marks the demoable end-state of that story.

### Parallel Opportunities

- All Setup tasks marked **[P]** (T003–T010) — independent files.
- All Foundational tasks marked **[P]** (T011–T018, T020, T027–T030) — separate sub-systems.
- T019 (driver) and T022–T026 (engine pass-through) can proceed concurrently; they only intersect at end-to-end integration.
- All test tasks within a user-story phase are **[P]** (different test files).
- Within US3, all entity / serializer / UI tasks marked **[P]** (T072, T073, T078, T079, T080, T084, T085, T087, T088) are independent.
- Phase 8 (Polish) has many **[P]** tasks runnable in parallel by separate developers.
- ASIO multi-client tasks (T131–T136) can run in parallel once the audio engine output buffer contract is stable (after Phase 2). T131 (research/design) gates T132–T136.

---

## Parallel Example: User Story 1

```
# Tests for US1 (all [P]) — launch concurrently:
Task: "Integration test: virtual device registered in tests/integration/us1_virtual_device_registered_test.cpp"  (T031)
Task: "Integration test: single-plugin routing in tests/integration/us1_single_plugin_routing_test.cpp"           (T032)
Task: "Integration test: device removal/restore in tests/integration/us1_device_removal_restore_test.cpp"        (T033)
Task: "Integration test: sleep/wake in tests/integration/us1_sleep_wake_test.cpp"                                 (T034)
Task: "Contract test: AudioEngine API in tests/contract/us1_audio_engine_api_test.cpp"                            (T035)

# Independent implementation tasks for US1 (all [P]) — launch concurrently:
Task: "Plugin / PluginInstance entities in src/audio-engine/vst-host/plugin_instance.{h,cpp}"                     (T036)
Task: "HardwareOutputDevice entity in src/audio-engine/routing/hardware_output.{h,cpp}"                           (T037)
Task: "Single-instance enforcement in src/tray-app/single-instance/single_instance.cpp"                           (T044)
Task: "Device resolution priority chain in src/audio-engine/routing/device_resolver.cpp"                          (T046)
```

---

## Implementation Strategy

### MVP First (US1 only)

1. Complete Phase 1 (Setup).
2. Complete Phase 2 (Foundational) — driver, audio engine skeleton, JSON validators, test harness all green.
3. Complete Phase 3 (US1) end-to-end.
4. **STOP, run Scenario 1 from `quickstart.md`, validate manually + automated.**
5. Demo / dogfood.

### Incremental Delivery

1. Setup + Foundational → foundation ready (1–2 weeks).
2. US1 → first audible audio through one plugin (2–3 weeks). **MVP demo.**
3. US2 → multi-plugin chains, scanning, editor (2 weeks). **Public alpha.**
4. US3 → presets, placeholders, auto-save, settings split (2 weeks). **Public beta.**
5. US4 → meters, latency, CPU warning, buffer selector (1 week). **Feature complete.**
6. Phase 7 (Audio Validation) → release gate. Fixes any regressions found here re-enter the appropriate phase.
7. Phase 8 (Polish) → accessibility, service mode, update check, WHQL submission. **v1.0 release.**

### Parallel Team Strategy

Once Phase 2 is done:
- **Driver / engine engineer**: drives US1 implementation tasks (T036–T049), then absorbs US2 engine work (T055–T060).
- **UI engineer**: drives US1 minimal UI (T045, T047) then US2 chain editor + scan UI (T061–T064), then US4 meters (T097–T100).
- **Presets / settings engineer**: drives US3 (T072–T089) independently after US2 chain shape lands.
- **Accessibility / service-mode engineer**: drives Phase 8 in parallel with US3/US4.
- **ASIO driver engineer**: drives T131–T136 in parallel with US3/US4 once the engine output buffer contract is stable.
- **QA / release engineer**: runs Phase 7 continuously as features land; owns the loopback harness, Pluginval matrix, soak runs, and ASIO DAW compatibility matrix.

---

## Notes

- **[P]** = different files, no dependencies on incomplete tasks.
- **[Story]** label = `US1` / `US2` / `US3` / `US4` for user-story phase tasks; absent in Setup, Foundational, Audio Validation, and Polish phases.
- **[AUDIO/LATENCY]** label on Phase 7 tasks per Constitution §IV; flags tasks requiring latency review before merge per Code Review Gate.
- Every task in `src/audio-engine/` MUST carry the REALTIME CONSTRAINTS header per Constitution §V. Audited by T107.
- Commit after each task or logical group; commit messages MUST follow `feat(<component>): <change> — latency ±X.Xms` per Constitution §IV (zero impact stated as "latency 0ms").
- No quarantine state, no crash detection, no persistent logs — these are explicit non-goals (clarifications #12, #22; FR-022d; FR-022n; Constitution §II).
- Update this `tasks.md` if a discovered dependency forces a re-ordering; do not silently move tasks across phases.

**Task totals**: 139 tasks
- Setup: 10
- Foundational: 20
- US1 (MVP): 20
- US2: 15
- US3: 25
- US4: 11
- Audio Validation: 9
- Polish: 23
- ASIO multi-client: 6
