# Phase 1 Data Model: Driverless System-Audio Capture

Entities derived from spec.md's Key Entities section, expressed against the existing codebase's
types (`jyglobalvst::shared::EndpointId`, `EndpointDescriptor`, `EndpointFlow` from
`src/shared/platform/audio_endpoints.h`) rather than introducing parallel types where an existing
one already fits.

## Capture Source

Represents the render endpoint being loopback-captured.

| Field | Type | Notes |
|---|---|---|
| `selection_mode` | enum `{ SystemDefault, SpecificEndpoint }` | Corresponds to spec's "symbolic system default vs. specific endpoint identity" (Key Entities). |
| `pinned_endpoint_id` | `EndpointId` (optional/empty when `SystemDefault`) | Only meaningful when `selection_mode == SpecificEndpoint`. |
| `resolved_endpoint_id` | `EndpointId` | The concrete, current endpoint actually being captured — always populated once resolved, regardless of `selection_mode`. This is what conflict detection (R4) compares against Output Device. |
| `negotiated_format` | existing `WAVEFORMATEX`-derived fields already tracked by `WasapiCapture` (`negotiatedSampleRate()` etc.) | No new type — reuse `WasapiCapture` accessors. |
| `availability_state` | see **Device Availability State** below | |
| `mute_state` | struct `{ bool was_muted_before; bool currently_suppressed_by_app; }` | Backing for FR-018 mute/restore; owned by the new `EndpointVolumeGuard` (research.md R5), not duplicated elsewhere. |

**Validation rules**:
- `resolved_endpoint_id` MUST be re-derived (not cached indefinitely) whenever
  `onDefaultEndpointChanged` fires and `selection_mode == SystemDefault` (FR-009).
- `resolved_endpoint_id` MUST NOT equal Output Device's `resolved_endpoint_id` while processing is
  active (FR-005/FR-014) — enforced by the conflict guard (research.md R4), not by this struct
  itself; the struct only carries the value being compared.

## Output Device

Represents the render endpoint the processed audio is sent to.

| Field | Type | Notes |
|---|---|---|
| `resolved_endpoint_id` | `EndpointId` | Must be a specific endpoint; unlike Capture Source, spec.md does not define a "follow system default" mode for output (FR-004 says "select an output device", no default-follow language) — output is always a pinned selection. |
| `negotiated_format` | Backed by the new `WasapiOutput` implementation's equivalent of `WasapiCapture`'s format accessors (research.md R3). | |
| `availability_state` | see **Device Availability State** below | |

**Validation rule**: Constraint from spec.md Key Entities — "must not equal the Capture Source's
resolved endpoint" — same hard-block rule as above, single source of truth in the conflict guard.

## Routing Configuration

The persisted pairing, matching spec.md's Key Entities entry and research.md R6's settings schema
extension.

| Field | Type | Persisted as (roaming_settings.h) |
|---|---|---|
| `capture_selection_mode` | enum `{ SystemDefault, SpecificEndpoint }` | `follow_default_capture: bool` (`true` ⇔ `SystemDefault`) |
| `capture_pinned_endpoint_id` | `EndpointId` (optional) | `capture_endpoint_id: string` (empty when following default) |
| `output_endpoint_id` | `EndpointId` | `output_endpoint_id: string` |

**State transition**: On load, if a persisted endpoint ID is no longer present among enumerated
endpoints (`AudioEndpointEnumerator::list()`), degrade gracefully per FR-016 — treat as unset and
prompt the user, rather than failing to start. No other lifecycle/state-machine beyond
present/absent is required for this entity; it is a flat, versionless preference record consistent
with the rest of `roaming_settings.h`.

## Device Availability State

Per-device runtime status, shared by both Capture Source and Output Device (spec.md Key Entities).

| State | Meaning | Drives |
|---|---|---|
| `Present` | Endpoint enumerated and usable. | Normal operation. |
| `Removed` | Endpoint no longer enumerated (unplugged / disabled). | FR-008 (output) / FR-009 (capture) recovery prompts; no crash. |
| `ExclusiveHeld` | Endpoint present but loopback/shared-mode `Initialize` failed because another app holds it exclusively. | FR-013 actionable message. |
| `Conflicting` | Resolved capture and output endpoint IDs coincide (only reachable via a default-device change while running, since initial selection is hard-blocked). | FR-014 — pause processing, clear message. |

**State source**: This is not a new stored/serialized entity — it is derived at runtime from
existing `AudioEndpointEnumerator`/`DeviceWatchdog` notifications (`OnDeviceRemoved`,
`OnDeviceStateChanged`, `OnDefaultDeviceChanged`) plus the WASAPI `Initialize()` result code (for
`ExclusiveHeld`) and the conflict guard's comparison (for `Conflicting`). No new polling loop is
introduced; this table exists to name the states consistently across the engine/UI boundary (see
contracts/audio-engine-interface.md).
