# Spec: WinGet Distribution (Driverless v1.0)

**Feature Code**: 007  
**Status**: Scaffolding complete, build issue pending  
**Related**: 005 (driverless audio capture), 006 (built-in plugins)

## Overview

Package the driverless testable-dev build (features 005–006) for distribution via Windows Package Manager (WinGet) as v1.0.0, avoiding the WHQL/signing overhead of the virtual driver path while maximizing user reach.

## Rationale

- **User-friendly**: Single `winget install JyGlobalVST.GlobalVSTHost` command
- **No driver headaches**: Leverages existing WASAPI loopback (Windows 10 1909+)
- **Fast to market**: Scaffold complete; build issues are temporary CMake/JUCE config
- **Professional**: Signed MSI, proper Add/Remove Programs registration
- **Upgradeable**: WinGet auto-detection of newer versions

## Scope

### In Scope

- WiX 4 MSI installer (per-user install, silent mode support)
- Per-user installation to `%LocalAppData%\JyGlobalVST\`
- Auto-launch on user login via Registry Run key
- Start Menu + Desktop shortcuts
- WinGet manifest (YAML)
- GitHub Releases distribution
- Basic code signing (OV certificate, not EV)

### Out of Scope (for v1.0)

- Virtual driver (WaveRT/ASIO) — deferred to future release
- WHQL submission
- Service mode (pre-login audio)
- Accessibility (UIA, keyboard nav, screen readers)
- Auto-update via Windows Update
- Microsoft Store distribution

## Technical Requirements

**Installer:**
- MSI file format (Windows Installer)
- x64 architecture only
- Windows 10 1909+ and Windows 11 support
- Silent install support (`/quiet`, `/quiet /norestart`)
- Clean uninstall with no orphaned files
- Support install/uninstall for both admin and standard users

**Binaries:**
- `JyGlobalVST.exe` (tray application)
- `jyglobalvst_audio_engine.dll` (audio engine)
- All dependencies bundled or use Windows system libraries

**Distribution:**
- GitHub Releases: stable, version-tagged
- WinGet metadata: YAML manifests (schema v1.12.0)
- SHA256 hashing for integrity validation
- HTTPS URLs only

## Deliverables

### Phase 1: Scaffolding (Complete ✅)

- [x] WiX product.wxs definition
- [x] CMakeLists.txt integration
- [x] test-msi.ps1 automated test script
- [x] SETUP.md detailed build guide
- [x] README.md quick reference
- [x] IMAGES.md branding guide
- [x] license.rtf template
- [x] INSTALLER_SETUP_COMPLETE.md checklist
- [x] WiX Toolset v4 installation

**Status**: All files committed to Fix-plugins branch

### Phase 2: Build & Test (Pending)

- [ ] Release binaries build successfully (blocked: JUCE linker error)
- [ ] MSI builds without errors
- [ ] test-msi.ps1 passes all checks (install → verify → uninstall)
- [ ] Manual silent install test: `msiexec /i ... /quiet`

### Phase 3: GitHub Release

- [ ] Tag v1.0.0
- [ ] Create GitHub Release
- [ ] Attach signed MSI binary
- [ ] Compute SHA256 hash
- [ ] Write release notes

### Phase 4: WinGet Submission

- [ ] Create manifests: version, installer, locale YAML
- [ ] Create manifests directory: `manifests/j/JyGlobalVST/GlobalVSTHost/1.0.0/`
- [ ] Validate manifests locally: `winget validate`
- [ ] Fork microsoft/winget-pkgs
- [ ] Create PR with manifests
- [ ] Pass automated validation (AV scan, silent install test)
- [ ] Approved by WinGet maintainers (1–7 days typical)

## Acceptance Criteria

**Build Phase:**
- [x] WiX scaffolding complete and committed
- [ ] Release binaries build without errors
- [ ] MSI builds to `GlobalVSTHost-1.0.0-x64.msi`
- [ ] File size <150 MB

**Test Phase:**
- [ ] Silent install succeeds: `msiexec /i ... /quiet`
- [ ] All files present post-install:
  - `%LocalAppData%\JyGlobalVST\JyGlobalVST.exe`
  - `%LocalAppData%\JyGlobalVST\jyglobalvst_audio_engine.dll`
- [ ] Registry entries created (Add/Remove Programs)
- [ ] Uninstall succeeds: `msiexec /x {GUID} /quiet`
- [ ] No orphaned files post-uninstall

**Release Phase:**
- [ ] GitHub Release v1.0.0 created
- [ ] MSI file attached with correct SHA256
- [ ] WinGet PR passes automated validation
- [ ] WinGet PR approved and merged
- [ ] Command works: `winget install JyGlobalVST.GlobalVSTHost`

## Known Issues

**Issue**: JUCE/CMake linker error preventing Release build  
**Impact**: Blocks Phase 2  
**Status**: Pending investigation  
**Workaround**: See INSTALLER_SETUP_COMPLETE.md

## Timeline

| Phase | Estimated | Blocker |
|-------|-----------|---------|
| Scaffolding | Complete ✅ | — |
| Build & Test | 1 hour | JUCE build system |
| GitHub Release | 15 min | Build success |
| WinGet Submission | 1–3 days | WinGet approval |
| **Total** | **~2–4 days** | — |

## Success Criteria Summary

✅ WiX installer fully scaffolded and documented  
⏸️ Release binaries pending build fix  
⏸️ MSI build pending binaries  
⏸️ GitHub release pending MSI  
⏸️ WinGet submission pending manifests  

**Next Action**: Resolve JUCE linker issue or use workaround from INSTALLER_SETUP_COMPLETE.md
