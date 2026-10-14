# Global VST Host

This is a little tool I needed for myself, and I tried building it just for fun!  It turned out quite good!  Better than all alternatives out there if you need something simple and good-looking. 

It is available on the Microsoft Store for free with no ads!

<img width="2107" height="1095" alt="image" src="https://github.com/user-attachments/assets/10b77ade-520c-410b-ac33-1a87c49599fe" />

https://apps.microsoft.com/detail/9ND7HRZGWCWN?hl=en-us&gl=CA&ocid=pdpshare

## What it does

Route Windows system audio through VST3 effects and built-in processors — no driver, no virtual device, no reboot.

Routes your PC's system audio via WASAPI loopback through an ordered chain of effects and plays the result to any output device you choose. Useful immediately even without third-party plugins, thanks to its built-in effects:

- **Auto Volume Leveller / Compressor** — stereo compressor/limiter for consistent low-volume listening.
- **EQ + Bass Boost** — 10-band graphic EQ with a dedicated bass boost.
- **Noise Gate** — silences background hiss below an adjustable threshold, with attack/hold/release smoothing.
- **Stereo Width** — mid/side width control, from mono (0) through normal (1) to extra-wide (2).
- **Loudness Normalizer** — keeps program loudness near a target level, with bounded boost and cut.

## Requirements

- Windows 10 (1909+) or Windows 11, x64
- One functioning audio output device
- VST3 plugins are optional; the built-ins work on their own

## Quick start

1. Launch the app. It lives in the system tray; click the tray icon to open the window.
2. Select your **output device**.
3. Click **Scan Plugins** to discover installed VST3 plugins — both directly in the standard VST3 folders and in vendor subfolders (for example `VST3\Vendor\Plugin.vst3`).
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

## Accessibility

Status changes — device connect/disconnect, CPU warnings, plugin add/remove, preset loads — are announced to screen readers (Narrator, NVDA) via Windows UI Automation.

## Updates

The tray menu's **Check for Updates...** looks for a newer version on demand (never automatically). If one is found, you can download and run the installer right from the prompt.

## Languages

The interface ships in English, French and Spanish. It follows your Windows display
language by default; choose one yourself in **About → Settings → Language** (a new
language applies the next time you start the app).

## Privacy

No telemetry, no crash reports, no persistent logging, and no background network activity — the only network use is the update check you trigger yourself. Presets and settings stay on your machine.

## Build

```powershell
cmake -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Requires Visual Studio 2022 or later with C++20. The Steinberg ASIO SDK must be present at `third_party/asiosdk`.

## License

Copyright (c) 2026 Jean-Yves Pelletier. All rights reserved.
