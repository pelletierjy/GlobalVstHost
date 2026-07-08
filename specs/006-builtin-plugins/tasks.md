---

description: "Task list for Built-In Audio Effect Plugins (Night-time & EQ + Bass Boost)"
---

# Tasks: Built-In Audio Effect Plugins (Night-time & EQ + Bass Boost)

**Input**: Design documents from `specs/006-builtin-plugins/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/builtin-effects-contract.md, quickstart.md

**Tests**: INCLUDED. The project constitution (Testing Discipline: unit tests for DSP, integration tests, latency validation) and `contracts/builtin-effects-contract.md` §4 require them, so test tasks are generated for each story.

**Paths**: Single-project layout. Engine code under `src/audio-engine/`, UI under `src/tray-app/`, tests under `tests/`. All paths relative to the repo project root `GlobalVSTHost/`.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on incomplete tasks)
- **[Story]**: US1 / US2 / US3 (setup, foundational, validation, polish carry no story label)

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create the new source area and wire the build.

- [X] T001 Create directory `src/audio-engine/builtin-effects/` and add `"${CMAKE_CURRENT_SOURCE_DIR}/builtin-effects/*.cpp"` to the `_engine_sources` glob in `src/audio-engine/CMakeLists.txt`
- [X] T002 Add `juce::juce_gui_basics` and `juce::juce_graphics` to the `jyglobalvst_audio_engine` link targets in `src/audio-engine/CMakeLists.txt` (required so built-in `createEditor()` can build a real editor; see plan.md architectural decision)

**Checkpoint**: `cmake -B build -A x64` reconfigures cleanly with the new folder and modules.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Reserved identities, the built-in registry, and the engine resolve/catalog wiring that BOTH effects depend on.

**⚠️ CRITICAL**: No user story can be exercised end-to-end until this phase completes.

- [X] T003 [P] Create `src/audio-engine/builtin-effects/builtin_ids.h` with the reserved 16-byte `PluginUid` ASCII-seed constants for Night-time and EQ, and the `ParamId` maps (data-model.md §1/§3/§4); include a `static_assert`/runtime assert that seeds are 16 bytes and the two UIDs differ. Include the REALTIME CONSTRAINTS header block (constitution §V)
- [X] T004 Create `src/audio-engine/builtin-effects/builtin_effect_registry.h/.cpp` implementing `BuiltinEffectRegistry` (`entries()`, `isBuiltin()`, `findByRef()` UID-then-(vendor,name), `create()` UID dispatch) per `contracts/builtin-effects-contract.md` §1. Table starts empty of processors; each story registers its factory
- [X] T005 Wire the registry into `src/audio-engine/routing/audio_engine_impl.cpp`: add a `BuiltinEffectRegistry` member; make `catalog()` return `registry.entries()` followed by `scan_cache_->plugins()`; make `addPlugin()` construct via `registry.create(ref)` when `registry.isBuiltin(ref)` before falling back to the scan-cache/loader path (contract §2)
- [X] T006 Wire registry precedence into the preset/autosave resolve callback in `src/audio-engine/routing/audio_engine_impl.cpp` (both `loadPreset` and `restoreChain` paths) so built-in refs resolve via `registry.create()` before the scan cache, and never become placeholders (data-model.md §6)

**✓ Checkpoint**: Registry + engine seams exist; a registered built-in would appear in `catalog()` and be addable. (No effects registered yet.)

**Checkpoint**: Registry + engine seams exist; a registered built-in would appear in `catalog()` and be addable. (No effects registered yet.)

---

## Phase 3: User Story 1 - Night-time leveler for late-night listening (Priority: P1) 🎯 MVP

**Goal**: A "Night-time" built-in that raises quiet dialogue and pulls down loud passages toward a target loudness with a hard limiter ceiling, selectable via Light/Medium/Strong presets, with a configurable limiter look-ahead in its edit view.

**Independent Test**: With an empty scan cache, "Night-time" appears in the plugin list, adds to the chain, and on quiet/loud alternating content reduces the loud↔quiet loudness gap ≥ 50% vs bypass while output never exceeds the ceiling (quickstart Scenarios 1–3).

### Tests for User Story 1 ⚠️ (write first, ensure they fail)

- [X] T007 [P] [US1] Unit test the loudness meter (BS.1770 K-weighting response + short-term loudness estimate) in `tests/unit/loudness_meter_test.cpp`
- [X] T008 [P] [US1] Unit test `NightTimeProcessor` in `tests/unit/nighttime_processor_test.cpp`: loud/quiet gap reduced ≥ 50% (SC-002), output peak ≤ ceiling on full-scale input (FR-008), no upward gain on near-silence (floor/gate, FR-011), L/R gain linked (FR-010), `getLatencySamples()` matches look-ahead (FR-008a)
- [X] T009 [P] [US1] Integration test in `tests/integration/builtin_nighttime_add_test.cpp`: with no scan cache, `catalog()` lists Night-time and `addPlugin(nightTimeRef, 0)` returns a non-null `InstanceId` (never a placeholder) (FR-001/002)

### Implementation for User Story 1

- [X] T010 [US1] Implement `src/audio-engine/builtin-effects/loudness_meter.h/.cpp` — K-weighting biquads + gated short-term mean-square loudness estimate; all state preallocated in a prepare step; RT-safe (REALTIME header)
- [X] T011 [US1] Implement `src/audio-engine/builtin-effects/nighttime_processor.h/.cpp` — `NightTimeProcessor : juce::AudioPluginInstance`: loudness-driven AGC toward per-preset target + look-ahead peak limiter; params `Preset`(0) and `LookAheadMs`(1); `get/setStateInformation` JSON schema (data-model §3); look-ahead delay line preallocated to max in `prepareToPlay`, coefficients recomputed in place; `getLatencySamples()` from active look-ahead; `createEditor()`/`hasEditor()`=true; REALTIME header
- [X] T012 [US1] Implement `src/audio-engine/builtin-effects/nighttime_editor.h/.cpp` — `AudioProcessorEditor` with Light/Medium/Strong preset selector and a look-ahead control (FR-004a: settings live only in this edit view) — depends on T011
- [X] T013 [US1] Register the Night-time descriptor + factory in `src/audio-engine/builtin-effects/builtin_effect_registry.cpp` (uid/name/vendor from `builtin_ids.h`, factory constructs `NightTimeProcessor`) — depends on T011

**Checkpoint**: MVP — Night-time is discoverable, addable, audibly levels content, limits peaks, and opens its edit view. US1 is independently demoable.

---

## Phase 4: User Story 2 - Simple EQ with Bass Boost (Priority: P2)

**Goal**: An "EQ" built-in with ~10 fixed gain bands (applied equally L/R) plus an adjustable Bass Boost amount and a Flat/Reset action.

**Independent Test**: With an empty scan cache, "EQ" appears in the list, adds to the chain; raising a band boosts that range (others unchanged), Bass Boost adds low end with no distortion, Flat/Reset zeroes everything (quickstart Scenario 4).

### Tests for User Story 2 ⚠️ (write first, ensure they fail)

- [X] T014 [P] [US2] Unit test `EqProcessor` in `tests/unit/eq_processor_test.cpp`: each band boost/cut affects only its range ≥ several dB (SC-003, FR-013), Bass Boost raises low end (FR-014), Flat/Reset returns bands→0 dB and bass→0 (FR-015), combined max boosts do not clip the output (FR-016), latency == 0
- [X] T015 [P] [US2] Integration test in `tests/integration/builtin_eq_add_test.cpp`: with no scan cache, `catalog()` lists BOTH built-ins and `addPlugin(eqRef, pos)` succeeds

### Implementation for User Story 2

- [X] T016 [US2] Implement `src/audio-engine/builtin-effects/eq_processor.h/.cpp` — `EqProcessor : juce::AudioPluginInstance`: 10 fixed-freq peaking biquads (data-model §4 centers) + independent low-shelf Bass Boost + output safety ceiling; params `BandGain[0..9]`(0–9) and `BassBoost`(10); `get/setStateInformation` JSON schema; coefficients recomputed in place into preallocated storage; REALTIME header
- [X] T017 [US2] Implement `src/audio-engine/builtin-effects/eq_editor.h/.cpp` — `AudioProcessorEditor` with 10 band gain sliders + Bass Boost amount + Flat/Reset button (writes params through the normal path) — depends on T016
- [X] T018 [US2] Register the EQ descriptor + factory in `src/audio-engine/builtin-effects/builtin_effect_registry.cpp` — depends on T016

**Checkpoint**: Both effects are independently usable and appear together in the catalog.

---

## Phase 5: User Story 3 - Built-ins behave like any other plugin (Priority: P3)

**Goal**: Reorder, bypass, duplicate, and persist the built-ins exactly like scanned plugins — presets and autosave round-trip their settings — and mark them as built-in in the UI (FR-003).

**Independent Test**: Build a chain with both built-ins (+ a scanned plugin), reorder/bypass, save a preset, restart, load — both restored in order with identical settings; autosave restores them on relaunch without an explicit load (quickstart Scenario 5).

### Tests for User Story 3 ⚠️ (write first, ensure they fail)

- [X] T019 [P] [US3] Unit test `BuiltinEffectRegistry` in `tests/unit/builtin_registry_test.cpp`: golden reserved-UID hex stability, `isBuiltin`/`findByRef` precedence, `entries()` present with an empty scan cache, UIDs distinct (contract §4)
- [X] T020 [P] [US3] Integration test in `tests/integration/builtin_effects_persistence_test.cpp`: save preset → new engine → load re-resolves both built-ins with identical parameter state; reorder + bypass behave like scanned slots; autosave write→restore round-trips both (FR-004/005, SC-004)

### Implementation for User Story 3

- [X] T021 [US3] Verify/complete built-in resolution on the autosave restore path in `src/tray-app/presets/autosave.cpp` (and confirm `restoreChain` in `src/audio-engine/routing/audio_engine_impl.cpp` uses the registry seam from T006); fix any gap so autosaved built-in slots resolve without placeholders
- [X] T022 [US3] Add a "Built-in" badge/label (shown when `vendor == "JyGlobalVST"` and `file_path` empty) in `src/tray-app/ui/catalog_dialog.cpp` and `src/tray-app/ui/chain_editor.cpp` (FR-003)

**Checkpoint**: Built-ins are first-class chain citizens with full preset/autosave round-trip and clear identification.

---

## Phase 6: Audio Validation *(required — constitution II/V, spec AUDIO-001..004)*

**⚠️ CRITICAL**: Must pass before this feature is considered production-ready.

- [X] T023 [AUDIO/LATENCY] Measure round-trip latency with both built-ins enabled and Night-time `LookAheadMs = 0`; verify ≤ 10 ms; document method/environment (AUDIO-001)
- [X] T024 [AUDIO/LATENCY] Verify raising Night-time look-ahead increases `LatencyProfile.plugin_chain_ms` by exactly the selected amount and is displayed to the user (FR-008a/AUDIO-001)
- [X] T025 [AUDIO/LATENCY] Profile CPU with both effects enabled during normal playback; verify ≤ 5%; document system specs/sample rate/buffer size (AUDIO-002)
- [X] T026 [AUDIO/LATENCY] Run a 30-minute soak with both effects enabled while adjusting parameters; verify zero dropouts/drift (AUDIO-003)
- [X] T027 [AUDIO] RT-safety audit of `NightTimeProcessor::processBlock` and `EqProcessor::processBlock` (no malloc/new, lock, file I/O, or logging) via the `tests/audit/` T106/T107-style checks + review (FR-018)
- [X] T028 [AUDIO] Verify both effects at 44.1 / 48 / 96 kHz — EQ band centers and Night-time timing consistent across rates (AUDIO-004)

**Checkpoint**: Performance and real-time gates green.

---

## Phase 7: Polish & Cross-Cutting Concerns

- [X] T029 [P] Update docs: note the two built-in effects and how to use them in `docs/` and any in-app help/README
- [X] T030 [P] Run clang-format over `src/audio-engine/builtin-effects/*` and confirm include-order rules
- [X] T031 Execute all `quickstart.md` validation scenarios (1–6) end-to-end and record results
- [X] T032 Final review: SEH failure containment for built-in slots behaves like scanned plugins (FR-020); confirm no settings surface in the main window (FR-004a)

---

## Dependencies & Execution Order

### Phase dependencies

- **Setup (P1)** → no deps.
- **Foundational (P2)** → depends on Setup. **Blocks all user stories.** (T004 depends on T003; T005 depends on T004; T006 depends on T005.)
- **US1 (P3)**, **US2 (P4)**, **US3 (P5)** → all depend on Foundational. US1 is the MVP.
- **Audio Validation (P6)** → depends on the stories being validated (realistically after US1+US2; full run after US3).
- **Polish (P7)** → after desired stories complete.

### Cross-story notes

- US1 and US2 are independent **except** both register into `builtin_effect_registry.cpp` (T013, T018) — sequential edits to that one file, not parallel.
- US3 relies on the Foundational resolve seam (T006); its tests exercise both effects, so run US3 after US1 (and US2 for the "both restored" assertions).

### Within a story

- Tests (T007–T009 / T014–T015 / T019–T020) written first and failing.
- Night-time: meter (T010) → processor (T011) → editor (T012) + registration (T013).
- EQ: processor (T016) → editor (T017) + registration (T018).

---

## Parallel Opportunities

- **Foundational**: T003 [P] (new file) while planning T004; T005/T006 are sequential (same file).
- **US1 tests**: T007, T008, T009 in parallel (different files).
- **US2 tests**: T014, T015 in parallel.
- **US3 tests**: T019, T020 in parallel.
- **Polish**: T029, T030 in parallel.

```text
# US1 tests together:
Task: "Unit test loudness meter in tests/unit/loudness_meter_test.cpp"
Task: "Unit test NightTimeProcessor in tests/unit/nighttime_processor_test.cpp"
Task: "Integration test Night-time add in tests/integration/builtin_nighttime_add_test.cpp"
```

---

## Implementation Strategy

### MVP first (User Story 1 only)

1. Phase 1 Setup → Phase 2 Foundational (critical, blocks stories).
2. Phase 3 US1 (Night-time).
3. **STOP & VALIDATE**: quickstart Scenarios 1–3 + latency at look-ahead 0.
4. Demo the late-night leveler — this alone delivers the headline value.

### Incremental delivery

- Foundation → US1 (MVP: Night-time) → US2 (EQ) → US3 (persistence + badge) → Audio Validation → Polish. Each story is independently testable and adds value without breaking the previous.

---

## Notes

- `[P]` = different files, no incomplete-task dependency.
- Test files auto-register via the globbed `tests/CMakeLists.txt` (`CONFIGURE_DEPENDS`) — no CMake edit needed for tests.
- Every new audio `.cpp/.h` carries the REALTIME CONSTRAINTS header (constitution §V).
- No public `IAudioEngine`/IPC contract changes — all integration is through the engine-internal registry seam.
- Commit after each task or logical group.
