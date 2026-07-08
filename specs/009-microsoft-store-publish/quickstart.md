# Quickstart: MSIX Package Validation & Partner Center Submission

**Phase**: 1 (Design) | **Date**: 2026-07-05 | **Status**: Complete

This guide provides step-by-step procedures for validating a generated MSIX package locally and preparing it for Microsoft Partner Center submission.

---

## Part A: Local Package Validation

Use this section to verify that an MSIX package is correct **before** uploading it to Partner Center.

### Prerequisites

- Windows 10 1909+ or Windows 11 (x64)
- Administrator PowerShell access (for installation only; not required for daily use)
- Latest `.msixbundle` file from the build output (`build/store-packages/`)
- Optional: MSAP tool (Microsoft Store App Certification Kit) for deeper validation

### Scenario 1: Manifest Schema Validation

**Objective**: Verify that the manifest XML is well-formed and conforms to Microsoft's schema.

**Setup**:
1. Obtain the `.msixbundle` file
2. Rename it to `.zip` extension temporarily (MSIX is a ZIP archive)
3. Extract the .zip to a temporary folder
4. Locate `AppxManifest.xml` inside

**Steps**:

1. **Extract manifest**:
   ```powershell
   # Rename bundle to .zip
   Copy-Item -Path "build\store-packages\JyGlobalVST_1.0.0.0_x64.msixbundle" `
             -Destination ".\temp_package.zip"
   
   # Extract
   Expand-Archive -Path ".\temp_package.zip" -DestinationPath ".\extracted_bundle"
   
   # Locate manifest
   Get-Item -Path ".\extracted_bundle\AppxManifest.xml"
   ```

2. **Validate with Visual Studio** (if available):
   - Open Visual Studio 2022
   - File → Open → navigate to extracted AppxManifest.xml
   - VS will highlight schema errors if any exist

3. **Manual validation**:
   - Check that `<Identity>`, `<Properties>`, `<Dependencies>`, `<Capabilities>`, `<Applications>` elements exist
   - Verify all file paths use forward slashes (`Assets/Icon.png`, not `Assets\Icon.png`)
   - Ensure version format is `Major.Minor.Build.Revision` with all integers

**Expected Outcome**: No XML parser errors; all required elements present.

**Pass Criteria**: ✓ Manifest parses without errors and contains all required sections.

---

### Scenario 2: Install Package Locally

**Objective**: Verify that the MSIX package can be installed on a clean Windows system without admin elevation.

**Setup**:
- Windows 10 1909+ or Windows 11 (x64)
- Standard user account (not admin) OR admin user testing without elevation
- Package file: `JyGlobalVST_1.0.0.0_x64.msix`

**Steps**:

1. **Get package info**:
   ```powershell
   Add-AppxPackage -RegisterByFamilyName `
     -MainPackageName "JyGlobalVST_1.0.0.0_x64.msix" `
     -Path "C:\path\to\JyGlobalVST_1.0.0.0_x64.msix" `
     -WhatIf  # Test without installing
   ```

2. **Install package** (requires admin the first time):
   ```powershell
   Add-AppxPackage -Path "C:\path\to\JyGlobalVST_1.0.0.0_x64.msix"
   ```

3. **Wait for installation**:
   - Typical time: 30 seconds - 2 minutes
   - Watch for errors in PowerShell output

4. **Check installation status**:
   ```powershell
   Get-AppxPackage | Where-Object { $_.Name -like "*JyGlobalVST*" }
   ```
   
   **Expected output**:
   ```
   Name              : JyGlobalVST
   Publisher         : CN=JyGlobalVST
   Architecture      : X64
   ResourceId        : 
   Version           : 1.0.0.0
   PackageFullName   : JyGlobalVST_1.0.0.0_x64_n8xr4scnvf4w0
   InstallLocation   : C:\Program Files\WindowsApps\JyGlobalVST_1.0.0.0_...
   IsFramework       : False
   ```

**Expected Outcome**: Package installs successfully; appears in Get-AppxPackage output.

**Pass Criteria**: ✓ Installation completes without errors; package appears in installed list.

**Failure Troubleshooting**:
- Error: `Deployment failed` → Check manifest for missing files or invalid paths
- Error: `Access denied` → Ensure running as admin or on a machine where you have install rights
- Error: `Certificate validation failed` → Package signature is invalid; rebuild

---

### Scenario 3: Launch Application

**Objective**: Verify that the installed application starts correctly and responds to user interaction.

**Setup**:
- Package installed (from Scenario 2)
- Clean Windows environment (no prior JyGlobalVST installation)

**Steps**:

1. **Launch via Start Menu** (GUI):
   - Click Windows key to open Start menu
   - Type "JyGlobalVST"
   - Click "JyGlobalVST" in results

2. **Launch via PowerShell**:
   ```powershell
   # Get the app object
   $app = Get-AppxPackage | Where-Object { $_.Name -like "*JyGlobalVST*" }
   
   # Get the application ID from the manifest
   $manifest = [xml](Get-Content -Path "$($app.InstallLocation)\AppxManifest.xml")
   $appId = $manifest.Package.Applications.Application.Id
   
   # Launch
   explorer.exe shell:appsFolder\$($app.PackageFamilyName)!$appId
   ```

3. **Observe**:
   - Application window appears (if GUI) or process starts (if background service)
   - System tray icon appears for JyGlobalVST
   - No error dialogs appear
   - Application responds to user input (menu clicks, drag-drop, etc.)

4. **Wait**:
   - Allow application 10-15 seconds to fully initialize
   - Observe any startup logging or progress indicators

**Expected Outcome**: Application launches, displays UI (or system tray), and responds to user interaction.

**Pass Criteria**: ✓ App starts without errors; user can interact normally.

**Failure Troubleshooting**:
- App doesn't start: Check that `StartPage` in manifest points to correct executable
- App crashes on startup: Look for missing dependencies (DLLs); check Event Viewer for exception details
- Tray icon doesn't appear: Check icon asset paths in manifest; verify icon files exist and are 44×44 PNG

---

### Scenario 4: Test Core Functionality

**Objective**: Verify that the application's core features work identically to the traditional EXE-based installation.

**Setup**:
- Application running (from Scenario 3)
- Reference: running the same version installed via traditional EXE installer (on same or different machine)

**Test Cases**:

| Test | Steps | Expected Outcome | Pass/Fail |
|------|-------|------------------|-----------|
| **Load VST Plugin** | Open plugin manager; scan plugins; verify at least one plugin loads | Plugin appears in list; can be added to chain | |
| **Process Audio** | Add plugin to chain; enable audio routing; play system audio | Audio passes through plugin; volume/waveform changes observed | |
| **Save Settings** | Adjust plugin parameters; save as preset; close app | Preset file is created; app exits cleanly | |
| **Restore Settings** | Reopen app; load saved preset | Preset loads; parameters match saved values | |
| **Settings Persistence** | Change app settings (e.g., default device); close app; reopen | Settings are restored to previous values | |

**Expected Outcome**: All test cases pass; application behaves identically to traditional installer version.

**Pass Criteria**: ✓ Core functionality works; settings persist; no crashes or errors observed.

---

### Scenario 5: Uninstall Package

**Objective**: Verify that the package uninstalls cleanly without leaving orphaned files or registry entries.

**Setup**:
- Application installed (from Scenario 2)
- Not currently running

**Steps**:

1. **Uninstall via Settings GUI** (recommended for user testing):
   - Open Settings → Apps → Installed Apps
   - Find "JyGlobalVST" in the list
   - Click the three-dot menu → Uninstall
   - Confirm uninstallation

2. **Verify removal** (PowerShell):
   ```powershell
   # Check that app is no longer in the list
   Get-AppxPackage | Where-Object { $_.Name -like "*JyGlobalVST*" }
   # Should return: (nothing/empty)
   ```

3. **Check for orphaned files**:
   ```powershell
   # Look for leftover files in AppData
   Get-ChildItem -Path "$env:APPDATA\JyGlobalVST" -ErrorAction SilentlyContinue
   # Look for registry entries
   Get-ItemProperty -Path "HKCU:\Software\JyGlobalVST" -ErrorAction SilentlyContinue
   ```

**Expected Outcome**: Package fully removed; no orphaned files or registry entries.

**Pass Criteria**: ✓ App does not appear in installed list; `%AppData%\JyGlobalVST` and registry entries are cleaned up.

**Failure Troubleshooting**:
- Uninstall fails: App may still be running; close it first
- Orphaned files remain: Check permissions; some user-created files (presets, settings) may be intentionally retained
- Registry entries remain: Expected if user saved settings; MSIX does not forcibly remove user-created data

---

## Part B: Automated Validation with MSAP

### Installing MSAP (Optional but Recommended)

**Download**:
1. Visit: https://learn.microsoft.com/en-us/windows/uwp/publish/
2. Download "Microsoft Store App Certification Kit" (MSAP)
3. Run the installer

**Location after install**:
```
C:\Program Files (x86)\Windows Kits\10\App Cert Kit\appcert.exe
```

### Running Validation

1. **Open MSAP**:
   ```powershell
   & "C:\Program Files (x86)\Windows Kits\10\App Cert Kit\appcert.exe"
   ```

2. **Select package**:
   - Choose "Validate app package"
   - Navigate to your `.msixbundle` or `.msix` file

3. **Run validation**:
   - Click "Start" or "Validate"
   - MSAP runs 50+ automated checks

4. **Review results**:
   - Red X = Error (must fix before Partner Center submission)
   - Yellow ! = Warning (should review, but not blocking)
   - Green ✓ = Passed

**Expected outcome**: All items pass or show only warnings.

---

## Part C: Partner Center Submission Checklist

Use this checklist to prepare all materials for Partner Center upload.

### Account Setup (One-Time)

- [ ] Create Microsoft Account (if not already created)
- [ ] Sign up for Partner Center (partner.microsoft.com)
- [ ] Complete Publisher registration
- [ ] Verify identity (may require documentation)
- [ ] Add payment method (if charging users; not required for free apps)

### App Registration (One-Time per App)

- [ ] Reserve app name in Partner Center
- [ ] Confirm name matches manifest `<Identity Name="...">`
- [ ] Select app category (e.g., "Utilities", "Media & Design")
- [ ] Complete IARC questionnaire for content ratings
- [ ] Create privacy policy (link to published privacy document)
- [ ] Create app description (50-character short; 200+ character long)

### Package Submission (Per Release)

- [ ] Obtain `.msixbundle` from `build/store-packages/`
- [ ] Validate manifest (Scenario 1 above)
- [ ] Run MSAP validation (all checks pass)
- [ ] Test installation locally (Scenarios 2-5 above)
- [ ] Verify version number format (`Major.Minor.Build.Revision`)
- [ ] Confirm version is higher than previously published version
- [ ] Generate or update app icon assets (if changed)
- [ ] Prepare release notes / "What's new" text

### Partner Center Upload

1. **Sign into Partner Center**:
   - https://partner.microsoft.com/en-us/dashboard/home

2. **Navigate to your app**:
   - Select your app from the dashboard

3. **Start new submission**:
   - Click "Create new submission"

4. **Upload package**:
   - In "Packages" section, click "Browse files"
   - Select your `.msixbundle` file
   - Wait for upload to complete

5. **Auto-validation**:
   - Partner Center runs automated checks (30 min - 2 hours)
   - Review results; if errors appear, fix and re-upload

6. **Add app details** (if not already done):
   - Title, description, keywords
   - Screenshots and trailers (optional)
   - System requirements and compatibility notes

7. **Submit for certification**:
   - Click "Submit to the Store" or "Certify and Publish"
   - Confirm submission

8. **Wait for certification review**:
   - Typical time: 24 hours (can be faster or slower)
   - Microsoft team reviews for policy compliance
   - App either publishes automatically or you receive feedback for fixes

### Post-Submission Monitoring

- [ ] Check Partner Center for certification status (daily if needed)
- [ ] Monitor for any rejection emails with feedback
- [ ] If rejected: Review feedback, fix issues, re-submit
- [ ] If approved: App is published to Microsoft Store (typically within 1-24 hours)
- [ ] Share app link with users

---

## Troubleshooting

### Common Installation Errors

| Error | Cause | Fix |
|-------|-------|-----|
| `Deployment failed` | Manifest or package corruption | Rebuild from source; validate manifest with MSAP |
| `File not found` | Icon/asset path missing or wrong | Check Assets/ directory; verify paths in manifest |
| `Invalid certificate` | Self-signed cert invalid for this user | For dev: test cert must be installed; for Store: cert is applied by Microsoft |
| `App is already installed` | Different version already installed | Uninstall previous version first |

### Common Manifest Issues

| Issue | Cause | Fix |
|-------|-------|-----|
| `Invalid version` | Version not in `Major.Minor.Build.Revision` format | Update to `1.0.0.0` format |
| `Missing logo` | `Assets/StoreLogo.png` not found | Create 50×50 PNG icon in Assets/ |
| `Capability over-declared` | App uses more capabilities than actually needed | Audit manifest; remove unused capabilities |
| `MinVersion too low` | Manifest says Windows 7, but MSIX is Windows 10+ only | Update to `10.0.18363.0` or higher |

---

## Quick Reference: File Checklist

Before submitting to Partner Center, ensure these files exist and are valid:

```
build/
├── store-packages/
│   ├── JyGlobalVST_1.0.0.0_x64.msix              ← Package file
│   ├── JyGlobalVST_1.0.0.0_x64.msixbundle       ← Bundle file (upload this)
│   └── AppxManifest.xml                          ← Manifest copy
│
StorePackaging/
├── AppxManifest.xml                             ← Source manifest
├── JyGlobalVST.msixproj                          ← MSIX project file
└── Assets/
    ├── Square44x44Logo.png                      (✓ 44×44)
    ├── Square50x50Logo.png                      (✓ 50×50)
    ├── Square150x150Logo.png                    (✓ 150×150)
    ├── Square310x310Logo.png                    (✓ 310×310)
    └── StoreLogo.png                            (✓ 50×50)
```

---

## Next Steps

1. **Local validation**: Run through Scenarios 1-5 for every build
2. **MSAP validation**: Run MSAP tool for final pre-submission check
3. **Partner Center submission**: Follow Part C checklist
4. **Monitoring**: Check Partner Center daily during certification review
5. **Feedback loop**: If rejected, review feedback and re-submit with fixes

