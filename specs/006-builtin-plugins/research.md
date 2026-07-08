# Phase 0 Research: Built-In Audio Effect Plugins

All spec clarifications were resolved in `/speckit-clarify` (see spec.md §Clarifications). The remaining unknowns were integration strategy and DSP algorithm choices. Each is resolved below.

---

## D1. How do built-in effects appear in the plugin list and chain without a `.vst3` file?

**Decision**: Implement each effect as a `juce::AudioPluginInstance` subclass and add an engine-side `BuiltinEffectRegistry` that (a) produces `PluginCatalogEntry` values with reserved synthetic `PluginUid`s and empty `file_path`, and (b) constructs instances by UID. `AudioEngineImpl::catalog()`, `addPlugin()`, and the preset/autosave resolve callback consult the registry **before** the scan cache.

**Rationale**:
- The chain slot (`PluginChain::Slot`) already holds `std::shared_ptr<PluginInstance>`, and `PluginInstance` wraps a `std::unique_ptr<juce::AudioPluginInstance>` (`src/audio-engine/vst-host/plugin_instance.h:45`). `AudioPluginInstance` derives from `AudioProcessor`, so `processBlock`, `getStateInformation`/`setStateInformation`, `createEditor`, and parameter access all work through the base interface the chain, SEH wrapper, preset serializer, and `openEditor` already use.
- `catalog()` currently returns `scan_cache_->plugins()` (`audio_engine_impl.cpp:606`); `addPlugin()` resolves via `scan_cache_->findByRef(ref)` then `plugin_loader_->load(entry->file_path)` (`audio_engine_impl.cpp:621`). Both are single, well-isolated seams — injecting a registry check ahead of them is additive and touches no real-time code.
- Zero new IPC/contract surface: the UI already lists whatever `catalog()` returns and adds whatever `addPlugin(PluginRef,pos)` accepts.

**Alternatives considered**:
- *Add a third `PluginSlotKind::Builtin` with its own instance type* — rejected: forces changes across chain, snapshot, preset serializer, and UI; the `AudioPluginInstance` route needs none.
- *Generalize `PluginInstance` to hold `juce::AudioProcessor`* — rejected: `AudioPluginInstance` is used by the loader and param mapping; widening it is more invasive than subclassing it.
- *Write real bundled `.vst3` files and scan them* — rejected: reintroduces scanning/signing/packaging and the exact friction the "always available, no scan/download" requirement (FR-002) forbids.

---

## D2. Stable identity for preset round-trip

**Decision**: Assign each built-in a fixed, reserved 16-byte `PluginUid` derived from a stable ASCII seed, with `vendor = "JyGlobalVST"`. Registry resolution matches on UID first, falling back to (vendor,name). Exact constants live in data-model.md.

**Rationale**: Presets and autosave persist `plugin_uid` + `vendor` + `name` per slot and resolve on load (`preset_serializer.cpp`). A constant UID guarantees a saved chain re-resolves to the same built-in across app versions and machines, exactly as VST3 TUIDs do. Reserved ASCII-seeded values are human-recognizable in JSON and collision-free against real Steinberg TUIDs in practice.

**Alternatives**: Random UIDs (rejected — not reproducible); name-only matching (rejected — brittle if display names are localized/renamed).

---

## D3. Night-time — how to make loudness consistent (spec: "broadcast-style loudness normalization")

**Decision**: A two-stage processor:
1. **Loudness estimator + auto-gain**: an ITU-R BS.1770 / EBU R128-style K-weighting filter (a high-shelf + high-pass biquad pair) feeding a mean-square/short-term loudness estimate. A gain-control loop moves the applied gain so measured short-term loudness converges toward a target level, with asymmetric time constants (moderately fast to pull down loud content, slower to raise quiet content) and a **floor/gate** below which no upward gain is applied (FR-011).
2. **Peak limiter** with configurable look-ahead guaranteeing the output ceiling (FR-008).

Preset levels **Light / Medium / Strong** (FR-009) map to a table of {target loudness, max upward gain, attack/release constants, ceiling}. Left/right share one gain value (linked) so the stereo image is stable (FR-010).

**Rationale**: `juce_dsp` is available (`src/audio-engine/CMakeLists.txt:20`) and provides IIR filters and envelope utilities. Full integrated-LUFS with the complete gating spec is unnecessary for a listening aid; a K-weighted short-term estimate driving a smoothed AGC delivers the perceived "consistent volume" the use case needs while staying cheap and RT-safe. This is a listening-comfort effect, not a certified loudness-compliance meter — that distinction is stated in data-model.md and quickstart.md.

**Alternatives**:
- *Full EBU R128 integrated-loudness normalization with 400 ms/3 s gating* — deferred: heavier, and integrated (whole-program) loudness reacts too slowly for live TV. Short-term estimate chosen.
- *Simple downward compressor only* — rejected by clarification (does not raise quiet dialogue).

**RT-safety note**: K-weighting + loudness ring buffer + limiter delay line are all sized and allocated in `prepareToPlay`. `processBlock` does fixed-cost arithmetic only.

---

## D4. Night-time — configurable limiter look-ahead vs the ≤10 ms budget

**Decision**: The limiter uses a look-ahead delay line preallocated to the **maximum** look-ahead (target ≈ 10 ms). The user-selected look-ahead (including a **zero/near-zero** option) sets the active delay length within that buffer and is reported via `AudioProcessor::setLatencySamples`, surfacing in `LatencyProfile.plugin_chain_ms`. The look-ahead control lives in the effect's edit view (FR-008a). Changing it re-runs `prepareToPlay`-level setup on the control thread (never in `processBlock`).

**Rationale**: Look-ahead is what lets a limiter attenuate a transient *before* it clips; it inherently adds latency. Making it user-selectable lets latency-sensitive users pick 0 (keeping ≤10 ms round-trip, AUDIO-001) and others pick cleaner limiting. Preallocating at max avoids audio-thread allocation when the setting changes; only the active read offset changes.

**Alternatives**: `juce::dsp::Limiter` (rejected — no look-ahead, fixed behavior); fixed non-configurable look-ahead (rejected by clarification).

---

## D5. EQ — band topology, bass boost, overload guard

**Decision**: ~10 fixed-frequency bands (logarithmically spaced, e.g. 32 / 64 / 125 / 250 / 500 / 1k / 2k / 4k / 8k / 16k Hz), each a peaking biquad with adjustable gain (≈ −12…+12 dB), applied identically to L and R. **Bass Boost** is a separate low-shelf biquad with an adjustable amount, summed independently of the band sliders (FR-014). Overload guard: internal processing headroom plus a final safety ceiling so combined boosts cannot clip the output (FR-016). Flat/reset returns all band gains to 0 dB and bass amount to 0 (FR-015).

**Rationale**: Fixed-frequency peaking bands are the simplest recognizable graphic-EQ model and match the fxsound *conceptual* reference (per-band boost/cut + a linked bass control) without adopting its GPL code. Biquad coefficients are recomputed **in place** into preallocated per-band storage on parameter change (plain float math, no `juce::dsp::IIR::Coefficients` reference-counted allocation), preserving RT-safety.

**Alternatives**: Parametric EQ with adjustable Q/frequency (rejected — out of scope per FR-017); FFT/linear-phase EQ (rejected — latency + complexity).

---

## D6. Where the effect settings UI lives (FR-004a)

**Decision**: Each effect implements `createEditor()` returning a custom `juce::AudioProcessorEditor`. The engine's existing `openEditor(position)` creates it under SEH and hands it to the tray-app's `PluginEditorWindow` — the same path used for hosted VST3 editors. This puts all settings in a dedicated edit window, never the main app window. **This requires adding `juce_gui_basics`/`juce_graphics` to the `jyglobalvst_audio_engine` CMake target** (hosted-VST3 editors don't need them because their GUI ships inside the plugin binary; ours does not).

**Rationale**: Reuses the entire editor lifecycle (open/close tracking in `editor_windows_`, SEH wrapping, window chrome) with zero new contract methods. `createEditor()` runs on the UI thread, so no constitution §V audio-thread violation is introduced.

**Alternatives**: *Tray-app-owned editor windows keyed off a "built-in" snapshot flag* — rejected: forks the editor-open path in the UI and requires a new `getParameter(position, ParamId)` contract method so the editor can display current/loaded values. Kept as a fallback if adding GUI modules to the engine proves undesirable for the future service build; noted in plan.md.

---

## D7. Parameter transport & state serialization

**Decision**: Expose effect parameters through JUCE parameters addressed by index, so the existing `setParameter(position, ParamId, value)` → `EngineCommand::SetParameter` → `PluginChain::setParameter` path drives them with no changes. `ParamId` == parameter index (map in data-model.md). State round-trips through `getStateInformation`/`setStateInformation`, which the preset serializer already base64-encodes per slot and applies as a pending chunk after `prepareToPlay`.

**Rationale**: Matches the mechanism scanned plugins already use; presets/autosave "just work" (FR-005) once resolution (D1/D2) is in place.

**RT-safety note**: The audio thread only reads the latest queued parameter values (atomics/`AudioParameterFloat`); coefficient/target recomputation is triggered by those reads and done with in-place fixed math.
