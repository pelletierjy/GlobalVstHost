# Contract: `IAudioEngine` / `IAudioEngineListener` Additions

This project's only cross-boundary contract is `IAudioEngine`
(`src/audio-engine/include/jyglobalvst/audio_engine.h`) — the interface the tray-app UI depends on
instead of engine internals (CLAUDE.md, Key Design Principle 1). This document specifies the
additive changes this feature requires to that contract. It does **not** repeat the existing
interface; only deltas are listed. Existing method signatures are unaffected unless explicitly
called out as **CHANGED**.

## What already covers this feature without change

- `listInputs()` / `selectInput(EndpointId)` / `currentInput()`: The existing doc comment on
  `listInputs()` already anticipates this feature — *"the engine captures from a user-selected
  existing input device (WASAPI loopback on default render, or a virtual cable)"*. This feature
  makes that comment true in practice: `listInputs()` returns real render endpoints (via
  `AudioEndpointEnumerator::list(EndpointFlow::Render)`), and `selectInput()` resolves the chosen
  endpoint for loopback capture instead of a virtual-cable-fed capture endpoint. **No signature
  change.**
- `listOutputs()` / `selectOutput(EndpointId)` / `currentOutput()`: unchanged signature. Internally,
  `selectOutput` now binds a real `WasapiOutput` render client (research.md R3) instead of routing
  through JUCE's `AudioDeviceManager`, but the UI-facing contract is identical.
- `latencyProfile()` / `LatencyProfile`: already exposes `total_round_trip_ms` and per-stage
  breakdown fields. This satisfies FR-017 ("measure and expose the actual round-trip latency")
  as-is — the implementation must populate real capture/output timing for the loopback path;
  **no new method needed**.
- `onDeviceLost` / `onDeviceRestored` (`IAudioEngineListener`): already exist for FR-024-style
  device loss/restore. This feature reuses them for FR-008/FR-009/FR-010 (output removed, capture
  source removed/default-changed, device reappears) — **no new listener method needed** for the
  base hot-plug case.

## New additions required

### `IAudioEngine::start()` — **CHANGED behavior, not signature**

`start()` MUST now perform the same-device resolution check (research.md R4) before binding
capture/output, and MUST fail without partially starting if the resolved capture and output
endpoint IDs are equal (FR-005). Since `IAudioEngine` methods do not throw (per the interface's own
REALTIME CONSTRAINTS header: "Failures... are NOT thrown; they are delivered asynchronously through
IAudioEngineListener"), this failure is reported via a new listener callback (below), not a return
value or exception.

### `IAudioEngineListener::onSameDeviceConflict(const EndpointId& device)` — **NEW**

```cpp
// FR-005 / FR-014: capture and output resolved to the same endpoint. Fired either
// when start() is refused for this reason, or when a running session's "follow
// system default" selection changes such that capture and output would coincide
// (processing is paused, not stopped-and-discarded, when this fires mid-session).
virtual void onSameDeviceConflict(const EndpointId& device) = 0;
```

- **Rationale for a new callback** rather than overloading `onDeviceLost`: `onDeviceLost` implies
  the device itself became unavailable; a same-device conflict is a *configuration* state, both
  devices are present and healthy. Conflating the two would make it impossible for the UI to show
  the correct message (FR-005's "explains that capture and output must be different devices" vs.
  FR-008/009's "device was removed, choose another").

### `IAudioEngineListener::onCaptureMuteFallbackRequired(const EndpointId& device)` — **NEW**

```cpp
// FR-018 fallback path: the engine determined (research.md R1) that muting this
// captured endpoint would also silence its loopback capture, so the mute/restore
// UX cannot be used. Fired once, when the condition is first detected for a given
// capture endpoint. The UI must instead prompt the user to pick a capture endpoint
// they do not listen to directly.
virtual void onCaptureMuteFallbackRequired(const EndpointId& device) = 0;
```

- **Rationale**: FR-018 explicitly requires the system to "detect" and "clearly communicate" the
  fallback requirement. Since whether mute silences loopback is per-endpoint/driver (research.md
  R1), this cannot be a static UI decision — the engine must surface it live.

### `IAudioEngine::isCaptureDeviceMuted() const` — **NEW**

```cpp
// FR-018: true while the engine is actively suppressing the captured endpoint's
// own audible output (mute-based UX path). False when not running, or when the
// fallback (non-listened endpoint) path is in effect instead.
virtual bool isCaptureDeviceMuted() const = 0;
```

- **Rationale**: Lets the UI show the user *why* their speakers went quiet (this app muted them)
  rather than leaving it opaque, and gives a hook for the spec's edge case about restoring state if
  the app is closed normally (query-then-confirm-restored in manual QA, see quickstart.md).

## Unchanged but newly load-bearing

- `EndpointId` (`std::string`, opaque Windows endpoint identifier) is the type used for all
  resolved comparisons in this feature (research.md R4) — no new identifier type introduced.
- `DeviceResolutionSource` (`EndpointIdMatch | FriendlyNameMatch | WindowsDefaultFallback`) already
  models how a persisted selection was resolved on load; reused as-is for the Routing
  Configuration's degrade-gracefully behavior (FR-016, data-model.md).
