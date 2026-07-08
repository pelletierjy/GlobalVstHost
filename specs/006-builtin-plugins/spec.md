# Feature Specification: Built-In Audio Effect Plugins (Night-time & EQ + Bass Boost)

**Feature Branch**: `006-builtin-plugins`

**Created**: 2026-07-01

**Status**: Draft

**Input**: User description: "add a new in-app builtin plugins to list in the plugins list but those two would be in app included. First would be a stereo compressor/limiter called 'Night-time' that automatically adapts the sound to be a lot more compressed so that the sound volume is consistent when watching TV show late night with sleeping people around. The second one should be an EQ with bass boost option. Inspire yourself on that one from what is in D:\Repos\Others\fxsound-app without going too far, I want something very simple."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Add the "Night-time" leveler for quiet late-night listening (Priority: P1)

A user is watching a movie or TV show late at night while others are asleep. Loud action scenes and sudden effects are jarringly louder than quiet dialogue. The user opens the plugin list, finds the built-in **Night-time** effect already available (no download or installation), adds it to the processing chain, and turns it on. From that moment the loud parts are pulled down and the quiet dialogue is brought up, so the overall volume stays consistent and comfortable at a low listening level.

**Why this priority**: This is the headline motivation for the feature — the whole request is anchored on the late-night "consistent volume" use case. It delivers the core value on its own and is fully usable without the EQ.

**Independent Test**: Can be fully tested by adding only the Night-time effect to an otherwise empty chain, playing content that alternates between loud and quiet passages, and confirming the perceived loudness difference between passages is substantially reduced while nothing ever exceeds a safe peak ceiling. Delivers value with no other part of this feature present.

**Acceptance Scenarios**:

1. **Given** system audio is routing through the app, **When** the user opens the plugin list, **Then** "Night-time" appears in the list as a built-in effect that requires no scanning, download, or file selection.
2. **Given** the Night-time effect is in the chain and enabled, **When** loud content plays, **Then** the output loudness is reduced so it does not spike above the effect's ceiling, without audible clipping or distortion.
3. **Given** the Night-time effect is in the chain and enabled, **When** quiet dialogue plays, **Then** the dialogue is brought closer in loudness to the louder passages so it remains intelligible at a low volume.
4. **Given** the Night-time effect is enabled, **When** the user bypasses/disables it, **Then** audio returns to its unprocessed dynamics immediately with no dropout.

---

### User Story 2 - Add the built-in EQ with a bass boost for tone shaping (Priority: P2)

A user wants to shape the overall tone of their system audio — for example, boosting the bass for music or taming harsh highs — without hunting for and installing a third-party EQ plugin. The user opens the plugin list, finds the built-in **EQ** effect, adds it to the chain, adjusts a set of frequency-band sliders, and raises a dedicated **Bass Boost** amount to add low-end warmth.

**Why this priority**: Complements the primary use case (tone control alongside loudness control) and reuses the same built-in-plugin infrastructure, but the feature still delivers its core value with only the Night-time effect. Deliberately kept simple per the request.

**Independent Test**: Can be fully tested by adding only the EQ effect to an otherwise empty chain, moving individual band sliders and toggling Bass Boost, and confirming the corresponding frequency ranges are audibly and measurably boosted or cut.

**Acceptance Scenarios**:

1. **Given** system audio is routing through the app, **When** the user opens the plugin list, **Then** the built-in "EQ" appears as an available effect requiring no scanning or download.
2. **Given** the EQ is in the chain, **When** the user raises a band's gain slider, **Then** the corresponding frequency range in the output is boosted; when lowered, it is cut.
3. **Given** the EQ is in the chain, **When** the user raises the Bass Boost amount, **Then** low frequencies are boosted proportionally without distortion; when the amount is returned to zero, the boost is removed.
4. **Given** the user has set band gains and a Bass Boost amount, **When** the user selects a "flat/reset" action, **Then** all bands return to 0 dB (no coloration) and the Bass Boost amount returns to zero.

---

### User Story 3 - Built-in effects behave like any other plugin in the chain (Priority: P3)

A user treats the built-in effects exactly like scanned VST3 plugins: adding, removing, reordering them in the chain, adjusting their settings, and saving/loading them as part of a preset or the auto-saved session so their configuration persists across restarts.

**Why this priority**: Ensures the built-ins integrate cleanly with the existing chain/preset system rather than being a special case. Important for a coherent experience, but the effects are individually useful even before full preset round-tripping is polished.

**Independent Test**: Can be fully tested by adding both built-in effects to a chain, reordering them, saving a preset, restarting the app, loading the preset, and confirming both effects and all their settings are restored in the same order.

**Acceptance Scenarios**:

1. **Given** a chain containing built-in and scanned plugins, **When** the user reorders or removes items, **Then** the built-in effects respond identically to scanned plugins.
2. **Given** a chain containing configured built-in effects, **When** the user saves a preset and later loads it, **Then** each built-in effect and all of its parameter values are restored exactly.
3. **Given** built-in effects are in the auto-saved session chain, **When** the app is closed and reopened, **Then** the effects and settings are restored automatically.

---

### Edge Cases

- **Extreme input levels**: When input is already at or near full scale, the Night-time effect must prevent output from exceeding its ceiling (limiting) without introducing audible clipping.
- **Silence / very quiet input**: The Night-time effect must not aggressively amplify near-silence into audible noise or a "pumping" hiss (i.e., upward leveling must have a floor / noise gate behavior).
- **Sudden loud transient (e.g., explosion)**: The limiter must catch fast transients quickly enough that no startling peak passes through.
- **Mono vs. stereo content**: Both effects must process stereo content while keeping the left/right image stable (the compressor should act on the stereo pair together so the image does not wander; the EQ must apply matching curves to both channels).
- **Sample-rate changes** (44.1 / 48 / 96 kHz): Both effects must produce the intended frequency and time behavior at any supported sample rate.
- **Bass boost stacked on an already bass-heavy band**: Combined EQ band gain + Bass Boost must not drive low frequencies into distortion; the effect must guard against internal overload.
- **Rapid enable/disable / reorder while audio is playing**: Toggling or moving a built-in effect during playback must not cause dropouts, clicks, or crashes.
- **Duplicate instances**: Adding the same built-in effect more than once in a chain must be supported and each instance keeps independent settings.

## Clarifications

### Session 2026-07-01

- Q: How should Night-time's limiter handle the look-ahead needed to catch transients, given the ≤10 ms round-trip budget? → A: Make look-ahead (and the latency-vs-quality tradeoff) user-configurable inside the plugin's own edit view — not in the main app window; effective latency varies with the chosen setting.
- Q: How should Night-time achieve consistent volume for late-night listening? → A: Broadcast-style loudness normalization toward a target loudness level, combined with a limiter ceiling.
- Q: What controls should the Night-time effect expose to stay simple? → A: Preset levels (e.g., Light / Medium / Strong) rather than a continuous knob or fully-hidden automation.
- Q: How should the simple EQ be laid out and how should Bass Boost work? → A: ~10 fixed gain bands (sub-bass through treble), with Bass Boost as an adjustable amount control.
- Q: Where are the built-in effects' settings adjusted? → A: In each plugin's own edit/detail view, consistent with how scanned plugins expose their editors — not surfaced in the main app window.

## Requirements *(mandatory)*

### Functional Requirements

#### Built-in plugin catalog integration

- **FR-001**: The system MUST present two built-in audio effects — "Night-time" and "EQ" — in the same plugin list where scanned third-party plugins appear.
- **FR-002**: The built-in effects MUST be available without any scanning, download, installation, or file selection; they ship with the application.
- **FR-003**: The built-in effects MUST be visually or textually identifiable as built-in (so users can distinguish them from scanned third-party plugins).
- **FR-004**: Users MUST be able to add, remove, reorder, enable/disable, and duplicate the built-in effects in the processing chain using the same interactions as for scanned plugins.
- **FR-004a**: All of a built-in effect's adjustable settings (Night-time preset level and limiter look-ahead; EQ band gains and Bass Boost amount) MUST be adjusted within that effect's own edit/detail view — the same place a scanned plugin exposes its editor — and MUST NOT be surfaced in the main application window.
- **FR-005**: Each built-in effect's settings MUST be persisted and restored as part of presets and the auto-saved session chain, round-tripping exactly across app restarts.
- **FR-006**: The built-in effects MUST NOT depend on the yet-to-be-built virtual driver; they MUST work in the current testable-dev configuration and in later configurations without change.

#### "Night-time" compressor/limiter

- **FR-007**: The Night-time effect MUST perform broadcast-style loudness normalization — continuously measuring program loudness and applying gain to bring it toward a target loudness level — so that quiet dialogue is raised and loud passages are pulled down and overall perceived volume stays consistent at low listening levels.
- **FR-008**: The Night-time effect MUST apply a peak/output ceiling (limiting) so that output never exceeds a safe maximum level, preventing sudden loud spikes.
- **FR-008a**: The limiter's look-ahead (and the resulting latency-vs-transient-cleanliness tradeoff) MUST be user-configurable within the plugin's edit view, including a zero/near-zero-latency option; the selected value determines the effect's added latency.
- **FR-009**: The Night-time effect MUST operate via a small set of named preset levels (e.g., Light / Medium / Strong) selectable by the user, requiring no manual threshold/ratio/attack/release tuning; each preset maps to appropriate normalization strength and limiter behavior.
- **FR-010**: The Night-time effect MUST process the stereo pair in a linked manner so that the left/right stereo image remains stable during gain changes.
- **FR-011**: The Night-time effect MUST avoid amplifying near-silence into audible noise (it MUST include a floor below which it does not boost).
- **FR-012**: The Night-time effect MUST NOT introduce audible clipping, and MUST minimize audible "pumping" artifacts under normal TV/movie content.

#### "EQ" with bass boost

- **FR-013**: The EQ effect MUST provide approximately 10 fixed frequency bands (spanning sub-bass through treble), each with an adjustable boost/cut gain applied equally to both stereo channels.
- **FR-014**: The EQ effect MUST provide a dedicated "Bass Boost" control with an adjustable amount that increases low-frequency content, independent of the individual band sliders.
- **FR-015**: The EQ effect MUST provide a "flat/reset" action that returns all band gains to 0 dB and the Bass Boost amount to its neutral (zero) state.
- **FR-016**: The EQ effect MUST guard against internal overload so that combined band gains and Bass Boost amount do not produce distortion at the output.
- **FR-017**: The EQ MUST remain intentionally simple in scope: a fixed set of ~10 bands with gain only (no adjustable Q, no parametric per-band frequency editing) for this version.

#### Real-time safety and quality (project-wide constraints)

- **FR-018**: Both built-in effects MUST honor the project's real-time audio discipline: no memory allocation, locking, file I/O, or logging on the audio processing path.
- **FR-019**: Parameter changes made in the UI MUST be applied to the audio path without dropouts, using the project's existing lock-free command mechanism.
- **FR-020**: A failure inside a built-in effect MUST be contained the same way plugin failures are (the effect is bypassed and audio continues) and MUST NOT crash the audio engine.

### Key Entities *(include if feature involves data)*

- **Built-in Effect Descriptor**: Represents a bundled effect as it appears in the plugin list — stable identity/name, "built-in" marker, and the set of adjustable parameters. Distinct from a scanned-plugin descriptor in that it has no file path and is always present.
- **Night-time Effect Instance**: A placed instance of the Night-time effect in a chain, holding its enable state, selected preset level (e.g., Light/Medium/Strong), and limiter look-ahead setting.
- **EQ Effect Instance**: A placed instance of the EQ effect in a chain, holding its per-band gain values and bass-boost state/amount.
- **Effect Settings Snapshot**: The serializable representation of a built-in effect instance's parameters, embedded in presets and the auto-saved chain alongside scanned-plugin state.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A user can locate a built-in effect in the plugin list and add it to the chain in under 15 seconds, with no scan/download step.
- **SC-002**: With Night-time enabled on content that alternates between quiet dialogue and loud effects, measured integrated loudness converges toward the effect's target level and the loudness difference between the loudest and quietest passages is reduced by at least 50% compared to bypassed, with output never exceeding the effect's ceiling.
- **SC-003**: With the EQ, moving any band slider produces a clearly audible and measurable change (≥ several dB) in the intended frequency range and no other, and enabling Bass Boost produces a clearly audible low-frequency increase with no output distortion.
- **SC-004**: 100% of built-in effect settings survive a save-preset → restart → load-preset cycle and an auto-save → restart cycle, restored in the same chain order.
- **SC-005**: Both built-in effects are discoverable and usable with no written instructions by a first-time user in an informal try-it test (the intent of each control is clear from its label).

### Audio-Specific Success Criteria *(required if audio component)*

- **AUDIO-001**: With Night-time's limiter look-ahead set to its zero/near-zero-latency option, adding both built-in effects to the chain keeps round-trip latency within the project budget (≤ 10 ms). When the user selects a look-ahead setting that adds latency, the added amount is displayed to the user and is attributable solely to that choice.
- **AUDIO-002**: With both built-in effects enabled, CPU usage stays ≤ 5% on a modern multi-core system during normal playback.
- **AUDIO-003**: Zero audio dropouts during a 30-minute continuous soak test with both effects enabled and parameters being adjusted.
- **AUDIO-004**: Both effects behave correctly at 44.1 / 48 / 96 kHz, with frequency-dependent behavior (EQ band centers, bass-boost range, limiter timing) consistent across sample rates.

## Assumptions

- **Reference, not port**: The fxsound-app EQ is used only as conceptual inspiration (a fixed-band graphic EQ with a linked bass-boost/"Hyperbass" control). No fxsound code is copied; that project is GPL-licensed and copying it would impose licensing constraints the host project does not intend to adopt. The built-in EQ is an original, simplified implementation.
- **Built-in ≠ external VST3**: The two effects are implemented as internal processors that surface in the plugin list, not as separately installed VST3 files. They are always present regardless of what third-party plugins are scanned.
- **Simplicity is a hard scope boundary**: Per the request ("something very simple", "without going too far"), the EQ has ~10 fixed bands with gain only plus a Bass Boost amount, and Night-time is a preset-driven loudness normalizer with a built-in limiter (its only exposed controls are the preset level and the limiter look-ahead setting). Advanced controls (parametric Q, sidechain, multiband compression, per-band frequency editing, spectrum analyzer, saved presets within the effect) are explicitly out of scope for this version.
- **Stereo focus**: Content is assumed to be stereo (the request specifies "stereo compressor/limiter"). Multichannel/surround handling is out of scope.
- **Chain/preset system reuse**: The existing chain editor, preset format, and auto-save session mechanism are reused; built-in effect state is embedded in the same files as scanned-plugin state.
- **No new persistence locations**: No new settings files or storage locations are introduced beyond the existing roaming/local/preset stores.
- **Sensible default tuning**: Concrete numeric values (target loudness level, limiter ceiling, per-preset normalization strength, look-ahead options, the exact 10 EQ band center frequencies, and Bass Boost range) will be chosen during planning/implementation to sound good for late-night TV/movie use; the exact values are implementation details, not spec requirements. "~10 bands" and "Light/Medium/Strong" presets are the intended shape, not rigidly fixed counts/names.
