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
of audio effects, then out to the device you choose. It hosts VST3 plugins that you
have installed on your own computer, and it includes two effects built in, so it is
useful immediately with no plugins at all.

No driver to install. No reboot. No virtual audio cable to configure.

BUILT-IN EFFECTS — NOTHING TO DOWNLOAD

Auto Volume Leveller / Compressor
A stereo compressor and limiter for late-night listening. Loud action scenes are
pulled down and quiet dialogue is brought up, so the volume stays consistent and
comfortable while other people are asleep. No more reaching for the remote every
time the music swells.

EQ with Bass Boost
A simple multi-band equalizer for shaping the overall tone of your system audio,
with a dedicated bass boost for adding low-end warmth. One click resets everything
back to flat.

HOST YOUR OWN VST3 PLUGINS

Global VST Host loads VST3 effect plugins already installed on your PC, from the
standard VST3 folder or a folder you point it at. Build a chain, drag effects into
the order you want, and adjust each plugin's own interface while audio is playing.

Add, remove, reorder, and bypass effects live. Changes take effect without stopping
the audio.

DESIGNED FOR EVERYDAY LISTENING

- Process your system audio, or select a hardware input device instead
- Choose any audio output on your system, including ASIO devices
- Save and load effect chains as preset files
- Stereo input and output level meters, plus a live latency and CPU readout
- An energy-saver mode that steps back when there is no audio to process
- A faulty plugin is bypassed and reported rather than taking the app down with it

PRIVATE BY DESIGN

Global VST Host performs no network communication whatsoever. No accounts, no
telemetry, no analytics, no crash reporting, no update pings. Your settings and
presets stay on your machine.

It captures the audio your computer is playing, processes it in memory, and passes
it straight to your output device. Audio is never recorded, saved, or sent anywhere.

By default it reads what your speakers are already playing, not your microphone.
You can choose a hardware input device instead if you want to process a line
input or an audio interface channel.

Windows requires the microphone permission for any audio capture, so you may see
a microphone prompt and the microphone privacy indicator even when only system
audio is being processed.

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

1. The window title and header read `GlobalVSTHost`, not `Global VST Host` — see below.
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

### Two cosmetic issues worth fixing first

- **The app calls itself "GlobalVSTHost", the Store calls it "Global VST Host".** The window
  title bar and the in-app header both read `GlobalVSTHost` (no spaces), while the reserved
  name, manifest `DisplayName`, and Store listing are `Global VST Host`. Not a certification
  blocker, but customers see both names and it reads as sloppy. One-line UI text change, and
  worth doing before screenshots are taken so they do not need retaking.
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
