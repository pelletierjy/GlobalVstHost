# Local MSIX Testing Guide

**Purpose**: Step-by-step procedures for testing MSIX packages on clean Windows systems  
**Audience**: QA engineers, developers, release managers

---

## Prerequisites

- Windows 10 version 1909 (build 18363) or Windows 11
- 64-bit (x64) architecture
- Administrator PowerShell access (for installation only)
- Latest `.msixbundle` file from build output

---

## Test Environment Setup

### Option 1: Physical Machine (Recommended for Real-World Testing)

1. Use a real Windows 10 or Windows 11 system
2. Note the current OS version and build number
3. Have administrator access ready

### Option 2: Virtual Machine (Recommended for Consistent Testing)

1. Create a new Hyper-V or VirtualBox VM with Windows 10/11
2. Take a snapshot before testing (enables rollback)
3. Install latest Windows updates
4. Obtain administrator credentials

### Option 3: Cloud Machine (Azure VM)

1. Deploy Windows 10/11 VM from Azure
2. Connect via RDP
3. Follow same testing procedures as physical machine

---

## Test Scenario 1: Manifest Validation

**Objective**: Verify manifest XML is structurally correct

**Prerequisites**: MSIX package file available

**Steps**:

1. **Extract Package Contents**:
   ```powershell
   $packagePath = "C:\path\to\JyGlobalVST_1.0.0.0_x64.msixbundle"
   $tempDir = "C:\temp\msix_extract"
   
   # MSIX is a ZIP file
   $zipPath = "$tempDir\package.zip"
   Copy-Item $packagePath $zipPath
   
   # Extract
   Expand-Archive -Path $zipPath -DestinationPath $tempDir
   ```

2. **Locate Manifest**:
   ```powershell
   Get-ChildItem -Path $tempDir -Filter "AppxManifest.xml" -Recurse
   ```

3. **Validate XML**:
   ```powershell
   $manifest = [xml](Get-Content "C:\temp\msix_extract\AppxManifest.xml")
   Write-Host "Manifest is valid XML"
   ```

4. **Verify Required Elements**:
   - Check for `<Identity>` element (has Name, Publisher, Version)
   - Check for `<Properties>` element
   - Check for `<Dependencies>` element
   - Check for `<Applications>` element

**Expected Result**: Manifest parses without errors; all required elements present.

**Pass Criteria**: ✓ XML is well-formed and contains all required sections

---

## Test Scenario 2: Package Installation

**Objective**: Verify package installs without administrator elevation

**Prerequisites**: 
- MSIX package available
- Clean Windows system (or clean snapshot)
- Administrator PowerShell

**Steps**:

1. **Open PowerShell as Administrator**:
   - Press `Win+X`, select "Windows PowerShell (Admin)"
   - Or: Right-click PowerShell → "Run as administrator"

2. **Install Package**:
   ```powershell
   $packagePath = "C:\path\to\JyGlobalVST_1.0.0.0_x64.msixbundle"
   Add-AppxPackage -Path $packagePath
   ```

3. **Monitor Installation**:
   - Watch PowerShell output for progress
   - Installation typically takes 30 seconds to 2 minutes
   - No prompts should appear

4. **Verify Installation**:
   ```powershell
   Get-AppxPackage | Where-Object { $_.Name -like "*JyGlobalVST*" }
   ```
   
   Expected output:
   ```
   Name              : JyGlobalVST
   Publisher         : CN=JyGlobalVST
   Architecture      : X64
   Version           : 1.0.0.0
   PackageFullName   : JyGlobalVST_1.0.0.0_x64_[hash]
   InstallLocation   : C:\Program Files\WindowsApps\JyGlobalVST_1.0.0.0_...
   ```

**Expected Result**: Package installs without errors; appears in `Get-AppxPackage`

**Pass Criteria**: ✓ Installation completes; package registered in system

---

## Test Scenario 3: Application Launch

**Objective**: Verify application starts correctly after installation

**Prerequisites**: Package installed (from Scenario 2)

**Steps**:

1. **Method 1: Start Menu**
   - Click Windows key
   - Type "JyGlobalVST"
   - Click result to launch

2. **Method 2: PowerShell**:
   ```powershell
   $app = Get-AppxPackage | Where-Object { $_.Name -like "*JyGlobalVST*" }
   $aumid = "$($app.PackageFamilyName)!JyGlobalVSTApp"
   explorer.exe shell:appsFolder\$aumid
   ```

3. **Observe Application**:
   - Application window should appear within 5-10 seconds
   - No error dialogs should appear
   - Application should respond to user interaction (menus, buttons, etc.)

4. **Check System Tray** (if applicable):
   - Look for JyGlobalVST icon in system tray
   - Icon should be visible and responsive

5. **Verify Process**:
   ```powershell
   Get-Process | Where-Object { $_.Name -like "*JyGlobalVST*" }
   ```

**Expected Result**: Application launches and is responsive

**Pass Criteria**: ✓ App starts without errors; responds to user input

---

## Test Scenario 4: Core Functionality Verification

**Objective**: Verify application's main features work correctly

**Prerequisites**: Application is running (from Scenario 3)

**Test Cases**:

| Test | Steps | Expected Outcome | Pass |
|------|-------|------------------|------|
| **Load Plugins** | Open plugin manager → scan plugins | At least one plugin available | |
| **Add to Chain** | Add plugin to processing chain | Plugin appears in chain | |
| **Process Audio** | Play system audio (YouTube, music) | Audio routing works; no dropouts | |
| **Save Settings** | Adjust plugin parameters → save | Settings file created | |
| **Restore Settings** | Close app → reopen → load settings | Previous settings restored | |
| **Uninstall** | Settings → Apps → Uninstall | App cleanly removed | |

**Expected Result**: All functionality works as expected

**Pass Criteria**: ✓ All test cases pass; no crashes or errors

---

## Test Scenario 5: Settings Persistence

**Objective**: Verify user settings survive application restart

**Prerequisites**: Application configured and running

**Steps**:

1. **Configure Settings**:
   - Change plugin parameters
   - Adjust app settings
   - Set preferences (e.g., default device, window size)

2. **Close Application**:
   - Close normally (not forced kill)
   - Verify application shuts down cleanly

3. **Reopen Application**:
   - Launch from Start menu or PowerShell
   - Allow 5-10 seconds for startup

4. **Verify Settings**:
   - Check that previous settings are restored
   - Check that window position is remembered
   - Check that plugin configuration is restored

**Expected Result**: Settings are persisted across restart

**Pass Criteria**: ✓ All user settings are restored after app restart

---

## Test Scenario 6: Uninstallation

**Objective**: Verify clean uninstallation without orphaned files

**Prerequisites**: Application installed and tested

**Steps**:

1. **Uninstall via Settings**:
   - Settings → Apps → Installed Apps
   - Find "JyGlobalVST"
   - Click ... → Uninstall

2. **Verify Removal**:
   ```powershell
   Get-AppxPackage | Where-Object { $_.Name -like "*JyGlobalVST*" }
   # Should return: (nothing)
   ```

3. **Check for Orphaned Files**:
   ```powershell
   # Check AppData
   Get-ChildItem "$env:APPDATA\JyGlobalVST" -ErrorAction SilentlyContinue
   
   # Check registry
   Get-ItemProperty "HKCU:\Software\JyGlobalVST" -ErrorAction SilentlyContinue
   ```

4. **Verify Cleanup**:
   - No files in `%AppData%\JyGlobalVST\` (except user presets)
   - No registry entries under `HKEY_CURRENT_USER\Software\JyGlobalVST`

**Expected Result**: Application fully removed; minimal cleanup

**Pass Criteria**: ✓ App not in installed list; no orphaned system files

---

## Automated Testing Script

Use the provided PowerShell script for automated testing:

```powershell
.\StorePackaging\Scripts\Test-PackageFull.ps1 -PackagePath "path\to\package.msixbundle"
```

This script runs all scenarios above automatically and reports results.

---

## Test Report Template

| Scenario | Status | Notes |
|----------|--------|-------|
| 1. Manifest Validation | PASS/FAIL | |
| 2. Package Installation | PASS/FAIL | |
| 3. Application Launch | PASS/FAIL | |
| 4. Core Functionality | PASS/FAIL | |
| 5. Settings Persistence | PASS/FAIL | |
| 6. Uninstallation | PASS/FAIL | |

**Overall Result**: PASS / FAIL  
**Date Tested**: ________  
**Tester**: ________  
**OS Version**: ________  
**Build**: ________

---

## Troubleshooting

### Installation Fails with "Deployment Failed"

**Cause**: Manifest issues or corrupt package  
**Solution**:
1. Run Manifest Validation (Scenario 1)
2. Check for schema errors
3. Rebuild package
4. Retry installation

### Application Won't Start

**Cause**: Missing dependencies or permission issues  
**Solution**:
1. Check Event Viewer for error logs
2. Verify system tray icon appears
3. Check in Task Manager for process
4. Try launching from PowerShell for error output

### Settings Don't Persist

**Cause**: Registry or file permission issues  
**Solution**:
1. Verify registry write permission to HKEY_CURRENT_USER
2. Check Documents folder access
3. Look for permission errors in Event Viewer
4. Test on different user account

### Uninstall Leaves Files

**Cause**: User preset files intentionally retained  
**Solution**:
1. Check if leftover files are user presets (expected)
2. Manually delete remaining files if needed
3. Check registry for app keys (can be deleted manually)

---

## Sign-Off

- [ ] All scenarios tested
- [ ] All pass criteria met
- [ ] Package approved for Partner Center submission

**Date**: ________  
**Approved by**: ________

