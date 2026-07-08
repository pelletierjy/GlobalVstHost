# Quickstart: Validating Driverless System-Audio Capture

Prerequisites for all scenarios below: a Windows 10 1909+/Windows 11 x64 machine, MSVC toolchain,
build configured per repo root `CLAUDE.md` (`cmake -B build -A x64`), and **at least two audio
output devices** available (e.g., onboard speakers/headphone jack + a USB headset/DAC, or two USB
devices). No third-party virtual cable and no signed driver are required for this feature's
default path (FR-002, FR-015).

```powershell
cmake -B build -A x64
cmake --build build --config Debug
```

## 0. Research spike — run before trusting the mute UX (research.md R1)

Before relying on FR-018's mute-based UX in manual testing below, confirm on your test hardware
whether muting a render endpoint's volume also silences its loopback capture:

1. Start loopback capture (via a debug harness or the app itself) on Device A while Device A is
   playing audio.
2. Call the mute path (once implemented) or `IAudioEndpointVolume::SetMute(TRUE)` directly on
   Device A.
3. Observe whether the loopback capture stream goes silent.
4. Repeat on a second, differently-driven device (e.g., onboard HD Audio vs. a USB DAC) — results
   may differ by hardware/driver.

Record the result. If loopback goes silent on your representative hardware, the app should be
exercising FR-018's fallback path (require a non-listened capture endpoint) for the remaining
scenarios instead of the mute UX.

## 1. User Story 1 — Core happy path (P1)

**Goal**: Hear processed system audio on a second device with no install, no admin rights.

1. Launch the app as a standard (non-administrator) Windows user.
2. In device selection, choose Device A (currently playing/default) as the **capture source** and
   Device B (distinct) as the **output device**.
3. Start processing.
4. Play audio from any application (browser, media player).
5. **Expected**: audio is audible on Device B, processed by the plugin chain; no elevation prompt
   occurred; no driver install occurred.
6. While audio is playing, add/remove/reorder a plugin in the chain.
7. **Expected**: the change is audible live, without stopping audio (existing chain behavior,
   unaffected by this feature).

**Pass criteria**: matches spec.md User Story 1 Acceptance Scenarios 1–3; SC-001 (under 2 minutes
launch-to-audible, no install/elevation).

## 2. User Story 2 — Same-device hard block (P2)

1. Attempt to select Device A as both capture source and output device.
2. **Expected**: the app hard-blocks starting (or the selection itself), with an explanation that
   capture and output must differ. No override control is offered (FR-005, SC-002).
3. With processing active on distinct devices A (capture) and B (output), and capture source set
   to "follow system default," change the Windows default playback device to B (i.e., make it
   equal to the output device).
4. **Expected**: the app detects the resulting conflict, pauses processing, and shows a clear
   message (FR-014) — no feedback/double-audio occurs.

**Pass criteria**: matches spec.md User Story 2 Acceptance Scenarios 1–2; zero feedback/double-audio
occurrences (SC-002).

## 3. User Story 3 — Device hot-plug recovery (P2)

1. With processing active, physically unplug (or disable) the output device.
2. **Expected**: the app detects the loss, stops rendering safely, and prompts to choose another
   output — no crash (FR-008).
3. With capture source set to "system default," change the Windows default playback device.
4. **Expected**: the app follows the new default and continues capturing, or clearly indicates the
   source changed (FR-009).
5. Reconnect the device removed in step 1.
6. **Expected**: the app can resume using it without restarting the application (FR-010).

**Pass criteria**: matches spec.md User Story 3 Acceptance Scenarios 1–3; SC-003 (recovery or clear
prompt in 100% of trials, zero crashes, zero restarts required).

## 4. User Story 4 — No cable, no driver, on the default path (P3)

1. On a clean machine (no third-party virtual cable installed) with the driver build disabled
   (default `cmake -B build -A x64`, i.e. `JYGLOBALVST_BUILD_DRIVER` not set), build and run the
   app.
2. **Expected**: the device-selection UI offers real render endpoints for loopback capture, with no
   reference to an external virtual cable (FR-015; Acceptance Scenario 1).
3. Confirm via Task Manager / Device Manager that no kernel driver was installed or loaded for the
   capture path (Acceptance Scenario 2).

**Pass criteria**: SC-004 (end-to-end flow succeeds with cable/driver both absent).

## 5. Automated test entry points

Once implemented, the following should exist and pass (naming mirrors plan.md's Project Structure):

```powershell
# Unit
.\build\tests\Debug\same_device_guard_test.exe
.\build\tests\Debug\endpoint_volume_test.exe

# Integration (extends tests/integration/loopback_fixture.h)
.\build\tests\Debug\us1_loopback_to_output_test.exe
.\build\tests\Debug\us2_same_device_hard_block_test.exe
.\build\tests\Debug\us3_device_hotplug_recovery_test.exe

# Full suite
ctest --test-dir build -C Debug --output-on-failure
```

## 6. Latency and CPU measurement (AUDIO-001 / AUDIO-002 — required even though the latency gate itself is deferred)

Per spec.md AUDIO-001 and this plan's Complexity Tracking, the ≤10 ms constitutional latency target
is **not gated** for this feature by explicit user instruction, but the actual figure MUST still be
measured and documented, not skipped:

1. Query `IAudioEngine::latencyProfile()` while processing is active on a representative two-device
   configuration.
2. Record `total_round_trip_ms` and the per-stage breakdown.
3. Record CPU usage (Task Manager + in-process counters) during a 30-minute soak with audio
   playing, across at least one sample-rate-mismatched capture/output pair (AUDIO-003, AUDIO-004).
4. File the results against this feature before considering it done, so the deferred constitutional
   decision (plan.md Complexity Tracking) has real numbers to be revisited against later.
