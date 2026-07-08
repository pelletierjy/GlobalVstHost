# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

<!-- SPECKIT START -->
For additional context about technologies to be used, project structure,
shell commands, and other important information, read the current plan:

**Active Feature: Built-In Audio Effect Plugins — Night-time & EQ + Bass Boost (006)**

- Current plan: [specs/006-builtin-plugins/plan.md](specs/006-builtin-plugins/plan.md)
- Spec: [specs/006-builtin-plugins/spec.md](specs/006-builtin-plugins/spec.md)
- Research: [specs/006-builtin-plugins/research.md](specs/006-builtin-plugins/research.md)
- Data model: [specs/006-builtin-plugins/data-model.md](specs/006-builtin-plugins/data-model.md)
- Contracts: [specs/006-builtin-plugins/contracts/](specs/006-builtin-plugins/contracts/)
- Quickstart: [specs/006-builtin-plugins/quickstart.md](specs/006-builtin-plugins/quickstart.md)

**Prior Feature: Driverless Audio Capture (005)**
- Spec: [specs/005-driverless-audio-capture/spec.md](specs/005-driverless-audio-capture/spec.md)
- Research: [specs/005-driverless-audio-capture/research.md](specs/005-driverless-audio-capture/research.md)
- Data model: [specs/005-driverless-audio-capture/data-model.md](specs/005-driverless-audio-capture/data-model.md)

**System Host (Root Spec)**:
- Plan: [specs/001-jyglobalvst-system-host/plan.md](specs/001-jyglobalvst-system-host/plan.md)
- Constitution: [specs/001-jyglobalvst-system-host/constitution.md](specs/001-jyglobalvst-system-host/constitution.md)
<!-- SPECKIT END -->

## Project Overview

JyGlobalVST is a Windows desktop application that captures system audio via WASAPI loopback, routes it through an ordered VST3 plugin chain, and emits processed audio to a selected hardware output endpoint with sub-10 ms latency. It is built in C++20 using CMake, JUCE 8.x, and GoogleTest.

**Target platform**: Windows 10 1909+ and Windows 11 (x64 only). 32-bit and ARM64 are out of scope.

## Build System

CMake is the build system. MSVC is required; non-MSVC compilers emit a warning but are allowed for tooling only.

### Configure

```powershell
# Standard configuration (default)
cmake -B build -A x64

# With tests disabled (faster configure when you only need the tray app)
cmake -B build -A x64 -DJYGLOBALVST_BUILD_TESTS=OFF
```

### Build

```powershell
cmake --build build --config Release
cmake --build build --config Debug
```

Outputs go to `build/bin/` and `build/lib/`.

### Clean

```powershell
Remove-Item -Recurse -Force build
```

## Testing

Tests are built automatically when `JYGLOBALVST_BUILD_TESTS=ON` (the default). They use GoogleTest and are discovered by CMake.

### Run all tests

```powershell
ctest --test-dir build -C Release --output-on-failure
```

### Run a specific test executable

Test binaries are placed in `build/tests/`:

```powershell
# Example: run only the SPSC queue unit tests
.\build\tests\Release\spsc_queue_test.exe

# Example: run only integration tests matching a pattern
.\build\tests\Release\us1_single_plugin_routing_test.exe
```

### Test categories

- `tests/unit/` — GoogleTest: DSP utilities, JSON validators, data structures, format converters.
- `tests/integration/` — End-to-end scenarios using a loopback fixture (WASAPI loopback or virtual cable as input surrogate).
- `tests/contract/` — Contract validation against `IAudioEngine` guarantees.
- `tests/compat/` — Pluginval matrix runner (`tests/compat/run_pluginval.ps1`).
- `tests/latency/` — Hardware loopback latency harness (requires physical loopback cable; run manually).
- `tests/audit/` — RT-safety static analysis scripts (T106, T107).

## Code Style

Format with clang-format:

```powershell
# Format a specific file
clang-format -i src/audio-engine/routing/audio_engine_impl.cpp

# Format all C++ files (from repo root)
Get-ChildItem -Recurse -Include *.cpp,*.h,*.hpp | ForEach-Object { clang-format -i $_.FullName }
```

Style summary: namespace `jyglobalvst`, 4-space indent, 120-column limit, braces on new lines for classes/functions/control-statements. See `.clang-format` for the full rules. Include order is enforced via `IncludeCategories` in `.clang-format`:
1. `"jyglobalvst/..."` headers
2. Other quoted project headers
3. `<juce_...>` headers
4. `<nlohmann/...>` headers
5. `<gtest/...>` headers
6. Other angle-bracket headers
7. `<windows.h>`
8. C standard library headers

## Architecture

### Component Layout

```
src/
├── shared/          # Cross-component static lib: JSON validators, platform helpers,
│                    # concurrency primitives (SPSC queue), IPC wire framing.
│                    # Linked into audio_engine, tray_app, and (future) service.
├── audio-engine/    # Static lib: real-time audio routing + VST3 hosting.
│   ├── include/jyglobalvst/audio_engine.h   # IAudioEngine + IAudioEngineListener contract
│   ├── routing/     # WASAPI capture/output, format conversion, resampling
│   ├── chain/       # AudioProcessorGraph wrapper, live chain mutations
│   ├── vst-host/    # Plugin scan, load, SEH wrapping, state chunks
│   └── monitoring/  # CPU meter, latency probe, xrun counter
└── tray-app/        # JUCE GUI app: system-tray UI, chain editor, presets, settings.
                     # In user-mode (testable-dev), hosts the audio engine in-process.
                     # In release service-mode, acts as IPC client to the Windows Service.
```

### Key Design Principles

1. **IAudioEngine contract decouples UI from engine**. The tray app depends only on `src/audio-engine/include/jyglobalvst/audio_engine.h`, not on engine implementation details. The same UI binary works in both user-mode (engine in-process) and service-mode (engine in a Windows Service, communicating via named-pipe IPC).

2. **Audio thread real-time discipline is absolute**. No `malloc`/`new`, no mutex acquisition, no file I/O, no logging, no GUI calls inside the audio callback. All UI-to-engine mutations serialize through a lock-free SPSC command queue (`src/shared/concurrency/spsc_queue.h`) and are applied at the top of the next `processBlock`.

3. **Plugin isolation via SEH**. Each plugin's `processBlock` is wrapped in `__try / __except` plus an inner `try / catch(...)`. On catchable failure, the plugin is marked failed, audio bypasses it, and a UI notification is enqueued via the lock-free queue. No allocation on the failure path.

4. **In-process VST3 hosting**. Plugins run in-process for latency reasons (cross-process IPC would blow the <10 ms budget). SEH catches the recoverable failure modes; memory corruption and audio-thread hangs are accepted non-goals per spec clarification.

5. **No persistent logging, no telemetry, no crash reports**. Errors surface only as in-session UI notifications. The only network activity is an explicit user-initiated "Check for updates" HTTPS GET.

### Data Flow

Audio flows: `[capture source]` → `format convert` → `resample` → `AudioProcessorGraph` (VST3 chain) → `resample` → `format convert` → `[hardware output]`.

Internal processing is always 32-bit float. Source/destination format conversion and resampling happen at the boundaries.

### State Persistence

- `%AppData%\Roaming\JyGlobalVST\settings.json` — user-portable preferences (roaming).
- `%LocalAppData%\JyGlobalVST\` — machine-local state: scan cache, window geometry, auto-save chain, last endpoint.
- `%UserProfile%\Documents\JyGlobalVST\Presets\*.jvst` — user-visible, shareable preset files (JSON manifest + base64-encoded VST3 state chunks).

## Development Notes

- The project uses **driverless audio capture**: WASAPI loopback captures system audio; no virtual driver registration or installation is required.
- `src/tray-app/main.cpp` gates the GUI target. If it does not exist, CMake skips the `jyglobalvst_tray` target and emits a status message.
- Placeholder `.cpp` files are generated automatically for `jyglobalvst_audio_engine` and `jyglobalvst_shared` when no source files exist yet, so early-phase scaffolding configures cleanly.
- Warnings are treated as errors (`/WX`) on every audio-thread-reachable target.
