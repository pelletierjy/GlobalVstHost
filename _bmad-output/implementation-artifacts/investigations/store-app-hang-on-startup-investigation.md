# Investigation: MS Store build fails to start (hang during/after plugin scan)

## Hand-off Brief

1. **What happened.** The Store build (1.3.5.0) does not crash — it **freezes the UI thread during startup**: `engine_->start()` runs on the message thread via `MessageManager::callAsync`, the remembered WASAPI capture endpoint fails to open, and the ASIO output open for device **"USB IV"** then blocks **~12 seconds** before failing ("Device didn't start correctly"), during which the window is unresponsive (timer ticks stop). Windows records a MoAppHang and closes the app — observed live in `startup.log` and matching the WER AppHang of 2026-10-31 23:45.
2. **Where the case stands.** Root-cause mechanism Confirmed from app logs + code. The plugin scan (user's suspect) is exonerated: it runs on a background thread and completes normally. A second, related failure — a 37-minute "audio callback stalled >2000 ms, requesting engine restart" loop ending in the 23:45 hang — points at the same blocking device-open/stop paths.
3. **What's needed next.** Move device open/stop (ASIO open in particular) off the UI thread or bound it with a timeout; the fix direction is spelled out below. Verification: relaunch with the 'USB IV' ASIO output remembered and confirm the timer ticks never gap >5 s and no MoAppHang is recorded.

## Case Info

| Field            | Value                                                                                             |
| ---------------- | -------------------------------------------------------------------------------------------------- |
| Ticket           | N/A                                                                                                 |
| Date opened      | 2026-10-31                                                                                          |
| Status           | Active (root cause diagnosed; fix out of scope per investigation discipline)                       |
| System           | Windows 11 (10.0.26300), x64; Store package `JYPSolutions.GlobalVSTHost_1.3.5.0_x64__5dwj5njb3tf3w`; hardware: ASIO device "USB IV", several USB ASIO drivers (FiiO, Matrix, Art), a WASAPI capture endpoint `{0.0.0.00000000}.{f60fa5f1-…}` that fails to open |
| Evidence sources | Application event log, WER ReportArchive, app's own `startup.log` (53 MB, `%LocalAppData%\JyGlobalVST\startup.log`), package LocalState/LocalCache, source code, git history |

## Problem Statement

User report: the app installed from the Microsoft Store fails to start; "it seems to crash while scanning the plugins, or after." Registered as Hypothesis #1. **Evidence contradicts the "crash" part** (no event-1000 records for the Store build in October) but **supports the perceived timing**: the ScanDialog is shown before the freeze, so the user watches a "Scanning plugins..." dialog on a window that then goes unresponsive and gets closed.

## Evidence Inventory

| Source                              | Status      | Notes                                                                                       |
| ----------------------------------- | ----------- | ------------------------------------------------------------------------------------------- |
| Application event log               | Available   | MoAppHang 1001/1002 for 1.3.5.0 at 2026-10-31 23:45:13-14 local; MoAppHang for 1.3.3.0 at 2026-10-16; RADAR_PRE_LEAK_64 for 1.3.3.0 at 2026-10-18; no crash (1000) events for the Store build in October |
| WER ReportArchive (packaged app)    | Available   | `AppHang_JYPSolutions.Glo_fea66cea…` (10/31) Hang Signature df89; `Critical_JYPSolutions.Glo_…` (10/16) access-denied (admin only) |
| WER ReportArchive (old dev builds)  | Available   | Aug 2026, v0.1.0.0 from `D:\Repos\Others\GlobalVstHost`: c0000005/c000041d, fault offset 0x114913, multiple VST3s mapped — separate, older defect |
| **App `startup.log`**               | Available   | `%LocalAppData%\JyGlobalVST\startup.log`, 53 MB / 545,250 lines, UTC timestamps — per-launch breadcrumbs incl. timer-tick gaps |
| Package state                       | Available   | `LocalCache\Local\JyGlobalVST\autosave.json` (ASIO output "USB IV", audio_running, chain); `getenv("LOCALAPPDATA")` is NOT virtualized → startup.log lives in the real `%LocalAppData%` while known-folder paths virtualize into LocalCache |
| Source code                         | Available   | `src/tray-app/main.cpp`, `ui/main_window.cpp`, `ui/scan_dialog.*`, `audio_engine_impl.cpp`, `vst-host/*` |
| Crash dumps                         | Missing     | None for the Store build — not needed; logs localize the freeze                              |

## Investigation Backlog

| # | Path to Explore                                                                  | Priority | Status  | Notes |
| - | --------------------------------------------------------------------------------- | -------- | ------- | ----- |
| 1 | Trace tray-app startup path                                                       | High     | Done    | Full 22-step sequence reconstructed; see Source Code Trace |
| 2 | scan/scan_cache threading + in-process load                                       | High     | Done    | Scan = background `std::thread`, SEH-guarded loads, completes normally — exonerated |
| 3 | Read the 12-second gap in `startup.log` around engine start                       | High     | Done    | ASIO open 'USB IV' blocks UI thread 12 s after WASAPI capture open fails |
| 4 | Correlate the 23:45 MoAppHang with `startup.log`                                  | High     | Done    | Session launched 19:55/20:07 local; from 23:11 a 5-second "audio callback stalled → restart" loop runs 37 min; hang recorded 23:45:13; storm ends 23:47:56 |
| 5 | Admin-read `Critical_JYPSolutions.Glo_…` WER report (10/16 hang)                  | Low      | Open    | Would only corroborate; mechanism already identified |
| 6 | Confirm why the restart loop never recovers (restart posting vs control_mutex_)   | Medium   | Open    | Needed for the secondary failure mode, not for the startup freeze |

## Timeline of Events

| Time (local)              | Event                                                                                     | Source                         | Confidence  |
| ------------------------- | ------------------------------------------------------------------------------------------ | ------------------------------ | ----------- |
| 2026-08-05 20:23          | Old dev build v0.1.0.0 crashes (c0000005/c000041d, offset 0x114913) with 6+ VST3s mapped   | WER Report.wer                 | Confirmed   |
| 2026-10-16 19:28          | Store 1.3.3.0 MoAppHang                                                                    | Application event 1001/1002    | Confirmed   |
| 2026-10-18 23:28          | Store 1.3.3.0 RADAR_PRE_LEAK_64 (heap growth)                                              | Application event 1001         | Confirmed   |
| 2026-10-31 19:55 / 20:07  | Store 1.3.5.0 launched twice in 12 min (log lines 505767, 505880)                          | startup.log                    | Confirmed   |
| 2026-10-31 23:11 → 23:47  | Engine wedged: `energySaver: audio callback stalled >2000 ms, requesting engine restart` every ~5 s, 37 min, no recovery | startup.log lines ~508000-509728 | Confirmed |
| 2026-10-31 23:45:13-14    | UI thread unresponsive → WER AppHang (bucket 1210317255720304010), Windows closes the app  | Application event 1001/1002 + WER report (storm lines 04:45:10/04:45:15 UTC straddle it) | Confirmed |
| 2026-11-01 18:19:29 local | Launch: constructor OK → scan started → `callAsync: starting audio` → capture open FAILED → **ASIO 'USB IV' open fails after 12 s**; timerCallback #1 18:19:34 → #2 18:19:45 (UI frozen ≥7 s) | startup.log (lines 545156-545207) | Confirmed |
| 2026-11-01 18:52:11 local | Same pattern: timerCallback #1 at 18:52:14, #2 at 18:52:26 — **UI frozen 12 s**            | startup.log (lines 545208-545250) | Confirmed |

## Confirmed Findings

### Finding 1: The Store build never crashed in October — it hangs

**Evidence:** Application event 1002 at 2026-10-31 23:45:14 local ("stopped interacting with Windows and was closed") + event 1001 MoAppHang; WER `AppHang_JYPSolutions.Glo_fea66cea…` for the exact WindowsApps path. No event-1000 records for the Store build in October.

**Detail:** The user-visible "crash while scanning" is a frozen window that Windows terminates.

### Finding 2: Startup freezes the UI thread for 3–12 s inside `engine_->start()` — ASIO open blocks

**Evidence:** `startup.log` (UTC): launch 23:52:14 (log time) — `timerCallback #1`, `callAsync: starting audio`, `Engine start() called`, `openWasapiCapture(): WasapiCapture::open() FAILED`, then **no log output at all until 23:52:26** `ASIO open failed for 'USB IV': Device didn't start correctly — falling back to WASAPI`, immediately followed by `timerCallback #2`. The 10 Hz UI timer produced zero ticks for 12 s ⇒ the message loop was blocked inside `start()`. Same pattern (≥7 s) in the 18:19 launch and sub-threshold (3–4 s) in healthy launches.

**Detail:** `engine_->start()` is posted to the message thread by `MessageManager::callAsync` (`src/tray-app/ui/main_window.cpp:3239-3241`), queued during `initialise()` — so it typically runs before/interleaved with first paint. The restored session uses ASIO output "USB IV" (`autosave.json: output_endpoint_id: "USB IV"`), and the remembered WASAPI capture endpoint fails to open first. The ASIO driver's failed init takes ~12 s — all of it on the UI thread. Windows' not-responding threshold is ~5 s: the freeze exceeds it → MoAppHang → app closed → "fails to start".

### Finding 3: The plugin scan runs in the background and completes normally — it is not the cause

**Evidence:** Scan starts at the end of the MainWindow constructor (`src/tray-app/ui/main_window.cpp:1481-1485`) and spawns a background `std::thread` (`src/audio-engine/vst-host/plugin_scanner.cpp:132`); per-plugin loads are SEH-guarded (`plugin_scanner.cpp:201`). `startup.log` shows `onScanStarted` → `onScanFinished` ~10-17 s later with the UI timer running in between. The scan dialog is merely *visible* while the separate `start()` freeze happens — which is why the user attributes the failure to scanning.

### Finding 4: A second, related failure mode — engine restart loop that never recovers

**Evidence:** `startup.log` lines ~508000-509728: `energySaver: audio callback stalled >2000 ms, requesting engine restart` every ~5 s, continuously from 23:11:02 to 23:47:56 local on the hang night (37 minutes), with the MoAppHang recorded at 23:45:13 in the middle of it.

**Detail:** The audio callback stopped running and the watchdog's restart requests never fixed it; the session ended in the recorded UI hang. Same root family as Finding 2: blocking device open/stop operations and their interaction with the message thread / `control_mutex_`.

### Finding 5: Historic dev-build crash with VST3s mapped (separate, older defect)

**Evidence:** WER `AppCrash_JyGlobalVST.exe_*` (2026-08-05): c0000005 / c000041d at identical offset 0x114913, VST3 modules loaded (ARC X, ARC 4, AmpliTube 5, Guitar Rig 6, RoughRider3, TONEX).

**Detail:** Old v0.1.0.0 build from a different clone. Not the current symptom; kept as a known-issues note.

## Deduced Conclusions

### Deduction 1: User's premise decoded

**Based on:** Findings 1-3.

**Reasoning:** The ScanDialog appears ~3 s into the constructor, immediately before the `callAsync` start freeze. The user sees a scan dialog on an unresponsive window that Windows closes → "crashes while scanning or after".

**Conclusion:** Timing perception correct; mechanism wrong. The freeze is device-open on the UI thread, not the scan.

### Deduction 2: MSIX packaging is not the cause but changes observable behavior

**Based on:** known-folder vs env-var path divergence observed on disk.

**Reasoning:** Known-folder paths (`shared::localAppData()` etc.) virtualize into `Packages\…\LocalCache\Local\JyGlobalVST` (autosave.json lives there), while `StartupLog` uses `getenv("LOCALAPPDATA")` (main_window.cpp:40) which is not virtualized → writes to the real `%LocalAppData%\JyGlobalVST\startup.log`. Also `defaultVst3ScanPaths()` uses `shared::localAppData()` (default_scan_paths.cpp:41-44), so the per-user VST3 folder resolves to the package-virtualized copy under MSIX — per-user plugins are invisible to the Store build.

**Conclusion:** No link to the hang; two Store-specific side findings (split state dirs; per-user VST3 scan path misses real user plugins).

## Hypothesized Paths

### Hypothesis 1: App crashes during plugin scan or right after (user's claim)

**Status:** Refuted for the current Store build (October evidence)

**Theory:** In-process VST3 loading during scan faults the process.

**Supporting indicators:** Old dev builds did crash with VST3s loaded (Finding 5).

**Would confirm:** Crash (event 1000) or dump for 1.3.5.0 with scan code in the stack.

**Refuting evidence:** No event-1000 records in October; `startup.log` shows every observed scan reaching `onScanFinished`; the recorded failure is a hang (1001/1002), and the UI-tick gaps localize the block in `engine_->start()`, not the scan thread.

**Resolution:** Refuted 2026-11-01 — see Findings 1-3. Historic crash (Finding 5) filed as a separate old defect.

### Hypothesis 2: UI thread blocked by startup work (scan/cache/WebView2 init)

**Status:** Confirmed (mechanism) — narrowed from the original candidate list

**Theory:** Blocking startup work on the message-loop thread.

**Resolution:** Confirmed as `engine_->start()` on the message thread (callAsync) → 12 s ASIO open failure on 'USB IV' after WASAPI capture open failure (Finding 2). Scan, scan-cache load, settings I/O and WebView2 were ruled out or demoted: scan is threaded (Finding 3), WebView2/assistant/update are lazy (constructor never calls them), cache/settings loads total <100 ms in observed logs.

### Hypothesis 3: MSIX packaging changes behavior causing the block

**Status:** Refuted as cause, retained as side finding

**Resolution:** The identical 12 s freeze pattern is device-driver-driven and reproducible on any build that restores the ASIO output; packaging only relocates state (Deduction 2).

## Missing Evidence

| Gap                                            | Impact                                                     | How to Obtain                                  |
| ---------------------------------------------- | ---------------------------------------------------------- | ----------------------------------------------- |
| Exact UI-thread stack at the 23:45 hang        | Would name the precise blocking API (ASIOStart vs IAudioClient) | Attach debugger during a repro; enable LocalDumps |
| Why the 5 s restart loop never recovers        | Secondary failure mode root cause                          | Read energySaver watchdog + start/stop code path (Backlog #6) |
| 10/16 Critical WER report                      | Corroboration only                                          | Elevated PowerShell re-read                     |

## Source Code Trace

| Element       | Detail                                                                                                                                                                                                      |
| ------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Error origin  | `AudioEngineImpl::start()` (WASAPI capture open → ASIO open → fallback) executed on the JUCE message thread                                                                                                  |
| Trigger       | Autosaved session with ASIO output "USB IV" + `audio_running=true` → constructor `restoreFromAutosave` posts `MessageManager::callAsync(engine_->start())` (`src/tray-app/ui/main_window.cpp:3239-3241`)      |
| Condition     | Remembered WASAPI capture endpoint fails to open (device absent/busy), engine proceeds to ASIO open; the ASIO driver for "USB IV" blocks ~12 s then returns "Device didn't start correctly" → fallback WASAPI |
| Blocking also in | `AudioEngineImpl` ctor `device_manager_.initialiseWithDefaultDevices(2,2)` (`audio_engine_impl.cpp:236`) and sync `scan_cache_->load()` (`:237`) — both on the message thread before window creation; `restoreChain` holds `control_mutex_` while loading VST3s (`:1918-1924`); `~MainWindow → cancelScan → thread_.join()` can block quit on a hung plugin (`plugin_scanner.cpp:140-147`) |
| Related files | `src/tray-app/ui/main_window.cpp` (constructor 1295-1486, callAsync 3239-3251, timer 3267-3304), `src/audio-engine/routing/audio_engine_impl.cpp` (ctor 231-238, start 246-430, rescan 1021-1026, restoreChain 1890-1931), `src/tray-app/presets/autosave.cpp`, `src/audio-engine/vst-host/plugin_scanner.cpp` |

## Conclusion

**Confidence:** High for the startup-freeze mechanism (Confirmed in logs + code, deterministic pattern across launches); Medium for the exact blocking API inside the ASIO driver and for the full 23:45 hang-night chain (Deduced from the 37-minute restart storm straddling the WER event).

Confirmed: the Store app **hangs**, it does not crash; the freeze is the UI thread blocked in `engine_->start()` — a failed WASAPI capture open followed by a ~12 s failing ASIO open on device "USB IV" — lasting well past Windows' 5 s not-responding threshold, so Windows closes the app. The plugin scan the user blamed runs on a background thread and completes normally; its dialog is simply the thing on screen while the freeze happens. A second, related mode — a 37-minute audio-callback stall/restart loop ending in the 23:45 MoAppHang — implicates the same blocking device paths. The Aug dev-build crash with VST3s loaded is a separate, older defect.

## Recommended Next Steps

### Fix direction

1. **Primary — never block the message thread on device open:** move `engine_->start()` device open (WASAPI capture + ASIO init) off the UI thread (worker + completion callback), or bound the ASIO open with a hard timeout and fail over to WASAPI without waiting 12 s. This removes the >5 s unresponsive window. *(Mechanism: blocking I/O on UI thread.)*
2. **Secondary — the restart loop:** audit the energySaver watchdog restart path (`audio callback stalled >2000 ms`) — 37 min of 5 s restart requests never recovered, suggesting the restart is itself queued behind the same blocked thread/`control_mutex_`. *(Mechanism: livelock behind a blocked control path.)*
3. **Hardening (lower priority):** gate the unconditional every-launch rescan (`main_window.cpp:1481-1485`) on cache age; avoid loading the full chain on the message thread; guard `~MainWindow`'s scan-thread join with a timeout; unify path resolution (getenv vs known folders) so Store state and `startup.log` land in one place, and make the per-user VST3 scan path MSIX-aware.

### Diagnostic

If the freeze recurs after (1): enable LocalDumps or attach a debugger at the not-responding point and capture the UI-thread stack; read the 10/16 `Critical_JYPSolutions` WER report from an elevated shell.

## Reproduction Plan

1. Connect/state the machine as observed: ASIO output named "USB IV", remembered capture endpoint unavailable (unplugged mic), `autosave.json` with `audio_running: true`.
2. Launch the Store app.
3. Expected: ScanDialog appears, then the main window stops repainting for ~12 s (`timerCallback` gaps in `startup.log`, `ASIO open failed for 'USB IV'` after the gap); if other startup blocking pushes total >5 s before first paint, Windows logs MoAppHang 1001/1002 and closes the app.
4. Negative control: delete/mismatch the output endpoint (pure WASAPI restore) → no multi-second tick gap.

## Side Findings

- **`startup.log` is 53 MB / 545k lines** of persistent per-launch logging — contradicting the constitution/AGENTS.md rule "no persistent logging/telemetry"; the energySaver watchdog alone appends one line per 5 s when wedged, and `drift-SRC` stats append every second while running. Consider compile-out or ring-buffering for release.
- **Path-split under MSIX:** `StartupLog` uses `getenv("LOCALAPPDATA")` (unvirtualized, `main_window.cpp:40`) while `LocalStateStore`/`AutoSaveStore`/scan paths use known folders (virtualized) → Store installs write to two different `JyGlobalVST` directories.
- **Per-user VST3 folder invisible to Store build:** `defaultVst3ScanPaths()` resolves `%LocalAppData%\Programs\Common\VST3` through the virtualized known folder (`default_scan_paths.cpp:41-44`), so real per-user plugins are not scanned under MSIX.
- `preset_folder_scanner.cpp` / `preset_folder_init.cpp` appear to have no callers (possible dead code).
- CI's soak step invokes `--soak-test` (exists in source) but no test covers a failing-slowly ASIO device at startup — the entire failure class is invisible to the test suite.

## Follow-up: 2026-11-01

### New Evidence

- App's own `startup.log` found at `%LocalAppData%\JyGlobalVST\startup.log` (env-var path bypasses MSIX virtualization); full per-launch breadcrumbs.
- Live reproduction of the 12 s UI freeze in today's 18:19 and 18:52 launches (timer-tick gaps + 12 s hole between `start() called` and `ASIO open failed`).

### Additional Findings

- Findings 2, 3, 4 above; MSIX side findings (split state dirs, virtualized per-user scan path).

### Updated Hypotheses

- H1 (crash during scan): **Refuted** for current build; historic variant retained.
- H2 (UI-thread block at startup): **Confirmed**, mechanism identified.
- H3 (MSIX cause): **Refuted** as cause; side findings recorded.

### Backlog Changes

- #1-#4 Done; #5 lowered (corroboration only); #6 opened (restart-loop recovery).

### Updated Conclusion

Root cause of "fails to start": UI thread blocked 3-12 s in `engine_->start()` (failed WASAPI capture open + ~12 s failing ASIO open on "USB IV") exceeding Windows' 5 s not-responding threshold; scan dialog on screen during the freeze explains the user's attribution. Confidence High.

## Follow-up: 2026-11-01 (#2)

### New Evidence

- **Live capture of the frozen instance (PID 36772):** `Responding=False`, main window "Scanning VST3 Plugins", 2070 s CPU. cdb attach thread stacks: **UI thread (0) blocked in `MSVCP140!_Thrd_join` ← `AudioEngineImpl::stop()`** (`audio_engine_impl.cpp:464 scanner_->waitUntilFinished()`), which the log's last line (`stop() called`, no `completed`) corroborates; threads 22-27 = AmpliTube 5 VST3 helper threads spawned by the scan — the scan thread was wedged inside the plugin's in-process initialization.
- This unifies the whole case: energy-saver/mute calls `stop()` early in the session → old `stop()` joined the scan thread on the message thread → frozen for the scan's duration (a hung VST3 = forever). Also explains the 23:45 MoAppHang night (37-min restart-storm = watchdog posting restarts to a message thread stuck in the same join).
- User's "freezes halfway rendered until the scan completes" = exactly this join; scan itself already runs on its own thread and ScanDialog callbacks are lock/atomic-marshaled to the UI timer.

### Additional Findings

- The spinner gap: `busy_spinner_` cleared on the first timer tick (~100 ms), so it did not cover the scan; status label "Scanning plugins..." → "Ready" did.
- `US1EmptyChainPassthroughTest.LatencyMeasurementStable` fails on this machine **with and without the fix** (stash-verified): pre-existing, device-dependent (1.46 ms round-trip jitter vs 1 ms tolerance).
- Build note: each test target recompiles the full JUCE module set (~15 heavy TUs × 55 targets) — full local builds are very slow; build targeted test exes when iterating.

### Updated Hypotheses

- H2 (UI-thread block at startup): **Confirmed & root-caused to the scan join in `stop()`** (supersedes the earlier ASIO-primary reading: the 12 s ASIO block is real but bounded; the join is the unbounded one that matches "frozen until the scan completes").

### Backlog Changes

- #6 (restart-loop recovery): resolved by the same fix — the storm was restart requests queued behind `stop()`'s join holding `control_mutex_`.
- New backlog: ASIO open still blocks the UI thread ~12 s at startup (fix direction #1 remains open); quit-path `cancelScan` now bounded but a hung scan still leaves the scan dialog stuck open until closed.

### Resolution (implementation)

Committed `b32db67` on `chore/version-1.3.6.0` (on top of the 1.3.6.0 version bump):

- `stop()` cancels the scan but never waits for it.
- All scan joins bounded by `PluginScanner::kScanJoinTimeout` (2 s); second `start()` refused while one runs; listener callbacks read through an atomic so `detachRunningScan()` can sever them; exit published under `thread_mutex_` + condition-variable notify.
- `cancelScan()`/`~AudioEngineImpl`: bounded wait → detach; teardown leaks scanner + scan cache instead of freeing memory a detached thread still writes through.
- Main-view spinner stays active until the initial scan finishes.
- Tests: `tests/unit/plugin_scanner_wait_test.cpp` (2), `tests/contract/stop_scan_independence_test.cpp` (1, fails pre-fix by design); audits rt/header/no_i18n PASS; suite 302/303 (1 pre-existing device-dependent failure, stash-verified).

### Updated Conclusion

**Confidence: High.** Root cause Confirmed with a live debugger stack + the app's own logs; fix implemented, tested (including a regression test that fails on the old code), audited, and committed. Remaining known issue: ASIO open still blocks the UI thread ~12 s on startup with the failing "USB IV" device.