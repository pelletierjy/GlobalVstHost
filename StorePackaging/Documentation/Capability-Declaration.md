# Capability Declaration: JyGlobalVST

**Purpose**: Document and justify every capability declared in `StorePackaging/AppxManifest.xml`.

**Last verified against code**: 2026-07-24

> This file previously documented `audioLibrary`, `registryRead` and `registryWrite` as
> declared capabilities and cited source files that do not exist in this repository. None
> of that was accurate. It has been rewritten against the actual manifest and source. If
> you change the manifest, update this file in the same commit.

---

## What is actually declared

Exactly three capabilities, as of the manifest in this directory:

```xml
<Capabilities>
  <rescap:Capability Name="runFullTrust" />
  <uap:Capability Name="documentsLibrary" />
  <DeviceCapability Name="microphone" />
</Capabilities>
```

---

### 1. `runFullTrust` (restricted capability)

**Namespace**: `rescap:`
(`http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities`)

**Justification**: Mandatory. JyGlobalVST is a packaged Win32 desktop application, not a
UWP app. The manifest declares
`EntryPoint="Windows.FullTrustApplication"`, which requires `runFullTrust`. Without it the
package does not deploy.

**Consequence worth understanding**: under full trust the app already has the file-system
and registry reach of the launching user. Capabilities like `documentsLibrary` are
therefore not what *grants* the app its access — see below.

**Store note**: `runFullTrust` is a restricted capability. Partner Center may ask you to
justify it. The justification is simply that this is a desktop bridge application. This is
routine for packaged Win32 apps, but I have not verified the current wording Partner
Center expects — **confirm against Partner Center's current guidance before submitting.**

---

### 2. `documentsLibrary`

**Namespace**: `uap:`

**Current status**: **declared but redundant. Recommend removing.**

**What the code does**: presets live in
`%UserProfile%\Documents\JyGlobalVST\Presets\*.jvst`, created and scanned by:

- `src/tray-app/presets/preset_folder_init.cpp` — creates the folder on first launch
- `src/tray-app/presets/preset_folder_scanner.cpp` — scans for `*.jvst`

**Why it is redundant**: this is ordinary Win32 file I/O into the user's Documents folder,
which `runFullTrust` already permits. `documentsLibrary` exists for UWP-style broker access
and historically pairs with a declared file-type association. Declaring it here adds
nothing functional and invites over-declaration questions during certification.

**Recommendation**: remove it, rebuild, and confirm preset save/load still works (it
should — no code path depends on the capability). Left in place for now only because
removing it is a behavioural change that deserves its own verification pass.

---

### 3. `microphone` (device capability)

**Namespace**: base (`DeviceCapability`)

**Justification**: the app captures system audio through WASAPI **loopback**, implemented
in `src/audio-engine/routing/wasapi_capture.cpp`. Loopback capture initialises an audio
client on a *render* endpoint with `AUDCLNT_STREAMFLAGS_LOOPBACK`.

**Status: needs verification.** Two things are genuinely uncertain and should be confirmed
before submission:

1. Whether `microphone` is the capability Windows requires for loopback capture *inside an
   app container*. Loopback reads a render endpoint, not a capture device, so the
   requirement is not self-evident. Local testing on 2026-07-24 confirmed capture works in
   the installed package with this manifest, but that does not prove the capability is what
   made it work — full trust may be sufficient on its own.
2. How Store certification treats a `microphone` declaration in an app whose description
   never mentions recording the user. Expect a reviewer question; be ready to explain
   loopback.

**Privacy note for the Store listing**: loopback capture records *system output*, not
microphone input. Say so plainly in the listing and privacy policy, because users will see
a microphone permission prompt and the Windows privacy indicator.

---

## Registry use

The app does touch the registry, but **no registry capability is declared, and none is
needed.**

- `src/shared/platform/windows_device.cpp` reads and writes
  `HKEY_CURRENT_USER\Software\JyGlobalVST\AudioDevice`, value `DeviceGuid`
  (`RegOpenKeyExW` / `RegCreateKeyExW` / `RegQueryValueExW` / `RegSetValueExW`)

`registryRead` and `registryWrite` are **not real MSIX capability names** — earlier
versions of this document invented them. `HKEY_CURRENT_USER` access under `runFullTrust`
needs no declaration. Note that MSIX virtualizes `HKCU` writes for packaged apps, so this
value is stored in the package's private hive rather than the user's real one.

---

## Capability-to-feature map

| Feature | Code | Capability that enables it |
|---|---|---|
| System audio capture (WASAPI loopback) | `src/audio-engine/routing/wasapi_capture.cpp` | `microphone` (needs verification — may be covered by `runFullTrust`) |
| VST3 plugin hosting, in-process | `src/audio-engine/vst-host/` | `runFullTrust` |
| Preset files in Documents | `src/tray-app/presets/preset_folder_*.cpp` | `runFullTrust` (`documentsLibrary` redundant) |
| Settings JSON in `%AppData%` | `src/tray-app/settings/roaming_settings.cpp` | `runFullTrust` |
| Device GUID in `HKCU` | `src/shared/platform/windows_device.cpp` | `runFullTrust` (no capability needed) |

---

## Deliberately not declared

- **`networkInternet`** — the app makes no network calls. The spec's "Check for updates"
  HTTPS GET is not implemented; if it lands, this capability must be added and the privacy
  policy updated.
- **`webcam`**, **`removableStorage`**, **`userAccountInformation`**, **`audioLibrary`** —
  no code path uses any of these. `audioLibrary` in particular was documented here
  previously against a source file (`file_loader.cpp`) that does not exist.

---

## Open risk: loading user-supplied VST3 plugins

Not a capability question, but the largest unresolved certification risk and worth
recording next to the capability set: JyGlobalVST loads arbitrary user-supplied VST3 DLLs
from disk into its own process (`src/audio-engine/vst-host/plugin_scanner.cpp`).

**I do not have a verified source on how current Store policy treats a packaged full-trust
app that loads unsigned third-party binaries at runtime.** This could affect certification
regardless of the capability set. Resolve it against current Store policy before investing
in listing materials.

---

## Before you submit

- [ ] Decide on `documentsLibrary` (recommend: remove) and rebuild
- [ ] Verify the `microphone` requirement for loopback in an app container
- [ ] Confirm `runFullTrust` justification wording against current Partner Center guidance
- [ ] Resolve the third-party-plugin-loading policy question
- [ ] Re-verify this document against the manifest after any change
