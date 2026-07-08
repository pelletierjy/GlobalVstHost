# Phase 0 Research: JyGlobalVST (System Host)

**Branch**: `001-jyglobalvst-system-host` | **Date**: 2026-06-04

The feature specification contains no remaining `NEEDS CLARIFICATION` markers — all 24 clarification questions were resolved in the spec's Clarifications session. This document records technology/implementation decisions and the alternatives considered.

---

## 1. Virtual audio device driver model

**Decision**: WaveRT virtual endpoint (PortCls/Subdev) implemented as a kernel-mode driver shell with a user-mode service backing the cyclic buffer, accompanied by a Stream Effect APO for format negotiation. Implementation reference: the Microsoft Sysvad sample, narrowed to a single render endpoint.

**Rationale**:
- WaveRT is the documented modern path for virtual audio endpoints on Windows 10 1909+ and Windows 11; it is what every shipped commercial virtual-audio driver (VB-Audio VAC, Voicemeeter, Equalizer APO loopback) currently uses.
- It is WHQL-attestable (FR-030, FR-031), supports the full sample-rate matrix (44.1 / 48 / 96 / 176.4 / 192 kHz), and supports MMCSS for real-time priority.
- Sysvad is Microsoft's reference driver and is the de facto starting template; using it minimizes WHQL risk.

**Alternatives considered**:
- **AVStream / Filter-graph driver**: Higher complexity, intended for AV capture devices, more surface area for WHQL failures. Rejected.
- **User-mode-only "virtual cable" via APO**: APOs can transform but cannot register an endpoint; insufficient. Rejected.
- **Equalizer APO host fork**: Inherits GPL-incompatible licensing for our distribution model and does not give us our own endpoint. Rejected.
- **AudioGraph (UWP) virtual endpoint**: UWP-only, cannot register a global Windows audio device. Rejected.

**Open follow-ups**: WHQL submission requires an EV code-signing certificate and a partner.microsoft.com hardware account. Procurement is a release prerequisite (FR-031); flag in tasks.md as a long-lead-time item.

---

## 2. VST3 hosting framework

**Decision**: JUCE 8.x as the host framework, using `juce::AudioPluginFormatManager` + `juce::VST3PluginFormat` for scanning/instantiation and `juce::AudioProcessorGraph` as the chain runtime. Steinberg VST3 SDK 3.7.x is consumed transitively via JUCE.

**Rationale**:
- JUCE provides production-grade VST3 hosting code used by Tracktion Waveform, Cubase-adjacent tools, and most commercial plugin hosts — battle-tested for the exact use case here.
- It also provides the cross-platform audio device abstraction (WASAPI on Windows), a `WindowedSinc` resampler, lock-free FIFOs, and a UI toolkit. One framework covers audio + UI, reducing dependency surface.
- License is permissive for closed-source distribution under JUCE's Personal/Indie/Pro tiers; legal review is a release prerequisite but does not block implementation.
- VST3 state chunk handling (`AudioPluginInstance::getStateInformation` / `setStateInformation`) is the canonical opaque-blob API we need for preset storage (FR-022a).

**Alternatives considered**:
- **Direct VST3 SDK with custom host loop**: More control, but reimplements 18 months of plugin-quirk handling JUCE has already absorbed (parameter automation, GUI embedding, double-precision support, soft bypass). Rejected.
- **iPlug2**: Plugin-authoring framework, not host-oriented. Rejected.
- **Tracktion Engine**: DAW-grade overkill, drags in transport/clip/track abstractions we don't need. Rejected.

---

## 3. Audio capture/output backend

**Decision**: WASAPI shared mode via `IAudioClient3::InitializeSharedAudioStream` for both the virtual endpoint capture side and the hardware output side. Default buffer 256 samples (FR-015), user-selectable {32, 64, 128, 256, 512, 1024}.

**Rationale**:
- `IAudioClient3` (Windows 10 1703+) supports sub-10 ms shared-mode latency, satisfying AUDIO-001 without requiring exclusive mode.
- Shared mode avoids the user-experience footgun where exclusive mode locks other apps out of the device — antithetical to "system-wide audio processor."
- Aligns with the Assumption that WASAPI shared mode is sufficient for the default experience.

**Alternatives considered**:
- **WASAPI exclusive mode**: Lower latency but blocks other applications from the hardware endpoint; defeats the purpose. Documented as a possible future enhancement.
- **ASIO multi-client**: Now IN SCOPE for v1 (see §16 below).
- **DirectSound**: Deprecated, higher latency. Rejected.

---

## 4. Plugin scan strategy

**Decision**: In-process plugin scanning on a background `std::thread` (not on the audio thread). Each candidate `.vst3` is loaded, queried via `IPluginFactory`, and unloaded; the scan thread populates `KnownPluginList` (JUCE) which is then mirrored into `%LocalAppData%\JyGlobalVST\scan-cache.json`. Pre-seeded paths: `%ProgramFiles%\Common Files\VST3` and `%LocalAppData%\Programs\Common\VST3` (FR-005). Cancellation via a `std::atomic<bool>` checked between plugins. Incremental UI population via a thread-safe enqueue to the message thread.

**Rationale**:
- FR-005 explicitly requires background scanning with cancellation and incremental UI updates.
- The user clarified (clarification #21) that no sandbox/probe process is required; in-process SEH (FR-023) is sufficient runtime protection.
- JUCE's `KnownPluginList::scanAndAddFile` already implements roughly this pattern; we wrap it for cancellation and progress reporting.

**Alternatives considered**:
- **Out-of-process scan worker** (`pluginval`-style child process): Would survive a buggy plugin crashing on instantiation, but the user explicitly accepted the in-process risk and chose not to add a sandbox process. Rejected per clarification.
- **Eager scan on every launch**: Spec Assumption explicitly rejects this ("Plugin scanning is user-initiated, not automatic on every launch"). Rejected.

**Risk**: A plugin that crashes during scan will take down the tray app. Mitigation: the scan cache persists across launches so a known-good plugin set survives a bad rescan; the user can remove the offending plugin from scan paths before rescanning.

---

## 5. Plugin runtime isolation (SEH wrapping)

**Decision**: Each plugin's `processBlock` call is wrapped in a Windows SEH `__try / __except(filter)` block. The filter (`EXCEPTION_EXECUTE_HANDLER` for catchable exceptions; `EXCEPTION_CONTINUE_SEARCH` for `EXCEPTION_STACK_OVERFLOW` and similar uncatchable conditions) marks the plugin's `AudioProcessorGraph` node as failed, sets a lock-free flag read by the next callback to bypass it, and enqueues a UI notification message. No allocation in the failure path.

C++ exceptions are caught by an additional `try / catch(...)` immediately inside the SEH frame. SEH and C++ exceptions are mutually exclusive at the same level under MSVC; the two-layer wrap covers both.

**Rationale**:
- Constitution §V forbids allocation in the audio callback, so the failure path uses pre-allocated message slots in a lock-free SPSC queue read by the UI thread.
- FR-023 explicitly mandates this behavior and accepts that memory corruption and audio-thread hangs are non-recoverable.
- Marking the graph node "bypassed" rather than removing it preserves chain order so the UI can show the failure inline and offer the user a re-enable action.

**Alternatives considered**:
- **`SetUnhandledExceptionFilter` only**: Catches process-wide unhandled exceptions, not per-plugin granular bypass. Rejected.
- **Vectored Exception Handler**: Runs before SEH frames; useful for diagnostics but harder to scope per-plugin. Rejected.

**Edge case**: An SEH exception inside a JUCE-managed call (e.g., GUI-thread plugin editor) bubbles up to the message loop's top-level handler, not the audio thread's. We need a separate `__try` around editor message-pump invocations. Tracked as an implementation detail for tasks.md.

---

## 6. Real-time parameter / chain mutation

**Decision**: All mutations to the running chain (add plugin, remove, reorder, bypass, parameter change) originate on the UI/message thread and are posted to the audio thread via a lock-free SPSC ring buffer of pre-allocated `ChainCommand` structs (fixed size, no dynamic allocation). The audio thread drains the queue at the top of each `processBlock` and applies commands before processing samples.

**Rationale**:
- Constitution §V forbids mutex acquisition and allocation in the audio callback.
- This is the canonical JUCE pattern (`AbstractFifo` + pre-allocated buffer).
- Bounded queue depth gives backpressure: if the UI generates commands faster than the audio thread can drain, the UI thread sees a "queue full" and either coalesces or surfaces a non-modal notification (rare in practice — chain edits are human-paced).

**Alternatives considered**:
- **Mutex-protected chain with try-lock in audio thread**: Risk of priority inversion; if the audio thread misses the lock it must skip the mutation, which complicates correctness. Rejected.
- **Read-Copy-Update with `std::atomic<shared_ptr>`**: `shared_ptr` reference counting allocates; not real-time-safe. Rejected.

---

## 7. Resampling

**Decision**: JUCE `Interpolators::WindowedSinc` for sample-rate conversion at both the virtual-device capture boundary and the hardware-output boundary. Resampler state is pre-allocated; no allocation per buffer.

**Rationale**:
- Built into JUCE, well-tested, AUDIO-004-grade quality at the rates we support.
- Operates on 32-bit float internal buffers (matches our internal pipeline).
- Sufficient quality for all five supported source rates (44.1 / 48 / 96 / 176.4 / 192 kHz).

**Alternatives considered**:
- **r8brain-free-src**: Excellent quality but additional dependency; JUCE's resampler is good enough for this use case. Rejected for v1; reconsider if AUDIO-004 fails listening tests.
- **Soxr**: GPL-tinged, adds licensing complexity. Rejected.
- **Linear / cubic interpolation**: Insufficient for AUDIO-004 (audible aliasing at 44.1 → 48 transitions). Rejected.

---

## 8. CPU monitoring (≤ 5% budget)

**Decision**: Per-callback wall-clock measurement using `QueryPerformanceCounter` at the start and end of `processBlock`; convert to a percentage of the buffer's wall-clock duration. A rolling 1-second average is exposed to the UI for FR-026. The 5% threshold is computed against total CPU available across all cores at the process's affinity (typical: full machine).

**Rationale**:
- `QueryPerformanceCounter` is real-time-safe (no allocation, no blocking) and has nanosecond resolution on modern Windows.
- Per-buffer measurement is what actually matters for audio glitching, not OS-level "process CPU usage" which averages over too long a window.

**Alternatives considered**:
- **`GetProcessTimes` / `QueryProcessCycleTime`**: Lower resolution, syscall overhead in audio thread. Rejected.
- **PDH performance counters**: Heavyweight, requires polling thread. Acceptable for the UI's overall CPU readout but not for the audio-thread budget check.

---

## 9. Installer & packaging

**Decision**: Single signed MSI built with WiX 4.x. One administrator-elevation prompt. WHQL-signed driver embedded as a driver-package component (`.cat` + `.inf` + `.sys`). MSIX considered but rejected for v1.

**Rationale**:
- WHQL submission today still targets `.inf`-driven driver packages; MSIX modern package format does not yet support driver-package installation through the Microsoft Store path that virtual audio drivers need.
- WiX 4 supports per-machine install + per-user runtime configuration cleanly via custom actions.
- Single MSI satisfies FR-030 (one UAC prompt, no reboot, full uninstall).

**Alternatives considered**:
- **MSIX**: Excellent for sandboxed apps, but driver-package handling is constrained in 2026. Track as a future migration once Microsoft's MSIX driver flow matures.
- **Two installers (driver + app)**: Doubles the UAC prompts and creates partial-install states. Rejected.
- **Inno Setup / NSIS**: Less integrated with WHQL driver-package flow. Rejected.

---

## 10. Service / tray IPC (service mode)

**Decision**: Windows named pipe (`\\.\pipe\JyGlobalVST\v1\<session_id>`) with length-prefixed JSON message framing. Pipe ACL restricts connections to the interactive user session that owns the SID. Authentication uses the connecting process's token via `GetNamedPipeClientProcessId` + `OpenProcessToken` to verify session match before exposing commands.

**Rationale**:
- Named pipes are the standard Windows IPC and integrate naturally with service-account ↔ user-session boundaries.
- ACL + session-ID check satisfies FR-028a (local-only, no network listener, per-session auth).
- JSON framing keeps the wire schema diff-able and inspectable for support.

**Alternatives considered**:
- **Local RPC (MS-RPC)**: More authentic Windows idiom but heavier scaffolding (IDL files, marshaller); benefit doesn't outweigh complexity here. Rejected for v1.
- **gRPC over local socket**: Loopback-only network listener — violates FR-028a's "no network-listening endpoint." Rejected.
- **Shared memory**: Lower latency but harder to version and authenticate. Rejected for control plane (might still use ring buffer in shared memory for meter data — see follow-up).

**Follow-up**: Meter data (level meters, latency readout) at 30–60 Hz would saturate a JSON pipe; a tiny shared-memory ring buffer alongside the control pipe is in scope as an optimization. Defer to tasks.md.

---

## 11. Preset / settings / scan-cache JSON schemas

**Decision**: Schema version field on every JSON document. Forward-compatibility rule: unknown top-level fields are preserved on write (round-trip) for preset and settings files; unknown fields are *rejected* on import (FR-022g-1). nlohmann/json + a hand-rolled validator (no full JSON Schema engine in v1; the validation rules are simple enough that a 200-line C++ validator is cheaper than pulling in `json-schema-validator`).

**Rationale**:
- FR-022a, FR-022b, FR-022g-1, FR-022k all demand a schema-versioned, tolerant-but-secure JSON model.
- The set of fields is small (preset has ≤ 20 fields, settings ≤ 15); a full JSON Schema validator is overkill.
- Hand-rolled validator can encode the per-field size caps (16 MB state chunk, 50 MB file) that FR-022g-1 mandates against denial-of-service.

**Alternatives considered**:
- **nlohmann/json-schema-validator**: Adds a transitive `nlohmann::json::patch` dependency and ~3000 lines of code for a feature we use in two places. Reconsider in v2 if schemas grow.
- **Protocol Buffers / FlatBuffers**: Binary, not human-readable; the spec explicitly called for human-readable diff-able JSON. Rejected.

---

## 12. Single-instance enforcement (session-scoped)

**Decision**: `CreateMutexW` with name `Local\JyGlobalVST.SingleInstance.<SessionId>`. The `Local\` namespace makes the mutex session-scoped (vs. `Global\` machine-wide), satisfying FR-022j's requirement that Fast-User-Switching / RDP sessions each get their own instance.

If `CreateMutexW` returns `ERROR_ALREADY_EXISTS`, the second instance finds the existing tray app's main window via a session-scoped well-known window class name, posts a custom `WM_BRINGTOFRONT` message, then exits.

**Rationale**: Idiomatic Win32, zero new dependencies, exactly matches FR-022j semantics.

**Alternatives considered**:
- **Named pipe lock file**: Works but heavier; mutex is the direct primitive. Rejected.
- **File-system lock**: Survives process crash awkwardly; mutex is cleaned up by the kernel on process exit. Rejected.

---

## 13. UI Automation / screen-reader support

**Decision**: JUCE's `AccessibilityHandler` and `AccessibleState` APIs for every interactive component. Custom UIA notifications via `UiaRaiseNotificationEvent` for dynamic events (plugin added/removed, bypass toggled, device disconnect, CPU warning) per FR-019b. Keyboard navigation via JUCE's `KeyListener` and explicit tab-order overrides; visible focus indicator drawn by overriding `Component::paintOverChildren` to draw a 2 px focus ring.

**Rationale**:
- JUCE 8 added first-class UIA support that maps `AccessibilityHandler` to the Windows UIA tree; no need to hand-roll IRawElementProviderSimple.
- FR-019a/b can be validated with NVDA and Narrator in CI by scripting interaction.

**Alternatives considered**:
- **Raw UIA provider implementation**: Doable but reimplements what JUCE already provides. Rejected.
- **No accessibility (defer to v2)**: Spec explicitly requires it in v1. Rejected.

---

## 14. Update manifest format

**Decision**: HTTPS GET against a configured manifest URL (default: a versioned HTTPS endpoint owned by the project). Response is a JSON document `{ "latest_version": "x.y.z", "minimum_supported": "x.y.z", "release_notes_url": "...", "download_url": "..." }`. Schema versioned, validated client-side, no User-Agent beyond the platform default (FR-022n).

**Rationale**:
- FR-022n forbids identifying payloads; the request is a vanilla HTTPS GET with no headers we control, no body, no query string carrying machine identifiers.
- A small JSON manifest is enough for human-readable updates without inventing a custom protocol.

**Alternatives considered**:
- **RSS / Atom feed**: Heavier parser, no real benefit. Rejected.
- **Plain-text version file**: Doesn't carry release notes / download link in a structured way. Rejected.

---

## 15. Test harness for latency / soak

**Decision**: A loopback test fixture that:
- (a) registers JyGlobalVST's virtual endpoint,
- (b) plays a known impulse signal from a test application into the virtual endpoint,
- (c) routes the chain output to a hardware loopback device (a USB audio interface with a TRS loopback cable),
- (d) records the loopback into a second WASAPI capture stream,
- (e) measures impulse arrival time delta vs. the playback timestamp.

For CI, the loopback device is virtualized via a second WaveRT instance configured as a loop-mode endpoint. For release validation, a physical USB interface (e.g., Focusrite Scarlett 2i2) is used to measure actual hardware-path latency.

**Rationale**: Round-trip latency claims (AUDIO-001/005) are only credible with hardware-in-the-loop measurement. The virtualized loopback path is for fast CI feedback; the physical loopback is for release sign-off.

---

## Summary of decisions

| # | Area | Decision | Notes |
|---|---|---|---|
| 1 | Driver model | WaveRT virtual endpoint + APO | WHQL-attestable, Sysvad reference |
| 2 | VST host framework | JUCE 8 + VST3 SDK 3.7 | Production-grade, license review needed |
| 3 | Audio backend | WASAPI shared via `IAudioClient3` | Sub-10 ms shared-mode latency |
| 4 | Plugin scan | In-process, background thread, cached | Per spec clarification — no sandbox |
| 5 | Plugin runtime isolation | SEH `__try`/`__except` + C++ `try`/`catch` | Per FR-023 |
| 6 | Chain mutation | Lock-free SPSC command queue | RT-safe |
| 7 | Resampling | JUCE WindowedSinc | Default; reconsider if AUDIO-004 fails |
| 8 | CPU monitoring | `QueryPerformanceCounter` per buffer | RT-safe |
| 9 | Installer | WiX 4 MSI, WHQL driver inside | MSIX deferred |
| 10 | Service IPC | Named pipe + JSON, session-scoped ACL | Plus shared-mem ring for meters |
| 11 | JSON validation | nlohmann/json + hand-rolled validator | Schema versioned, size capped |
| 12 | Single-instance | `Local\` namespace mutex per session | FR-022j |
| 13 | Accessibility | JUCE 8 `AccessibilityHandler` + UIA notifications | FR-019a/b |
| 14 | Update manifest | HTTPS GET → small JSON manifest | FR-022n; no identifying payload |
| 15 | Latency test | Hardware loopback for release; virtual loopback for CI | AUDIO-001/005 sign-off |

---

## 16. ASIO multi-client virtual driver

**Decision**: Implement a custom ASIO driver (`jyglobalvst_asio.dll`) that wraps the existing audio engine output buffer via a shared-memory ring buffer. The driver implements the Steinberg ASIO 2.3 host API (or a compatible subset sufficient for major DAWs: Ableton Live, Cubase, Reaper, FL Studio). The driver registers under `HKLM\SOFTWARE\ASIO\JyGlobalVST ASIO` so DAWs enumerate it without custom configuration. Multi-client support is provided by allowing multiple ASIO clients to open the driver when buffer parameters (sample rate, buffer size) match; mismatched open attempts return `ASE_NoMemory` or `ASE_FormatNotSupported` with a descriptive error.

**Rationale**:
- FR-013b/FR-032 mandate ASIO multi-client as a driver-level feature in v1.
- ASIO-only DAWs cannot consume WASAPI endpoints directly; without a native ASIO driver, users would need ASIO4ALL or FlexASIO as a secondary hop, reintroducing the multi-tool chain complexity JyGlobalVST aims to eliminate.
- A custom ASIO driver gives full control over latency reporting, buffer negotiation, and multi-client semantics.

**Architecture**:
- The audio engine writes processed samples to a lock-free shared-memory ring buffer (named section, e.g., `Global\JyGlobalVST_AsioRing_v1`) at the end of each `processBlock`.
- The ASIO driver DLL runs inside the DAW's process space. Its `bufferSwitch` callback reads from the shared-memory ring buffer into the DAW-supplied ASIO buffers.
- The ASIO driver does NOT host the audio engine; it is a pure consumer of the engine's output buffer. No audio processing occurs inside the ASIO driver.
- Synchronization uses a manual-reset event (`Global\JyGlobalVST_AsioEvent_v1`) signaled by the audio engine after each block write; the ASIO driver's `bufferSwitch` waits on this event with a timeout to detect engine stop.
- The shared-memory layout is versioned (header with magic, version, sample rate, buffer size, channel count, write index) so the driver can detect mismatches and reject open attempts gracefully.

**Alternatives considered**:
- **FlexASIO**: Open-source ASIO wrapper around PortAudio. Would require users to install and configure FlexASIO separately, reintroducing multi-tool complexity. Rejected.
- **ASIO4ALL**: Generic ASIO wrapper for WDM/KS devices. Does not expose a virtual ASIO endpoint from our own engine; would require the WaveRT driver to be visible as a WDM device, which adds latency and configuration burden. Rejected.
- **Steinberg ASIO SDK sample driver (`asiosample`)**: Good reference for bufferSwitch / createBuffers / disposeBuffers mechanics, but the sample is a loopback dummy. We use it as a structural template, replacing the dummy generator with our shared-memory ring buffer consumer.

**Licensing**:
- Steinberg ASIO SDK 2.3 is provided under a proprietary license that permits redistribution of the compiled driver binary. The SDK headers themselves are NOT redistributed in source form. A legal review is required before shipping. The SDK is downloaded at build time from Steinberg's developer site (registration required) and consumed as a build-only dependency.

**Open follow-ups**:
- Exact shared-memory ring buffer size (double-buffer vs. triple-buffer) to be validated against DAW startup behavior.
- ASIO driver uninstall: registry key removal + shared-memory section cleanup on engine shutdown.
- Test matrix: Ableton Live 12, Cubase 13, Reaper 7, FL Studio 21, Studio One 6.

---

## Summary of decisions

| # | Area | Decision | Notes |
|---|---|---|---|
| 1 | Driver model | WaveRT virtual endpoint + APO | WHQL-attestable, Sysvad reference |
| 2 | VST host framework | JUCE 8 + VST3 SDK 3.7 | Production-grade, license review needed |
| 3 | Audio backend | WASAPI shared via `IAudioClient3` | Sub-10 ms shared-mode latency |
| 4 | Plugin scan | In-process, background thread, cached | Per spec clarification — no sandbox |
| 5 | Plugin runtime isolation | SEH `__try`/`__except` + C++ `try`/`catch` | Per FR-023 |
| 6 | Chain mutation | Lock-free SPSC command queue | RT-safe |
| 7 | Resampling | JUCE WindowedSinc | Default; reconsider if AUDIO-004 fails |
| 8 | CPU monitoring | `QueryPerformanceCounter` per buffer | RT-safe |
| 9 | Installer | WiX 4 MSI, WHQL driver inside | MSIX deferred |
| 10 | Service IPC | Named pipe + JSON, session-scoped ACL | Plus shared-mem ring for meters |
| 11 | JSON validation | nlohmann/json + hand-rolled validator | Schema versioned, size capped |
| 12 | Single-instance | `Local\` namespace mutex per session | FR-022j |
| 13 | Accessibility | JUCE 8 `AccessibilityHandler` + UIA notifications | FR-019a/b |
| 14 | Update manifest | HTTPS GET → small JSON manifest | FR-022n; no identifying payload |
| 15 | Latency test | Hardware loopback for release; virtual loopback for CI | AUDIO-001/005 sign-off |
| 16 | ASIO multi-client | Custom ASIO driver via shared-memory ring buffer | FR-013b/FR-032; Steinberg SDK licensing review needed |

---

## 17. Windows Registry Patterns for WASAPI Virtual Audio Device Registration

**Research context**: This section documents the Windows registry architecture for WASAPI virtual audio device endpoints, idempotent registration, cleanup procedures, Windows Update resilience, and permission models. It serves as the reference for the WaveRT driver implementation (§1) and informs the service/installer design (§9).

### 17.1 Registry Keys for WASAPI Device Endpoints

#### **Authoritative Registry Paths**

All WASAPI endpoint devices are stored exclusively in **HKLM (registry key permissions restricted to SYSTEM by default)**:

```
HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Render\{GUID}
HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Capture\{GUID}
```

**Facts**:
- Each endpoint has a persistent globally-unique ID (`{GUID}`) that remains stable across reboots, even after driver uninstall/reinstall (GUID is tied to the endpoint's identity, not the driver instance).
- Endpoint management is performed exclusively by the Windows AudioEndpointBuilder service (runs as SYSTEM). User-mode applications and even driver installers typically do not write directly to these keys; instead, drivers register a device interface via `IoRegisterDeviceInterface()`, and AudioEndpointBuilder enumerates and creates endpoint entries reactively.
- **HKCU has no audio endpoint support**: Virtual audio endpoints cannot be registered per-user; they are always machine-wide (HKLM).

#### **Key Registry Values Under Each Endpoint**

Each `{GUID}` subkey contains properties subkeys and metadata:

1. **Device Identity and Naming**:
   - `FriendlyName` — User-facing name (e.g., "JyGlobalVST Virtual Output"). Set via device interface properties by the driver INF or by user override in Sound Control Panel (Mmsys.cpl).
   - Device GUID — The `{GUID}` itself is the endpoint's immutable identity for WASAPI enumeration.

2. **Role Timestamps** — Per-role default device tracking:
   - `Role:0` = eConsole (default for general playback, games)
   - `Role:1` = eCommunications (default for VoIP, system notifications)
   - `Role:2` = eMultimedia (default for media players)
   - Each role stores a Windows FILETIME value indicating when the device was last set as default for that role.

3. **Device State Flags** (inside `Properties` subkey):
   - `PKEY_DeviceClass_Subtype` — "Speakers", "Microphone", etc.
   - `PKEY_AudioEndpoint_Enabled` — Boolean: whether the endpoint is active/disabled
   - `PKEY_AudioEndpoint_PhysicalSpeakers` — Channel configuration bitmap (SPEAKER_FRONT_LEFT, SPEAKER_FRONT_CENTER, etc.)
   - `PKEY_AudioEndpoint_FormFactor` — Form factor hint (Speaker, Headphones, Microphone, etc.)

4. **Format Capabilities** (also under `Properties`):
   - `PKEY_AudioEngine_DeviceFormat` — Default format: sample rate, bit depth, channel count, speaker configuration. Example: `0x00180280` encodes 48 kHz, stereo, 16-bit.
   - Format negotiation at playback time is handled by the Stream Effect APO registered in the driver INF (via `AddReg` or `AudioHook` in the INF's device software key).

5. **Audio Processing Objects (APO) Registration** (under `FxProperties` subkey):
   - APO CLSIDs, effect ordering, and parameters.
   - Set by the driver INF under the device's software registry key (not under MMDevices).

#### **Pin Category GUIDs and Friendly Names**

Friendly names are resolved via registry lookup of standard KSNODETYPE GUIDs:

```
HKLM\SYSTEM\CurrentControlSet\Control\MediaCategories\{GUID}
```

Example standard pin categories:
- `KSNODETYPE_SPEAKER` (`{DFF220F3-F70F-11D0-B917-00A0C9223196}`)
- `KSNODETYPE_MICROPHONE` (`{DFF220F2-F70F-11D0-B917-00A0C9223196}`)

**Windows 10/11 modern pattern** (per [Microsoft Learn - Friendly Names for Audio Endpoint Devices](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/friendly-names-for-audio-endpoint-devices)):
- Drivers should register device-specific friendly names in the device's software registry key (`HKR\MediaCategories`) via the INF's `AddReg` section, not in the global `HKLM\SYSTEM\CurrentControlSet\Control\MediaCategories`.
- The device's software key is located at `HKLM\SYSTEM\CurrentControlSet\Services\<DriverServiceName>\...` or under the Enum/ROOT PnP path.

### 17.2 Idempotent Registration

#### **Detection Strategy: Is the Device Already Registered?**

1. **Enumerate existing endpoints**:
   ```
   HKEY regKey = HKEY_LOCAL_MACHINE
   RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
     "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\MMDevices\\Audio\\Render",
     0, KEY_READ, &hKey)
   // Then enumerate subkeys to find {GUID} matches
   ```

2. **Check by GUID**:
   ```
   IF EXISTS(HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Render\{YourTargetGUID})
       THEN already_registered = true
       ELSE not_registered
   ```

3. **Verify name and format properties**:
   ```
   RegQueryValueEx(hGuidKey, "FriendlyName", ..., &buffer, &bufferSize)
   // Compare against expected name
   IF name_matches AND format_matches
       THEN skip_registration (idempotent)
       ELSE update_properties_or_warn
   ```

#### **What Happens When Registering the Same Device Twice**

- **Windows does NOT auto-deduplicate**: If a device interface is registered twice without cleanup, the system creates orphaned/phantom entries.
- **GUID persistence is strict**: Once a GUID is assigned to an endpoint via `IoRegisterDeviceInterface()`, that exact GUID persists even if the device is disabled/removed and re-enabled (the GUID is tied to the device's identity, not the driver instance).
- **Sequence number increment**: The MMDevices hive tracks per-device-class sequence counters; re-registering without cleanup may increment these, leaving stale counters.
- **Risk**: Applications holding references to old GUIDs may experience intermittent "device not found" errors if a new GUID is created.

#### **Best Practice for Idempotent Registration**

```
1. Compute a deterministic device GUID from a stable identifier 
   (e.g., hash of device name + driver instance ID)
   
2. Query: 
   IF HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Render\{GUID} exists
       THEN device_already_registered = true
       ELSE device_needs_registration = true

3. If already registered:
     3a. Read FriendlyName and format properties
     3b. If properties match expected values:
         → Skip registration (idempotent, no change)
     3c. If properties differ:
         → Update properties (modify FriendlyName or format via INF/registry update)
         → Do NOT create a new GUID/endpoint
     3d. If device is marked Disabled or Unplugged:
         → Re-enable via Device Manager or registry Phantom bit reset
         → Do NOT create a new GUID

4. If not registered:
     4a. Call IoRegisterDeviceInterface() from driver (kernel mode)
     4b. Set device properties via INF's AddReg or registry API calls
     4c. AudioEndpointBuilder (system service) enumerates and creates MMDevice endpoint
     4d. Endpoint appears in HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Render\{GUID}
```

**Critical caveat**: There is **no documented Windows API for direct, idempotent registry writes to MMDevices**. The AudioEndpointBuilder service is the sole authority for endpoint creation. Attempting to directly write to `HKLM\...\MMDevices\...` from user mode (or even an installer) will typically fail due to SYSTEM-only ACLs, or worse, create orphaned entries if ACLs are forcibly changed. **The correct pattern is to register a device interface from the kernel driver; AudioEndpointBuilder does the rest automatically.**

### 17.3 Cleanup and Orphaned Entries

#### **Registry Entries That Must Be Cleaned Up**

When a virtual audio device is unregistered:

1. **Primary Endpoint Entry**:
   ```
   HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Render\{GUID}
   ```
   Delete the entire GUID subkey and its children.

2. **Device Service Entry** (if driver is WaveRT kernel service):
   ```
   HKLM\SYSTEM\CurrentControlSet\Services\<YourDriverServiceName>
   ```
   Delete or mark the service as disabled; uninstall via Device Manager or driver uninstall.

3. **Plug-and-Play Enum Entry**:
   ```
   HKLM\SYSTEM\CurrentControlSet\Enum\ROOT\<DeviceClassPath>
   ```
   Look for subkeys with the device name; check for `Phantom=dword:00000001` (indicates disabled/orphaned device).

4. **Pin Category Definitions** (if custom, not standard pin types):
   ```
   HKLM\SYSTEM\CurrentControlSet\Control\MediaCategories\{YourCustomPinCategoryGUID}
   ```
   Optional; clean up if you registered custom categories.

#### **Safe Deletion Procedures**

**Do NOT use simple `RegDeleteTree` on MMDevices entries**: These are protected by strict SYSTEM-only ACLs and registry encryption in some Windows versions.

**Safe patterns**:

1. **For endpoint registry cleanup via Device Manager**:
   - Open `devmgmt.msc` (Device Manager) as Administrator
   - View → Show hidden devices
   - Expand "Sound, video and game controllers"
   - Right-click the device → **Uninstall device**
   - Tick "Delete the driver software for this device" (recommended)
   - This triggers PnP deregistration, which cascades to endpoint deletion in MMDevices

2. **For direct registry cleanup** (requires explicit ACL change):
   - Right-click `HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio` in Registry Editor
   - → Properties → Security → Advanced
   - Change Owner from SYSTEM (or TrustedInstaller) to Administrators group
   - Grant Administrators "Full Control"
   - Then delete the device's GUID subkey
   - **Revert ACLs to SYSTEM ownership after cleanup** (important for security)

3. **For orphaned/phantom devices** (`Phantom=dword:00000001`):
   - Simple registry deletion alone is insufficient
   - Use Device Manager uninstall (as above)
   - Alternatively, use third-party tools like DDU (Display Driver Uninstaller) that forcibly remove phantom entries
   - Reboot after cleanup to finalize phantom removal

#### **Detecting Orphaned/Stale Device Registry Entries**

Orphaned entries have these characteristics:

- **FriendlyName mismatch**: Shows "(Disabled)", "(Not present)", or a stale name from a previous version
- **Permanent device state**: `PKEY_AudioEndpoint_Enabled = false` and never returns to true, or state shows "Unplugged" indefinitely
- **Phantom bit set**: In `HKLM\SYSTEM\CurrentControlSet\Enum\ROOT\...\<DeviceID>`, the registry value `Phantom=dword:00000001` is present
- **No corresponding service**: No matching service in `HKLM\SYSTEM\CurrentControlSet\Services\<DriverServiceName>`
- **GUID becomes invalid**: WASAPI calls with the GUID return `E_INVALIDARG` or `AUDCLNT_E_DEVICE_INVALIDATED`

**Detection code pattern** (pseudo-C++):
```cpp
bool IsDeviceOrphaned(const wchar_t* deviceGuidStr) {
    // 1. Check if GUID still exists in MMDevices
    HKEY hGuid = nullptr;
    LONG res = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\MMDevices\\Audio\\Render",
        0, KEY_READ, &hGuid);
    if (res != ERROR_SUCCESS) {
        return true; // Device doesn't exist at all
    }
    
    // 2. Query FriendlyName and device state
    DWORD cbData = 0;
    RegQueryValueExW(hGuid, L"FriendlyName", nullptr, nullptr, nullptr, &cbData);
    if (cbData == 0) {
        // FriendlyName is missing or empty
        RegCloseKey(hGuid);
        return true;
    }
    
    // 3. Check for "Not present" state
    DWORD state = 0;
    cbData = sizeof(state);
    RegQueryValueExW(hGuid, L"State", nullptr, nullptr, (LPBYTE)&state, &cbData);
    RegCloseKey(hGuid);
    if (state == DEVICE_STATE_NOTPRESENT) {
        return true;
    }
    
    return false;
}
```

### 17.4 Windows Update Impact

#### **Registry Persistence Across Windows Updates**

**Finding: Windows Update has INCONSISTENT and version-dependent effects on virtual audio device registry entries.**

**Typically preserved**:
- GUID identity of registered endpoints persists (by design — the GUID is tied to the endpoint's logical identity, not the driver binary).
- Role timestamps remain stable.
- Most FriendlyName values are retained.

**Sometimes reset or cleared**:
- If a new Windows audio driver stack is installed (e.g., audio chipset driver update), the system re-enumerates audio endpoints, and format capabilities may be re-negotiated or reset.
- Virtual audio driver compatibility issues: Some Windows Updates (notably Windows 11 21H2 and later) have **blocked system upgrades** if an incompatible virtual audio driver is detected. Examples: VB-Audio VAC, older VoiceMeeter versions, Equalizer APO.
- Custom APO registrations can be lost if the driver INF is overwritten or the driver service is re-installed.
- Device state flags may be reset if the audio driver infrastructure is updated.

#### **Detection and Recovery Strategy**

Per [Microsoft Learn - Recovering from an Invalid-Device Error](https://learn.microsoft.com/en-us/windows/win32/coreaudio/recovering-from-an-invalid-device-error):

**Application-level recovery**:
1. **Listen for disconnection events**: Implement `IAudioSessionEvents::OnSessionDisconnected` callback.
2. **Check for device invalidation errors**: If WASAPI calls return `AUDCLNT_E_DEVICE_INVALIDATED`, the device state changed (possibly due to Windows Update, audio driver change, or user action).
3. **Possible reasons for disconnection**:
   - User removed/disabled the device via Device Manager
   - Audio hardware reconfiguration
   - **Preferred stream format changed** (common trigger for Windows Update)
   - Windows Audio Service restarted
   - Device's driver was updated

**Application code pattern**:
```cpp
HRESULT OnDeviceDisconnected(LPCWSTR pwstrDeviceId) {
    // Called when AUDCLNT_E_DEVICE_INVALIDATED is returned
    // 1. Release current audio client and device
    // 2. Re-enumerate endpoints via IMMDeviceEnumerator
    // 3. If the device GUID is still present, try to reinitialize with new format
    // 4. If the GUID is gone, use default device or notify user
    return S_OK;
}
```

**Driver/Installer-level recovery**:
- Do NOT assume registry entries persist unchanged across major Windows Updates.
- Implement device re-registration on driver startup: check if GUID exists, verify properties, repair/update if necessary.
- Check for "Phantom" bit and stale entries on each boot.
- Log registry state changes for diagnostics.

#### **Persistence Across Windows 10 ↔ Windows 11 Upgrade**

- **Generally preserved**: Device GUIDs and FriendlyNames typically survive 10→11 upgrade.
- **Potential issues**: Audio driver stack changes (Windows 11 introduced new audio policies, APO requirements). Drivers must be re-signed and tested for Windows 11.
- **Recommendation**: Test the virtual device after upgrade; re-register/update if necessary via the installer's upgrade mode.

### 17.5 Permission Requirements

#### **HKLM Write Access**

**Strict requirement: Administrator (SYSTEM) privileges only**

- Non-admin users have **zero write access to HKLM** by default. Even with explicit ACL changes, this introduces security vulnerabilities.
- The only safe, supported way to modify HKLM is via installer (running with UAC elevation) or system services (running as SYSTEM).

**Workarounds for user-mode applications**:
1. **Run the installer/registration as Administrator**: Installer prompts for UAC elevation once during setup.
2. **Implement a Windows Service running in SYSTEM context**: Service handles registry writes; user-mode tray app communicates via IPC (named pipes, RPC, or message queue).
3. **User-mode only (no direct registry writes)**: Non-admin users can enumerate existing endpoints and use them, but cannot register new ones.

#### **Can Standard (Non-Admin) Users Register Virtual Audio Devices?**

**Answer: No, they cannot.** Virtual audio endpoint registration requires SYSTEM privileges because:
- Writing to `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\...` requires SYSTEM/Administrators ACL.
- Writing to `HKLM\SYSTEM\CurrentControlSet\Services\...` requires SYSTEM/Administrators ACL.
- AudioEndpointBuilder service (runs as SYSTEM) is the sole process permitted to enumerate and create endpoints.

**Implication for JyGlobalVST**: 
- The installer (running as Administrator via UAC elevation) must register the WaveRT kernel driver and create the initial virtual endpoint.
- The tray app (runs as standard user) enumerates the registered endpoint via WASAPI but does not attempt to register new endpoints.
- Any runtime device registration or repair must be delegated to the Windows Service (running as SYSTEM) or a background task triggered by the installer.

#### **HKLM vs. HKCU Registry Hives**

| Hive | Access | Audio endpoints? | Use case |
|------|--------|---|---|
| **HKLM** | Admin/SYSTEM only | Yes, mandatory | Virtual device registration, driver config, system-wide settings |
| **HKCU** | Current user (standard user OK) | No | User preferences (theme, window geometry), roaming profile data |

**For JyGlobalVST**:
- **HKLM** is used for virtual endpoint registration (installer-time, SYSTEM privilege).
- **HKCU** is used for user preferences (roaming settings, preset favorites) — tray app writes as standard user.

#### **Permission Model for JyGlobalVST Architecture**

**Testable-dev mode** (no kernel driver):
- No HKLM writes required for device registration.
- Enumerate existing WASAPI endpoints via `IMMDeviceEnumerator` (read-only, any privilege level).
- Use an existing loopback device (virtual cable or WASAPI loopback) as the capture source.
- User runs tray app as standard user; no admin required.

**Release service mode** (WaveRT driver + Windows Service):
- **Installer** (runs as Administrator via UAC elevation):
  - Register WaveRT driver package (`.inf` + `.sys` + driver `.cat`).
  - Create virtual audio endpoint in `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Render\{GUID}`.
  - Set up Windows Service (jyglobalvst_service.exe) to run as SYSTEM.
  - One UAC prompt total during install.

- **Tray app** (runs as standard user):
  - Enumerate endpoints via WASAPI (read-only, no privilege required).
  - Communicate with Windows Service via named pipes (session-scoped, IPC ACL enforced).
  - All audio engine mutations delegated to service.

- **Windows Service** (runs as SYSTEM):
  - Host the audio engine (processBlock callback).
  - Handle HKLM registry updates if needed (device re-registration, repair).
  - Publish audio state to shared-memory ring buffer for tray app to read.

---

**Decision**: 
- Virtual audio device registration MUST occur at installer-time with Administrator privileges (or via SYSTEM service).
- Standard users can enumerate and use registered endpoints, but cannot register new ones.
- The tray app operates in standard-user mode; no admin required for runtime operation.
- Installer provides a clear permission model: one UAC prompt at setup; thereafter, no elevation needed.

---

**No remaining unknowns. Phase 0 complete.**
