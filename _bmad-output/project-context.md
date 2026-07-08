---
project_name: 'GlobalVSTHost'
user_name: 'J-Y'
date: '2026-06-08'
sections_completed:
  ['technology_stack', 'language_specific_rules', 'framework_specific_rules', 'testing_rules', 'code_quality_rules', 'workflow_rules', 'critical_rules']
status: 'complete'
rule_count: 63
optimized_for_llm: true
---

# Project Context for AI Agents

_This file contains critical rules and patterns that AI agents must follow when implementing code in this project. Focus on unobvious details that agents might otherwise miss._

---

## Technology Stack & Versions

- **C++20** — `CMAKE_CXX_STANDARD 20`, `/permissive- /Zc:__cplusplus`
- **CMake 3.24+** — Build system
- **MSVC** — Required for release builds (`/W4 /WX`); non-MSVC compilers emit a warning but are allowed for tooling only
- **Windows 10 1909+ / Windows 11** — x64 only; 32-bit and ARM64 are explicitly out of scope
- **JUCE 8.0.4** — UI framework, VST3 hosting, AudioProcessorGraph, audio device abstraction
- **nlohmann/json v3.11.3** — Presets, settings, scan cache, IPC envelopes
- **GoogleTest v1.14.0** — Unit and integration tests
- **Steinberg ASIO SDK** — Optional ASIO transport support (must contain `common/iasiodrv.h`)
- **Windows Driver Kit (WDK)** — Optional WaveRT virtual driver build

**Build Options:**
- `JYGLOBALVST_BUILD_TESTS=ON` (default)
- `JYGLOBALVST_BUILD_DRIVER=OFF` (default, requires WDK)
- `JYGLOBALVST_BUILD_ASIO=OFF` (default, requires ASIO SDK path via CMake var, env var, or `third_party/asiosdk`)

## Critical Implementation Rules

### Language-Specific Rules (C++)

- MSVC is **required** for release builds. Non-MSVC compilers emit a warning but are allowed for tooling only.
- `/W4 /WX` — warnings treated as **errors** on every audio-thread-reachable target.
- CMake always defines: `WIN32_LEAN_AND_MEAN`, `NOMINMAX`, `UNICODE`, `_UNICODE`, `_CRT_SECURE_NO_WARNINGS`. Do not redefine.
- All code lives in namespace `jyglobalvst` (sub-namespaces: `::engine`, `::shared`, `::shared::json`, etc.).
- File naming is `snake_case.cpp` / `snake_case.h`. Classes use `PascalCase` when interfacing with JUCE.
- The audio callback is **noexcept in practice** — no exceptions may propagate. Mark audio-thread functions `noexcept`.
- `std::atomic` must use **explicit memory orders** (`release`/`acquire`/`relaxed`). Do not default to `seq_cst` on hot paths.
- Types in lock-free structures must satisfy: `std::is_nothrow_move_assignable_v<T> || std::is_trivially_copyable_v<T>`.
- Plugin `processBlock` calls must be wrapped in `__try / __except` (MSVC SEH) with an inner `try / catch(...)`.
- On catchable failure, mark plugin failed and bypass it — **never allocate on the failure path**.
- Engine errors are delivered asynchronously via `IAudioEngineListener`, not thrown.

### Framework-Specific Rules

- **JUCE 8.0.4** provides `AudioProcessorGraph`, VST3 hosting, and `AudioDeviceManager` abstraction.
- In **testable-dev**, `juce::AudioDeviceManager` abstracts WASAPI. In **release**, this is replaced with direct `IAudioClient3` — but UI code must **not branch** on build mode.
- Internal audio processing is **always 32-bit float**. Format conversion and resampling happen **only at boundaries**.
- `juce::MessageManager` dispatches listener notifications to the UI thread. **Never call MessageManager from the audio thread.**
- Plugins run **in-process** for latency. Cross-process IPC would violate the <10 ms budget.
- Plugin state is persisted as **base64-encoded VST3 state chunks** inside `.jvst` preset files (JSON manifest + chunk).
- `PluginUid` is Steinberg's 16-byte TUID (`std::array<std::uint8_t, 16>`).
- **WASAPI** is the default transport; **ASIO** is optional (`JYGLOBALVST_BUILD_ASIO=ON`).
- `setBufferSize` accepts `{128, 256, 512, 1024}` in WASAPI mode and `{64, 128, 256, 512, 1024}` in ASIO mode. Other values throw.
- `setAsioOutputPair` sets the first output channel offset (0-based stereo pair).
- The tray app is a JUCE GUI app. `src/tray-app/main.cpp` gates the GUI target — if missing, CMake skips `jyglobalvst_tray`.
- Plugin editors are hosted in a `juce::DocumentWindow` subclass.

**Data Flow Boundary Pattern:**
```
[capture source] → format convert → resample → AudioProcessorGraph (VST3 chain) → resample → format convert → [hardware output]
```

### Testing Rules

- **GoogleTest v1.14.0** — All tests use `TEST_F` with fixture classes inheriting `::testing::Test`.
- Tests are organized into **6 categories** under `tests/`:
  - `unit/` — DSP utilities, JSON validators, data structures, format converters
  - `integration/` — End-to-end scenarios using `loopback_fixture` (WASAPI loopback or virtual cable as input surrogate)
  - `contract/` — Validation against `IAudioEngine` guarantees
  - `compat/` — Pluginval matrix runner (`tests/compat/run_pluginval.ps1`)
  - `latency/` — Hardware loopback latency harness (**requires physical loopback cable**; run manually)
  - `audit/` — RT-safety static analysis scripts (T106, T107)
- Integration/contract tests use `us1_`, `us2_` prefixes mapping to user stories (e.g., `us1_single_plugin_routing_test.cpp`).
- Unit tests use descriptive names by component (e.g., `json_validator_test.cpp`, `format_convert_test.cpp`).
- Test binaries are placed in `build/tests/Release/` or `build/tests/Debug/`.
- Run all tests: `ctest --test-dir build -C Release --output-on-failure`
- Run specific executable: `.\build\tests\Release\spsc_queue_test.exe`
- **RT-Safety Audit (T106, T107):** Static analysis scripts verify audio-thread-reachable code does not call `malloc`, acquire mutexes, or perform file I/O. New audio-thread code must pass these audits.

### Code Quality & Style Rules

- `.clang-format` is LLVM-based, C++20 standard. Run `clang-format -i` on all changed files before committing.
- **Indent:** 4 spaces, `UseTab: Never`, `TabWidth: 4`
- **Column limit:** 120
- **Braces:** Custom `BreakBeforeBraces` — new lines for classes, functions, control statements, catch/else. Namespaces do NOT get brace wrapping.
- **Short functions:** `AllowShortFunctionsOnASingleLine: Empty` only.
- **Pointers/References:** Left alignment (`int* p`, `int& r`).
- **Include order:** Enforced by `.clang-format`. Order: `"jyglobalvst/..."` → project headers → `<juce_...>` → `<nlohmann/...>` → `<gtest/...>` → system → `<windows.h>` → C stdlib.
- **Code layout:**
  - `src/shared/` — Cross-component static lib
  - `src/audio-engine/` — Audio engine static lib
  - `src/tray-app/` — JUCE GUI app
- **File headers:** Reference task IDs (e.g., `// T011 — IAudioEngine...`). Audio-thread files include a `REALTIME CONSTRAINTS (Constitution §V)` block.

### Development Workflow Rules

- **Configure:** `cmake -B build -A x64` (standard), `-DJYGLOBALVST_BUILD_TESTS=OFF` (no tests), `-DJYGLOBALVST_BUILD_DRIVER=ON` (requires WDK)
- **Build:** `cmake --build build --config Release` or `Debug`. Outputs to `build/bin/` and `build/lib/`.
- **Clean:** `Remove-Item -Recurse -Force build`
- **Format:** `clang-format -i <file>` or bulk via `Get-ChildItem -Recurse -Include *.cpp,*.h,*.hpp | ForEach-Object { clang-format -i $_.FullName }`
- **Test:** `ctest --test-dir build -C Release --output-on-failure` or run individual `.exe` from `build/tests/Release/`
- **Commits:** Conventional commit style observed (e.g., `feat(asio): ...`, `chore(asio): ...`)
- **Branches:** Feature branch style observed (e.g., `feat/asio-support`)

### Critical Don't-Miss Rules

- **Real-time audio thread discipline is ABSOLUTE.** No `malloc`/`new`, no mutex, no file I/O, no logging, no GUI calls inside the audio callback. All UI mutations serialize through the lock-free SPSC queue.
- **`/W4 /WX` warnings-as-errors** on every audio-thread-reachable target. New audio-path code must compile with zero warnings.
- **Plugin `processBlock` must be SEH-wrapped** (`__try`/`__except` + `try`/`catch(...)`). On failure: mark failed, bypass, enqueue notification — **zero allocation on the failure path**.
- **No persistent logging, telemetry, or crash reports.** Errors surface only as in-session UI notifications. The only network activity is an explicit "Check for updates" HTTPS GET.
- **Plugins run in-process only.** Cross-process IPC would violate the <10 ms latency budget.
- **Tray app depends ONLY on `IAudioEngine` contract.** UI code must **not branch** on build mode (testable-dev vs service-mode).
- **State persistence paths:**
  - `%AppData%\Roaming\JyGlobalVST\settings.json` — roaming preferences
  - `%LocalAppData%\JyGlobalVST\` — machine-local state
  - `%UserProfile%\Documents\JyGlobalVST\Presets\*.jvst` — shareable presets
- **Internal processing is always 32-bit float.** Format conversion and resampling happen **only at boundaries**.

---

## Usage Guidelines

**For AI Agents:**

- Read this file before implementing any code in this project.
- Follow ALL rules exactly as documented.
- When in doubt, prefer the more restrictive option.
- Update this file if new patterns emerge during implementation.

**For Humans:**

- Keep this file lean and focused on agent needs.
- Update when the technology stack or architectural patterns change.
- Review quarterly for outdated rules.
- Remove rules that become obvious over time as agents learn them.

Last Updated: 2026-06-08

