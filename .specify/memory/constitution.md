<!-- Sync Impact Report
     Version: 1.1.0 → 1.1.1 (PATCH: refresh sample-rate ceiling example to match spec.md FR-002)
     Modified Sections: Audio Performance Standards → Buffer & Sample Rate Management
     Templates Requiring Updates: none
     Prior amendments retained:
       1.0.0 → 1.1.0 (MINOR: Plugin Compatibility scope clarified to VST3-only for v1; VST2 roadmapped)
-->

# JyGlobalVST (System Host) Constitution

A system-wide VST audio processor for Windows: virtual WASAPI audio device + JUCE plugin host + real-time audio routing.

## Core Principles

### I. Spec Hierarchy & Component Isolation

All features decompose from the root spec `system-wide-vst-host` into isolated component specs:
- `virtual_device_driver` (WASAPI virtual audio device)
- `vst_host_engine` (VST2/VST3 plugin hosting and chain management)
- `audio_routing` (real-time DSP graph and sample rate negotiation)
- `ui_controls` (plugin chain UI, device selection, meters, presets)
- `installer` (Windows driver installation and registration)

Each component spec MUST include acceptance criteria tied to latency/CPU targets. Audio-specific specs MUST document buffer sizes, sample rates, and real-time constraints explicitly.

**Rationale**: System audio is unforgiving; component boundaries prevent latency bugs from leaking across layers. Clear specs prevent driver-level issues cascading to the UI layer.

### II. Performance Requirements (NON-NEGOTIABLE)

All specs MUST define and validate against these success metrics:
- **Latency**: < 10 ms round-trip (WASAPI capture → VST chain → hardware output)
- **CPU Usage**: < 5% on modern multi-core systems during normal playback
- **Stability**: Zero audio dropouts during 12-hour continuous operation
- **Sample Rate Negotiation**: Graceful handling of 44.1 / 48 / 96 kHz with automatic resampling if required

Performance requirements override convenience. No feature bypasses latency validation. No task closeout without latency/CPU verification.

**Rationale**: System audio is realtime. Jitter and dropouts break the user experience immediately. These are non-negotiable because audio artifacts are user-facing and cannot be patched silently.

### III. Task Sequencing & Dependencies

All tasks ordered by strict dependency chain:

1. **Infrastructure** (WASAPI driver registration, JUCE environment setup)
2. **Core Audio Logic** (virtual device capture, sample rate negotiation)
3. **VST Host** (plugin scanning, loading, parameter mapping, GUI embedding)
4. **Audio Routing** (DSP graph construction, real-time processing)
5. **UI Layer** (device selector, plugin chain view, meters, presets)
6. **Integration Testing** (end-to-end latency validation, plugin compatibility)

Audio-critical tasks (WASAPI setup, VST hosting, real-time routing) marked as high-priority/blocking. Each task specifies affected component, performance impact, and testing approach. No UI task begins before plugin chain architecture is finalized.

**Rationale**: System-level audio has critical path dependencies. Out-of-order work creates rework and latency regressions.

### IV. Naming & Conventions

**Branch Naming**: `feat/<component-name>` in kebab-case
- Example: `feat/virtual-device-driver`, `feat/vst-host-engine`, `feat/audio-routing`

**Commits**: Include affected component and any performance impact
- Example: `feat(audio-routing): implement sample rate negotiation — latency +0.2ms`

**Specs**: `spec__<component>.md` (double underscore)
- Example: `spec__virtual_device_driver.md`, `spec__vst_host_engine.md`

**Tasks**: Descriptive names with performance label if audio-critical
- Example: `[AUDIO/LATENCY] Implement WASAPI capture loop`, `[AUDIO/LATENCY] Embed VST GUI in Qt window`

**Rationale**: Clear naming prevents scope creep and makes dependency graphs readable. Performance labels flag tasks requiring latency review before merge.

### V. Real-Time Code Documentation & Constraints

All C++ modules handling audio MUST document real-time constraints at the top of every `.cpp` and `.h` file:

```
// REALTIME CONSTRAINTS:
// - No blocking I/O (file read/write, network, mutexes)
// - No malloc/new after initialization
// - No logging to disk during audio callback
// - Maximum execution time: 5ms (at 48kHz, 256 samples)
```

JUCE integration points MUST document latency implications:
- Use `AudioProcessor::processBlock()` for DSP, not callbacks
- Disable GUI updates during audio processing
- Queue parameter changes; don't apply directly in audio thread

Driver code MUST include comments on WASAPI sample rate negotiation and exclusive vs. shared mode tradeoffs.

**Rationale**: Real-time systems are brittle. Explicit constraints prevent invisible latency bugs and audio artifacts.

### VI. Dependencies & Build Order

Implementation order is hard-locked:

1. **Virtual device driver** (WASAPI) implementation MUST complete before VST host integration
2. **Audio routing engine** depends on WASAPI and JUCE initialization
3. **VST host** can be prototyped in parallel with driver, but integration waits for routing
4. **UI** depends only on finalized plugin chain architecture, NOT implementation details

Cross-component testing only begins after prior component is latency-validated.

**Rationale**: System-level audio is unforgiving. Loose coupling prevents cascading rework.

## Audio Performance Standards

### Latency Validation

Every feature touching audio MUST include:
- Measured round-trip latency before/after
- CPU profiling on Windows Task Manager + in-process counters
- 30-minute soak test (no dropouts, no drift)

Acceptance: latency ≤ 10ms, CPU ≤ 5%, zero artifacts.

### Buffer & Sample Rate Management

- Minimum buffer size: 128 samples (2.67ms @ 48kHz)
- Maximum supported sample rate: 192kHz (extensible; v1 supports 44.1 / 48 / 96 / 176.4 / 192 kHz per spec FR-002)
- Sample rate mismatch: Automatic resampling or graceful error
- Buffer underruns: Log warning, insert silence (do not skip audio)

### Plugin Compatibility

JyGlobalVST hosts third-party VST plugins. The host MUST gracefully degrade if a plugin crashes or exceeds the CPU budget (see Principle II / FR-023).

**Format scope** — v1: VST3 only. VST2 hosting is deferred to a future release contingent on user demand. This is a deliberate scope reduction from prior versions of this constitution:

- **Rationale**: VST3 is Steinberg's current and supported standard; VST2 has been formally deprecated by Steinberg since 2018 and the SDK is no longer distributed to new licensees. Constraining v1 to VST3 reduces SDK-licensing risk and halves the host-format surface area we must validate against the Audio Validation gates in Phase 7.
- **Impact analysis**: All five components (driver, audio-engine, vst-host, ui-controls, installer) are unaffected at the architecture level. Only the vst-host plugin-format registration narrows. JUCE's plugin-format manager still supports VST2 via a build flag, so re-enabling it later is a configuration change, not a redesign.
- **Migration plan**: A future feature spec ("VST2 host support") will reopen VST2 hosting under its own clarification cycle. Existing presets remain forward-compatible (the preset schema's `plugin_uid` field accommodates VST2 unique IDs identically).

## Development Workflow

### Code Review Gate

All PRs touching audio MUST include:
- Latency measurement (before/after)
- CPU usage report
- Soak test results (minimum 30 minutes)

Review checklist: real-time constraints honored, no blocking ops in audio thread, no malloc inside callback, latency regression < 0.5ms.

### Testing Discipline

- Unit tests for DSP algorithms (offline)
- Integration tests for WASAPI capture + VST chain
- Soak tests (12+ hours) before release
- Plugin compatibility matrix (ARC X, Sonarworks, etc.)

### Commit & PR Standards

- Atomic commits: one logical change per commit
- Performance impact declared in commit message
- Feature branches cleaned up immediately after merge
- Main branch always latency-validated

## Governance

Constitution supersedes all other practices and working documents. Amendments require:
1. Documented rationale (why change, what problem solved)
2. Impact analysis (which components/tasks affected)
3. Migration plan (how to update dependent specs/plans)

All PRs and reviews MUST verify compliance with principles. Complexity must be justified in terms of principles. Runtime development guidance lives in `CLAUDE.md`; this constitution is the law.

**Version**: 1.1.1 | **Ratified**: 2026-06-04 | **Last Amended**: 2026-06-04
