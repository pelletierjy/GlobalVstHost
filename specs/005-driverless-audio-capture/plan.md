# Implementation Plan: Driverless System-Audio Capture (WASAPI Loopback → Separate Output)

**Branch**: `005-driverless-audio-capture` | **Date**: 2026-07-01 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/005-driverless-audio-capture/spec.md`

## Summary

Replace the third-party-virtual-cable-fed capture path with real WASAPI loopback capture of a
user-selected render (playback) endpoint, and add a real WASAPI render-client output path to a
second, distinct render endpoint — removing both the third-party virtual cable and the blocked,
unsigned kernel driver (spec 004) from the default capture path. The existing VST3
`AudioProcessorGraph`, format-conversion, and resampling layers are reused unchanged; this feature
only changes how audio enters and leaves that graph. New work: (1) extend `WasapiCapture` to open
in loopback mode against a render endpoint; (2) implement the currently-stubbed `WasapiOutput`
render client; (3) add same-device conflict detection (hard block, FR-005/FR-014); (4) add
mute/restore of the captured endpoint's own volume via `IAudioEndpointVolume` (FR-018), with a
documented fallback if muting is found to zero the loopback signal; (5) extend settings persistence
to store capture/output endpoint IDs; (6) wire the existing `DeviceWatchdog` /
`AudioEndpointEnumerator` device-change notifications into recovery instead of requiring a restart.

## Technical Context

**Language/Version**: C++20 (MSVC required per project standard; unchanged by this feature)

**Primary Dependencies**: JUCE 8.x (`AudioProcessorGraph`, existing chain UI), Win32 WASAPI
(`mmdeviceapi.h`, `audioclient.h`, `endpointvolume.h` — new dependency: `IAudioEndpointVolume`),
GoogleTest

**Storage**: `%AppData%\Roaming\JyGlobalVST\settings.json` (existing roaming settings file,
`roaming_settings.h`) — extended with capture/output endpoint ID fields

**Testing**: GoogleTest unit tests (`tests/unit/`) + integration tests against the existing
`loopback_fixture.h` harness (`tests/integration/`)

**Target Platform**: Windows 10 1909+ and Windows 11, x64 only (unchanged from root spec)

**Project Type**: Desktop application, single existing project layout (no new top-level
component); all new code lives under the existing `audio-engine` and `shared` static libs

**Performance Goals**:
- CPU ≤ 5% for the capture + resample + render path, excluding plugin cost (AUDIO-002, unchanged
  constitutional target)
- Round-trip latency: **NEEDS CLARIFICATION — deferred by explicit user decision (spec
  Clarifications, Q2).** The constitutional ≤ 10 ms NON-NEGOTIABLE target (Principle II) is flagged
  as at-risk for shared-mode WASAPI loopback (AUDIO-001) and is measured but not gated during this
  implementation; the user has instructed this be revisited at the end only if needed. This is
  carried forward, not resolved, in Phase 0 research (see research.md R1) and recorded as a
  documented constitutional deviation in Complexity Tracking below.

**Constraints**:
- No administrator privileges, no driver install/load/signing on the capture path (FR-002)
- Audio-thread real-time discipline unchanged (no malloc/mutex/blocking I/O/logging in the audio
  callback — Constitution Principle V)
- Capture device and output device must never be permitted to resolve to the same endpoint while
  processing is active (FR-005, FR-014 — hard block, no override)

**Scale/Scope**: Single active capture-source + output-device pair per running instance
(unchanged from existing engine's single-graph model); no multi-instance or multi-user scope

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

**Result: CONDITIONAL PASS — two documented deviations, both explicitly accepted by the user
during clarification (spec.md Clarifications, Session 2026-07-01) rather than introduced silently
here.**

| Principle | Check | Status |
|---|---|---|
| I. Spec Hierarchy & Component Isolation | Feature decomposes cleanly into the existing `audio_routing` component; no new component boundary needed. | PASS |
| II. Performance Requirements (NON-NEGOTIABLE) — CPU ≤5%, zero dropouts/12h | Unaffected by this feature's scope; existing resample/graph budget carries over. Verified in Phase 1 test plan (AUDIO-002, AUDIO-003). | PASS |
| II. Performance Requirements (NON-NEGOTIABLE) — latency ≤10ms round-trip | **AT RISK.** Shared-mode WASAPI loopback is expected to add materially more latency than the kernel-driver path. Per the user's explicit instruction (spec Q2), this is measured and documented (AUDIO-001) but the accept/reject decision on the constitutional deviation is **deferred to the end of implementation**, not resolved now. Recorded in Complexity Tracking as an open deviation, not silently waived. | **DEVIATION — DEFERRED (see Complexity Tracking)** |
| III. Task Sequencing & Dependencies | Tasks phase will order: device-loopback capability → device-loss/conflict detection → mute/restore → settings persistence → UI wiring, per standard infra→core→routing→UI sequencing. | PASS (to be verified at `/speckit-tasks`) |
| IV. Naming & Conventions | Spec-Kit sequential branch naming (`005-driverless-audio-capture`) is used, consistent with the precedent already set by specs 002–004 in this repo, superseding the constitution's older `feat/<component>` example. No new deviation introduced by this feature. | PASS (pre-existing precedent) |
| V. Real-Time Code Documentation & Constraints | New files (`wasapi_capture.cpp` loopback path, `wasapi_output.cpp` real implementation) MUST carry the REALTIME CONSTRAINTS header block, matching existing `wasapi_capture.cpp`/`wasapi_capture.h` precedent. | PASS (enforced at implementation) |
| VI. Dependencies & Build Order — driver before VST host integration | **DEVIATION.** This feature intentionally ships VST-host-integrated audio routing (loopback capture + output) without a completed, signed virtual device driver, because the driver is blocked by an external constraint (no valid Microsoft signing certificate) outside this feature's control. The driver (spec 004) is not deleted — it remains behind its existing build gate (`JYGLOBALVST_BUILD_DRIVER`) for a future signed-distribution path. | **DEVIATION — JUSTIFIED (see Complexity Tracking)** |

Both deviations are pre-existing, user-acknowledged tradeoffs (not something this plan invents) and
are carried forward with explicit tracking rather than silently resolved. Re-checked after Phase 1
design below.

### Audio Component Dependency Graph *(applicable — audio feature)*

```
[Real render endpoint (loopback source)] → [WasapiCapture: loopback mode] → [resample] →
[AudioProcessorGraph (VST3 chain, unchanged)] → [resample] → [WasapiOutput: render client] →
[Real render endpoint (distinct output)]
                          ↓
        [AudioEndpointEnumerator + DeviceWatchdog: hot-plug / default-change / conflict detection]
                          ↓
        [IAudioEndpointVolume mute/restore on captured endpoint]
```

- **Phase blocking**: Loopback-capable `WasapiCapture` extension MUST land before the conflict
  detector and mute/restore logic (both depend on knowing the resolved capture endpoint).
- **Phase blocking**: `WasapiOutput` real implementation MUST land before end-to-end User Story 1
  can be validated (currently a stub routed through JUCE's `AudioDeviceManager` in testable-dev
  mode).
- **Non-blocking**: Settings persistence (endpoint-ID fields) and UI wiring depend only on the
  finalized `IAudioEngine` surface additions (contracts/audio-engine-interface.md), not on internal
  WASAPI details.

### Audio Technical Context *(applicable — audio feature)*

- **Buffer Size**: Reuses existing negotiated shared-mode buffer sizing in `WasapiCapture`
  (no change to the 128-sample constitutional minimum)
- **Sample Rates**: 44.1 / 48 / 96 / 176.4 / 192 kHz, unchanged (FR-002 elsewhere in project);
  loopback capture negotiates the render endpoint's shared-mode mix format, converted at the
  boundary same as today
- **Latency Budget**: Target ≤10 ms round-trip (constitutional); **actual figure to be measured
  or shared-mode WASAPI loopback path and documented per AUDIO-001 — deviation acceptance deferred
  per spec Q2**
- **Real-Time Constraints**: No blocking I/O, no malloc/new after `open()`, no mutex acquisition,
  no logging inside the capture/render audio threads — identical discipline to the existing
  `WasapiCapture` implementation
- **CPU Target**: ≤5% for capture+resample+render, excluding plugin cost (AUDIO-002)

## Project Structure

### Documentation (this feature)

```text
specs/005-driverless-audio-capture/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md         # Phase 1 output
├── quickstart.md         # Phase 1 output
├── contracts/            # Phase 1 output
│   └── audio-engine-interface.md
└── tasks.md              # Phase 2 output (/speckit-tasks — not created here)
```

### Source Code (repository root)

No new top-level directories. This feature extends the existing `audio-engine` and `shared`
static libraries in place:

```text
src/
├── shared/
│   └── platform/
│       ├── audio_endpoints.h / .cpp        # EXISTING — reused unchanged (enumeration, observer)
│       └── endpoint_volume.h / .cpp        # NEW — IAudioEndpointVolume mute/restore wrapper (FR-018)
├── audio-engine/
│   ├── routing/
│   │   ├── wasapi_capture.h / .cpp         # EXTEND — add loopback-mode open() path (FR-001)
│   │   ├── wasapi_output.h / .cpp          # IMPLEMENT — currently a stub; real IAudioClient render path
│   │   ├── device_watchdog.h / .cpp        # EXTEND — wire OnDeviceRemoved/OnDefaultDeviceChanged
│   │   │                                   #   into engine recovery instead of no-op (FR-008–FR-010)
│   │   ├── same_device_guard.h / .cpp      # NEW — conflict detection, hard block (FR-005, FR-014)
│   │   └── audio_engine_impl.h / .cpp      # EXTEND — wire loopback source + real output + mute + guard
│   └── include/jyglobalvst/
│       └── audio_engine.h                  # EXTEND — IAudioEngine/IAudioEngineListener surface
│                                            #   additions (see contracts/audio-engine-interface.md)
└── tray-app/
    └── settings/
        └── roaming_settings.h / .cpp       # EXTEND — persist capture/output endpoint IDs (FR-016)

tests/
├── unit/
│   ├── wasapi_capture_test.cpp             # EXTEND (if present) or NEW — loopback-mode assertions
│   ├── same_device_guard_test.cpp          # NEW
│   └── endpoint_volume_test.cpp            # NEW — mute/restore round-trip (mockable interface)
└── integration/
    ├── us1_loopback_to_output_test.cpp     # NEW — full pipeline, two-device happy path
    ├── us2_same_device_hard_block_test.cpp # NEW
    ├── us3_device_hotplug_recovery_test.cpp # NEW — extends loopback_fixture.h
    └── loopback_fixture.h                  # EXISTING — reused test harness
```

**Structure Decision**: Single existing project (Option 1). This feature is additive/extending
work inside the current `audio-engine` and `shared` static libs and the `tray-app` settings layer;
no new build target or top-level directory is introduced. The already-referenced-but-stub
`wasapi_output.cpp` file (`src/audio-engine/routing/wasapi_output.cpp`) is the primary net-new
implementation surface; everything else is extension of existing, tested modules.

## Complexity Tracking

> Filled because the Constitution Check above recorded two deviations.

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|---------------------------------------|
| Principle II: ≤10ms round-trip latency target not gated for this feature (AUDIO-001 deferred) | The team's own kernel driver (spec 004) — the only path that can realistically hit the constitutional ≤10ms target — is blocked by an external, out-of-project-control constraint (no valid Microsoft driver-signing certificate). Shipping nothing until that is resolved leaves the user without any working capture path at all (the third-party virtual cable is the only current fallback). The user explicitly chose (spec Q2) to measure and document the real figure and revisit the constitutional gate decision at the end of implementation rather than block on it now. | Waiting for the driver to be signable has no defined timeline and is not a decision this feature can force; re-scoping to "measure, document, decide later" was the user's explicit, informed choice, not an oversight. |
| Principle VI: driver must precede VST host integration | The driver cannot currently be completed to a signable, distributable state (external cert blocker), so hard-locking VST host work behind it would stall the project indefinitely. This feature provides a functional, non-driver capture path while the driver (spec 004) remains in the tree behind its build gate for a future signed-distribution path — it does not delete or abandon the driver work. | Continuing to wait on the driver, or attempting to self-sign for production distribution, are both outside this feature's control and do not solve the immediate need for a working capture path during development and for non-driver distribution. |

**Action item for governance**: Per the Constitution's Governance section, amendments require
documented rationale, impact analysis, and a migration plan. The justifications above satisfy that
bar for *this feature's scope*, but a formal constitution amendment (or an explicit, dated
ratification of these two deviations) is recommended before this feature is considered done, so the
deviation is not just implicit in a plan document. This is flagged as a risk, not resolved here —
consistent with the user's own instruction to defer the latency question.

## Constitution Check — Post-Design Re-evaluation

Re-checked after Phase 1 (data-model.md, contracts/audio-engine-interface.md, quickstart.md):

- No new principle violations were introduced by the Phase 1 design. The two deviations identified
  before Phase 0 (latency gate deferral; driver-before-VST-host build order) are unchanged in kind
  and scope — Phase 1 did not expand them (e.g., the contract additions are two new listener
  callbacks and one new accessor, not a redesign of the audio path).
- Phase 1 design reuses existing, already-validated components (`AudioEndpointEnumerator`,
  `DeviceWatchdog`, `LatencyProfile`, `EndpointId`) rather than introducing new undocumented APIs,
  which keeps risk scoped to the two already-tracked deviations plus the single open empirical
  question (research.md R1) rather than adding new open questions.
- **Gate status unchanged: CONDITIONAL PASS.** Proceeding to `/speckit-tasks` is reasonable; the R1
  spike (research.md) should be scheduled as an early task since several downstream tasks (FR-018
  implementation shape) depend on its result.
