# Quickstart: JyGlobalVST Validation Scenarios

**Branch**: `001-jyglobalvst-system-host` | **Date**: 2026-06-04

This document defines runnable validation scenarios that prove JyGlobalVST works end-to-end. Each scenario lists prerequisites, setup, the action to perform, and the expected outcome. Implementation detail belongs in `tasks.md` and the implementation phase — this file is purely a validation/run guide.

For entity shapes see [`data-model.md`](./data-model.md). For wire formats see [`contracts/`](./contracts/). For the underlying technology decisions see [`research.md`](./research.md).

---

## Prerequisites (all scenarios)

- **Hardware**: A Windows 10 1909+ or Windows 11 machine, x64, with at least one functioning audio output (built-in speakers, USB DAC, or HDMI sink).
- **Build artifacts**: A signed MSI built by the installer project; for development, the test-signed MSI plus `bcdedit /set testsigning on` reboot.
- **Test plugins**: At least one VST3 plugin installed in `%ProgramFiles%\Common Files\VST3\`. Validated set for release sign-off:
  - IK Multimedia ARC X (room correction)
  - Sonarworks Reference (room correction)
  - ReaEQ (free, Cockos)
  - FabFilter Pro-Q 3 (commercial, optional)
- **Optional for latency measurement**: A USB audio interface with a TRS loopback cable (e.g., Focusrite Scarlett 2i2). See Scenario 4.
- **Reset between scenarios**: Delete `%LocalAppData%\JyGlobalVST\` to get a true cold-start; keep `%AppData%\Roaming\JyGlobalVST\` to preserve preferences unless explicitly stated.

---

## Scenario 1 — First-launch install and audio routing (P1)

**Validates**: FR-001, FR-004, FR-005, FR-006, FR-012, FR-013, FR-030, FR-031, SC-001 (under 2 minutes), AUDIO-001 baseline.

**Setup**:
1. Uninstall any previous JyGlobalVST build.
2. Run `JyGlobalVST-Setup-x.y.z.msi`. Approve the single UAC prompt.
3. Confirm the installer completes without a reboot prompt.

**Action**:
1. Open Windows Sound settings → Output. Confirm "JyGlobalVST Virtual Output" appears.
2. Select it as the default output device.
3. Launch JyGlobalVST from the system tray icon (or wait for auto-launch).
4. In the device-selector dropdown, choose your hardware output (built-in speakers or USB DAC).
5. Click "Scan plugins"; wait for the background scan to complete and at least one plugin to appear.
6. Drag one plugin onto the chain.
7. Play audio from any source (Spotify, YouTube, a system sound).

**Expected outcome**:
- Audio is audible on the selected hardware output.
- The plugin's processing is audible (e.g., ARC X correction is applied).
- Total wall-clock time from MSI double-click to audio audible is **under 2 minutes** (SC-001).
- No restart of any application or the OS was required.

---

## Scenario 2 — Plugin chain management (P2)

**Validates**: FR-008, FR-009, FR-010, FR-011, FR-019.

**Setup**: Scenario 1 completed; engine is running with at least three plugins available in the catalog.

**Action**:
1. Add three plugins to the chain in this order: room-correction → parametric EQ → limiter.
2. Confirm processing order: bypass the limiter, then the EQ, in turn; audible result should match each isolation.
3. Reorder by dragging the EQ above the room-correction.
4. Confirm audio continues without dropout during the reorder.
5. Toggle bypass on the middle plugin while audio plays.
6. Remove the EQ from the chain.

**Expected outcome**:
- Chain depth indicator updates after each mutation.
- No audible dropout during any operation (FR-010).
- Bypass state is visible in the UI and audibly correct.
- Latency readout (FR-019) updates after each chain mutation.

---

## Scenario 3 — Preset save, load, and missing-plugin placeholder (P3)

**Validates**: FR-020, FR-021, FR-022, FR-022a, FR-022f, FR-022g, FR-022g-1, FR-022g-2.

**Setup**: Scenario 2 completed; chain contains three plugins.

**Action — save and reload**:
1. Save the current chain as preset "Gaming".
2. Confirm `%UserProfile%\Documents\JyGlobalVST\Presets\Gaming.jvst` exists.
3. Open `Gaming.jvst` in a text editor; confirm it is human-readable JSON with `schema_version: 1`, plugin list, base64 state chunks.
4. Clear the chain (remove all plugins).
5. Load "Gaming" preset. Confirm the chain rebuilds with all three plugins in the original order, parameters preserved, bypass states preserved.

**Action — missing-plugin placeholder**:
1. Quit JyGlobalVST.
2. Remove (or rename) the `.vst3` bundle of the middle plugin from disk.
3. Relaunch JyGlobalVST. Load "Gaming" preset.
4. Confirm the chain shows three slots: the first and third are real plugin instances, the middle is a **greyed-out placeholder** preserving position and bypass state.
5. A non-modal notification lists the missing plugin (name + recorded path).
6. Restore the plugin to disk; rescan plugins; re-point the placeholder via right-click → "Re-point to scanned plugin". Confirm the chain re-instantiates with the saved state chunk applied.

**Action — strict import validation**:
1. Edit a copy of `Gaming.jvst` to add a top-level field `"unknown_field": 123`.
2. Drag the edited file onto JyGlobalVST.
3. Confirm import is **rejected** with a non-modal notification citing the unknown field (FR-022g-1).
4. Edit another copy to inflate a `state_chunk_b64` to > 16 MB decoded.
5. Drag it onto JyGlobalVST. Confirm rejection with a size-cap error.

**Expected outcome**:
- Round-trip save/load preserves chain structure and state.
- Missing plugins become placeholders; chain audio flows through the remaining plugins (FR-022f).
- Import validation rejects malformed files cleanly with no partial state.

---

## Scenario 4 — Latency measurement (AUDIO-001 / AUDIO-005)

**Validates**: AUDIO-001 (≤ 10 ms typical), AUDIO-005 (≤ 20 ms heavy), FR-019.

**Setup**:
- USB audio interface (e.g., Focusrite Scarlett 2i2) connected.
- A TRS cable looped from Output 1 of the interface to Input 1.
- A second WASAPI capture session (any DAW or a small Python script using `sounddevice`) configured to record from the interface's Input 1.
- JyGlobalVST set as Windows default output; interface set as JyGlobalVST's hardware output.

**Action**:
1. Buffer size: 256 samples. Chain: one plugin (ReaEQ flat).
2. Play a known click impulse (e.g., 100 ms of silence + 1-sample impulse + 100 ms of silence) from a test application.
3. Record the loopback into the second capture session.
4. Measure the time delta between the impulse send timestamp and the recorded impulse arrival.
5. Repeat with chain depth = 5 (five plugins, each adding moderate DSP load).

**Expected outcome**:
- Single-plugin round-trip ≤ **10 ms** (AUDIO-001).
- Five-plugin round-trip ≤ **20 ms** (AUDIO-005).
- JyGlobalVST's in-app latency readout (FR-019) is within ±1 ms of the measured value.

---

## Scenario 5 — Hardware output device removal and restore (FR-024)

**Validates**: FR-024.

**Setup**: Engine running; output device is a USB DAC; audio is playing.

**Action**:
1. While audio is playing, physically unplug the USB DAC.
2. Observe what happens.
3. After ~5 seconds, plug the USB DAC back in.

**Expected outcome**:
- Audio continues without a long silence; the engine falls back to the current Windows default output within a few hundred milliseconds.
- A non-modal notification appears, identifying the lost device and the fallback device.
- On reconnect, audio automatically returns to the USB DAC; a second notification announces the restoration.
- No user interaction was required during either transition.

---

## Scenario 6 — Plugin crash isolation (FR-023)

**Validates**: FR-023.

**Setup**: A known-crashing test plugin is available. (For CI, ship a tiny test VST3 in the test harness whose `processBlock` triggers a divide-by-zero on a magic input value.)

**Action**:
1. Add the test plugin to the chain.
2. Play audio that triggers the divide-by-zero path.

**Expected outcome**:
- The plugin's slot is marked **failed** in the chain UI (red badge or equivalent).
- A non-modal notification identifies the plugin and the failure code.
- Audio continues through the rest of the chain without dropout (FR-023).
- No log file is written (Constitution §V, FR-022n).
- Quitting and relaunching JyGlobalVST re-loads the auto-save with the failed plugin still in the chain (no quarantine, clarification #12). The user is responsible for removing it.

---

## Scenario 7 — Sample-rate and bit-depth handling (FR-002, FR-002a, AUDIO-004)

**Validates**: FR-002, FR-002a, FR-014, AUDIO-004.

**Setup**: A hardware output device that supports multiple sample rates (most USB DACs do — set the OS-level device default to 48 kHz, then to 96 kHz, then to 176.4 kHz between attempts).

**Action**:
1. Set the hardware output's OS-level default to 48 kHz.
2. Play audio from a 44.1 kHz source (most Spotify content) through JyGlobalVST. Confirm no glitches.
3. Change the hardware device's OS-level default to 96 kHz (in Windows Sound → Properties → Advanced). Play 44.1 kHz audio. Confirm seamless transition.
4. Play 24-bit FLAC content (e.g., from a media player); confirm no audible artifacts.
5. Repeat with the hardware default at 176.4 kHz and 192 kHz.

**Expected outcome**:
- All sample-rate transitions are inaudible (AUDIO-004).
- 16-bit and 24-bit source content plays without audible degradation.
- Internal processing remains 32-bit float (verifiable via the diagnostics panel in About → Diagnostics).

---

## Scenario 8 — CPU budget warning (FR-026, AUDIO-002)

**Validates**: FR-026, AUDIO-002.

**Setup**: A modern target machine (Intel i5 8th gen / Ryzen 5 3000 series or newer).

**Action**:
1. Configure a deliberately heavy chain: load 15 instances of a CPU-intensive plugin (a convolution reverb with a long impulse, for example).
2. Play audio.
3. Observe the CPU readout.

**Expected outcome**:
- When `rolling_1s_pct` reaches or exceeds 5%, a persistent non-modal warning appears in the UI ("CPU approaching limit; consider increasing buffer size").
- The engine does **not** auto-bypass plugins or auto-increase the buffer (clarification #8). The user retains full control.
- Removing some plugins clears the warning within ~1 second.

---

## Scenario 9 — System sleep / wake (FR-025)

**Validates**: FR-025.

**Action**:
1. With audio playing, put the machine to sleep via Start menu.
2. Wake the machine.
3. Resume the audio source.

**Expected outcome**:
- JyGlobalVST reinitializes the audio path automatically.
- No user intervention required; no error popup.
- Audio resumes flowing within ~2 seconds of wake.

---

## Scenario 10 — Service-mode pre-login audio (FR-028, FR-028a, FR-028b, FR-029)

**Validates**: FR-028, FR-028a, FR-028b, FR-029.

**Setup**: Reinstall via the MSI with the advanced "Install as Windows Service" option enabled.

**Action**:
1. Reboot the machine. Stop at the lock screen.
2. Trigger a lock-screen sound (e.g., a Windows accessibility tone) or play system sound via remote desktop.
3. Confirm audio routes through the JyGlobalVST chain even though no user is logged in.
4. Log in. Confirm the tray app launches and connects to the service.
5. In the tray app, mutate the chain (add/remove a plugin). Confirm changes take effect.
6. Confirm the UI looks and behaves identically to user-mode (FR-029).
7. Open About → Diagnostics. Confirm the "Engine host" line reads "Windows Service" (only diagnostics surface that reveals the mode).

**Expected outcome**:
- Pre-login audio works.
- Tray-app UI is identical to user-mode aside from the diagnostics surface.
- IPC is local-only (verifiable by running `netstat -ano` and confirming no JyGlobalVST process listens on a network port — FR-028a).

---

## Scenario 11 — Single-instance per session (FR-022j)

**Validates**: FR-022j.

**Action**:
1. Launch JyGlobalVST. Confirm the tray icon appears.
2. Launch JyGlobalVST a second time from another shortcut.
3. Observe.

**Expected outcome**:
- The second launch focuses/restores the existing tray-app's main window and exits cleanly.
- The audio engine is not re-bound; the virtual device is not re-claimed.
- In a second user session (Fast-User-Switching or RDP), launching JyGlobalVST starts an independent tray-app instance (FR-022j).

---

## Scenario 12 — Accessibility: keyboard-only operation and screen reader (FR-019a, FR-019b)

**Validates**: FR-019a, FR-019b.

**Setup**: NVDA installed; alternatively use the built-in Windows Narrator.

**Action**:
1. Launch JyGlobalVST. Use **Tab** to traverse the UI without touching the mouse.
2. Confirm every interactive control is reachable; focus indicator is visible at each step.
3. Use keyboard accelerators to: load preset, save preset, bypass a plugin, open a plugin GUI, scan plugins.
4. Enable NVDA. Tab through the UI again; confirm each control's accessible name is announced.
5. Add a plugin to the chain via keyboard. Confirm NVDA announces the change (UIA notification).
6. Toggle bypass via keyboard. Confirm NVDA announces the bypass state change.

**Expected outcome**:
- All primary actions (FR-019a) reachable via keyboard.
- NVDA announces every control by name, role, and value.
- Dynamic changes (plugin added/removed, bypass toggled, device disconnect, CPU warning) raise UI Automation notifications announced by NVDA.

---

## Scenario 13 — Update check (FR-022n, FR-022o)

**Validates**: FR-022n, FR-022o.

**Setup**: A test version-manifest URL configured in `settings.json` that returns a known higher version.

**Action**:
1. Open Help → About → "Check for updates…".
2. Observe network traffic via Fiddler or Wireshark during the click.
3. Confirm the response dialog displays the new version, release notes link, and download link.
4. Click the download link. Confirm a browser opens to the URL — JyGlobalVST does **not** download the file itself.
5. Wait 60 seconds without clicking anything. Confirm **no further network requests** from JyGlobalVST.

**Expected outcome**:
- Exactly one HTTPS GET request observed, fired only on the explicit click (FR-022n).
- No identifying headers (no `User-Agent` beyond the platform default; no `X-` headers).
- No background polling (FR-022n).
- No auto-download (FR-022o).
- Failures (offline, DNS, TLS, 4xx/5xx) appear only as in-session UI notifications; no log on disk.

---

## Scenario 4a — Mixed-driver mode: ASIO output + WASAPI input (FR-015a)

**Validates**: FR-015a, FR-019, AUDIO-001.

**Setup**:
- A machine with at least one ASIO driver installed (e.g., Focusrite USB ASIO, Steinberg Generic Low-Latency ASIO Driver).
- A separate WASAPI input device available (e.g., onboard Realtek microphone input, or a WASAPI loopback device).

**Action**:
1. Launch JyGlobalVST.
2. In the output device selector, choose an **ASIO** device.
3. In the input device selector, choose a **WASAPI** capture device (e.g., "Stereo Mix" or a microphone).
4. Load one VST plugin into the chain.
5. Play audio into the selected WASAPI input.
6. Observe the latency readout and meters.

**Expected outcome**:
- Audio is captured from the WASAPI input, processed by the plugin chain, and output via the ASIO device with no audible dropout.
- The latency readout (FR-019) shows a capture component from WASAPI and an output component from ASIO, with the total round-trip within the AUDIO-001 budget (≤ 10 ms with default buffer).
- Input meters respond to the WASAPI source; output meters respond to the ASIO destination.
- Switching back to pure WASAPI mode (WASAPI output + WASAPI input) works seamlessly without restart.

---

## Scenario 14 — Soak test (AUDIO-003)

**Validates**: AUDIO-003.

**Setup**: A 12-hour audio playlist (e.g., looping FLAC), a 3-plugin chain typical of normal use, no other heavy processes running.

**Action**:
1. Start playback.
2. Leave running for 12 hours.
3. At the end, inspect the in-app `xrun_count_session` counter.

**Expected outcome**:
- `xrun_count_session == 0`.
- No drift (audible or measured) at the end of the 12-hour window.
- No memory growth above the 200 MB ceiling (SC-005).

---

## Validation matrix

| Scenario | Requirements covered | Frequency |
|---|---|---|
| 1. First-launch install | FR-001, FR-004, FR-005, FR-006, FR-012, FR-013, FR-030, FR-031, SC-001 | Every build |
| 2. Chain management | FR-008..FR-011, FR-019 | Every build |
| 3. Preset save/load + placeholder | FR-020, FR-021, FR-022, FR-022a, FR-022f, FR-022g, FR-022g-1, FR-022g-2 | Every build |
| 4. Latency | AUDIO-001, AUDIO-005, FR-019 | Release candidates |
| 4a. Mixed-driver mode | FR-015a, FR-019, AUDIO-001 | Release candidates |
| 5. Device removal | FR-024 | Every build |
| 6. Plugin crash | FR-023 | Every build |
| 7. Sample rate / bit depth | FR-002, FR-002a, FR-014, AUDIO-004 | Every build |
| 8. CPU budget | FR-026, AUDIO-002 | Release candidates |
| 9. Sleep/wake | FR-025 | Release candidates |
| 10. Service mode | FR-028, FR-028a, FR-028b, FR-029 | Release candidates |
| 11. Single-instance | FR-022j | Every build |
| 12. Accessibility | FR-019a, FR-019b | Release candidates |
| 13. Update check | FR-022n, FR-022o | Release candidates |
| 14. Soak | AUDIO-003 | Release candidates |
