# Implementation Notes: Microsoft Store MSIX Publishing

**Project**: JyGlobalVST  
**Feature**: Microsoft Store Publication via MSIX Packaging  
**Last verified against code**: 2026-07-24  
**Status**: packaging works locally; **not submitted, not submittable** — see
[Partner-Center-Checklist.md](Partner-Center-Checklist.md)

---

## Project Scope

### In Scope (v1)
- Windows 10 1909+ and Windows 11 support (x64 only) — **done**
- MSIX packaging via Windows SDK MakeAppx — **done**
- Self-signed local sideload build for testing — **done, verified end to end**
- GitHub Actions CI pipeline — **done**, but produces a non-submittable package until the
  Partner Center identity variables are configured
- Local testing procedures — **partly automated** (`Test-LocalInstall.ps1`); functional and
  persistence checks remain manual
- Partner Center submission materials — **not started**: no screenshots, description, age
  rating or privacy policy

### Out of Scope (v2+)
- 32-bit (x86) and ARM64 architecture support
- macOS or Linux distribution
- VST2 plugin hosting
- Multiple language versions
- Localized UI beyond English

---

## Key Decisions & Rationale

### 1. MSIX Format (vs. MSI or Standalone EXE)

**Decision**: Use MSIX exclusively for Store distribution  
**Rationale**:
- Modern Microsoft standard format
- Automatic app isolation and updates
- Clean uninstall (no orphaned files)
- Microsoft Store native support
- No cost vs. traditional signing

### 2. Self-Signed Local Certificate (vs. Purchased Certificates)

**Decision**: sign local test packages with a self-signed certificate generated on demand;
submit Store packages unsigned.

**How it actually works** — there is no "automatic signing from MSIX tools":

- `Build-MSIX-Local.ps1` creates a self-signed code-signing certificate in
  `Cert:\CurrentUser\My` whose subject matches the manifest `Publisher` exactly (SignTool
  requires the match), then signs with SignTool. It reuses the certificate on later runs.
- Installing that package requires importing the exported `.cer` into
  `LocalMachine\TrustedPeople` once, which needs elevation.
- `Build-MSIX.ps1` produces an **unsigned** package. Partner Center applies the Store
  signature. A developer signature on a submission is not wanted.

**Rationale**: zero cost, no certificate authority needed, and the Store handles production
signing. The tradeoff is that the local package is not distributable — anyone else would
have to trust the certificate manually.

No `.pfx` is ever written to disk or committed; the private key stays in the user's
certificate store.

### 3. x64 Only (vs. Multi-Architecture)

**Decision**: v1 targets x64 only  
**Rationale**:
- 32-bit Windows declining market share
- Reduces testing and build complexity
- Can add ARM64 support in future for Surface devices
- Aligns with project's C++20 and JUCE requirements

### 4. GitHub Actions (vs. Azure Pipelines or Other CI)

**Decision**: Use GitHub Actions for CI/CD  
**Rationale**:
- Native GitHub integration
- Free tier sufficient for releases
- Windows runner available
- Simpler configuration than alternatives

### 5. Free Hosting (vs. Self-Hosted Distribution)

**Decision**: Use Microsoft Store for distribution  
**Rationale**:
- Free hosting (v1)
- Automatic updates via Store
- No server maintenance
- User trust via Store certification

---

## Assumptions & Constraints

### Assumptions Made

1. **Build Environment**: CMake 3.22+, a Visual Studio C++ toolset (any version providing
   `VC\Redist\MSVC`), and a Windows 10/11 SDK for MakeAppx/SignTool. Not pinned to VS 2022 —
   the scripts discover the newest installed SDK and redist at runtime.
2. **Application Stability**: Core audio functionality is stable and tested
3. **No Breaking Changes**: API/settings format won't change between releases
4. **Settings Migration**: Not implementing cross-version settings migration in v1
5. **User Presets**: User-created presets not included in package; only system presets included

### Project Constraints

1. **No Cost**: Cannot purchase code-signing certificates
2. **Windows Only**: No macOS, Linux, or mobile targets
3. **x64 Only**: 32-bit and ARM64 deferred to future releases
4. **No Telemetry**: No crash reporting or usage analytics
5. **No Network**: v1 doesn't use network features (Check for Updates deferred)

---

## Known Limitations

### Current (v1)

1. **Icon Placeholders**: Icons are placeholders; must be replaced with proper designs
2. **No VST2**: Only VST3 plugins supported (VST2 deferred)
3. **English Only**: No localized versions in v1
4. **No Cloud Sync**: Settings don't sync across devices
5. **Manual Presets**: No cloud preset sharing

### By Design

1. **No Admin Rights**: App doesn't require elevation (by design, good for Store)
2. **No Telemetry**: No usage reporting to Microsoft or third parties
3. **Minimal Capabilities**: Only declares capabilities actually used
4. **Driverless Audio**: Uses WASAPI loopback (no virtual driver needed)

---

## Technical Decisions

### Manifest Version Format

Format: `Major.Minor.Build.Revision` (e.g., `1.0.0.0`)
- All components are integers
- Increments with each Store release
- Matches Windows version numbering convention
- MUST increase for each submission (no downgrades allowed)

### Settings Persistence

- **Roaming Settings** (`%AppData%\JyGlobalVST\settings.json`): User preferences, window position, last device
- **Local Settings** (`%LocalAppData%\JyGlobalVST\`): Scan cache, auto-save chain, temporary files
- **Presets** (`%UserProfile%\Documents\JyGlobalVST\Presets\*.jvst`): User-created and system presets

### Release Versioning

Use semantic versioning:
- `1.0.0.0` - Initial release
- `1.0.1.0` - Bug fixes (patch)
- `1.1.0.0` - New features (minor)
- `2.0.0.0` - Major breaking changes

---

## Maintenance & Future Work

### Ongoing Maintenance

- Monitor Store reviews and ratings
- Respond to user feedback
- Track any certification feedback patterns
- Update privacy policy if data practices change

### v1.1+ Roadmap (Potential)

- [ ] High-quality app icons designed by professional
- [ ] Store screenshots and app description videos
- [ ] Multi-language localization
- [ ] Windows 11 Widgets (if applicable)
- [ ] Performance optimizations

### v2.0+ Roadmap (Future)

- [ ] VST2 plugin support (requires SDK licensing review)
- [ ] ARM64 architecture support
- [ ] Cloud preset sharing (requires backend)
- [ ] Remote device control (requires network)
- [ ] Real-time audio analysis and visualization

---

## Testing Strategy

### Pre-Submission Testing

1. **Unit / integration tests**: `ctest --test-dir build -C Release`
2. **Package validation**: `Validate-Package.ps1` — manifest elements, full-trust wiring,
   logo dimensions and alpha, bundled CRT DLLs
3. **Local installation**: install the self-signed local build (the Store build cannot be
   sideloaded), ideally on a clean VM
4. **Functional verification**: plugin chain with real VST3 plugins, settings and preset
   persistence across restart, clean uninstall

### Certification Testing

Microsoft performs its own review. I have no verified figure for current review turnaround —
rely on the status Partner Center reports for your submission rather than any number quoted
in this repository.

---

## Security Considerations

### Signing & Distribution

- **Development Signing**: Auto-generated test certificate (for development testing only)
- **Store Signing**: Microsoft applies production signature before distribution
- **Certificate Chain**: Trusted by Windows via Microsoft's certificate authority

### Capability Declarations

Two declared: `runFullTrust` and `microphone` (`documentsLibrary` was removed 2026-07-24). See
[Capability-Declaration.md](Capability-Declaration.md) for justification and open questions.

Be clear-eyed about what this does and does not constrain: **`runFullTrust` gives the app
the file-system and registry reach of the launching user.** The other two capabilities do
not narrow that. The app *by convention* confines itself to:

- `HKCU\Software\JyGlobalVST\AudioDevice` (device GUID)
- `%AppData%\Roaming\JyGlobalVST\`, `%LocalAppData%\JyGlobalVST\`
- `%UserProfile%\Documents\JyGlobalVST\Presets\`

That is a property of the code, not a sandbox the platform enforces. MSIX does virtualize
`%AppData%` and `HKCU` writes for packaged apps, so packaged state is separate from an
unpackaged build's state.

A `microphone` prompt and the Windows privacy indicator will appear because of the loopback
capture. Explain this in the Store listing.

### No Telemetry

- No crash reporting
- No usage analytics
- No phone-home functionality
- All telemetry is opt-in (not implemented in v1)

---

## Build & Release Process

### Local Build

```powershell
# Installable test package
.\StorePackaging\Scripts\Build-MSIX-Local.ps1 -Install

# Store submission package (requires the Partner Center identity)
.\StorePackaging\Scripts\Build-MSIX.ps1 -PackageName "..." -Publisher "..." -PublisherDisplayName "..."
```

### CI Build

1. Configure repository variables `MSIX_PACKAGE_NAME`, `MSIX_PUBLISHER`,
   `MSIX_PUBLISHER_DISPLAY_NAME` — without them the workflow warns and builds a
   non-submittable placeholder package
2. Tag and push: `git tag v1.0.0.0 && git push origin v1.0.0.0`
3. GitHub Actions builds, validates the `.msix` and the `.msixbundle`, uploads artifacts,
   and opens a **draft** release (drafted deliberately: the artifact is unsigned and will
   not sideload, so it should not be offered as a public download)

### Store Submission

Not yet performed. See [Partner-Center-Submission.md](Partner-Center-Submission.md) for the
walkthrough and [Partner-Center-Checklist.md](Partner-Center-Checklist.md) for what still
blocks it.

---

## Reference Documentation

- [MSIX-Build-Guide.md](MSIX-Build-Guide.md) - How to build locally
- [Local-Testing-Guide.md](Local-Testing-Guide.md) - How to test packages
- [Partner-Center-Checklist.md](Partner-Center-Checklist.md) - Submission checklist
- [Capability-Declaration.md](Capability-Declaration.md) - Why each capability is declared
- [Certification-Reference.md](Certification-Reference.md) - Common issues & solutions

---

## Contact & Support

For questions about MSIX packaging:
1. Review documentation in `StorePackaging/Documentation/`
2. Check Microsoft's official MSIX documentation
3. Visit Microsoft Partner Center support

