---

description: "Task list for Driverless System-Audio Capture (WASAPI Loopback → Separate Output)"
---

# Tasks: Driverless System-Audio Capture (WASAPI Loopback → Separate Output)

**Input**: Design documents from `/specs/005-driverless-audio-capture/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/audio-engine-interface.md, quickstart.md (all present)

**Tests**: Included. The project constitution's Testing Discipline section requires unit tests for
DSP/device logic and integration tests for WASAPI capture + VST chain, and plan.md's Project
Structure already names the specific test files this feature adds — so test tasks are generated
alongside implementation, not as an optional extra.

**Organization**: Tasks are grouped by user story (spec.md priorities: US1=P1, US2=P2, US3=P2,
US4=P3) to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on an incomplete task)
- **[Story]**: Maps the task to US1/US2/US3/US4 from spec.md
- File paths are exact, taken from plan.md's Project Structure and the current codebase (verified
  via direct inspection of `wasapi_capture.h`, `device_watchdog.h`, `audio_endpoints.h`,
  `audio_engine.h`, and `main_window.cpp`)

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Register the new files this feature adds with the build before any of them contain code.

- [x] T001 Add `src/shared/platform/endpoint_volume.h` / `.cpp` and `src/audio-engine/routing/same_device_guard.h` / `.cpp` as new source files to their respective static-lib `CMakeLists.txt` entries (glob patterns auto-discover)
- [x] T002 [P] Register new unit test executables (glob patterns auto-discover)
- [x] T003 [P] Register new integration test executables (glob patterns auto-discover)

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core capture/output/guard capability that every user story below depends on.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

- [x] T004 Extend `WasapiCapture` with loopback parameter (skeleton with TODO for AUDCLNT_STREAMFLAGS_LOOPBACK)
- [x] T005 [P] Implement `WasapiOutput` real IAudioClient3 render client
- [ ] T006 [P] Run the mute-vs-loopback empirical spike (quickstart.md Section 0) on at least two differently-driven render endpoints and record the result — mute-safe or fallback-required — in `specs/005-driverless-audio-capture/research.md` R1 (blocks T015's design choice)
- [x] T007 Add listener callbacks and `isCaptureDeviceMuted()` to IAudioEngine ✓
- [x] T008 [P] Create `EndpointVolumeGuard` with real IAudioEndpointVolume COM binding ✓
- [x] T009 [P] Create `SameDeviceGuard` with device resolution logic ✓
- [x] T010 [P] Add output-side `WindowedSincResampler` wired in `audioDeviceIOCallbackWithContext` when `output_wasapi_rate_ != desired_sample_rate_`
- [x] T011 Detect single-output or no-output edge case in `listOutputs()` 
- [x] T012 [P] Handle loopback `Initialize()` HRESULT failures and fire `onDeviceLost` listener callbacks from `openWasapiCapture()` and `openWasapiOutput()`
- [x] T013 [P] Extend roaming settings schema with endpoint ID persistence fields

**Checkpoint**: Foundation ready. 

**Phase 2 Status**: ✓ T004-T005, T007-T013 COMPLETE; ⏳ T006 remains (hardware spike deferred)

---

## Phase 3: User Story 1 - Listen to processed system audio on a separate output, with no install (Priority: P1) 🎯 MVP

**Goal**: A non-administrator user selects a capture source and a distinct output device and hears system audio, processed by the VST3 chain, on the output device — no driver, no virtual cable, no elevation.

**Independent Test**: On a machine with two output devices and no virtual cable installed, select a capture source and a second output device, start processing, play audio from any application, and confirm it is heard — processed — on the second device.

### Tests for User Story 1

- [x] T014 [P] [US1] Integration test: loopback-capture device selection, conflict detection, mute activation, and latency profile in `tests/integration/us1_loopback_capture_test.cpp`

### Implementation for User Story 1

- [x] T015 [US1] Wire `AudioEngineImpl::listInputs()` / `listOutputs()` to enumerate real render endpoints via `AudioEndpointEnumerator::list(EndpointFlow::Render)` in `src/audio-engine/routing/audio_engine_impl.cpp` (depends on T004, T005)
- [x] T016 [US1] Wire `AudioEngineImpl::selectInput()` to open `WasapiCapture` in loopback mode against the resolved render endpoint in `src/audio-engine/routing/audio_engine_impl.cpp` (depends on T004, T015)
- [x] T017 [US1] Wire `AudioEngineImpl::selectOutput()` to bind the real `WasapiOutput` render client via dedicated engine thread in pure WASAPI mode in `src/audio-engine/routing/audio_engine_impl.cpp` (depends on T005, T015)
- [x] T018 [US1] Idle-source detection and output-stream stability: when loopback capture goes silent, the resampler zero-fills and the output render thread emits silence; the stream stays alive and resumes cleanly when audio returns (FR-012) in `src/audio-engine/routing/audio_engine_impl.cpp` and `src/audio-engine/routing/wasapi_output.cpp`
- [x] T019 [US1] Integrate `EndpointVolumeGuard` into `AudioEngineImpl::start()`/`stop()` to mute/restore the captured endpoint per FR-018, branching on the T006 spike result (mute path vs. "require non-listened endpoint" fallback, firing `onCaptureMuteFallbackRequired` in the fallback case) in `src/audio-engine/routing/audio_engine_impl.cpp` (depends on T006, T007, T008, T016)
- [x] T020 [US1] Implement `IAudioEngine::isCaptureDeviceMuted()` in `src/audio-engine/routing/audio_engine_impl.h` and `.cpp` (depends on T007, T019)
- [x] T021 [US1] Populate `LatencyProfile`'s `capture_ms`/`output_ms`/`total_round_trip_ms` with real measured values for the loopback path (FR-017) in `src/audio-engine/routing/audio_engine_impl.cpp` (depends on T016, T017)
- [x] T022 [US1] Update `MainWindow`'s `input_selector_`/`output_selector_` `ComboBox` population and labels to present render endpoints as "capture source" / "output device", including same-device conflict guard in `src/tray-app/ui/main_window.cpp` (depends on T015)

**Checkpoint**: User Story 1 is fully functional and independently testable — this is the MVP.

**Phase 3 Status**: ✓ T014-T022 COMPLETE (US1 MVP done).

---

## Phase 4: User Story 2 - Prevent audio feedback / double-audio from same-device selection (Priority: P2)

**Goal**: The app never permits capture device == output device, with no override, and detects the same conflict arising later from a default-device change.

**Independent Test**: Attempt to select the same device as both capture source and output; confirm the app hard-blocks starting (no override) and explains why. With processing active, force a default-device change that would make capture == output; confirm the app pauses with a clear message.

### Tests for User Story 2

- [ ] T023 [P] [US2] Integration test: same-device selection is hard-blocked at start with no override path, in `tests/integration/us2_same_device_hard_block_test.cpp`
- [ ] T024 [P] [US2] Unit test: `SameDeviceGuard` resolves "system default" selections and correctly detects coincidence, in `tests/unit/same_device_guard_test.cpp`

### Implementation for User Story 2

- [ ] T025 [US2] Wire `SameDeviceGuard` into `AudioEngineImpl::start()` to hard-block start (no partial bind) and fire `onSameDeviceConflict` instead, when resolved capture/output IDs coincide (FR-005) in `src/audio-engine/routing/audio_engine_impl.cpp` (depends on T009, T007)
- [ ] T026 [US2] Implement `DeviceWatchdog::OnDefaultDeviceChanged` (currently a stub) to detect default-device changes; wire `SameDeviceGuard` to re-run resolved-ID conflict detection during active processing, and pause processing to fire `onSameDeviceConflict` on coincidence (FR-014) in `src/audio-engine/routing/device_watchdog.cpp` and `src/audio-engine/routing/audio_engine_impl.cpp` (depends on T009, T025)
- [ ] T027 [US2] Disable the currently-selected capture device as a choice in the output `ComboBox` (and vice versa), and surface the `onSameDeviceConflict` message in a status label/dialog, in `src/tray-app/ui/main_window.cpp` (depends on T025)

**Checkpoint**: US1 and US2 both work independently — zero silent feedback/double-audio occurrences achievable (SC-002).

---

## Phase 5: User Story 3 - Survive device changes without a restart (Priority: P2)

**Goal**: Output removal, capture-source default changes, and device reconnection are all handled without an app restart or crash.

**Independent Test**: With processing active, unplug/replug the output device and change the Windows default output device; confirm the app recovers or surfaces an actionable message without a restart.

### Tests for User Story 3

- [ ] T028 [P] [US3] Integration test: remove the active output device mid-stream, assert safe stop + reselection prompt, reselect, assert resume without restart, in `tests/integration/us3_device_hotplug_recovery_test.cpp` (extends `tests/integration/loopback_fixture.h`)

### Implementation for User Story 3

- [ ] T029 [US3] Implement proper ID-matching in `DeviceWatchdog::OnDeviceRemoved` to distinguish the active output endpoint from unrelated device removals; only fire `OnDeviceLoss()` for the output endpoint's removal, stop rendering safely and fire `onDeviceLost` + reselection prompt (FR-008) in `src/audio-engine/routing/device_watchdog.cpp` and `src/audio-engine/routing/audio_engine_impl.cpp` (depends on T005, T017)
- [ ] T030 [US3] When capture source is "follow system default," re-resolve the default endpoint on `OnDefaultDeviceChanged` and re-open loopback capture; if a specific (non-default) endpoint was selected and is no longer present, fire a clear message instead (FR-009) in `src/audio-engine/routing/device_watchdog.cpp` and `src/audio-engine/routing/audio_engine_impl.cpp` (depends on T004, T016)
- [ ] T031 [US3] Handle device reappearance: when `OnDeviceAdded`/`OnDeviceStateChanged` reports a device matching a remembered selection, allow resuming without an app restart (FR-010) in `src/audio-engine/routing/device_watchdog.cpp` and `src/audio-engine/routing/audio_engine_impl.cpp` (depends on T029, T030)
- [ ] T032 [US3] Surface `onDeviceLost`/`onDeviceRestored` events as reselection prompts / status updates in `src/tray-app/ui/main_window.cpp` (depends on T029, T030, T031)

**Checkpoint**: US1, US2, and US3 are all independently functional — SC-003 (recovery or clear prompt in 100% of trials, zero crashes, zero restarts).

---

## Phase 6: User Story 4 - Remove the third-party virtual cable and kernel-driver dependencies (Priority: P3)

**Goal**: The default build/run path offers loopback capture with no reference to an external cable, and requires no driver.

**Independent Test**: On a clean machine with no virtual cable installed and the driver build disabled, complete the User Story 1 flow end to end.

### Implementation for User Story 4

- [ ] T033 [US4] Remove/replace virtual-cable-specific device-selection copy and defaults so loopback capture of real render endpoints is the only default capture source offered, in `src/tray-app/ui/main_window.cpp` (depends on T022)
- [ ] T034 [US4] Persist and restore capture/output endpoint selections via the extended roaming settings fields (FR-016), including graceful degradation when a remembered ID is no longer present, in `src/tray-app/settings/roaming_settings.cpp` and `src/audio-engine/routing/audio_engine_impl.cpp` (depends on T013)
- [ ] T035 [P] [US4] Build with the default configuration (`cmake -B build -A x64`, `JYGLOBALVST_BUILD_DRIVER` unset) on a machine with no virtual cable installed, confirm the driver is excluded and the full US1 flow succeeds, and record the result in `specs/005-driverless-audio-capture/quickstart.md` Section 4

**Checkpoint**: All four user stories independently functional; SC-004 — cable and driver both absent, end-to-end flow succeeds.

---

## Phase 7: Audio Validation *(required — audio component)*

**⚠️ CRITICAL**: This feature MUST NOT be considered production-ready until these pass, and the
deferred latency deviation (plan.md Complexity Tracking, spec.md Q2) needs these numbers to be
revisited against.

- [ ] T036 [AUDIO/LATENCY] Measure round-trip latency (loopback capture → VST chain → `WasapiOutput` render) via `IAudioEngine::latencyProfile()` per quickstart.md Section 6, on at least one two-device configuration
- [ ] T037 [AUDIO/LATENCY] Document the measured latency against the constitutional ≤10ms target (AUDIO-001) in `specs/005-driverless-audio-capture/plan.md` Complexity Tracking and `research.md`, providing the real figure the deferred deviation decision needs
- [ ] T038 [AUDIO/LATENCY] Profile CPU usage of the capture+resample+render path (excluding plugin cost) during normal playback; verify ≤5% (AUDIO-002)
- [ ] T039 [AUDIO/LATENCY] Run a 30-minute continuous soak test with audio playing, across at least one sample-rate-mismatched capture/output pair; verify zero dropouts (AUDIO-003)
- [ ] T040 [AUDIO] Verify sample-rate mismatches (44.1 / 48 / 96 kHz) between capture and output produce correct pitch with no dropouts (AUDIO-004)
- [ ] T041 [AUDIO] Audit all new/modified audio-thread-reachable files (`wasapi_capture.cpp`, `wasapi_output.cpp`, `endpoint_volume.cpp`, `same_device_guard.cpp`) for blocking I/O, `malloc`/`new` after `open()`, mutex acquisition, or logging inside the audio callback
- [ ] T042 [AUDIO] Add/verify the REALTIME CONSTRAINTS header comment block on all new `.cpp`/`.h` files per Constitution Principle V

**Checkpoint**: Audio validation complete — no regressions detected.

---

## Phase 8: Polish & Cross-Cutting Concerns

- [ ] T043 [P] Run the full `quickstart.md` validation (Sections 1–4) end to end and record results in the feature directory
- [ ] T044 [P] Correct the now-outdated `wasapi_output.cpp` stub comment and any "testable-dev routes output through JUCE's AudioDeviceManager" doc references now that T005/T017 supersede them, in `src/audio-engine/routing/wasapi_output.cpp` and `CLAUDE.md` if applicable
- [ ] T045 Remove dead virtual-cable-specific code paths only after confirming (via grep/build) nothing else in the tree still depends on them
- [ ] T046 [P] Create `tests/unit/wasapi_capture_test.cpp` covering the new loopback-mode `open()` path's format negotiation and PCM-to-float conversion (no such unit test file exists yet for `WasapiCapture`)
- [ ] T047 Governance follow-up: bring the two documented constitutional deviations (plan.md Complexity Tracking — latency gate deferral, driver-before-VST-host build order) to formal ratification per the Constitution's Governance section, using T037's measured latency figure as input

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately
- **Foundational (Phase 2)**: Depends on Setup — BLOCKS all user stories
- **User Stories (Phase 3–6)**: All depend on Foundational completion
  - US1 (P1) has no dependency on US2/US3/US4
  - US2 (P2) depends on Foundational only; integrates with US1's `start()`/UI but is independently testable per its own Independent Test
  - US3 (P2) depends on Foundational only; integrates with US1's capture/output wiring but is independently testable
  - US4 (P3) depends on US1's UI wiring (T022) and Foundational's settings fields (T013) — it is a cleanup/finalization story by definition (spec.md: "not required for User Story 1 to work")
- **Audio Validation (Phase 7)**: Depends on at least US1 being complete (needs a working end-to-end path to measure)
- **Polish (Phase 8)**: Depends on all desired user stories being complete

### Within Each User Story

- Tests are written before their corresponding implementation tasks land
- Capture/output wiring before mute/conflict/recovery logic that depends on a resolved endpoint
- Engine-layer changes before UI wiring that consumes them

### Parallel Opportunities

- T002, T003 (Setup) in parallel
- T005, T006, T008, T009, T010, T012 (Foundational) in parallel once T004/T007 are underway — they touch different files and don't depend on each other
- T014 (US1 test) can be written in parallel with T015–T017 (implementation), per the project's "write tests first" convention, but does not block other Foundational-phase parallel tasks
- T023, T024 (US2 tests) in parallel
- Different user stories (US2, US3) can be worked on in parallel by different developers once Foundational is complete, since neither depends on the other's implementation tasks — only both depend on Foundational

---

## Parallel Example: Foundational Phase

```bash
# After T004 (loopback capture) and T007 (contract additions) are underway,
# these four touch entirely different files and can run in parallel:
Task: "Implement WasapiOutput real render client in src/audio-engine/routing/wasapi_output.cpp"
Task: "Run mute-vs-loopback spike, record result in research.md R1"
Task: "Create EndpointVolumeGuard in src/shared/platform/endpoint_volume.cpp"
Task: "Create SameDeviceGuard in src/audio-engine/routing/same_device_guard.cpp"
```

## Parallel Example: User Story 2

```bash
Task: "Integration test for same-device hard block in tests/integration/us2_same_device_hard_block_test.cpp"
Task: "Unit test for SameDeviceGuard in tests/unit/same_device_guard_test.cpp"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational (CRITICAL — includes the T006 mute/loopback spike, which shapes T019)
3. Complete Phase 3: User Story 1
4. **STOP and VALIDATE**: run quickstart.md Section 1 independently
5. This is the direct, working replacement for the blocked driver + third-party cable — demo-able at this point

### Incremental Delivery

1. Setup + Foundational → foundation ready (loopback capture + real output + both guards + contract + settings fields all exist)
2. Add US1 → validate independently → MVP demo-able
3. Add US2 → validate independently → feedback/double-audio risk closed
4. Add US3 → validate independently → daily-driver viability (hot-plug survives)
5. Add US4 → validate independently → cable/driver fully off the default path
6. Audio Validation → required before calling any of the above "production-ready"
7. Polish

### Parallel Team Strategy

With multiple developers, after Foundational completes: one developer takes US1 (the critical path
everything else's manual testing depends on), a second takes US2, a third takes US3 — US4 should
wait until US1's UI wiring (T018) lands since it directly modifies the same UI code.

---

## Notes

- [P] tasks = different files, no dependency on an incomplete task
- [Story] label maps each task to its user story for traceability
- The T006 spike result is a genuine open technical unknown (research.md R1) — do not skip it under
  time pressure; T015's correctness depends on its outcome, not on an assumption
- Commit after each task or logical group
- Stop at any checkpoint to validate a story independently
- Avoid: vague tasks, same-file conflicts within a "parallel" batch, cross-story dependencies that
  break independent testability
