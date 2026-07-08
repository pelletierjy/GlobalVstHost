# Phase 0 Research: Driverless System-Audio Capture

## R1: Does muting a render endpoint's `IAudioEndpointVolume` also silence its concurrent WASAPI loopback capture?

- **Status**: **UNRESOLVED — requires an empirical spike at the start of implementation.** I do not
  have a verified source that gives a single, driver-independent answer to this question, and it is
  the load-bearing assumption behind FR-018's primary design (mute the captured device, keep
  listening only on the output device).
- **Why it matters**: Whether shared-mode endpoint mute/volume is applied by the audio engine
  (upstream of where loopback taps the stream) or by the driver/hardware (downstream of the tap)
  is not something documented consistently across all WASAPI endpoint driver implementations. This
  varies by whether the specific device's volume control is implemented in software (APO) or in
  hardware (DAC-level mute), which differs by sound card/driver vendor.
- **Decision**: Do not commit to the mute-based UX (FR-018 primary path) until this is verified on
  at least two representative endpoint types (a typical laptop/onboard HD Audio output, and one
  USB/external DAC). Build the fallback path (FR-018's "require a non-listened endpoint" mode)
  from day one as a first-class option, not an afterthought, so the feature is not blocked if mute
  turns out to zero the loopback signal on common hardware.
- **Rationale**: This matches the spec's own Q1 clarification, which already anticipated this
  exact failure mode and specified the fallback. Treating it as resolved without testing would be
  asserting a claim I cannot verify.
- **Alternatives considered**:
  - *Skip verification, ship only the fallback ("non-listened endpoint") mode* — rejected because
    it forecloses the mute UX the user selected as the recommended option in clarification Q1
    without giving it a real chance.
  - *Assume mute works everywhere based on general WASAPI architecture reasoning* — rejected per
    the project's honesty standard; this is exactly the kind of unverified technical claim that
    must be flagged, not assumed.
- **Spike plan** (to run before committing to FR-018's UI copy/flow): open loopback capture on a
  render endpoint, start playback, call `IAudioEndpointVolume::SetMute(TRUE)` on that endpoint, and
  observe whether the loopback capture buffer goes silent. Repeat on a second, differently-driven
  endpoint. Record results in this file once available.

## R2: Extending `WasapiCapture` to support loopback mode

- **Decision**: Add an explicit loopback flag/mode to `WasapiCapture::open()` (or a sibling
  factory) that, when set, resolves the given `EndpointId` as a **render** endpoint (not capture),
  activates an `IAudioClient` on it, and calls `Initialize(..., AUDCLNT_STREAMFLAGS_LOOPBACK, ...)`
  using the render endpoint's own shared-mode mix format (`IAudioClient::GetMixFormat`) rather than
  a caller-supplied format — this is the standard WASAPI loopback pattern.
- **Rationale**: This is well-established, documented Win32 API usage (not the undocumented
  `IPolicyConfig` surface used elsewhere in the project for spec 004). It requires no new COM
  interfaces beyond ones already linked (`audioclient.h`), only a different `EDataFlow` target and
  stream flag. `wasapi_capture.cpp` already confirms (existing code, `wasapi_capture.cpp:345-346`)
  a single, controlled site where `dwStreamFlags` is assembled, so this is a localized, additive
  change, not a rewrite.
- **Alternatives considered**:
  - *Write a brand-new `WasapiLoopbackCapture` class* — rejected: duplicates ~90% of the existing
    thread/ring-buffer/format-conversion logic in `WasapiCapture` for no benefit; higher
    maintenance cost.
  - *Route loopback through JUCE's `AudioDeviceManager`* — rejected: JUCE's device abstraction does
    not expose WASAPI loopback-of-a-specific-render-endpoint as a first-class capture source on
    Windows in the version this project pins; going direct-WASAPI (as the project already does for
    capture) keeps behavior explicit and testable.

## R3: Implementing `WasapiOutput` (currently a stub)

- **Decision**: Implement `WasapiOutput` as a direct `IAudioClient`/`IAudioRenderClient` shared-mode
  render client, mirroring `WasapiCapture`'s structure (dedicated thread, event-driven where
  available, pre-allocated conversion buffers, no allocation after `open()`), rather than continuing
  to route output through JUCE's `AudioDeviceManager`.
- **Rationale**: `wasapi_output.cpp`'s existing placeholder comment already anticipates this
  ("Release prep: direct IAudioClient3 binding"). Two independent, user-selectable render
  endpoints (capture-source device and output device) cannot both be modeled through JUCE's single
  current-device abstraction in testable-dev mode; a direct WASAPI render client is required
  regardless of eventual driver status.
- **Alternatives considered**:
  - *Keep JUCE `AudioDeviceManager` for output, add a second JUCE device instance* — rejected:
    JUCE's `AudioDeviceManager` is designed around a single active device pair per instance; running
    two independent instances to get two independent devices adds JUCE-level complexity without
    removing the need for direct WASAPI control over the *capture* side, which already bypasses
    JUCE.

## R4: Same-device conflict detection (FR-005, FR-014)

- **Decision**: Resolve both the capture source and the output device to their concrete
  `EndpointId` (not just "system default" vs. a name) before allowing `start()`, and hard-block
  (return an error, no partial start) if the two resolved IDs are equal. During active processing,
  subscribe to `AudioEndpointEnumerator`/`DeviceWatchdog`'s `onDefaultEndpointChanged` /
  `OnDefaultDeviceChanged` callbacks (already present in the codebase) and re-run the same
  resolved-ID comparison whenever a "follow system default" selection could change; pause processing
  with a clear message on conflict (FR-014), rather than allow a feedback path.
- **Rationale**: The existing `AudioEndpointEnumerator` (`src/shared/platform/audio_endpoints.h`)
  already resolves symbolic defaults to concrete `EndpointDescriptor`s and already has an observer
  hook; this reuses that machinery instead of adding a second device-tracking mechanism.
- **Alternatives considered**:
  - *Compare friendly names instead of endpoint IDs* — rejected: friendly names are not guaranteed
    unique or stable (e.g., duplicate USB devices), and the project's existing `EndpointId` type is
    already the canonical identity used elsewhere (`WasapiCapture::open`, `AudioEndpointEnumerator`).

## R5: Mute/restore of the captured endpoint's own volume (FR-018)

- **Decision**: Add a small `EndpointVolumeGuard`-style wrapper (new file,
  `src/shared/platform/endpoint_volume.h/.cpp`) around `IAudioEndpointVolume`
  (`IMMDevice::Activate(__uuidof(IAudioEndpointVolume))`) that, on activation, records the current
  `GetMute()` state and, only if the loopback-does-not-go-silent result from R1 holds, calls
  `SetMute(TRUE)`; on stop/exit (including abnormal exit paths where feasible — see spec Edge Case
  "App crash or forced exit while the captured device is muted"), restores the recorded state.
- **Rationale**: `IAudioEndpointVolume` is a documented, public Win32 COM interface (unlike the
  undocumented `IPolicyConfig` interfaces spec 004 uses for default-endpoint switching), so no new
  undocumented-API risk is introduced.
- **Open risk carried over from R1**: this entire decision is conditional on R1's spike results.
  If muting silences the loopback capture, FR-018's fallback path (require a non-listened captured
  endpoint) becomes the shipped behavior instead, and this wrapper is not exercised in the mute
  role — it may still be useful for the fallback path's UX (e.g., confirming the user hasn't
  routed their only listening device as the capture source).
- **Crash-safety alternative considered**: registering an atexit/signal handler to force-restore
  mute state — rejected as the primary mechanism because Windows already restores per-application
  audio session state on process exit in most cases and the project's own principles disclaim
  crash/hang handling as a non-goal (CLAUDE.md, Key Design Principle 4: "memory corruption and
  audio-thread hangs are accepted non-goals per spec clarification"); the plan instead relies on
  normal `stop()`/destructor paths and documents the residual risk in quickstart.md's manual test
  steps rather than over-engineering crash recovery.

## R6: Settings persistence extension (FR-016)

- **Decision**: Add two optional string fields to the existing roaming settings schema
  (`roaming_settings.h`) — e.g. `capture_endpoint_id` and `output_endpoint_id` — following the same
  "optional, degrade gracefully if absent/stale" pattern already used by
  `default_hardware_device_friendly_name`. A `follow_default_capture` boolean flag captures whether
  the user pinned a specific endpoint or wants the symbolic system default.
- **Rationale**: Reuses the existing `unknown_fields`-preserving forward-compat pattern already in
  `roaming_settings.h`; no new persistence mechanism or file is introduced, consistent with FR-016
  and the project's existing state-persistence design (CLAUDE.md, State Persistence section).
- **Alternatives considered**:
  - *New dedicated JSON file for routing configuration* — rejected: adds a second settings file to
    reason about for no benefit; the existing roaming settings file already has spare, low-churn
    capacity for a couple of ID strings and a bool.

## Summary of NEEDS CLARIFICATION resolution

| Technical Context item | Resolution |
|---|---|
| Latency performance goal | **Not resolved — intentionally deferred** per user instruction (spec Q2). Carried into Complexity Tracking (plan.md) as an open, tracked deviation rather than silently closed here. |
| All other Technical Context fields | Resolved directly from existing project conventions (C++20/MSVC, JUCE 8.x, GoogleTest, Windows 10 1909+/11 x64, single-project structure) — no ambiguity required research. |
