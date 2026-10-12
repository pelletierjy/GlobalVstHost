# AGENTS.md — GlobalVstHost (JyGlobalVST)

Windows tray app that captures system audio via WASAPI loopback, routes it through an ordered VST3 + built-in effect chain (AudioProcessorGraph), and plays to a hardware output at sub-10 ms latency. C++20 / CMake / JUCE 8.0.4 / GoogleTest, MSVC x64 only. Also ships as a WiX MSI and Microsoft Store (MSIX) package. Design contracts and background live in `CLAUDE.md` and `specs/001-jyglobalvst-system-host/constitution.md` — read those before touching audio-engine internals.

## Layout

- `src/shared/` — static lib linked everywhere: JSON validators (`json/validators`), SPSC/lock-free queues (`concurrency/`), Win device/endpoint helpers (`platform/`).
- `src/audio-engine/` — engine static lib. Public contract: `include/jyglobalvst/audio_engine.h` (IAudioEngine + listener). `routing/` WASAPI, `chain/` graph + presets, `vst-host/` scan/load + SEH, `builtin-effects/`, `monitoring/`.
- `src/tray-app/` — JUCE GUI (tray, chain editor, meters, settings, presets, updates).
- `src/service/` — Windows Service host + named-pipe IPC (only built with `-DJYGLOBALVST_BUILD_SERVICE=ON`).
- `src/installer/wix/` — WiX v4 MSI (on by default; disable with `-DJYGLOBALVST_BUILD_INSTALLER=OFF`).
- `tests/` — `unit/`, `integration/`, `contract/` (GoogleTest), `compat/` (pluginval), `audit/` (PowerShell gates), `latency/` (manual hardware harness).
- `specs/NNN-feature-name/` — spec-kit feature docs (spec.md, plan.md, tasks.md). `.specify/` holds the templates/scripts. Keep the "Active Feature" block in `CLAUDE.md` current.

## Environment

- Windows 10 1909+/11, x64 only. CMake refuses other platforms and non-64-bit (`-A x64` required).
- Visual Studio 2022+ (MSVC, C++20). Non-MSVC configure warns and is tooling-only.
- Steinberg ASIO SDK is MANDATORY at `third_party/asiosdk/` (dir containing `common/iasiodrv.h`) or via `-DJYGLOBALVST_ASIO_SDK_PATH=...` / env `JYGLOBALVST_ASIO_SDK_PATH`. Configure fails without it. Proprietary license — see `THIRD_PARTY_LICENSES.md`.
- JUCE 8.0.4, nlohmann/json 3.11.3, googletest 1.14.0 are FetchContent'd into `third_party/` — first configure needs network and takes a while.

## Build & test

```
cmake -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

- Faster configure without tests: `cmake -B build -A x64 -DJYGLOBALVST_BUILD_TESTS=OFF`.
- Single test binary: `.\build\tests\Release\<name>_test.exe` (e.g. `spsc_queue_test.exe`). Test working dir is `build/tests`.
- Coverage build: add `-DJYGLOBALVST_ENABLE_COVERAGE=ON` (see `docs/coverage.md`).
- CI gates (`.github/workflows/audio-pr-gate.yml`) also run, in pwsh:
  - `.\tests\audit\rt_audit.ps1`, `.\tests\audit\header_audit.ps1`, `.\tests\audit\no_i18n_audit.ps1` — run these before claiming done on engine/UI changes.
  - `.\tests\compat\run_pluginval.ps1` — plugin validation matrix.
- Hardware latency harness (manual, never in CI): set `$env:JYGLOBALVST_LOOPBACK_IFACE = "<device friendly name>"`, then `.\build\tests\Release\hardware_loopback_runner.exe`. Needs a duplex interface + TRS loopback cable; missing hardware = tests skip, not fail.

## Conventions

- Namespace `jyglobalvst`; 4-space indent, 120-col, braces on new lines. Format with `clang-format -i <file>`; include order is enforced via `IncludeCategories` in `.clang-format` (project `"jyglobalvst/..."` first, then quoted, juce, nlohmann, gtest, system, `windows.h`, C stdlib).
- `/W4 /WX` (warnings are errors) on all targets; `/permissive- /Zc:__cplusplus /utf-8 /MP`. Fix new warnings; suppressions exist only for third-party headers (`/wd5105 /wd4458`).
- Real-time discipline in audio callbacks: no `malloc`/`new`, no locks, no file/IO, no logging, no UI calls. UI→engine mutations go through the lock-free SPSC queue (`src/shared/concurrency/`) and apply at the top of `processBlock`.
- Plugin `processBlock` calls are SEH-wrapped (`src/audio-engine/vst-host/seh_wrapper.*`); on failure the plugin is bypassed and a UI notification is enqueued — never crash the engine.
- The tray app must depend only on `src/audio-engine/include/jyglobalvst/audio_engine.h`, not engine internals (same UI binary works in user-mode and service-mode).
- New test file? Just drop `<name>_test.cpp` into `tests/unit|integration|contract/` — CMake globs `*_test.cpp` and creates one executable per file (latency: `*_runner.cpp`). No CMakeLists edit needed.
- Commit subjects: short imperative, often prefixed `fix:` / `feat:` / `refactor:` / `bump:` — prefer the prefix form.

## Pitfalls

- Configure error about ASIO → the SDK isn't at `third_party/asiosdk`; it is not optional and not fetched automatically.
- First-ever configure clones JUCE/gtest/json into `third_party/` (huge); don't commit that directory's contents.
- CI's soak step runs `jyglobalvst_tray.exe --soak-test --duration-minutes 30`, but no such flag exists in the source — don't rely on that step as a real test.
- Warnings-as-errors: an innocuous sign/unused-parameter warning will fail the whole build on `/WX`.
- The engine has no persistent logging/telemetry by design — errors surface as in-session UI notifications; don't add logging on audio-thread paths or network calls outside the explicit update check.
- Presets/settings paths and schema live in `src/tray-app/settings/` + `src/shared/json/validators`; preset `.jvst` = JSON manifest + base64 VST3 state chunks — migrate via `us3_schema_migration` pattern, don't break old files.
