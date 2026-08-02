# Global VST Host

Route Windows system audio through VST3 effects and built-in processors — no driver, no virtual device, no reboot.

## What it does

Captures your PC's system audio via WASAPI loopback, runs it through an ordered chain of effects, and plays the result to any output device you choose. Useful immediately even without third-party plugins, thanks to two built-in effects:

- **Auto Volume Leveller / Compressor** — stereo compressor/limiter for consistent low-volume listening.
- **EQ + Bass Boost** — 10-band graphic EQ with a dedicated bass boost.

## Requirements

- Windows 10 (1909+) or Windows 11, x64
- One functioning audio output device
- VST3 plugins are optional; the built-ins work on their own

## Quick start

1. Launch the app. It lives in the system tray; click the tray icon to open the window.
2. Select your **output device**.
3. Click **Scan Plugins** to discover installed VST3 plugins.
4. Add effects to the chain (built-ins or VST3) and adjust them.
5. Click **Start Audio**. System audio now flows through the chain.

## Managing the chain

- **Add** — drag from the scanned list or use **Load Plugin…**.
- **Reorder** — move slots up/down. Audio continues without dropout.
- **Bypass** — toggle a slot's checkbox to A/B the effect.
- **Remove** — click the × on a slot.
- **Edit** — double-click a slot to open the plugin's own interface.

## Presets

- **Save Preset** — writes the current chain to `%UserProfile%\Documents\JyGlobalVST\Presets\<name>.jvst`.
- **Load Preset** — restores a saved chain instantly.
- Missing plugins appear as grey placeholders; re-point or remove them.

## Buffer size

Choose from **32 / 64 / 128 / 256 / 512 / 1024** samples. Lower = lower latency, higher CPU. Default is 256.

## Monitoring

- **Latency** — estimated round-trip milliseconds.
- **CPU** — audio-thread usage. A warning appears if it stays high.
- **Level meters** — stereo input and output peak/RMS.

## Privacy

No network communication, no telemetry, no crash reports, no persistent logging. Presets and settings stay on your machine.

## Build

```powershell
cmake -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Requires Visual Studio 2022 or later with C++20. The Steinberg ASIO SDK must be present at `third_party/asiosdk`.

## License

Copyright (c) 2026 Jean-Yves Pelletier. All rights reserved.
