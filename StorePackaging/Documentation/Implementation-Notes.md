# Implementation Notes: Microsoft Store MSIX Publishing

**Project**: JyGlobalVST  
**Feature**: Microsoft Store Publication via MSIX Packaging  
**Implementation Date**: 2026-07-05  
**Status**: v1.0 (Initial Release)

---

## Project Scope

### In Scope (v1)
- ✓ Windows 10 1909+ and Windows 11 support (x64 only)
- ✓ MSIX packaging with automatic signing
- ✓ GitHub Actions CI/CD pipeline
- ✓ Local testing procedures
- ✓ Partner Center submission materials
- ✓ Comprehensive documentation

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

### 2. Automatic Signing (vs. Purchased Certificates)

**Decision**: Use free automatic signing from MSIX tools  
**Rationale**:
- Zero cost (satisfies project constraint)
- Development signing for testing
- Microsoft Store handles production signing
- Industry-standard practice for Store apps

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

1. **Build Environment**: Developer machines have Visual Studio 2022 Community + CMake
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

1. **Unit Tests**: Run existing audio engine tests
2. **Integration Tests**: Full audio chain with VST3 plugins
3. **MSIX Validation**: MSAP tool validation (zero errors)
4. **Local Installation**: Test on clean Windows 10/11 VM
5. **Functionality Verification**: Core features work in MSIX environment

### Certification Testing

- Microsoft performs additional security and functionality testing
- Typical timeline: 24-48 hours
- Feedback provided if rejections occur

---

## Security Considerations

### Signing & Distribution

- **Development Signing**: Auto-generated test certificate (for development testing only)
- **Store Signing**: Microsoft applies production signature before distribution
- **Certificate Chain**: Trusted by Windows via Microsoft's certificate authority

### Capability Declarations

- **Principle of Least Privilege**: Only declare truly necessary capabilities
- **User Transparency**: Users see permission requests for sensitive operations
- **Registry Isolation**: Access limited to `HKEY_CURRENT_USER\Software\JyGlobalVST\`
- **File Access**: Limited to Documents, Music, and AppData folders

### No Telemetry

- No crash reporting
- No usage analytics
- No phone-home functionality
- All telemetry is opt-in (not implemented in v1)

---

## Build & Release Process

### Local Build

```powershell
.\StorePackaging\Scripts\Build-MSIX.ps1 -Version 1.0.0.0
```

### CI/CD Build

1. Create git tag: `git tag v1.0.0.0`
2. Push tag: `git push origin v1.0.0.0`
3. GitHub Actions automatically:
   - Builds MSIX package
   - Validates package
   - Creates release with artifacts

### Store Submission

1. Download .msixbundle from release
2. Sign into Partner Center
3. Create new submission
4. Upload .msixbundle
5. Fill app details
6. Submit for certification
7. Monitor certification progress

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

