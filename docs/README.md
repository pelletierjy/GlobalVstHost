# JyGlobalVST

Low-latency Windows audio processor that routes system audio through a chain of VST3 plugins and outputs to your hardware device.

## What it does

JyGlobalVST appears as a virtual audio output in Windows Sound settings. Select it as your default playback device, load VST3 plugins (EQ, room correction, limiters, etc.), and hear processed audio on your speakers or headphones with under 10 ms round-trip latency.

## Requirements

- Windows 10 (1909+) or Windows 11, x64
- At least one VST3 plugin installed
- One functioning audio output device (built-in, USB DAC, or HDMI)

## First-run setup

1. Install JyGlobalVST from the MSI (single UAC prompt, no reboot).
2. Open **Windows Settings → System → Sound → Output** and select **JyGlobalVST Virtual Output**.
3. Launch JyGlobalVST from the system tray.
4. In the app, pick your **hardware output** from the Output dropdown.
5. Click **Scan Plugins** to discover installed VST3 plugins.
6. Drag a plugin into the chain or use **Load Plugin…** to browse for a `.vst3` file.
7. Click **Start Audio**. All system audio now flows through the plugin chain.

## Managing the plugin chain

- **Add**: drag from the scanned list or use Load Plugin.
- **Reorder**: drag slots up/down. Audio continues without dropout.
- **Bypass**: toggle the checkbox on a slot to A/B the effect.
- **Remove**: click the × on a slot.
- **Edit plugin GUI**: double-click a slot or press Enter while it is focused.

## Presets

- **Save Preset**: writes the current chain + settings to `%UserProfile%\Documents\JyGlobalVST\Presets\<name>.jvst`.
- **Load Preset**: restores a saved chain instantly.
- If a preset references a plugin that is no longer installed, a greyed-out **placeholder** appears. You can re-point it to a relocated plugin or remove it.

## Buffer size

Choose from **32 / 64 / 128 / 256 / 512 / 1024** samples in the Buffer dropdown.
- Lower = lower latency, higher CPU load.
- Higher = more CPU headroom, slightly more latency.
- Default is 256 (good for most systems).

## Hardware output selection

JyGlobalVST resolves your output device in this priority:
1. Exact endpoint ID from last session (machine-specific).
2. Friendly name match from roaming settings (follows you across PCs).
3. Current Windows default output.

If you unplug a USB DAC while audio is playing, JyGlobalVST automatically falls back to the Windows default device and restores your preferred device when it reconnects.

## Monitoring

- **Latency readout**: shows estimated round-trip milliseconds.
- **CPU meter**: shows audio-thread CPU usage. If it exceeds 5%, a warning banner suggests increasing the buffer size.
- **Level meters**: input and output peak/RMS (US4 — TBD).

## Sleep / wake

JyGlobalVST automatically reinitializes the audio path when Windows sleeps and wakes. No manual restart is required.

## Troubleshooting

| Problem | Solution |
|---|---|
| No audio after starting | Confirm JyGlobalVST is selected as Windows default output. Check that an output device is selected in the app. |
| High CPU warning | Increase buffer size to 512 or 1024. Remove heavy plugins. |
| Plugin fails to load | Ensure the `.vst3` file is 64-bit. Try rescanning plugins. |
| Crackles or dropouts | Increase buffer size. Close other audio apps. Check that sample rates match between virtual device and hardware output. |
| Auto-save lost chain | Auto-save is stored in `%LocalAppData%\JyGlobalVST\autosave.json`. If corrupted, JyGlobalVST starts blank per design. |

## Privacy

- No persistent logging. No telemetry. No crash reports. No background network activity.
- The only network call is an explicit **Check for updates…** from the About menu (not implemented in testable-dev).
- Presets live in your Documents folder; settings roam with your profile.

## Build (developers)

```powershell
cmake -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Requires Visual Studio 2022 or later with C++20 support.

> **ASIO support:** ASIO is mandatory and always built. The Steinberg ASIO SDK is auto-detected at `third_party/asiosdk` (override with `-DJYGLOBALVST_ASIO_SDK_PATH=...`); configuration fails if it cannot be found.

## License

Copyright (c) 2026 Jean-Yves Pelletier. All rights reserved.
