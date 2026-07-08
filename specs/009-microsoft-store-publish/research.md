# Research: Microsoft Store Publishing via MSIX (Phase 0)

**Date**: 2026-07-05 | **Status**: Complete

## Overview

Research Phase 0 examined five key areas for Microsoft Store publication: MSIX packaging best practices, automatic code signing mechanisms, Microsoft Store certification requirements, Partner Center workflows, and local testing procedures. This document consolidates findings and resolves all clarification points from the specification.

---

## 1. MSIX Packaging Best Practices for Desktop Applications

### Decision
Adopt Microsoft's MSIX format using MSIX Packaging Tool (free) or Visual Studio 2022 Community Edition (free), both available without licensing cost.

### Rationale
- MSIX is Microsoft's modern, recommended packaging format for Windows desktop applications
- Provides application isolation, automatic updates, and rollback capabilities
- Both official tools (MSIX Packaging Tool and Visual Studio) are free and well-documented
- MSIX supports one-package-per-architecture (no universal binaries needed)
- Industry-standard format; widely supported by Microsoft Store and Windows distributions

### Best Practices Found

#### Manifest Structure
- AppxManifest.xml is the heart of MSIX packages
- Must declare all capabilities, extensions, and entry points upfront
- Version format: `Major.Minor.Build.Revision` (e.g., 1.0.0.0) — all values are integers
- Publisher Identity should use Microsoft-recommended format: `CN=CompanyName`

#### Icon & Asset Sizing
| Asset | Size(s) | Format | Purpose |
|-------|---------|--------|---------|
| Tile Logo | 44×44, 50×50, 150×150, 310×310 px | PNG | Start menu tiles |
| Store Logo | 50×50 px | PNG | Microsoft Store listings |
| Splash Screen | 620×300 px | PNG | Optional launch screen |
| Wide Logo | 310×150 px | PNG | Optional wide tile variant |

All PNG assets must support transparency (RGBA, not RGB).

#### Capability Declaration Strategy
- Declare ONLY capabilities the application actually uses
- Over-declaration risks rejection (Microsoft enforces strict capability validation)
- Common capabilities for audio/VST apps: `microphone`, `audioLibrary`, `documents`, `registryRead`, `registryWrite`
- Each capability must have a clear functional justification

### Alternatives Considered
- **AppInstaller format** (not chosen) — adds complexity for unimportant features
- **Cabinet (.cab) format** (not chosen) — legacy, deprecated by Microsoft
- **MSI format** (not chosen) — older standard, less sandbox support

---

## 2. Automatic Code Signing in MSIX

### Decision
Use automatic signing provided by MSIX Packaging Tool or Visual Studio, with no external .pfx certificate required.

### Rationale
- MSIX Packaging Tool can generate a test signing certificate automatically during development
- Visual Studio 2022 Community Edition can also auto-sign packages
- Test certificates are valid for development and testing; Microsoft Store handles signing for published packages
- Zero licensing cost; no Certificate Authority purchases needed
- Secure chain: Store validates signatures before distribution

### How Automatic Signing Works

**MSIX Packaging Tool Approach**:
1. Tool auto-generates a self-signed certificate during packaging
2. Signature is embedded in the package
3. For Store submission, upload unsigned package to Partner Center; Store applies their own production certificate
4. Downloaded packages are then signed by Microsoft Store infrastructure

**Visual Studio Approach**:
1. Project properties allow specifying an auto-generated temporary certificate
2. Build process signs the package automatically
3. Same principle: Store re-signs before distribution

**Development vs. Production Signing**:
- **Development**: Auto-signed packages for local testing only (will not install on clean systems)
- **Store Submission**: Submit UNSIGNED package to Partner Center; they apply production signatures
- **User Download**: Downloaded from Store are Microsoft-signed and can be installed anywhere

### Alternatives Considered
- **External Code Signing Certificate** (not chosen) — requires annual purchase ($200-400+), violates project constraint
- **Timestamp Authority signing** (not chosen) — adds complexity without benefit for Store-signed packages

---

## 3. Windows Store Certification Requirements & Common Rejections

### Decision
Implement strict validation during development to minimize Store certification failures; use Microsoft's official validation tools (MSAP, Partner Center validation APIs).

### Rationale
- Certification failures delay release and require resubmission cycles
- Microsoft maintains a public list of common failures; catching them early is cost-free
- MSAP (Microsoft Store App Certification Kit) is free and available for download
- Partner Center provides pre-submission validation APIs

### Common Certification Failures (and How to Avoid)

| Failure Category | Reason | Prevention |
|-----------------|--------|-----------|
| **Over-declared capabilities** | App requests capabilities it doesn't use | Audit manifest against actual code; test with minimal capabilities |
| **Minimum OS version mismatch** | Manifest declares OS 10.0.14393 but app uses 10.0.17763 APIs | Set realistic minimum (suggest 10.0.19041 for Windows 10 1909+) |
| **Missing entry point** | Applications element doesn't point to valid executable | Verify EXE path matches build output location |
| **Invalid icon dimensions** | Sizes wrong, format not PNG, no transparency | Use automated icon generator, validate sizes match spec |
| **Hardcoded paths** | App uses `C:\Users\...` instead of AppData or ProgramFiles | Test on clean VM; use environment variables only |
| **Persistent registry entries** | App stores state outside MSIX-managed locations | Use AppData or registry under `HKEY_CURRENT_USER\Software\` |
| **Telemetry without disclosure** | App sends data without user consent | Ensure privacy policy is accurate; disable telemetry or opt-in only |
| **Driver dependencies** | App requires kernel-mode drivers | Use WASAPI loopback (driverless) — JyGlobalVST does this correctly |

### Key Capability Declarations for Audio Applications

```xml
<!-- Audio capture & processing -->
<Capability Name="microphone" />
<Capability Name="audioLibrary" />

<!-- Settings persistence -->
<Capability Name="documentsLibrary" />

<!-- Registry access (if needed) -->
<uap:Capability Name="registryRead" />
<uap:Capability Name="registryWrite" />
```

**Note**: JyGlobalVST uses WASAPI loopback (Windows feature, not a driver), so no special driver capabilities needed.

### Alternatives Considered
- **Ignoring validation tools** (not chosen) — leads to rejection cycles and delays
- **Over-declaring capabilities** (not chosen) — increases rejection risk; Microsoft enforces strict validation

---

## 4. Partner Center Configuration & Submission Workflow

### Decision
Use Microsoft Partner Center (partner.microsoft.com) as the official submission platform; one-time account setup required.

### Rationale
- Partner Center is Microsoft's official platform for all Microsoft Store submissions
- Free account creation (no Publisher certification cost)
- Provides analytics, update management, and distribution controls
- Automated validation before certification review
- Clear rejection feedback if issues found

### Account Setup Steps (Summary)

1. **Create Microsoft Account** (free)
2. **Sign up for Partner Center** (free)
3. **Register as Publisher** (free for individual/small business; larger organizations may have verification requirements)
4. **Create App** in Partner Center (register app name)
5. **Submit for Certification** (choose pricing, regions, content ratings)

### Submission Workflow

| Step | Owner | Time | Notes |
|------|-------|------|-------|
| 1. Prepare metadata | Dev Team | 1-2 days | Icon, description, screenshots, version notes |
| 2. Prepare package | Dev Team | 1 day | Final .msixbundle from automated build |
| 3. Create app entry | Product Owner | 1 hour | Register app name, select categories |
| 4. Upload package | Product Owner | 1 hour | Upload .msixbundle to Partner Center |
| 5. Auto-validation | Microsoft | 30 min - 2 hours | MSAP checks run; feedback provided immediately |
| 6. Certification review | Microsoft | 24 hours (typical) | Humans review app for policy compliance |
| 7. Publish | Product Owner | (automated) | Store publishes after certification passes |

### Key Partner Center Concepts

- **App Name**: Must be unique in Microsoft Store; reserve early
- **Package Version**: Incremented for each submission; must be > previous version
- **Pricing Tier**: Free, trial, paid — choose one
- **Certification Categories**: Select appropriate categories (e.g., Utilities, Media, Tools)
- **Content Ratings**: Complete IARC questionnaire (quick, automated)
- **Privacy Policy**: Required (link to published privacy page)

### Alternatives Considered
- **Manual submission to Microsoft** (not chosen) — Partner Center is the only official path
- **Third-party distribution** (not chosen) — violates Store-only distribution goal

---

## 5. Local MSIX Testing Tools & Procedures

### Decision
Use PowerShell cmdlets (Add-AppxPackage, Remove-AppxPackage) and Microsoft's official validation tools (MSAP) for local testing.

### Rationale
- PowerShell is built-in to Windows; no additional downloads for basic testing
- MSAP (free download from Microsoft) provides comprehensive pre-submission validation
- Testing on a clean VM catches sandbox incompatibilities early
- Standard procedures that all Windows developers follow

### Local Testing Tools

#### PowerShell Cmdlets (Built-in)

```powershell
# Install a package (requires admin for first-time installation)
Add-AppxPackage -Path "C:\path\to\JyGlobalVST.msix"

# List installed MSIX apps
Get-AppxPackage | Where-Object { $_.Name -like "*JyGlobalVST*" }

# Uninstall package
Get-AppxPackage "*JyGlobalVST*" | Remove-AppxPackage

# Get detailed package info
Get-AppxPackageManifest -Package (Get-AppxPackage "*JyGlobalVST*")
```

#### MSAP (Microsoft Store App Certification Kit)

1. **Download**: https://learn.microsoft.com/en-us/windows/uwp/publish/
2. **Run**: `"C:\Program Files (x86)\Windows Kits\...\appcert.exe"` or GUI version
3. **Validate**: MSAP runs automated checks on the package manifest and files
4. **Output**: HTML report with pass/fail for 50+ checks

#### Visual Studio Built-in Tools

- **Manifest Designer**: Visual editor for AppxManifest.xml with validation
- **Create App Packages Wizard**: Integrated package generation with signing options
- **Package Validation**: Built-in validation against manifest schema

### Testing Checklist (Local)

- [ ] Install package without admin elevation (should work on clean user account)
- [ ] Application launches and system tray icon appears
- [ ] Core feature works (plugin chain loads, audio processing enabled)
- [ ] Settings persist across restart
- [ ] Uninstall via Settings → Apps → Installed Apps works cleanly
- [ ] No leftover files in `%AppData%` or registry after uninstall
- [ ] MSAP validation passes with zero errors, zero warnings
- [ ] Package manifest schema validation passes

### Alternatives Considered
- **Manual testing only** (not chosen) — misses manifest schema errors
- **WinAppDriver/Selenium testing** (deferred) — valid for UI testing, not needed for initial validation
- **Virtual machine snapshots** (good practice) — recommended for clean test environments

---

## 6. Minimum Windows Version Determination

### Decision
Target minimum Windows 10 1909 (build 18363) for the first Microsoft Store release.

### Rationale
- Windows 10 1909 was released in November 2018; EOL is May 2023 (still supported by Microsoft as of 2026)
- MSIX feature support is mature in this version
- Covers most users running supported Windows versions
- Application code uses C++20 standard, supported from Windows 10 1909 onwards with MSVC
- JUCE 8.x supports Windows 10 1909+ without compatibility issues

### Version Identifiers

```xml
<!-- In AppxManifest.xml -->
<MinVersion>10.0.18363.0</MinVersion>  <!-- Windows 10 1909 -->
<MaxVersionTested>10.0.26200.0</MaxVersionTested>  <!-- Windows 11 current -->
```

### Alternatives Considered
- **Windows 10 1909 (18363)** ✓ **Selected** — balance of reach and features
- **Windows 10 21H2 (19044)** — more recent, fewer older systems; reduces audience
- **Windows 11 only (22000)** — too restrictive; loses pre-Windows 11 audience

---

## 7. Signing Certificate Strategy

### Decision
- **Development**: Use auto-generated test certificate from MSIX Packaging Tool
- **Store Submission**: Submit UNSIGNED package to Partner Center (they apply production signing)
- **User Distribution**: Users download Store-signed packages from Microsoft Store

### Rationale
- **Zero cost**: Test certificates auto-generated, production signing done by Microsoft
- **Compliance**: Store signature is trusted by Windows; users get verified code
- **Simplicity**: No certificate management, renewal, or key escrow needed
- **Industry standard**: All Store apps follow this pattern

### Certificate Details

| Scenario | Certificate | Validity | Used For |
|----------|-------------|----------|----------|
| Local development | Test/self-signed (auto-generated) | Dev/test only | Testing on developer machine |
| Store submission | None (unsigned) | N/A | Upload to Partner Center |
| Distribution | Microsoft-signed | Verified by Windows | Users download from Store |

**Important**: Never ship development test certificates in production packages. Partner Center explicitly rejects unsigned or test-signed packages.

---

## Summary of Key Findings

| Research Area | Key Finding | Impact |
|---------------|------------|--------|
| **Packaging Format** | MSIX + free tools (MSIX Packaging Tool, Visual Studio) | ✓ Zero additional cost |
| **Signing** | Auto-signed in dev, Store-signed in production | ✓ No paid certificates needed |
| **Validation** | MSAP tool + Partner Center pre-checks | ✓ Catch issues early |
| **Submission** | Partner Center (free account) | ✓ Standard Microsoft platform |
| **Testing** | PowerShell cmdlets + MSAP | ✓ Built-in tools, no cost |
| **Min OS** | Windows 10 1909 (18363) | ✓ Good historical coverage |
| **Capabilities** | Strict validation; declare only what's used | ✓ Reduces rejection risk |
| **Assets** | PNG with transparency, specific sizes | ✓ Use asset generator tools |

---

## Dependencies & Tool Versions

### Required Tools

1. **MSIX Packaging Tool** (free)
   - Download: https://www.microsoft.com/en-us/p/msix-packaging-tool/9n5lw3jbcxkf
   - Version: Latest (supports all current Windows versions)
   - Purpose: Convert EXE → MSIX or create from scratch

2. **Visual Studio 2022 Community Edition** (free)
   - Download: https://visualstudio.microsoft.com/downloads/
   - Required workload: "Desktop development with C++"
   - Purpose: Build application, create/validate MSIX projects, auto-sign packages

3. **Windows SDK** (free, included with VS 2022)
   - Includes MSAP tool for package validation
   - Includes AppxManifest.xml schema validator

4. **PowerShell 5.1+** (built-in to Windows 10+)
   - Purpose: Automation scripts, local package testing

5. **CMake 3.22+** (already in use)
   - Purpose: Build application binary before packaging

### Optional Tools

- **Asset Generator Tools**: Third-party tools to auto-generate icon variants from single source image
- **WinAppDriver**: For UI testing (not needed for initial validation)
- **Fiddler/Wireshark**: Network analysis (if app does network calls; JyGlobalVST does not)

---

## Risk Assessment

### Low Risk (Mitigated)
- ✓ Automatic signing doesn't require external certificates → Use MSIX tool signing
- ✓ Icon sizing complexity → Use automated asset generator or template
- ✓ Manifest validation errors → Use MSAP before submission
- ✓ Partner Center learning curve → Documentation + Partner Center support articles

### Medium Risk (Addressed in Phase 1/2)
- ⚠️ Registry isolation in MSIX sandbox → Test settings persistence on clean VM
- ⚠️ Capability over-declaration → Audit manifest line-by-line during review
- ⚠️ Build automation complexity → PowerShell script with clear error handling

### No Remaining Blockers
- All research items resolved
- All clarifications addressed
- Ready to proceed to Phase 1 design

