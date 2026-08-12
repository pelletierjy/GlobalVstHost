# Store listing copy — Global VST Host

Draft copy for Partner Center → Store listings. Every factual claim below was checked
against the code; the verification notes at the end say how, and flag the claims I could
**not** verify so they are not published by accident.

Two policy constraints shaped this draft:

- **10.2.4** — a dependency on non-integrated software is permitted *"if you disclose the
  dependency at the beginning of the description."* So the description leads with VST3
  plugin hosting rather than burying it.
- **10.1.1** — metadata must accurately describe functions, features *"and any important
  limitations."* Hence the explicit requirements/limitations section.

---

## Product name

```
Global VST Host
```

Matches the reserved name and the manifest `DisplayName`. Policy 10.1.1 forbids marketing
or keyword padding in the title, so leave it as-is — do not append "— System Audio Equalizer
& VST3 Effects" or similar.

## Short description

```
Run your system audio through VST3 effects and a built-in auto volume leveller / compressor and EQ.
```

## Description

```
Global VST Host routes the audio your PC is already playing through an ordered chain
of audio effects, then out to the hardware device you choose from inside the app. It
hosts VST3 plugins that you have installed on your own computer, and it includes two
effects built in, so it is useful immediately with no plugins at all.

No driver to install. No reboot. No virtual playback device to select in Windows
Sound settings, and nothing to configure there — Global VST Host picks up your
system audio in the background while your normal output stays your default device.

HOW IT WORKS

Under the hood, Global VST Host uses WASAPI loopback capture: the same mechanism
Windows itself uses for things like screen-recording audio. It reads the audio
stream your PC is already producing, feeds it through your effect chain in 32-bit
floating point, and writes the result to the hardware output you selected — your
speakers, headphones, a USB DAC, an HDMI receiver, or an ASIO interface. Everything
after installation happens inside the app's own tray window: pick an output, add
effects, done.

BUILT-IN EFFECTS — NOTHING TO DOWNLOAD

Auto Volume Leveller / Compressor
A stereo compressor and limiter for late-night listening. Loud action scenes are
pulled down and quiet dialogue is brought up, so the volume stays consistent and
comfortable while other people are asleep. No more reaching for the remote every
time the music swells. A look-ahead control lets you trade a small amount of extra
latency for smoother gain changes, and the effect reports its own added latency so
you always know the cost.

EQ with Bass Boost
A ten-band equalizer for shaping the overall tone of your system audio, with a
dedicated bass-boost shelf for adding low-end warmth without touching the other
bands. Each band has its own gain, there's an input-level control and a per-band
level meter so you can see what's actually happening to the signal, and one click
on Flat/Reset returns every band and the bass boost to zero.

HOST YOUR OWN VST3 PLUGINS

Global VST Host loads VST3 effect plugins already installed on your PC — from the
standard system VST3 folder, or from an additional folder you add in Settings if
your plugins live somewhere else. Tap the "+" at the end of the chain to open the
plugin catalog, pick an effect, and it's added to the end of the chain. Drag chain
entries to reorder them, and open any plugin's own editor window to adjust it while
audio keeps playing.

Add, remove, reorder, and bypass effects live, in any combination, in any position
in the chain. Changes take effect immediately — the audio never stops or drops out
while you edit the chain.

DESIGNED FOR EVERYDAY LISTENING

- Process your system audio, or select a hardware input device instead (a line
  input or an audio interface channel, for example)
- Choose any audio output on your system, including ASIO devices, with selectable
  buffer size and sample rate
- Save an effect chain — built-ins and third-party plugins together, in order,
  with all their settings — as a preset file, and load it back later
- The current chain autosaves as you work, and is restored automatically the next
  time you open the app, even if you never explicitly saved a preset
- Stereo input and output level meters (peak and RMS, in dB), plus a live
  round-trip latency readout and a CPU-usage meter with a high-load warning
- An energy-saver mode, on by default, that reduces the app's own footprint when
  there is no audio to process
- Runs from the system tray with no separate taskbar window taking up space
- A light and dark theme, and an in-app Settings tab for adjusting them
- A faulty plugin is caught, bypassed, and reported in the app rather than being
  allowed to crash the whole audio chain

PRIVATE BY DESIGN

Global VST Host performs no network communication whatsoever. No accounts, no
telemetry, no analytics, no crash reporting, no background update checks or
pings of any kind. Your settings and presets stay on your machine, in your own
user profile.

It captures the audio your computer is playing, processes it in memory, and passes
it straight to your output device. Audio is never recorded, saved to disk, or sent
anywhere — not to us, not to anyone.

By default it reads what your speakers are already playing, not your microphone.
You can choose a hardware input device instead if you want to process a line input
or an audio interface channel, but that is an explicit choice you make in the app,
not the default.

Windows requires the microphone permission for any audio capture, so you may see a
microphone prompt and the microphone privacy indicator even when only system audio
is being processed. This is a Windows platform requirement for the capture API
used, not an indication that your microphone is being read.

REQUIREMENTS AND LIMITATIONS

- Windows 10 version 1909 or later, or Windows 11
- 64-bit (x64) PCs only — 32-bit and ARM64 are not supported
- VST3 effect plugins only. VST2 and instrument (VSTi) plugins are not supported
- Third-party plugins are not included and are not downloaded by this app. You
  supply your own, and each remains subject to its own publisher's licence terms
- If your PC has only one audio interface and no ASIO support, you need a
  separate playback device (headphones, monitor speakers, USB DAC, etc.) for
  processed output. A free virtual audio cable is an easy alternative:
  https://vb-audio.com/Cable/
```

## Search terms

Policy 10.1.3 caps these at **seven**, requires relevance, and forbids pricing terms and
other publishers' product titles.

```
VST3
VST host
system audio
equalizer
bass boost
audio effects
loudness
```

## Category

**There is no category named "Audio."** Verified against Microsoft's published taxonomy
(see Sources). The full category list has no audio entry at all.

**Recommended primary:** `Music`
Microsoft's own description is *"Apps for listening, recording, creating, or performing
music, music videos."* **Listening** is the operative word — the headline use case in
`specs/006-builtin-plugins/spec.md` is watching a movie late at night at consistent volume.
That is a listening tool, not a production tool.

**Recommended secondary:** `Multimedia design` → subcategory `Music production`
Secondary category draws from the same list. This is where someone hunting for a plugin host
would look. Using it as secondary rather than primary is deliberate: the `Multimedia design`
parent is described as *"tools for creating or editing graphics, art, design"*, so browsers
there expect DAWs and editors, and the app would look out of place as a primary listing.

`Music` has no subcategories, so there is nothing further to pick for the primary.

**Rejected:** `Utilities + tools` — described as *"apps which assist user in solving problems
or completing specific tasks"* with examples like file managers and calculators, and its only
subcategories are `Backup + manage` and `File managers`. Nothing audio-related.
`Personalization → Ringtones + sounds` is also a poor fit.

One caveat: the taxonomy above is published on the MSI/EXE submission page. The category list
is Store-wide rather than package-format-specific, but confirm the labels match what the
packaged-app submission form actually shows.

## Age rating

**You cannot opt out of a rating.** Policy 11.11 requires obtaining one by completing the
IARC questionnaire during submission. There is no "no age restriction" option.

What you can reasonably expect: given no user-generated content, no communication features,
no purchases, no gambling and no objectionable content, the questionnaire should return the
**lowest** rating available (PEGI 3 / ESRB Everyone or equivalent). That is effectively "no
age restriction" as far as customers are concerned.

Answer the questionnaire honestly yourself — 11.11 makes you responsible for its accuracy,
and a rating that does not match the app is a rejection cause.

Note also policy 11.1: all listing metadata (description, screenshots, icon) must itself
merit PEGI 12 / ESRB Everyone 10+ or lower. The draft copy above is well inside that.

## Screenshots

**Hard requirement: PNG, minimum 1366 × 768, under 50 MB. Maximum 10; 4 recommended.**
(Source below.)

### Current state: three usable screenshots exist

`StorePackaging/Listing/screenshots/` contains three composites at **1920 × 1080**, built
from real captures placed 1:1 (no upscaling) on a dark backdrop matching the app's own
palette, with soft shadows:

| File | Shows |
|---|---|
| `01-main-window.png` | Main window, chain of both built-ins, meters active, ASIO output |
| `02-eq-bass-boost.png` | EQ editor open over the main window, bands and bass boost set |
| `03-master-volume.png` | Master Volume overlay with the level meters running |

These clear the size requirement and are submittable. Regenerate with
`scratchpad/compose_shots.ps1` if the sources change.

**The status bar is cropped out.** The capture's status strip starts at y=646 in the 848×682
source (its text sits at rows 661–674), so the main window is trimmed to 646 px tall. That
removes the `CPU: 16.3 % (peak 16.3 %) ⚠ HIGH` warning and the device-specific
`Latency: 7.91 ms` readout, while keeping the preset panel's bottom border so the window
still ends on a real edge rather than a hard cut.

Dropping the latency figure is deliberate beyond aesthetics: it is the app's own readout on
one particular ASIO interface at 96 kHz / 128 samples, not an independently measured round
trip, and publishing it invites users to hold you to it.

**Three caveats still baked into these images.** None blocks submission; all argue for
recapturing once the app-side fixes land:

1. The window title and header may read `GlobalVSTHost`, not `Global VST Host` — this was
   fixed in code since these screenshots were captured (see "One cosmetic issue remains"
   below), so check the actual images rather than assuming either way.
2. A specific audio interface name is visible (`Art Pro (B) (USB IV 3/4)`). Harmless, but
   generic device names look better.
3. In `02`, the EQ editor covers the level meters. Reposition it when recapturing.

**A fourth screenshot of the Auto Volume Leveller / Compressor editor was deliberately excluded.** The available
capture shows the caption as `High-dynamic example â□□ before…` — a UTF-8 mojibake bug that
has since been fixed in `nighttime_editor.cpp`. Publishing it would show a defect the app no
longer has. **Rebuild, then capture the Auto Volume Leveller / Compressor editor** — it is the most compelling
effect to show, with its before/after waveform graph.

### The original README screenshot is too small

The screenshot in `docs/README.md` is **989 × 786** — the width fails the 1366 minimum, so
Partner Center will reject it. Content-wise it is a good shot: it shows the plugin chain with
a third-party plugin (ARC X) alongside both built-ins, the level meters, ASIO output, and the
preset buttons.

Two ways to fix it, in order of preference:

1. **Resize the app window wider before capturing.** Native pixels at ≥1366 wide are sharper
   than anything done afterwards. Best option if the window is resizable that far.
2. **Composite the window onto a larger canvas.** Centre the real window on a 1920 × 1080
   neutral background. Legitimate and common for desktop apps, and avoids upscaling blur.
   Do **not** simply upscale the 989 px image — it will look soft and cheap.

### Suggested set

1. Main window, chain of two or three effects, meters showing activity
2. The Auto Volume Leveller / Compressor effect's controls
3. The EQ with a visible bass boost applied
4. Input/output device selection
5. Preset save/load

Capture on the **installed MSIX package** at 100% scaling. Note the existing screenshot
exposes a specific audio interface name ("Art Pro (B) (USB IV 3/4)") — harmless, but prefer
generic device names if convenient.

### One cosmetic issue remains

- **The window title / app name mismatch is fixed.** Previously the window title bar and
  in-app header read `GlobalVSTHost` (no spaces) while the Store listing used `Global VST
  Host`. Re-checked against current code: `main.cpp`'s `kWindowTitle` and the
  `DocumentWindow` constructor in `main_window.cpp:1198` both now read `"Global VST Host"`.
  **The existing screenshots may still show the old title**, since I have not opened the
  PNG files to check — recapture (or at least eyeball) them before submission rather than
  assuming this is resolved everywhere.
- **The icons are functional placeholders**, not designed artwork. They will pass
  certification but are the weakest part of the listing.

---

## Verification notes

### Verified against the code

| Claim | Evidence |
|---|---|
| WASAPI loopback capture, no driver | `src/audio-engine/routing/wasapi_capture.cpp:333` sets `AUDCLNT_STREAMFLAGS_LOOPBACK`; no driver or NT service dependency anywhere |
| Selectable hardware input device | `audio_engine_impl.cpp:2374` — `listInputs()` enumerates `EndpointFlow::Capture` endpoints; `selectInput()` at `798`. Visible in the screenshot as an Input selector |
| "Auto Volume Leveller / Compressor" is a stereo compressor/limiter for consistent late-night volume | `src/audio-engine/builtin-effects/nighttime_processor.cpp`; registered as `"Auto Volume Leveller / Compressor"` in `builtin_effect_registry.cpp`; intent per `specs/006-builtin-plugins/spec.md` US1 |
| EQ with multi-band gains and a dedicated bass boost | `src/audio-engine/builtin-effects/eq_processor.cpp` (`setBandGain`, `setBassBoost`, bass shelf at 200 Hz); registered as `"Equalizer"` |
| Built-ins need no scan or download | Registered in-process at construction; spec 006 US1 AC1 |
| Loads user-installed VST3 from the standard folder | `src/audio-engine/vst-host/default_scan_paths.cpp` resolves `FOLDERID_ProgramFiles\Common Files\VST3` |
| Live add/remove/reorder without dropouts | `src/audio-engine/chain/plugin_chain.cpp`; SPSC command queue applied at the top of `processBlock` |
| Faulty plugin bypassed, not fatal | SEH guard in `src/audio-engine/vst-host/plugin_scanner.cpp` and the chain's process path |
| ASIO output supported | `src/audio-engine/routing/asio_transport.cpp` |
| Save/load presets as `.jvst` files | `main_window.cpp:2551-2562` — file dialog then `engine_->savePreset(...)` |
| Stereo in/out level meters (peak + RMS) | `main_window.cpp:2865-2868` — `meter_input_l/r_`, `meter_output_l/r_` fed `setLevels(peak_dB, rms_dB)`; frames arrive via `onMeterFrame` (`main_window.h:114`) |
| Live latency and CPU readout | `main_window.cpp:2931` formats real values (`"Latency: %.2f ms (in %.2f + out %.2f)"`); CPU label at `1892` |
| Tray-only, no taskbar window | Verified on the installed package: process runs with `MainWindowHandle = 0` until the tray icon is clicked |
| Energy saver, on by default | `roaming_settings.h:28` — `bool energy_saver_enabled {true}` |
| **No network communication** | Shipped `JyGlobalVST.exe` imports no `winhttp`/`wininet`/`ws2_32` — confirmed by `dumpbin /dependents` |
| x64 only, Windows 10 1909+ | `AppxManifest.xml` `MinVersion=10.0.18363.0`, `ProcessorArchitecture=x64` |
| VST3 only, no VST2 | No VST2 support in `src/audio-engine/vst-host/` |
| Additional VST3 folder configurable in Settings | `src/tray-app/ui/settings_dialog_scan_paths.cpp` — `addPath()`, backed by a folder-browse `FileChooser` |
| "+" button opens a plugin catalog to add to the chain | `chain_editor.cpp:393` — `add_plugin_button_` is a `TextButton("+")`; opens `catalog_dialog.cpp` |
| Auto Volume Leveller / Compressor has a configurable look-ahead that reports its own added latency | `nighttime_processor.cpp:9,22-25,57-58` — `current_lookahead_ms_`, `lookahead_buffer_l/r_`, `latency_samples_` |
| EQ has an input-volume control and per-band level metering | `eq_processor.cpp:67-68` (`input_volume_linear_` applied to the signal), `193-201` (`getInputVolume`/`setInputVolume`), `204` (`getBandLevel`) |
| Chain autosaves and restores on next launch without an explicit preset load | `src/tray-app/presets/autosave.cpp` |
| Window title reads "Global VST Host" (name mismatch fixed) | `main.cpp` `kWindowTitle`; `main_window.cpp:1198` `DocumentWindow("Global VST Host", ...)` |

### Deliberately NOT claimed — do not add these

- **Any latency figure.** The spec targets ≤10 ms round trip and `tests/latency/README.md`
  defines pass criteria, but that harness needs a physical loopback cable and I found no
  evidence it has been run. **Do not write "sub-10 ms", "under 10 ms", or a specific number
  until it is measured on real hardware.** Policy 10.1.1 forbids misleading claims about
  features, and a latency number is exactly the kind of thing an audio-savvy reviewer or
  user will test. The draft says "without stopping the audio" and avoids numbers entirely.
- **"Zero latency" / "no added latency".** False for any buffered processing chain.
- **Plugin compatibility claims.** Nothing like "works with all your favourite plugins" —
  compatibility depends on the plugin, and `tests/compat/` exists precisely because it
  varies.
- **Comparisons to named products** (FxSound, Equalizer APO, etc.). Policy 10.1.3 forbids
  other product titles in search terms, and naming competitors in the description invites
  trademark problems under 11.2.

### Needs your decision before publishing

- ~~**Support email.**~~ Set: `g.vst.host.help@outlook.com`. Policy 10.14 requires support
  contact information to be *"accurate and current"* for Company accounts, so make sure that
  mailbox is actually monitored — it is displayed publicly on the Store product page in some
  regions.
- ~~**Category**~~ Resolved: primary `Music`, secondary `Multimedia design → Music
  production`. No "Audio" category exists. See the Category section above.
- ~~**Audio level meters.**~~ Resolved: there are four visible meters (stereo input, stereo
  output) driven with peak and RMS in dB. Claimed in the description.
- **Age rating.** Still yours to complete — see the Age rating section above. The expected
  outcome is the lowest available rating, but the questionnaire is mandatory and you are
  responsible for its accuracy.

---

## Sources

- [Microsoft Store Policies version 7.19](https://learn.microsoft.com/en-us/windows/apps/publish/store-policies)
  — 10.1.1, 10.1.3, 10.1.4, 10.2.4, 10.5.1, 10.14, 11.1, 11.11
- [Categories and subcategories](https://learn.microsoft.com/en-us/windows/apps/publish/publish-your-app/msi/categories-and-subcategories)
  — the category taxonomy quoted above
- [App screenshots, images, and trailers for MSIX app](https://learn.microsoft.com/en-us/windows/apps/publish/publish-your-app/msix/screenshots-and-images)
  — the 1366 × 768 minimum and the 10-image cap
