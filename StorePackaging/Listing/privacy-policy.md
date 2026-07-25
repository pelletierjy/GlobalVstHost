# Privacy Policy — Global VST Host

**Publisher:** JYP Solutions
**Last updated:** 2026-07-24

> **This file must be hosted at a public URL before submission.** Partner Center requires a
> privacy policy URL, and Store Policy 10.5.1 makes one mandatory for Desktop Bridge and
> Win32 products regardless of what data they access. See "Hosting" at the end of this file.

---

## Summary

Global VST Host does not collect, transmit, or share any personal information. It has no
network functionality, no telemetry, no analytics, and no crash reporting. All data it
creates stays on your device.

---

## Information we collect

**None.** No personal information is collected, stored off-device, or transmitted anywhere.

There is no account, no sign-in, no user identifier sent to us or to any third party, and
no usage or diagnostic reporting.

## Network activity

Global VST Host performs **no network communication of any kind**. It does not contact our
servers, check for updates online, load remote content, or connect to third-party services.

The application binary imports no HTTP, TLS, or socket library, so it is not capable of
network communication as shipped.

## Audio processing

Global VST Host captures the audio your computer is already playing, using the Windows
WASAPI loopback interface, processes it, and sends it to an audio output device you choose.

Two points worth being explicit about:

- **Audio is processed in memory only and is never recorded, saved, or transmitted.** It
  exists only for as long as it takes to pass through the processing chain, in real time.
- **The app declares the Windows "microphone" capability, but it does not listen to your
  microphone.** Windows requires this permission for audio capture generally, and it is the
  permission that covers loopback capture of your system's audio *output*. Windows may
  therefore show a microphone permission prompt and display the microphone privacy
  indicator while the app is running. The app captures what your speakers are playing, not
  what your microphone hears.

## Data stored on your device

Global VST Host stores configuration data locally so your settings persist between
sessions. None of it leaves your computer.

| What | Where |
|---|---|
| Preferences: audio devices, buffer size, energy saver setting | `%AppData%\Roaming\JyGlobalVST\settings.json` |
| Plugin scan cache, auto-saved effect chain, window position | `%LocalAppData%\JyGlobalVST\` |
| Effect chain presets you save | `%UserProfile%\Documents\JyGlobalVST\Presets\*.jvst` |
| An identifier used to remember your chosen audio device | `HKEY_CURRENT_USER\Software\JyGlobalVST\AudioDevice` |

The device identifier is generated locally, is meaningless outside your machine, and is
never transmitted.

Because the app is distributed as an MSIX package, Windows may store some of the above in a
per-application location rather than the literal paths shown. This does not change what is
stored or who can see it.

## Third-party VST3 plugins

Global VST Host can load VST3 audio plugins that **you** have already installed on your
computer. This is the app's primary purpose.

Please understand the boundary here:

- The app does not download, install, or distribute plugins. It only loads plugins already
  present on your system, from standard VST3 locations or a folder you select.
- Plugins run inside the app's process. **A third-party plugin is software from its own
  publisher, governed by that publisher's own privacy policy and terms — not by this one.**
  We have no control over and take no responsibility for what a third-party plugin does,
  including any data it may collect or transmit.
- If you are concerned about a specific plugin's behavior, consult its publisher's privacy
  policy, or do not add it to the chain.

## Children's privacy

Global VST Host is a general-purpose audio utility. It collects no information from anyone,
including children.

## Uninstalling and removing your data

Uninstall through Windows Settings → Apps → Installed apps. To remove remaining
configuration, delete the folders listed under "Data stored on your device". Presets you
saved in your Documents folder are deliberately left in place so you do not lose your own
work; delete that folder if you want them gone.

## Changes to this policy

If the app's data practices change — for example if an online update check is ever added —
this policy will be updated before that version is released, and the "Last updated" date
above will change.

## Contact

Questions about this policy: **&lt;SUPPORT EMAIL — FILL IN&gt;**

---

## Notes for the developer (remove before publishing)

**Verification behind the claims above**, so this can be re-checked rather than trusted:

- *No network activity*: `dumpbin /dependents` on the shipped `JyGlobalVST.exe` lists no
  `winhttp.dll`, `wininet.dll`, or `ws2_32.dll` — only COM, GDI/Direct2D, audio, and CRT
  imports. Re-run this check before each release.
- *Local storage locations*: `src/tray-app/settings/roaming_settings.cpp`,
  `src/tray-app/settings/local_state.cpp`,
  `src/tray-app/presets/preset_folder_init.cpp`,
  `src/shared/platform/windows_device.cpp`.
- *Loopback capture*: `src/audio-engine/routing/wasapi_capture.cpp`.

**Two things to fix before publishing:**

1. Fill in the support email. Partner Center requires support contact information, and
   Store Policy 10.14 requires it to be accurate and current for Company accounts.
2. `src/tray-app/updates/update_check.cpp` implements an HTTPS update check using WinHTTP.
   It currently compiles but is **dead code** — nothing calls it, and the linker strips it,
   which is why the binary has no `winhttp` import. **If anyone wires it up, this policy
   becomes false.** Either delete the file or gate it behind an explicit, documented
   decision to update this policy and re-evaluate whether the `networkInternet` capability
   is required.

**Hosting.** Partner Center needs a public HTTPS URL. Options:

- GitHub Pages on the project repo — free, and the URL is stable. Requires the repo to be
  public, or a separate public repo for the policy.
- Any static host you already control.

Do not link to a `raw.githubusercontent.com` URL; it serves as plain text and is not a
durable, user-friendly policy page. Render this file to HTML for the hosted version.
