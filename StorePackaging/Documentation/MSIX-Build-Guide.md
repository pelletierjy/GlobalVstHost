# MSIX Build Guide

How to build JyGlobalVST MSIX packages. For which script to use and why, start with
[README.md](README.md).

---

## Prerequisites

| Requirement | Notes |
|---|---|
| Windows 10 1909+ / Windows 11, x64 | |
| CMake 3.22+ | Must be on `PATH` |
| Visual Studio with C++ toolset | Any version providing `VC\Redist\MSVC\<ver>\x64\Microsoft.VC*.CRT` |
| Windows 10/11 SDK | Provides `MakeAppx.exe` and `SignTool.exe` |

The scripts discover the SDK via `%ProgramFiles(x86)%\Windows Kits\10\bin\<version>\x64\`
and the redist via `vswhere`, choosing the newest installed in each case. Both fail with an
explicit message if missing.

No MSIX Packaging Tool and no Visual Studio packaging-project workload are needed.
Packaging invokes `MakeAppx` directly.

---

## Local test package

```powershell
# From the repo root
.\StorePackaging\Scripts\Build-MSIX-Local.ps1
```

Useful switches:

| Switch | Effect |
|---|---|
| `-SkipBuild` | Reuse the existing executable; skip CMake |
| `-Install` | `Add-AppxPackage` after building (removes any prior install first) |
| `-InstallCert` | Import the cert into `LocalMachine\TrustedPeople` — **requires elevation** |
| `-Version` | Override the version from `version-info.txt` |
| `-Configuration` | `Release` (default) or `Debug` |
| `-BuildDir` | Alternate CMake build directory |

Output lands in `build/store-packages/local/`.

The script creates a self-signed certificate whose subject matches the manifest
`Publisher` exactly — SignTool requires this — and reuses it on later runs rather than
generating a new one each time.

---

## Store submission package

```powershell
.\StorePackaging\Scripts\Build-MSIX.ps1 `
    -PackageName          "12345Contoso.JyGlobalVST" `
    -Publisher            "CN=1B2C3D4E-5F60-7182-93A4-B5C6D7E8F901" `
    -PublisherDisplayName "Contoso"
```

Output lands in `build/store-packages/store/`:

- `JyGlobalVST_<version>_x64.msix`
- `JyGlobalVST_<version>_x64.msixbundle` — upload this
- `AppxManifest.xml` — copy of the stamped manifest, useful for diffing releases

The package is **unsigned by design**. Partner Center applies the Store signature; a
developer signature on a submission is not wanted.

Since only x64 is in scope, the bundle wraps a single package. It is produced because
Partner Center and the release pipeline expect a `.msixbundle`; a bare `.msix` is also
accepted for a single-architecture app.

### The identity gate

`Build-MSIX.ps1` refuses to build if `Identity/@Name` is `JyGlobalVST` or
`Identity/@Publisher` is `CN=JyGlobalVST, O=JyGlobalVST` — the placeholders checked into
`AppxManifest.xml`. Partner Center rejects packages carrying them.

Get the real values from Partner Center → your app → Product management → Product identity.

To exercise the pipeline before you have an account:

```powershell
.\StorePackaging\Scripts\Build-MSIX.ps1 -AllowPlaceholderIdentity
```

The output is valid for inspection and validation but **cannot be submitted**. Both the
build and the validator say so loudly.

---

## Validation

Runs automatically at the end of `Build-MSIX.ps1`, or standalone:

```powershell
.\StorePackaging\Scripts\Validate-Package.ps1 `
    -PackagePath build\store-packages\store\JyGlobalVST_1.0.0.0_x64.msixbundle
```

Accepts `.msix` or `.msixbundle` (bundles are unbundled first). It checks:

- Required manifest elements: `Identity`, `Properties`, `Dependencies`, `Resources`,
  `Applications`, `Capabilities`
- Version format `Major.Minor.Build.Revision`
- Full-trust desktop wiring: `Executable` present, `EntryPoint` is
  `Windows.FullTrustApplication`, `runFullTrust` declared, declared exe actually in the
  package
- Placeholder identity (warning)
- Every manifest-referenced logo: present, PNG, exact expected pixel dimensions, alpha
  channel
- `MSVCP140.dll`, `VCRUNTIME140.dll`, `VCRUNTIME140_1.dll` present in the payload

Exit code 1 on any error; 0 with warnings. This is a local sanity check — it cannot predict
certification outcomes.

---

## Release flow

```powershell
# 1. Bump the version
.\StorePackaging\Scripts\Prepare-Release.ps1 -Version "1.0.1"

# 2. Test the local package
.\StorePackaging\Scripts\Build-MSIX-Local.ps1 -Install

# 3. Tag and push - CI builds the Store package and opens a draft release
git tag v1.0.1.0
git push origin v1.0.1.0
```

Store versions must strictly increase; you cannot resubmit or downgrade a version already
accepted.

For CI identity configuration see [README.md](README.md#ci).

---

## Troubleshooting

| Symptom | Cause and fix |
|---|---|
| `MakeAppx.exe not found` | Install the Windows 10/11 SDK |
| `cmake not found on PATH` | Install CMake, or open a Developer PowerShell |
| `No VC++ runtime DLLs staged` | `vswhere` or the redist directory is missing — install the VS C++ toolset |
| MakeAppx `error 80080204` | Manifest schema violation; the message names the line and reason |
| `Add-AppxPackage` fails, untrusted signature | Certificate not in `LocalMachine\TrustedPeople`; run the `Import-Certificate` step elevated |
| Installs "successfully" but no Start menu entry | You installed as a different user. MSIX is per-user — run `Add-AppxPackage` unelevated as the logged-in account |
| App installs but will not launch | Missing CRT DLLs. Run `Validate-Package.ps1`; `WindowsApps` is not a redist search path |

---

## Related

- [Local-Testing-Guide.md](Local-Testing-Guide.md) — manual test scenarios
- [Capability-Declaration.md](Capability-Declaration.md) — declared capabilities and open questions
- [Partner-Center-Checklist.md](Partner-Center-Checklist.md) — pre-submission checklist
