# Implementation Plan: JyGlobalVST (System Host)

**Branch**: `001-jyglobalvst-system-host` | **Date**: 2026-06-04 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/001-jyglobalvst-system-host/spec.md`

## Summary

JyGlobalVST is a Windows desktop application that registers a WHQL-signed user-mode virtual audio output device, captures all audio routed to it, processes the stream through an ordered VST3 plugin chain at 32-bit float internally, and emits processed audio to one user-selected hardware output endpoint with a target round-trip latency under 10 ms (worst case under 20 ms with heavy chains). It replaces the typical Voicemeeter + Virtual Audio Cable + LightHost chain with a single signed MSI/MSIX installer that ships a virtual driver, a tray application, and (optionally) a Windows Service for pre-login audio. Approach: JUCE-based audio engine with in-process SEH plugin isolation, JSON `.jvst` presets, named-pipe IPC for service mode, no persistent logging, no telemetry.

## Build Modes

This plan describes the **release** build. Active development happens in a reduced **testable-dev** mode that exercises the full audio pipeline + VST chain + UI without the cost gates that only matter for shipping a signed product. Both modes share the same source tree and `IAudioEngine` contract; **testable-dev** is a strict subset of **release** — nothing in testable-dev is throw-away.

### Testable-Dev (the build we develop against)

**Goal**: end-to-end audio path runnable on a developer's machine, demoable, and unit/integration testable, with no kernel-mode, no installer, and no signing infrastructure required.

**In scope**:
- C++20 audio engine (JUCE 8.x), VST3 hosting, ordered chain with live mutations, SEH plugin isolation, lock-free SPSC command queue, real-time-safe processing.
- Tray-app UI: input picker, output picker, plugin scan + chain editor, plugin GUIs, presets, meters, latency readout, CPU monitor, buffer-size dropdown.
- JSON presets (`.jvst`), roaming settings, local state, scan cache. All four JSON validators.
- Unit tests (SPSC queue, format converters, resampler, validators, chain mutation semantics).
- Integration tests via a virtual-loopback fixture (second WASAPI loopback or a developer-installed virtual cable as the input surrogate for the not-yet-built driver).
- Pluginval matrix as a CI-runnable check (no hardware required).
- RT-safety audits (T106, T107, T107a) — these are static-analysis scripts and run in any build mode.

**Audio source for testable-dev**: instead of capturing from a not-yet-existing JyGlobalVST virtual endpoint, the engine captures from a user-selected **existing** Windows audio input — either WASAPI loopback on the current default render device or a user-installed virtual cable (Voicemeeter, VB-Cable). This is functionally equivalent for the engine and is a one-line change at the device-binding layer once the real driver lands. No engine, chain, UI, preset, or test code changes between modes.

**Out of scope for testable-dev** (deferred to release prep — see Testable Build Scope in `tasks.md`):
- WaveRT virtual-endpoint driver (`src/driver/`) and APO companion DLL.
- Test-signing flags and driver `.cat` signing.
- WiX 4 MSI installer, auto-launch Run-key registration.
- Authenticode binary signing and signing-script harness.
- Windows Service host, named-pipe IPC server/client, per-session pipe authentication.
- Accessibility tier (UIA handlers, focus rings, keyboard accelerators, NVDA/Narrator harness).
- Hardware-loopback latency measurements (Scenarios 4, 7, 8, 14 manual gates).
- Update-check endpoint integration and About → Check-for-updates menu.
- WHQL submission package.
- Release-engineering build/sign/stage script and audio-PR CI gate workflow.
- Multi-session / Fast-User-Switching / RDP integration tests.

**What testable-dev does NOT compromise**: the four Constitution NON-NEGOTIABLES still apply to all code in testable-dev. Latency targets (AUDIO-001..005) remain the design budget; they are *measured* in release-prep but *respected* now. RT-safety (no malloc/lock/log on audio thread) is enforced by audit scripts (T106, T107). The chain-mutation contract is identical to release. Test-driven development still applies — tests-first per the task plan.

### Release

Everything in testable-dev plus all deferred items above. Triggers WHQL submission, signing-cert provisioning, and the full Phase 7 hardware-loopback gate runs.

## Technical Context

**Language/Version**: C++20 (audio engine, tray app, service host); WiX 4 / MSIX for installer; PowerShell 7+ for build scripts. Driver layer: C++17 with Windows Driver Kit (WDK) headers if AVStream path is taken; otherwise WaveRT virtual endpoint with user-mode APO in C++20.

**Primary Dependencies**:
- **JUCE 8.x** — UI framework, audio device abstraction, VST3 hosting (`AudioPluginInstance`, `KnownPluginList`, `AudioProcessorGraph`)
- **Steinberg VST3 SDK 3.7.x** — bundled via JUCE; required for VST3 hosting and state chunk APIs
- **Windows SDK 10.0.22621+** — WASAPI (`IAudioClient3`, `IMMDeviceEnumerator`), MMDevice, Core Audio APIs, UI Automation, Job Objects
- **Windows Driver Kit (WDK) 10.0.22621+** — if AVStream/APO driver path is selected (see research.md)
- **WiX Toolset v4** or **MSIX Packaging** — single signed installer
- **nlohmann/json 3.11+** — JSON parsing for presets, settings, IPC messages, scan cache
- **GoogleTest 1.14+** — unit tests
- **Pluginval** (Tracktion, command-line) — plugin compatibility validation in CI

**Storage**: Local filesystem only. No database, no cloud, no telemetry.
- `%UserProfile%\Documents\JyGlobalVST\Presets\*.jvst` — user-portable preset files (JSON)
- `%AppData%\Roaming\JyGlobalVST\settings.json` — user-portable preferences
- `%LocalAppData%\JyGlobalVST\` — machine-local state: window geometry, last endpoint ID, plugin scan cache, auto-save chain

**Testing**:
- GoogleTest for C++ unit tests (DSP utilities, JSON schema validation, IPC framing, state-machine transitions)
- JUCE `UnitTest` for in-tree audio-graph tests
- Pluginval CI matrix against a known set of VST3 plugins (ARC X, Sonarworks Reference, ReaEQ, FabFilter Pro-Q test build)
- Custom latency harness: loopback measurement via a hardware loopback cable into a known input device
- 12-hour soak test in CI runner (xrun counter, drift monitor)

**Target Platform**: Windows 10 1909+ (x64) and Windows 11 (x64). 32-bit Windows is out of scope. ARM64 is out of scope for v1.

**Project Type**: Desktop application — multi-component native binary (driver + tray UI + optional service) packaged in one installer.

**Performance Goals**:
- Round-trip latency ≤ 10 ms with default buffer size, single plugin (AUDIO-001)
- CPU ≤ 5% with typical 3-plugin chain on Intel i5 8th gen / Ryzen 5 3000 (AUDIO-002)
- Zero dropouts over 12-hour continuous playback (AUDIO-003)
- Worst-case latency ≤ 20 ms under 5+ plugin chain (AUDIO-005)
- Memory ≤ 200 MB resident with typical chain (SC-005)
- First-run install → audio routed in under 2 minutes (SC-001)

**Constraints**:
- Audio thread MUST honor real-time discipline: no malloc/new, no file I/O, no mutex acquisition on hot path, no logging, no GUI calls (Constitution §V)
- No persistent logging anywhere on disk; no telemetry; no background network activity. Single allowed network call: explicit user-initiated update check (FR-022n)
- WHQL-signed driver + Authenticode-signed binaries are a release prerequisite; EV cert required for WHQL attestation
- Single hardware output v1 (FR-013); architecture must remain extensible to N-way fan-out (FR-013a)
- ASIO multi-client virtual output is IN SCOPE for v1 (FR-013b, FR-032); implemented as a separate driver-level component alongside the WaveRT virtual endpoint
- Stereo-only virtual endpoint v1 (FR-003); upstream handles downmix
- VST3 only in v1; no VST2, no AU, no LV2
- No MIDI input routing in v1 (assumption); architecture must not block adding it later
- Single-instance per user session via named mutex (FR-022j)

**Scale/Scope**:
- Single-user desktop application; no multi-tenant concerns
- Plugin chain: tested to 10 plugins (99th percentile), no enforced hard limit
- 5 sample rates × 3 bit-depth source formats × 1 internal format (32-bit float) supported
- 1 hardware output device active at a time
- Two installation modes (user-mode, service-mode) — mutually exclusive on a given machine

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|---|---|---|
| I. Spec Hierarchy & Component Isolation | PASS | Components decompose as `virtual_device_driver`, `audio_routing`, `vst_host_engine`, `ui_controls`, `installer`. Each will have testable acceptance criteria tied to latency/CPU targets. See Project Structure. |
| II. Performance Requirements (NON-NEGOTIABLE) | PASS | Spec carries AUDIO-001..006 explicitly. Latency budget allocated in Audio Technical Context below. No feature in v1 bypasses latency validation. ASIO virtual output overhead (AUDIO-006) is bounded at ≤ 1 ms. |
| III. Task Sequencing & Dependencies | PASS | Implementation order: driver → audio routing → VST host → UI → integration. Locked by Constitution §VI. |
| IV. Naming & Conventions | DEVIATION (justified) | Branch is `001-jyglobalvst-system-host` (Spec Kit numbering convention) rather than `feat/<component>`. Single-feature spec covering all components; per-component branches will follow `feat/<component>` once we split into subordinate features. Logged in Complexity Tracking. |
| V. Real-Time Code Documentation & Constraints | PASS | Every `.cpp/.h` touching the audio thread will carry the REALTIME CONSTRAINTS header. Enforced via code review checklist. |
| VI. Dependencies & Build Order | PASS | Phase ordering matches Constitution §VI. Driver first, routing second, host third, UI fourth. Cross-component testing only after each component is latency-validated. |

**Result**: All NON-NEGOTIABLE gates (II, III, V, VI) pass. One minor deviation on naming (IV) is documented and justified. Proceed to Phase 0.

### Audio Component Dependency Graph

```
[Virtual Device Driver (WASAPI endpoint)]
            │
            ▼
[Audio Routing Engine (capture → 32f → resample → graph → output)]
            │
            ├────► [VST Host Engine (JUCE AudioProcessorGraph + VST3)]
            │
            ▼
[Hardware Output Device (one selected WASAPI or ASIO endpoint)]

       (parallel, depends on Routing API only)
       ┌──────────────────────────────────────┐
       │  UI Layer (JUCE) — tray app, chain   │
       │  editor, meters, presets, settings   │
       └──────────────────────────────────────┘

       (service-mode only, lifts engine out of tray)
       [Windows Service (audio engine host)] ◄──IPC──► [Tray App (UI client)]

       (mixed-driver mode: ASIO output + WASAPI input)
       [WASAPI Capture Thread] ──► [LockFree Ring Buffer] ──► [ASIO Callback]
```

- **Phase blocking**: Driver registration MUST complete before the audio engine can capture from the virtual endpoint.
- **Phase blocking**: Audio routing engine depends on the virtual endpoint and a JUCE `AudioDeviceManager` bound to a hardware output.
- **Phase blocking**: ASIO virtual output depends on the audio routing engine's output buffer being published to a shared-memory ring buffer; the ASIO driver DLL reads from this ring buffer in its `bufferSwitch` callback.
- **Non-blocking**: UI depends only on the finalized AudioEngine API contract (`contracts/audio-engine-api.md`), NOT the implementation. Tray app and service share the same UI binary; the engine target is switched at startup based on install mode.

### Audio Technical Context

- **Buffer Size**: User-selectable from {32, 64, 128, 256, 512, 1024} samples. Default 256. Audio thread guarantees no allocation post-init at any buffer size.
- **Sample Rates**: 44.1, 48, 96, 176.4, 192 kHz (FR-002). Internal processing always 32-bit float; resample at virtual-device boundary and at hardware-output boundary using a high-quality polyphase resampler (JUCE `Interpolators::WindowedSinc`).
- **Bit Depth**: 16-bit int, 24-bit int (packed), 32-bit float (FR-002a). Source format converted on capture; output format converted to whatever the hardware endpoint negotiates.
- **Latency Budget** (default 256-sample buffer @ 48 kHz, single plugin):
  - WASAPI virtual capture: ≤ 2.5 ms
  - Resample + format conversion: ≤ 0.5 ms
  - VST3 plugin processing: ≤ 4 ms (depends on plugin)
  - Hardware output: ≤ 2.5 ms
  - ASIO virtual output overhead (shared-memory ring buffer read): ≤ 1 ms (AUDIO-006)
  - **Total target: ≤ 10 ms; hard ceiling 20 ms** (Constitution §II, AUDIO-001/005)
- **Mixed-driver latency** (ASIO output + WASAPI input):
  - WASAPI shared-mode capture latency is determined by the Windows audio engine (typically ~10 ms).
  - ASIO output latency is determined by the ASIO driver buffer size (e.g., 256 samples @ 48 kHz ≈ 5.3 ms).
  - A lock-free ring buffer (~170 ms capacity) absorbs clock drift between the two driver threads.
  - The `LatencyProfile` reports `capture_ms` from `WasapiCapture` and `output_ms` from the ASIO device separately.
  - If sample rates differ, resampling occurs on the WASAPI capture thread before writing to the ring buffer.
- **Real-Time Constraints** (Constitution §V):
  - No `malloc` / `new` / `std::*` allocating containers in audio callback
  - No mutex acquisition; lock-free SPSC queue for parameter changes from UI thread
  - No file I/O, no logging, no GUI calls
  - Plugin processing wrapped in SEH `__try / __except` (FR-023); failure path bypasses plugin and continues — no allocation on failure path
  - Maximum execution time per `processBlock`: 5 ms (at 48 kHz, 256 samples = 5.33 ms wall-clock budget)
- **CPU Target**: ≤ 5% on Intel i5 8th gen / Ryzen 5 3000 series with a typical 3-plugin chain (AUDIO-002).

## Project Structure

### Documentation (this feature)

```text
specs/001-jyglobalvst-system-host/
├── plan.md              # This file (/speckit-plan output)
├── research.md          # Phase 0 output (/speckit-plan)
├── data-model.md        # Phase 1 output (/speckit-plan)
├── quickstart.md        # Phase 1 output (/speckit-plan)
├── contracts/           # Phase 1 output (/speckit-plan)
│   ├── audio-engine-api.md      # Internal C++ contract (tray ↔ engine)
│   ├── ipc-protocol.md          # Named-pipe wire protocol (service mode)
│   ├── preset-schema.json       # JSON Schema for .jvst preset files
│   ├── settings-schema.json     # JSON Schema for roaming settings.json
│   ├── scan-cache-schema.json   # JSON Schema for local plugin scan cache
│   └── update-manifest-schema.json  # JSON Schema for update endpoint response
├── checklists/
│   └── requirements.md          # (pre-existing)
└── tasks.md             # Phase 2 output (/speckit-tasks — NOT created here)
```

### Source Code (repository root)

```text
src/
├── driver/                       # WHQL-signed virtual audio device
│   ├── waveRT/                   # WaveRT virtual endpoint (primary path)
│   ├── apo/                      # User-mode APO companion
│   └── asio/                     # ASIO virtual driver (shared-memory ring buffer → ASIO host API)
├── audio-engine/                 # Real-time audio routing + VST3 hosting
│   ├── routing/                  # Capture, resample, format convert, output
│   ├── chain/                    # AudioProcessorGraph wrapper, chain mutations
│   ├── vst-host/                 # Plugin scan, load, SEH wrapping, state chunks
│   └── monitoring/               # CPU meter, latency probe, xrun counter
├── tray-app/                     # JUCE-based tray UI (always installed)
│   ├── ui/                       # Main window, chain editor, meters, settings
│   ├── presets/                  # .jvst load/save/import/export, validation
│   ├── ipc-client/               # Named-pipe client (service-mode)
│   └── single-instance/          # Named-mutex enforcement
├── service/                      # Windows Service (service-mode only)
│   ├── host/                     # Service lifecycle, audio-engine host
│   └── ipc-server/               # Named-pipe server, per-session auth
├── shared/                       # Cross-component code
│   ├── contracts/                # C++ headers mirroring contracts/*.md
│   ├── json/                     # nlohmann/json wrappers, schema validators
│   ├── ipc/                      # Wire framing, message types
│   └── platform/                 # Windows wrappers (endpoints, mutexes, UIA)
└── installer/                    # WiX or MSIX project
    ├── wix/                      # Component fragments, custom actions
    └── signing/                  # Build-time signing scripts (uses ENV cert)

tests/
├── unit/                         # GoogleTest — DSP, JSON, IPC framing, state
├── integration/                  # End-to-end: driver→engine→output loopback
├── latency/                      # Loopback latency harness + soak tests
├── compat/                       # Pluginval matrix runner
└── asio/                         # ASIO driver validation (multi-client, DAW compatibility)
```

**Structure Decision**: Single repository, native C++20 multi-component layout (driver, engine, tray, service, installer) under `src/`. The audio engine is a static library linked into both the tray app (user-mode install) and the Windows Service (service-mode install); same binary code path, different host. The UI is identical in both modes. JSON schemas live alongside their consuming C++ code in `src/shared/json/` but the canonical schema documents live in `specs/001-jyglobalvst-system-host/contracts/` and are copied into the build at compile time. Tests are partitioned by phase: `unit/` and `integration/` run on every commit; `latency/` and `compat/` run on tagged release candidates.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| Constitution §IV branch name (`001-jyglobalvst-system-host` instead of `feat/<component>`) | Spec Kit workflow uses numbered feature branches; this is a single umbrella feature spanning all five components. | Splitting into five branches at planning time would prevent a single coherent plan/spec from existing; per-component branches will follow `feat/<component>` once we generate subordinate feature specs from this root. |
| Constitution §IV spec filename (`spec.md` instead of `spec__<component>.md`) | Spec Kit tooling (`/speckit-specify`, `/speckit-plan`, `/speckit-tasks`, `/speckit-analyze`) writes and reads `spec.md` by hard convention; renaming would break every workflow command. The constitution's double-underscore naming originated when this project predated Spec Kit adoption. | Renaming the file would require forking the Spec Kit skill scripts to take a configurable spec filename, doubling our maintenance surface. The Spec Kit convention is itself a defensible standard. |
| Two install modes (user-mode in-process AND service-mode IPC) | FR-027/028: pre-login audio is a real user need (lock-screen video, fast-startup gaming) but adds the cost of an IPC layer and a second deployable. | Service-only would force every install through a UAC + service registration that 90% of users don't need. User-only would block pre-login audio entirely. Hybrid is the minimum that satisfies both audiences and is documented as mutually exclusive per machine (FR-028b). |
| In-process plugin hosting with SEH (no sandbox process) | Latency budget cannot tolerate cross-process IPC for every audio buffer. SEH catches the catchable failure modes; memory corruption / audio-thread hangs are accepted non-goals (clarified in spec). | Out-of-process plugin sandboxing would add ≥ 2–5 ms latency per plugin and a full IPC stack. Rejected because it would blow the AUDIO-001 budget and the user explicitly accepted the residual risk in clarification #2 and #12. |
| ASIO multi-client driver alongside WaveRT | FR-013b/FR-032: ASIO-only DAWs (Ableton Live, Cubase, Reaper in ASIO mode) cannot consume WASAPI endpoints. A virtual ASIO driver exposing the post-chain signal is the only way to serve this audience without forcing them to use ASIO4ALL or FlexASIO as a secondary hop. | Shipping only WaveRT would exclude a significant pro-audio user segment. Rejected because it contradicts the product goal of replacing the entire multi-tool chain with a single app. |

## Post-Design Constitution Re-Check

*Re-evaluation after Phase 1 artifacts (research.md, data-model.md, contracts/, quickstart.md) are generated.*

| Principle | Status | Re-check notes |
|---|---|---|
| I. Spec Hierarchy & Component Isolation | PASS | `data-model.md` and Project Structure show clean component boundaries: driver (WaveRT + ASIO), audio-engine, vst-host, tray-app, service, installer. `contracts/audio-engine-api.md` enforces UI ↔ engine isolation; same UI binary works in both install modes. ASIO driver is isolated as a separate `src/driver/asio/` component. |
| II. Performance Requirements (NON-NEGOTIABLE) | PASS | Latency budget allocated across pipeline (Audio Technical Context). `quickstart.md` Scenarios 4 (latency), 8 (CPU), 14 (soak) gate AUDIO-001..006 with concrete measurements. ASIO virtual output overhead is bounded at ≤ 1 ms (AUDIO-006). No artifact silently bypasses the gates. |
| III. Task Sequencing & Dependencies | PASS | Component dependency graph in plan; `audio-engine-api.md` allows UI work to start once the contract is frozen, in parallel with engine implementation, while still respecting "UI depends on plugin chain architecture, not implementation details." `/speckit-tasks` will produce a dependency-ordered task list from this. |
| IV. Naming & Conventions | DEVIATION (unchanged) | Spec Kit branch `001-jyglobalvst-system-host` retained; per-component sub-feature branches will follow `feat/<component>` as the work decomposes. Logged in Complexity Tracking. No further deviation introduced by Phase 1. |
| V. Real-Time Code Documentation & Constraints | PASS | `audio-engine-api.md` embeds the REALTIME CONSTRAINTS comment in its header sketch; `research.md` §5–8 detail RT-safe patterns (SEH, lock-free SPSC, `QueryPerformanceCounter`, no allocation in failure path). All audio-thread code paths are explicitly bounded. |
| VI. Dependencies & Build Order | PASS | Hard-locked order preserved: driver → routing → host → UI. ASIO driver depends on routing engine output buffer contract but can be developed in parallel with host/UI once the shared-memory ring buffer interface is frozen. Audio-engine API contract decouples UI from engine implementation; service-mode reinforces the boundary by physically separating the binaries. No artifact introduces a back-channel that would let UI work race ahead of engine readiness. |

**Result**: All gates pass post-design. The single justified deviation (IV) carries forward unchanged. Proceed to `/speckit-tasks`.

