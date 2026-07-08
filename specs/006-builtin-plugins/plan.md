# Implementation Plan: Built-In Audio Effect Plugins (Night-time & EQ + Bass Boost)

**Branch**: `006-builtin-plugins` | **Date**: 2026-07-01 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/006-builtin-plugins/spec.md`

## Summary

Add two effects that ship inside the application and appear in the plugin list next to scanned VST3 plugins:

1. **Night-time** — a stereo broadcast-style loudness normalizer (drives program loudness toward a target level, raising quiet dialogue and pulling down loud passages) followed by a peak limiter. Driven by Light/Medium/Strong presets, with a user-configurable limiter look-ahead (zero-latency option available).
2. **EQ** — a simple ~10-band fixed-frequency graphic EQ (gain-only bands) plus an adjustable Bass Boost amount.

**Technical approach**: Both effects are implemented as `juce::AudioPluginInstance` subclasses built on `juce_dsp`, so they drop into the existing `PluginInstance`/`PluginChain` slot machinery with **no changes to the chain, preset, or SPSC-command paths**. A new engine-side `BuiltinEffectRegistry` supplies their catalog entries (reserved synthetic `PluginUid`s, empty `file_path`) and instantiates them. `AudioEngineImpl::catalog()`, `addPlugin()`, and the preset/autosave resolve callback consult the registry **before** the disk scan cache. Each effect provides its own `createEditor()`, so the existing `openEditor()` → `PluginEditorWindow` path renders their settings in a dedicated edit window (satisfying "settings live in the plugin edit view, not the main window"). Parameters, state chunks (preset/autosave round-trip), bypass, reorder, and SEH failure isolation all reuse the current implementation.

## Technical Context

**Language/Version**: C++20 (MSVC, `/WX` on audio-thread-reachable targets)

**Primary Dependencies**: JUCE 8.0.4 (`juce_dsp`, `juce_audio_processors`, `juce_audio_basics`, and — new for built-in editors — `juce_gui_basics`/`juce_graphics`); nlohmann/json (preset/state serialization); GoogleTest.

**Storage**: No new stores. Built-in effect settings serialize into the existing `.jvst` preset files (`%UserProfile%\Documents\JyGlobalVST\Presets\`) and the autosave chain (`%LocalAppData%\JyGlobalVST\autosave.json`) as the same per-slot `plugin_uid` + base64 `state_chunk_b64` used for scanned plugins.

**Testing**: GoogleTest offline DSP unit tests (`tests/unit/`); chain/preset integration tests (`tests/integration/`); latency/CPU verification per constitution.

**Target Platform**: Windows 10 1909+ / Windows 11, x64.

**Project Type**: Single desktop application (static libs `jyglobalvst_shared`, `jyglobalvst_audio_engine`; GUI app `jyglobalvst_tray`).

**Performance Goals**: Round-trip latency ≤ 10 ms (with Night-time look-ahead at its zero-latency setting); ≤ 5% CPU with both effects enabled; zero dropouts over a 30-minute soak.

**Constraints**: Absolute audio-thread real-time discipline — no allocation, locking, file I/O, or logging in `processBlock`. Filter/limiter state and coefficient buffers are preallocated in `prepareToPlay`; parameter changes arrive via the existing SPSC command queue and are applied at the top of `processBlock`; biquad coefficients are recomputed **in place** into fixed storage (no JUCE reference-counted coefficient allocation on the audio thread).

**Scale/Scope**: 2 effects; ~13 total parameters; no networking; no new persistence locations; additive change to one engine method group and one UI branch.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|---|---|---|
| I. Spec Hierarchy & Component Isolation | PASS | Fits the existing `vst_host_engine` + `ui_controls` components; no new component boundary. Acceptance criteria tied to latency/CPU are in the spec (AUDIO-001..004). |
| II. Performance (NON-NEGOTIABLE) | PASS (gated) | ≤10 ms / ≤5% / zero-dropout targets carried into quickstart validation. Night-time look-ahead is the only latency-adding control and is user-selectable with a zero-latency default; added latency is displayed (AUDIO-001). |
| III. Task Sequencing | PASS | DSP processors → registry/engine wiring → preset/state → UI editor → integration/latency tests. UI work depends only on the finalized chain/catalog contract, not DSP internals. |
| IV. Naming & Conventions | PASS | `jyglobalvst` namespace, 4-space/120-col, clang-format include order. (Repo uses `NNN-feature` spec dirs; the constitution's `feat/<component>` branch example is not enforced by tooling here — worktree branch is `006-builtin-plugins`.) |
| V. Real-Time Code Documentation & Constraints | PASS | Every new audio `.cpp/.h` carries the REALTIME CONSTRAINTS header block; coefficient/limiter buffers preallocated; parameter changes queued, not applied directly. |
| VI. Dependencies & Build Order | PASS | No driver dependency (FR-006). Engine work precedes UI work. |
| Plugin Compatibility (VST3-only v1) | PASS / N.A. | Built-ins are internal processors, not a new hosted plugin *format*; they do not reopen VST2 scope. |

No violations → Complexity Tracking table intentionally omitted.

### Audio Technical Context

- **Buffer Size**: Whatever the engine negotiates ({128,256,512,1024} WASAPI; {64,...} ASIO). Effects are buffer-size agnostic; all state sized to `samplesPerBlock` × channels in `prepareToPlay`.
- **Sample Rates**: 44.1 / 48 / 96 kHz (and the wider set the engine negotiates). All filter coefficients and loudness/limiter time constants are computed from the current sample rate in `prepareToPlay`.
- **Latency Budget**: EQ = 0 samples. Night-time = `lookAheadSamples` (0 at the zero-latency preset; else the user-selected value, reported via `setLatencySamples`, surfaced in `LatencyProfile.plugin_chain_ms`). Both effects at the zero-latency setting must keep total round-trip ≤ 10 ms.
- **Real-Time Constraints**: No malloc/lock/IO/logging in `processBlock`; look-ahead delay-line resize and coefficient recompute for a new sample rate happen only in `prepareToPlay` (control thread); per-block work is fixed-cost.
- **CPU Target**: ≤ 5% combined during normal playback.

## Project Structure

### Documentation (this feature)

```text
specs/006-builtin-plugins/
├── plan.md              # This file
├── research.md          # Phase 0 — DSP + integration decisions
├── data-model.md        # Phase 1 — entities, UIDs, parameter maps, state schema
├── quickstart.md        # Phase 1 — runnable validation scenarios
├── contracts/
│   └── builtin-effects-contract.md   # Phase 1 — registry + catalog/preset/editor contract
└── tasks.md             # Phase 2 (/speckit-tasks — NOT created here)
```

### Source Code (repository root: `GlobalVSTHost/`)

```text
src/audio-engine/
├── builtin-effects/                     # NEW directory (add to CMake glob)
│   ├── builtin_effect_registry.h/.cpp   # catalog entries + factory by reserved UID
│   ├── builtin_ids.h                    # reserved PluginUid constants + ParamId maps
│   ├── nighttime_processor.h/.cpp       # AudioPluginInstance: loudness normalizer + limiter
│   ├── nighttime_editor.h/.cpp          # AudioProcessorEditor (preset selector + look-ahead)
│   ├── eq_processor.h/.cpp              # AudioPluginInstance: 10-band graphic EQ + bass boost
│   ├── eq_editor.h/.cpp                 # AudioProcessorEditor (band sliders + bass amount + reset)
│   └── loudness_meter.h/.cpp            # BS.1770 K-weighting + gated loudness estimate (RT-safe)
├── routing/audio_engine_impl.cpp        # EDIT: catalog()/addPlugin()/preset-resolve consult registry
├── chain/                               # UNCHANGED (slots already hold AudioPluginInstance)
├── vst-host/                            # UNCHANGED
└── CMakeLists.txt                       # EDIT: add builtin-effects/*.cpp to glob; add juce_gui_basics/graphics

src/tray-app/
└── ui/
    ├── catalog_dialog.*                 # UNCHANGED (built-ins appear automatically once in catalog())
    └── chain_editor.*                   # (only if a "built-in" badge is added — see FR-003)

tests/
├── unit/
│   ├── nighttime_processor_test.cpp     # loudness convergence, limiter ceiling, floor/gate
│   ├── eq_processor_test.cpp            # per-band boost/cut, bass boost, flat/reset, no-overload
│   └── builtin_registry_test.cpp        # catalog injection, findByRef precedence, UID stability
└── integration/
    └── builtin_effects_chain_test.cpp   # add/reorder/bypass, preset round-trip, autosave restore
```

**Structure Decision**: Single-project layout is retained. All DSP + editors live in a new `src/audio-engine/builtin-effects/` folder compiled into `jyglobalvst_audio_engine`. The only engine edit is the resolve/catalog trio in `audio_engine_impl.cpp`. The tray-app requires **no functional change** for discovery/add/edit (the catalog dialog and `openEditor` path are format-agnostic); an optional edit adds a "built-in" badge for FR-003.

**Notable architectural decision** (see research.md): built-in **editors** are created via `AudioProcessor::createEditor()` inside the engine, reusing the existing `openEditor()` → `PluginEditorWindow` flow. This requires adding `juce_gui_basics`/`juce_graphics` to the `jyglobalvst_audio_engine` target. Alternative (tray-app-owned editors) was rejected because it forks the editor lifecycle and would add new contract methods to read parameter values.
