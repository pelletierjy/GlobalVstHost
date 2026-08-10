# Quickstart & Validation: Built-In Audio Effect Plugins

Validation guide proving the feature end-to-end. Implementation details live in `data-model.md`, `contracts/`, and (later) `tasks.md`.

## Prerequisites

- Windows 10 1909+ / 11, x64; MSVC + CMake; JUCE 8.0.4 fetched by the existing build.
- An input source for testable-dev (WASAPI loopback on the default render device, or a virtual cable) and a hardware output.

## Build & test

```powershell
# Configure (testable-dev default) and build
cmake -B build -A x64
cmake --build build --config Release

# Run the new unit + integration tests
ctest --test-dir build -C Release --output-on-failure -R "builtin|nighttime|eq_processor"

# Or individual binaries
.\build\tests\Release\nighttime_processor_test.exe
.\build\tests\Release\eq_processor_test.exe
.\build\tests\Release\builtin_registry_test.exe
.\build\tests\Release\builtin_effects_chain_test.exe
```

## Scenario 1 — Built-ins appear with no scan (US1/US2, FR-001/002/003)

1. Delete `%LocalAppData%\JyGlobalVST\scan-cache.json` (simulate no scanned plugins) and launch `jyglobalvst_tray`.
2. Open the plugin list ("Add Plugin" → Catalog dialog).
3. **Expected**: "Night-time" and "Equalizer" appear (vendor *JyGlobalVST*), with no scan/download step; optionally shown with a "Built-in" badge.

## Scenario 2 — Night-time levels late-night content (US1, FR-007/008/010/011, SC-002)

1. Add **Night-time** to an empty chain; start audio; select the **Medium** preset in its edit view.
2. Play content alternating quiet dialogue and loud effects.
3. **Expected**: loud passages are pulled down (output never exceeds the ceiling — no clipping), quiet dialogue is raised; overall level feels consistent. Offline unit test asserts the loud/quiet loudness gap shrinks ≥ 50% vs bypass and peak ≤ ceiling.
4. Toggle bypass on the slot → dynamics return immediately, no dropout.

## Scenario 3 — Night-time look-ahead is configurable and latency-aware (FR-008a, AUDIO-001)

1. In Night-time's edit view set **LookAheadMs = 0**; note `LatencyProfile` round-trip ≤ 10 ms.
2. Raise **LookAheadMs**; **Expected**: reported plugin-chain latency increases by the selected amount and is displayed; no dropouts while changing it during playback.

## Scenario 4 — EQ bands + Bass Boost (US2, FR-013/014/015/016, SC-003)

1. Add **EQ** to the chain; open its edit view (10 band sliders + Bass Boost amount + Flat/Reset).
2. Raise one band → that frequency range is boosted (measurable ≥ several dB, others unchanged); lower it → cut.
3. Raise **Bass Boost** → low end increases with no output distortion; set to 0 → removed.
4. Click **Flat/Reset** → all bands 0 dB, Bass Boost 0.

## Scenario 5 — Behaves like any plugin: reorder, presets, autosave (US3, FR-004/005, SC-004)

1. Build a chain of Night-time + EQ (+ any scanned plugin); reorder and bypass slots — identical interactions to scanned plugins.
2. **Save preset** to a `.jvst`; note the parameter values.
3. Restart the app and **load the preset**. **Expected**: both built-ins restored in the same order with identical settings.
4. With built-ins in the chain, close and reopen the app (no explicit preset load). **Expected**: autosave restores both effects and their settings.

## Scenario 6 — Real-time & performance gates (constitution II/V, AUDIO-002/003/004)

- **Soak**: 30 min continuous playback with both effects enabled and parameters being adjusted → zero dropouts.
- **CPU**: ≤ 5% combined during normal playback (in-process counter + Task Manager).
- **Sample rates**: repeat Scenarios 2 & 4 at 44.1 / 48 / 96 kHz → EQ band centers and Night-time timing behave consistently.
- **RT audit**: `processBlock` of both effects passes the T106/T107-style no-alloc/no-lock/no-IO static check.

## Non-goal reminder

Night-time is a **listening-comfort** loudness leveler, not a certified broadcast-loudness (EBU R128 integrated) compliance meter. Success is perceived consistency + a hard ceiling, per SC-002 — not exact LUFS certification.
