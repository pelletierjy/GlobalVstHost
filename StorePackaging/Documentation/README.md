# StorePackaging

MSIX packaging for JyGlobalVST: local sideload testing and Microsoft Store submission.

> **Submission status: not ready.** The package builds, installs and runs locally, but the
> manifest still carries a placeholder identity and no Partner Center account exists yet.
> See [Submission readiness](#submission-readiness) below.

---

## Two build paths

There are two scripts and they produce deliberately different artifacts. Using the wrong
one is the most common mistake.

| | `Build-MSIX-Local.ps1` | `Build-MSIX.ps1` |
|---|---|---|
| Purpose | Install and test on this machine | Upload to Partner Center |
| Signing | Self-signed (script creates the cert) | **Unsigned** — Partner Center signs |
| Identity | Placeholder from the manifest | Must be your Partner Center identity |
| Output | `build/store-packages/local/` | `build/store-packages/store/` |
| Installable by sideloading | Yes | No |

### Build and install locally

```powershell
.\StorePackaging\Scripts\Build-MSIX-Local.ps1
```

Then follow the two commands it prints. They run as **different accounts**:

```powershell
# 1) ELEVATED - trusts the signer machine-wide, once per certificate
Import-Certificate -FilePath 'build\store-packages\local\JyGlobalVST-LocalTest.cer' `
                   -CertStoreLocation Cert:\LocalMachine\TrustedPeople

# 2) NOT elevated, as the logged-in user
Add-AppxPackage -Path 'build\store-packages\local\JyGlobalVST_1.0.0.0_x64.msix'
```

MSIX installs are per-user. If your elevated shell runs as a different administrator
account, step 2 registers the app for *that* account and it will never appear in your Start
menu — with no error.

Once the certificate is trusted, later rebuilds are one command:

```powershell
.\StorePackaging\Scripts\Build-MSIX-Local.ps1 -Install
```

Remove with `Get-AppxPackage -Name JyGlobalVST | Remove-AppxPackage`.

### Build a Store package

```powershell
.\StorePackaging\Scripts\Build-MSIX.ps1 `
    -PackageName          "<Partner Center Identity/Name>" `
    -Publisher            "<Partner Center Identity/Publisher>" `
    -PublisherDisplayName "<Partner Center seller display name>"
```

These three values come from Partner Center → your app → Product management → Product
identity. The script **refuses to build** with the placeholder identity unless you pass
`-AllowPlaceholderIdentity`, because Partner Center rejects such packages at upload.

---

## Directory contents

```
StorePackaging/
├── AppxManifest.xml                 # Package manifest (placeholder identity)
├── version-info.txt                 # Version, single source of truth
├── Assets/                          # Logo PNGs (5 files)
├── Scripts/
│   ├── _Common.ps1                  # Shared helpers, dot-sourced (not run directly)
│   ├── Build-MSIX.ps1               # Store submission package (unsigned)
│   ├── Build-MSIX-Local.ps1         # Local sideload package (self-signed)
│   ├── Validate-Package.ps1         # Manifest / logo / runtime checks
│   ├── Test-LocalInstall.ps1        # Install, launch, uninstall smoke test
│   └── Prepare-Release.ps1          # Bump version-info.txt, then build
└── Documentation/
    ├── README.md                    # This file
    ├── MSIX-Build-Guide.md          # Build mechanics and prerequisites
    ├── Local-Testing-Guide.md       # Manual test scenarios
    ├── Capability-Declaration.md    # Why each capability is declared
    ├── Certification-Reference.md   # Failure modes and what is unverified
    ├── Partner-Center-Checklist.md  # Pre-submission checklist
    ├── Partner-Center-Submission.md # Submission walkthrough
    └── Implementation-Notes.md      # Design decisions and constraints
```

Nothing else exists. Earlier versions of this file documented `Test-PackageFull.ps1`,
`Run-MSAP.ps1`, `Version-Management.md` and `AppxManifest-Reference.md`; none of those were
ever written. `JyGlobalVST.msixproj` was deleted — it declared
`ConfigurationType=AppPackage` and imported `Microsoft.Cpp.targets`, which is not a valid
packaging project. Packaging now uses the Windows SDK `MakeAppx` directly.

---

## How packaging works

Both scripts share `New-PackagePayload` in `_Common.ps1`, which stages:

1. `JyGlobalVST.exe` from `build/src/tray-app/jyglobalvst_tray_artefacts/<Config>/`
2. `Assets/*.png` — PNG only, so stray files never enter the package
3. `AppxManifest.xml` with version (and, for Store builds, identity) stamped in
4. The **dynamic CRT DLLs** — `msvcp140.dll`, `vcruntime140.dll`, `vcruntime140_1.dll` and
   friends, copied from the VS redist directory

Step 4 is not optional. The app links the dynamic CRT and
`C:\Program Files\WindowsApps` is not a redist search path, so a package without these
DLLs installs fine and then fails to launch. `Validate-Package.ps1` checks for them.

The user guide is compiled into the executable as JUCE `BinaryData`, so no HTML asset needs
staging.

---

## Prerequisites

- Windows 10 1909+ / Windows 11, x64
- CMake 3.22+
- Visual Studio with the C++ toolset (any version with a `VC\Redist\MSVC` directory)
- Windows 10/11 SDK — supplies `MakeAppx.exe` and `SignTool.exe`

The scripts locate the SDK and redist automatically, picking the newest installed, and fail
with a clear message if either is absent. No MSIX Packaging Tool or packaging-project
workload is required.

---

## CI

`.github/workflows/store-package.yml` builds a Store package on `v*.*.*` /
`store-v*` tags and on manual dispatch. Identity comes from repository variables:

- `MSIX_PACKAGE_NAME`
- `MSIX_PUBLISHER`
- `MSIX_PUBLISHER_DISPLAY_NAME`

If those are unset the workflow still runs, but emits a warning and builds with the
placeholder identity, marking the artifact non-submittable. Tag builds create a **draft**
release — the artifact is an unsigned Store package that will not sideload, so it should not
be published as a public download.

---

## Submission readiness

Verified working:

- Manifest is schema-valid; package installs, appears in the Start menu, launches, and the
  tray app processes system audio (tested 2026-07-24, Windows 11 26200)
- All five logos: correct dimensions, 32-bit ARGB
- `Validate-Package.ps1` passes on both `.msix` and `.msixbundle`

Blocking submission:

- **Placeholder identity.** Needs a Partner Center account and a reserved app name.
- **No listing materials.** No screenshots, no privacy policy URL, no age rating. A
  privacy policy is likely required given the `microphone` declaration — confirm against
  current Partner Center requirements.
- **Unresolved policy question.** Whether a packaged full-trust app that loads arbitrary
  user-supplied VST3 DLLs passes certification. I have no verified source on this. It is
  the cheapest thing to check and could invalidate the rest of the work.

See [Partner-Center-Checklist.md](Partner-Center-Checklist.md) for the full list.

---

## References

- MSIX documentation: https://learn.microsoft.com/en-us/windows/msix/
- AppxManifest schema: https://learn.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/
- Partner Center: https://partner.microsoft.com/

Store policy and Partner Center requirements change. Treat the process descriptions in this
directory as a starting point and verify against Microsoft's current documentation.
