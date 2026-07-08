# Feature Specification: Driverless System-Audio Capture (WASAPI Loopback → Separate Output)

**Feature Branch**: `005-driverless-audio-capture`

**Created**: 2026-07-01

**Status**: Draft

**Input**: User description: "Switch to FxSound's approach."

## Context & Premise Correction

The originating request was to adopt "FxSound's approach" to grabbing Windows audio, on the belief that FxSound is fully driverless and therefore avoids the code-signing/certificate problem that blocks the current kernel driver (spec 004).

Inspection of the referenced `fxsound-app` source corrected that belief: **FxSound ships and installs a signed kernel-mode virtual audio driver** ("FxSound Audio Enhancer" / "FxSound Speakers"). Its WASAPI-loopback + default-endpoint management is the plumbing *around* that driver, not a replacement for it. FxSound solved signing the conventional way — by signing their driver.

The one capability that is genuinely driverless and certificate-free is **WASAPI loopback capture of a real render endpoint**. This is a *tap* on audio that is already playing, not a transparent *insert* into the output path. Consequently, to route captured audio through a plugin chain it must be rendered to a **different** output device than the one being captured.

This specification therefore scopes a **driverless capture model**: capture the system output via WASAPI loopback, process it through the VST3 chain, and render to a user-selected output device that is distinct from the captured device. This removes both the unsigned-driver dependency and the third-party virtual-cable dependency, at the cost of requiring a separate listening device. It does **not** provide FxSound-style transparent processing of a single device (that remains a signed-driver capability, tracked separately in spec 004).

## Clarifications

### Session 2026-07-01

- Q: In the loopback→separate-output model the captured render endpoint is still physically audible; what should happen to its own sound? → A: Mute/suppress the captured device while processing is active and restore it on stop, so the user hears only the processed signal on the output device. If planning confirms that muting a real endpoint also zeroes its loopback capture, fall back to requiring the captured endpoint to be one the user does not listen to directly.
- Q: How should the ≤10 ms constitutional latency gate be resolved for the driverless path (AUDIO-001)? → A: Deferred — latency optimization and any constitutional-deviation decision are postponed to the end of implementation and revisited only if needed. AUDIO-001 remains a documented open risk.
- Q: When the user selects the same device for both capture and output, should the app block or warn-and-allow? → A: Hard block — the app never permits starting with capture device == output device, and provides no override.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Listen to processed system audio on a separate output, with no install (Priority: P1)

A user wants all Windows audio (browser, media player, games) to pass through their VST3 chain and be heard on a dedicated listening device (an external DAC, USB interface, or headphones) — without installing a driver, without third-party virtual cables, and without administrator rights.

**Why this priority**: This is the core value of the feature and the direct replacement for both the blocked kernel driver and the third-party virtual cable. Without it, there is no product.

**Independent Test**: Start the app on a machine with two output devices and no virtual cable installed. Select the system's playing device as the capture source and a second device as the output. Play audio from any application and confirm it is heard, processed by the chain, on the second device.

**Acceptance Scenarios**:

1. **Given** a machine with at least two distinct output devices and no virtual cable installed, **When** the user selects a capture source and a distinct output device and starts processing, **Then** system audio is captured via loopback, passed through the plugin chain, and rendered to the selected output device.
2. **Given** the app is running as a standard (non-administrator) user, **When** the user starts audio processing, **Then** capture and playback succeed with no elevation prompt and no driver installation.
3. **Given** processing is active, **When** a plugin is added, removed, or reordered in the chain, **Then** the change takes effect on the live captured stream without stopping audio (existing chain behavior is preserved).

---

### User Story 2 - Prevent audio feedback / double-audio from same-device selection (Priority: P2)

Because loopback is a tap on audio that is still audible on the captured device, selecting the same device for both capture and output produces the original plus the processed signal (and a feedback path). The user must be protected from this misconfiguration.

**Why this priority**: The primary constraint of the driverless model is that capture and output must differ. If unenforced, the most common first mistake yields obviously broken audio and erodes trust.

**Independent Test**: Attempt to select the same device as both capture source and output. Confirm the app hard-blocks starting (no override) and explains why.

**Acceptance Scenarios**:

1. **Given** a capture source is selected, **When** the user attempts to select the same device as the output, **Then** the app prevents the selection or blocks starting with no override and explains that capture and output must be different devices.
2. **Given** processing is active on distinct devices, **When** the operating system changes the default device such that capture and output would coincide, **Then** the app detects the conflict and pauses processing with a clear message rather than producing feedback.

---

### User Story 3 - Survive device changes without a restart (Priority: P2)

Users plug and unplug headphones, docks, and interfaces, and Windows changes the default device. Capture and playback must recover gracefully.

**Why this priority**: Device hot-plug is routine on Windows laptops and desktops. A model that requires an app restart on every device change is not viable as a daily driver, and the reference implementation (FxSound) invests heavily here.

**Independent Test**: With processing active, unplug/replug the output device and change the Windows default device; confirm the app recovers and resumes processing (or surfaces an actionable message) without a restart.

**Acceptance Scenarios**:

1. **Given** processing is active, **When** the selected output device is removed, **Then** the app detects the loss, stops rendering safely, and prompts the user to choose another output rather than crashing.
2. **Given** processing is active and the capture source is the system default, **When** the Windows default output changes, **Then** the app follows the new default (or clearly indicates the source changed) and continues capturing.
3. **Given** a device that was removed is plugged back in, **When** it reappears, **Then** the app can resume using it without a restart.

---

### User Story 4 - Remove the third-party virtual cable and kernel-driver dependencies (Priority: P3)

The project should no longer require a third-party virtual cable for its default/testable path, and the kernel driver (spec 004) should no longer be on the default build/run path.

**Why this priority**: This is cleanup that realizes the strategic goal (no external dependency, no signing blocker) but is not required for a user to experience the core value once User Story 1 works.

**Independent Test**: On a clean machine with no virtual cable and the driver build disabled, complete the User Story 1 flow end to end.

**Acceptance Scenarios**:

1. **Given** a clean machine with no virtual audio cable installed, **When** the user runs the app in its default configuration, **Then** the app offers loopback capture of real render endpoints as the audio source with no reference to an external cable.
2. **Given** the default build configuration, **When** the project is built and run, **Then** no kernel driver is required, installed, or loaded for the capture path to function.

---

### Edge Cases

- **Only one output device present**: The machine has a single output endpoint, so no valid distinct output exists. The app must clearly explain that a second output device is required for driverless operation, rather than failing silently.
- **Capture source and output device are the same**: Covered by User Story 2 — prevented or gated behind an explicit warning.
- **Sample-rate / channel-count mismatch** between the captured stream and the output device (e.g., 48 kHz capture, 44.1 kHz output): resampled/converted at the boundary; no dropouts or pitch errors.
- **Captured device is idle/silent**: WASAPI loopback delivers silence or no packets when nothing is playing; the app must keep the output stream alive (or handle gaps) without underrun artifacts when audio resumes.
- **Muting the captured device zeroes its loopback capture**: on drivers where suppressing the captured endpoint also silences the loopback stream (FR-018), the app must detect this / fall back to the "capture a non-listened endpoint" model rather than silently capturing silence.
- **App crash or forced exit while the captured device is muted**: the captured device's prior mute/volume state must be restorable so the user is not left with permanently silent speakers.
- **Captured device is in exclusive mode** by another application: loopback may be unavailable; the app must surface an actionable message.
- **Default device changes mid-stream** while the user chose "follow system default": handled by User Story 3.
- **Latency expectation mismatch**: shared-mode loopback introduces more capture latency than a kernel driver; the app must not promise driver-class latency it cannot meet (see Success Criteria / Assumptions).

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST capture Windows system audio by performing WASAPI loopback capture on a real render (playback) endpoint, requiring no kernel driver and no third-party virtual cable.
- **FR-002**: The system MUST operate without administrator privileges and without installing, loading, or signing any driver for the capture path.
- **FR-003**: Users MUST be able to select the capture source as either the current system default output endpoint or a specific named render endpoint.
- **FR-004**: Users MUST be able to select an output (rendering) device for the processed audio.
- **FR-005**: The system MUST require the output device to be different from the captured device, and MUST hard-block any configuration where they coincide — the app never permits starting with capture device == output device and offers no override (see FR-014 for the runtime-conflict case).
- **FR-006**: The system MUST route captured audio through the existing VST3 plugin chain unchanged, preserving live add/remove/reorder behavior.
- **FR-007**: The system MUST convert format and sample rate at the boundaries so that a mismatch between the captured stream and the output device produces correct audio (no pitch shift, no dropouts).
- **FR-008**: The system MUST detect when the selected output device is removed and stop rendering safely, prompting the user to choose another output without crashing.
- **FR-009**: The system MUST detect when the captured render endpoint is removed or the system default changes, and either follow the new default (when the source is "system default") or surface a clear message (when a specific endpoint was selected).
- **FR-010**: The system MUST allow a previously removed device to be reselected and resume processing without an application restart.
- **FR-011**: When no valid distinct output device exists (e.g., only one output endpoint present), the system MUST clearly inform the user that a second output device is required for driverless operation.
- **FR-012**: The system MUST keep the output stream stable across periods of silence on the captured device (idle source), resuming cleanly when audio returns without underrun artifacts.
- **FR-013**: The system MUST surface an actionable message when loopback capture is unavailable for the selected endpoint (e.g., the endpoint is held in exclusive mode).
- **FR-014**: While processing is active, the system MUST detect if a device change would cause capture and output to coincide, and MUST pause processing with a clear message rather than create a feedback path.
- **FR-015**: The default build and run configuration MUST NOT require the kernel driver (spec 004) or a third-party virtual cable for the capture path.
- **FR-016**: The system MUST persist the user's capture-source and output-device selections and restore them on next launch, degrading gracefully if a remembered device is absent.
- **FR-017**: The system MUST measure and expose the actual round-trip latency of the loopback path so the user (and tests) can see the real figure for their configuration.
- **FR-018**: While processing is active, the system MUST mute or suppress the captured render endpoint's own audible output so the user hears only the processed signal on the output device, and MUST restore the captured device's prior mute/volume state when processing stops or the app exits. If planning confirms that muting a real endpoint also zeroes its loopback capture, the system MUST instead require the captured endpoint to be one the user does not listen to directly and clearly communicate that setup requirement (fallback path).

### Key Entities *(include if feature involves data)*

- **Capture Source**: The render endpoint being loopback-captured. Either a symbolic "system default output" selection or a specific endpoint identity. Attributes: identity/name, current format (sample rate, channels), default-follow flag, availability state.
- **Output Device**: The render endpoint the processed audio is sent to. Attributes: identity/name, current format, availability state. Constraint: must not equal the Capture Source's resolved endpoint.
- **Routing Configuration**: The persisted pairing of Capture Source + Output Device + follow-default preference, restored across launches.
- **Device Availability State**: Per-device runtime status (present / removed / exclusive-held / conflicting) driving user-facing messages and recovery.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: On a machine with two output devices and no virtual cable installed, a non-administrator user can go from app launch to hearing processed system audio on the second device in under 2 minutes, with no install or elevation step.
- **SC-002**: 100% of attempts to select the same device for capture and output are hard-blocked with an explanation and no override path (zero silent feedback/double-audio occurrences).
- **SC-003**: With processing active, removing and re-adding the output device, and changing the Windows default device, results in either automatic recovery or a clear actionable prompt in 100% of trials — zero crashes and zero states requiring an app restart.
- **SC-004**: On a clean machine with no third-party virtual cable and the driver build disabled, the end-to-end capture→chain→output flow succeeds (verifies the dependency removal goal).

### Audio-Specific Success Criteria *(required if audio component)*

- **AUDIO-001**: Round-trip latency (loopback capture → VST chain → output render) is **measured and documented** for representative configurations. NOTE: shared-mode WASAPI loopback is expected to add materially more latency than the kernel-driver path; the constitutional ≤ 10 ms target may not be achievable driverless. The actual figure MUST be measured, reported, and accepted as a documented tradeoff during planning rather than assumed. This is a known risk requiring verification, not a verified claim. See Assumptions.
- **AUDIO-002**: CPU usage of the capture + resample + render path (excluding plugin cost) stays ≤ 5% on a modern multi-core system during normal playback.
- **AUDIO-003**: Zero audio dropouts introduced by the capture/render path during a 30-minute continuous soak with audio playing, across at least one sample-rate-mismatched capture/output pair.
- **AUDIO-004**: Sample-rate mismatches (44.1 / 48 / 96 kHz between capture and output) are handled with correct pitch and no dropouts.

## Assumptions

- **The driverless model requires two distinct output devices.** The user has explicitly accepted this constraint. Transparent processing of a single device (apps → VST → same speakers) is out of scope here and remains a signed-driver capability (spec 004).
- **Latency will be higher than the kernel-driver path.** Shared-mode WASAPI loopback typically delivers audio in ~10 ms packets, so the constitution's ≤ 10 ms round-trip figure is treated as a target to measure against, not a guaranteed outcome. Any deviation from the NON-NEGOTIABLE constitutional latency requirement must be reviewed and explicitly accepted (or the feature re-scoped) during planning. This is flagged for verification.
- **The existing VST3 chain, format-conversion, and resampling layers are reused unchanged.** This feature changes the audio *source* (real loopback endpoint instead of a capture endpoint fed by a virtual cable) and adds device/output management; it does not redesign the DSP graph.
- **"System default output" is the most common capture-source choice**, so following default-device changes (User Story 3) is treated as expected behavior, not an edge case.
- **The kernel driver (spec 004) is set aside, not deleted.** It remains in the tree behind its build gate for a possible future signed-distribution path; this feature only removes it from the default capture path.
- **Windows 10 1909+ / Windows 11, x64** target is unchanged from the root spec.
- **This feature does not add any network activity, telemetry, or persistent logging**, consistent with existing project principles.

## Dependencies

- Reuses the existing audio engine's format-conversion, resampling, VST3 `AudioProcessorGraph`, and device-enumeration/selection layers.
- Reuses the existing settings/local-state persistence for remembering device selections.
- Supersedes, on the default path, the third-party virtual-cable input surrogate described in the testable-dev notes and the kernel-driver capture path from spec 004.
