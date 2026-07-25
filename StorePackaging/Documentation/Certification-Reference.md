# Microsoft Store Certification Reference

Failure modes to check before submitting, and an honest accounting of what is verified
versus assumed.

> **Read this first.** An earlier version of this document asserted specific Store policy
> numbers, package size expectations and review turnaround times, and recommended
> declaring `registryRead` / `registryWrite`, which are not real MSIX capability names.
> Those claims were not sourced. This rewrite separates what has been verified in this
> repository from what still needs checking against Microsoft's current documentation.
>
> **Store policy and Partner Center requirements change.** Nothing here substitutes for
> Microsoft's current published requirements.

---

## Verified locally

Confirmed by building and installing the package on 2026-07-24 (Windows 11 build 26200):

| Check | Status |
|---|---|
| Manifest passes MakeAppx schema validation | Pass |
| Package installs via `Add-AppxPackage` | Pass |
| App appears in Start menu and launches | Pass |
| Tray UI runs, WASAPI loopback capture works in the app container | Pass |
| All 5 logos: exact dimensions, 32-bit ARGB | Pass |
| Version format `Major.Minor.Build.Revision` | Pass |
| Architecture: x64 only, `MinVersion` 10.0.18363.0 | Pass |
| No elevation required to run | Pass |
| Package size | 3.5 MB |

`Validate-Package.ps1` re-checks all of these mechanically.

---

## Unresolved — check before submitting

### 1. Loading user-supplied VST3 plugins

**The biggest open risk.** JyGlobalVST loads arbitrary user-supplied VST3 DLLs from disk
into its own process. **I have no verified source on how current Store policy treats a
packaged full-trust app that loads unsigned third-party binaries at runtime.**

Check this first — it is cheap to research and a negative answer invalidates the rest of the
submission work.

### 2. `microphone` for loopback capture

The manifest declares `microphone` for WASAPI loopback. Capture works in the installed
package, but that does not prove the capability is what enables it — `runFullTrust` may be
sufficient alone. Separately, a reviewer may question a microphone declaration in an app
that does not record the user. Be ready to explain loopback. See
[Capability-Declaration.md](Capability-Declaration.md).

### 3. `runFullTrust` justification

A restricted capability. Routine for packaged Win32 desktop apps, but confirm the current
expected justification wording against Partner Center's guidance.

### 4. Privacy policy requirement

Very likely required given the `microphone` declaration. Confirm the current rule, then
write and host one. The app performs no network activity and no telemetry, which makes the
policy short — but it should explain that loopback captures system audio output, not
microphone input.

### 5. Accepted upload format

The pipeline produces `.msix` and `.msixbundle`. Verify which Partner Center currently
expects for a single-architecture desktop app before uploading.

---

## Common rejection causes and how this project stands

### Over-declared capabilities

Declare only what is used. **This project's status: resolved.** `documentsLibrary` was
removed on 2026-07-24 as redundant under `runFullTrust`; the manifest now declares only
`runFullTrust` and `microphone`. See
[Capability-Declaration.md](Capability-Declaration.md).

### Identity mismatch

The manifest identity must match the identity Partner Center assigned to the reserved app
name. **This project's status: blocking.** The manifest carries placeholders
(`JyGlobalVST` / `CN=JyGlobalVST, O=JyGlobalVST`). `Build-MSIX.ps1` refuses to build
without real values unless `-AllowPlaceholderIdentity` is passed.

### Version problems

Store versions must strictly increase; an accepted version cannot be reused or downgraded.
Keep `version-info.txt` as the single source of truth and let the scripts stamp the manifest
so the filename and manifest cannot disagree.

### Manifest schema errors

MakeAppx validates the manifest at pack time and names the offending line and reason. A
package that packs has already cleared schema validation. Two real examples hit while
building this project:

- `StartPage` instead of `Executable` + `EntryPoint="Windows.FullTrustApplication"` — a
  packaged Win32 app will not deploy with the hosted-web-app form
- `DefaultTile` specifying `Square310x310Logo` without a companion `Wide310x150Logo`

### Missing or wrong assets

All logos must exist, be PNG, and have exact pixel dimensions.
`Validate-Package.ps1` checks dimensions and the alpha channel. **Status: passing**, though
the icons are functional placeholders rather than designed artwork — worth improving before
a public listing.

### Signature problems

Submit **unsigned**; Partner Center applies the Store signature. `Build-MSIX.ps1` produces
unsigned output deliberately. Only the local sideload build is self-signed, and its
certificate lives in the certificate store, never in the repository.

### Hardcoded paths

The app uses per-user locations throughout, no hardcoded user paths:

- `%AppData%\Roaming\JyGlobalVST\settings.json` — roaming settings
- `%LocalAppData%\JyGlobalVST\` — scan cache, autosave, window geometry
- `%UserProfile%\Documents\JyGlobalVST\Presets\*.jvst` — user presets
- `HKCU\Software\JyGlobalVST\AudioDevice` — device GUID

Note that MSIX virtualizes `%AppData%` and `HKCU` writes for packaged apps, so a packaged
install keeps state separate from an unpackaged build of the same app. Expected, but it
means "settings did not carry over" is not a bug.

### Elevation

The app requires no elevation. Packaged full-trust apps run as the standard user.

### Incomplete listing

**Status: blocking.** No screenshots, description, category, age rating, support contact or
privacy policy URL exist yet. None of this lives in the repo — it is all entered in Partner
Center.

---

## Pre-submission checks

```powershell
# Build with real identity; validation runs automatically and fails on error
.\StorePackaging\Scripts\Build-MSIX.ps1 -PackageName "..." -Publisher "..." -PublisherDisplayName "..."

# Validate the bundle you will actually upload
.\StorePackaging\Scripts\Validate-Package.ps1 -PackagePath build\store-packages\store\JyGlobalVST_<ver>_x64.msixbundle
```

Then, manually — ideally on a clean VM, using a **local** self-signed build since the Store
package will not sideload:

- [ ] Install, launch, exercise the plugin chain with real VST3 plugins
- [ ] Confirm settings and presets persist across restart
- [ ] Uninstall and check for orphaned state

See [Local-Testing-Guide.md](Local-Testing-Guide.md) for detailed scenarios.

---

## If rejected

Read the feedback, fix, bump the version, rebuild, resubmit. I have no verified figure for
current review or resubmission turnaround times — Partner Center reports status for your
submission directly, so rely on that rather than any number quoted here.
