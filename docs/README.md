# Global VST Host — User Guide

> **Note:** This is a plain-text Markdown rendering of the user guide that ships as `docs/readme.html`. The original HTML file is a self-contained, dark-themed document. This Markdown version is provided so GitHub renders it directly on the `docs/` folder landing page.

## Signal Path

```
                 ┌─────────────┐
                 │ Windows apps│   (browser, games, media players)
                 └──────┬──────┘
                        │
                        ▼
    ┌──────────────────┬──────────────────┐
    │  Playback device 1 (Windows default  │
    │  output — muted while the host runs) │
    │            │  speaker: SILENT        │
    └────────────┼──────────────────────┘
                 │
       WASAPI loopback capture
                 │
                 ▼
    ┌─────────────────────────┐
    │   Global VST Host       │
    │   VST3 + built-in       │
    │   effects chain         │
    │   master volume + mute  │
    └─────────────────────────┘
                 │
        shared / exclusive
        WASAPI or ASIO
                 │
                 ▼
    ┌──────────────────────────┐
    │ Playback device 2        │
    │ (your speakers /         │
    │ headphones — this is      │
    │ what you HEAR)           │
    └──────────────────────────┘
```

Two playback devices are involved. Your apps keep playing to the Windows default device, but the host mutes that device's speaker, so the top path ends in silence. The dot is where the host taps the very same stream — WASAPI loopback capture — and the branch carries it down through the plugin chain and out to a second device, whose speaker is the one you actually hear. Capture and output must be different devices; the host refuses to start otherwise. If you capture a microphone or line-in instead, nothing is muted.

## Contents

- [Overview](#overview)
- [Getting Started](#getting-started)
- [Audio Devices](#audio-devices)
- [The Plugin Chain](#the-plugin-chain)
- [Built-In Effects](#built-in-effects)
- [Meters & Spectrum](#meters--spectrum)
- [Master Volume](#master-volume)
- [Presets](#presets)
- [System Tray & Power](#system-tray--power)
- [Appearance & Settings](#appearance--settings)
- [Troubleshooting](#troubleshooting)

## Overview

Global VST Host captures your Windows system audio, routes it through an ordered chain of VST3 plugins, and plays the processed result out to a hardware output device. This lets you apply effects — EQ, dynamics, spatial processing, and more — to *everything* your PC plays, regardless of which application produced it.

Audio is captured using WASAPI loopback, so no virtual audio driver needs to be installed. Internal processing runs in 32-bit float. You can also capture a real input device — a microphone or line-in — instead of system playback.

Output can go out over WASAPI (shared or exclusive) or through an ASIO driver. The two are independent: capturing system audio by loopback while playing out to an ASIO interface is a supported combination.

## Getting Started

1. Launch the application. On first run it scans for installed VST3 plugins.
2. Under **INPUT**, choose the **captured audio device** — normally the Windows default output, the device your apps already play to.
3. Under **OUTPUT**, choose a *different* device — the speakers or headphones you want to hear the processed audio on.
4. Press **Start Audio** to begin capturing and routing system audio.
5. Add plugins to the chain and adjust them while audio plays.

> While audio is running, the captured device is muted for you, so the unprocessed signal is not heard alongside the processed one. Its original mute state is restored when you stop audio or exit.

## Audio Devices

The **AUDIO DEVICE** panel is split into **INPUT** (where audio is captured from) and **OUTPUT** (where it is sent).

The input list holds both playback devices, captured by loopback, and real capture devices such as microphones and line inputs. Capture and output must resolve to two different devices; if they are the same, the host refuses to start rather than feed itself.

### Transport modes

| Mode | Description |
|------|-------------|
| WASAPI (Shared) | Standard shared-mode output. Coexists with other apps using the device. |
| WASAPI (Exclusive) | Takes exclusive control of the output device for lower latency. |
| ASIO | Uses an ASIO driver when available. Use **ASIO Settings** to open the driver's control panel. |

The buffer size setting trades latency against stability — smaller buffers give lower latency but are more sensitive to CPU spikes. If a device rejects the size you pick, the app tells you and keeps a size the device accepts.

Three sample rates are reported separately: the **captured** rate, the **output** rate, and the **VST** rate the plugin chain runs at. They do not have to match — the host resamples between them.

> **Drift compensation** adapts resampling when the capture source and output device run from different clocks. Turn it off when both share the same hardware clock (e.g. WASAPI loopback + ASIO on the same USB interface) to avoid pitch wobble.

## The Plugin Chain

The plugin chain is an ordered list of effects. Audio flows through each slot from top to bottom.

- **+** — add a VST3 plugin, or a built-in effect, to the chain.
- **Drag a slot** up or down to change the processing order.
- **ON / OFF** — bypass a slot without removing it.
- **E** — open the plugin's own editor to adjust its parameters.
- **Tag** — give the slot a short label of your own (up to 32 characters), shown beside the plugin name. Useful when the same plugin appears more than once in the chain.
- **Shortcut** — give the slot one of the two quick-toggle buttons in the tray volume popup. See [System Tray](#system-tray--power).
- **X** — delete the slot from the chain.

> If a plugin fails while processing, it is automatically bypassed so audio keeps flowing, and a notification appears in the window.

## Built-In Effects

Global VST Host ships with built-in effects that appear alongside your VST3 plugins and can be added to the chain like any other effect.

### Equalizer

A 10-band graphic equalizer with bands at 32 Hz, 64, 125, 250, 500 Hz, 1, 2, 4, 8 and 16 kHz, each adjustable by ±12 dB. A separate **Bass Boost** slider adds up to 12 dB of extra low end, and an input **Volume** trim (±12 dB) sets the level going into the filters, so you can pull a hot signal down before boosting bands. Each band shows its own level meter. **Flat/Reset** returns all bands to 0 dB.

### Volume Leveler

A dynamics processor that evens out loud and quiet passages so you can listen at low volume without missing detail or disturbing others. Choose a preset (Light, Medium, Strong, Extreme) — stronger presets compress the dynamic range more aggressively. Each preset is level-matched so switching between them (or bypassing the effect) keeps the overall loudness roughly consistent. The editor shows a before/after graph illustrating how the selected preset reshapes a high-dynamic example signal.

### Compressor

A conventional compressor with the usual controls — **Threshold**, **Ratio**, **Attack**, **Release** and **Makeup** gain — for when you want to dial in dynamics yourself rather than pick one of the Volume Leveler presets.

> The built-in effect editors follow the color theme you select in the main window.

## Meters & Spectrum

The **LEVELS** panel shows input and output meters side by side, so you can see at a glance whether audio is arriving and whether the chain is producing output. The **SPECTRUM** panel shows a live frequency analysis of the processed signal.

The status bar reports engine CPU load and latency while audio runs.

## Master Volume

The master volume slider controls the overall output gain applied after the plugin chain, and the mute button silences the host's output without stopping the engine. Neither affects the Windows volume of your apps. Your setting is saved automatically and restored the next time you open the app.

## Presets

A preset stores your entire plugin chain — the plugins, their order, and their saved state — in a single shareable file.

- **Save Preset…** — write the current chain to a `.jvst` file.
- **Load Preset…** — restore a chain from a saved preset.

Presets are stored under `Documents\JyGlobalVST\Presets`. You can also drag a preset file onto the window to load it.

> If a preset references a plugin that is not installed, the app lets you repoint that slot to a different plugin.

## System Tray & Power

Global VST Host lives in the Windows system tray. Closing the window keeps the app running in the background so audio processing continues. Right-click the tray icon for quick actions.

**Left-click the tray icon** for a compact volume popup: input and output meters, the master volume slider, a mute button, an ON/OFF button that starts and stops audio, and up to two quick-toggle shortcut buttons.

### Shortcut buttons

The two shortcut buttons bypass and un-bypass a plugin of your choosing without opening the main window — green when the plugin is active, grey when bypassed. Assign them with the shortcut button on a chain slot. Only two can be assigned at a time, so clear one before assigning another. The button is labelled with the first two letters of the slot's tag, or of the plugin name when there is no tag. Assignments are saved with your session and with presets, and follow the slot if you reorder the chain.

Enable **Start minimized to tray** to have the app launch directly to the tray without showing the window.

**Energy saver** automatically steps the engine back when no audio is playing, reducing CPU usage. It is on by default.

## Appearance & Settings

Open **Settings** from the main window to change:

- **Theme** — Neon Blue, Neon Purple, Neon Green, Neon Orange, Neon Red, Monochrome or Light. The built-in effect editors follow your choice.
- **Start minimized** — launch straight to the tray.
- **Tooltips** — turn the hover hints on or off.

Preferences, the plugin scan cache and your last session are saved automatically, so the app comes back the way you left it.

## Troubleshooting

### No sound / no processing

Make sure you pressed **Start Audio** and selected a valid output device. Confirm system audio is actually playing.

### I hear the audio twice, or it sounds doubled

Both devices are audible at once. The host mutes the captured device for you; if you unmute it in Windows while the host is running, you will hear the unprocessed signal underneath the processed one. On devices where Windows does not allow the host to set the mute, the app says so and asks you to pick a different pairing.

### Windows says my speakers are muted

That is the captured device, muted on purpose while the host runs. Stopping audio or exiting the app restores the mute state it had before.

### The app will not start audio

Capture and output must be two different devices. Selecting the same device for both — including by way of "system default" resolving to it — is blocked.

### Glitches or dropouts (xruns)

Increase the buffer size, close CPU-heavy applications, or reduce the number of plugins in the chain. The status bar shows CPU load and latency.

### Reset Engine

**Reset Engine** reinitializes the audio device and transport while *keeping* your loaded plugin chain intact. Use it if audio stops responding or after changing devices.

### A plugin crashed

Faulty plugins are isolated and bypassed automatically so the rest of the chain keeps working. Remove the offending plugin from the chain if the problem persists.

---

Global VST Host — Windows system-wide VST3 host.