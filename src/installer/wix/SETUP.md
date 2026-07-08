# WiX MSI Installer Setup & Build Guide

## Prerequisites

### 1. Install WiX Toolset v4

Download and install from: https://github.com/wixtoolset/wix4/releases

**Quick install:**
```powershell
# Using dotnet tool (requires .NET 6+)
dotnet tool install --global WiX

# OR download MSI from releases page and run installer
```

**Verify installation:**
```powershell
candle -help
light -help
```

### 2. Ensure Release Build

Make sure you've built the project in Release mode:
```powershell
cmake -B build -A x64
cmake --build build --config Release
```

This produces:
- `build\bin\Release\JyGlobalVST.exe` (tray application)
- `build\lib\Release\jyglobalvst_audio_engine.dll` (audio engine)

---

## Building the MSI

### Option A: CMake (Recommended)

```powershell
cd D:\repos\others\dev\GlobalVSTHost

# Configure with installer enabled (default)
cmake -B build -A x64

# Build MSI (requires WiX installed)
cmake --build build --config Release --target msi_installer
```

**Output**: `build\GlobalVSTHost-1.0.0-x64.msi`

### Option B: Manual WiX Commands

```powershell
cd src\installer\wix

# Compile .wxs → .wixobj
candle.exe -arch x64 `
  -d JyGlobalVSTExe=..\..\..\..\build\bin\Release\JyGlobalVST.exe `
  -d AudioEngineDll=..\..\..\..\build\lib\Release\jyglobalvst_audio_engine.dll `
  -d ProductVersion=1.0.0 `
  -o product.wixobj `
  product.wxs

# Link .wixobj → .msi
light.exe -out GlobalVSTHost-1.0.0-x64.msi `
  -ext WixUIExtension `
  -ext WixUtilExtension `
  product.wixobj
```

**Output**: `src\installer\wix\GlobalVSTHost-1.0.0-x64.msi`

---

## Testing the MSI

### 1. Syntax Validation

```powershell
# Validate .wxs file
candle.exe -arch x64 -preprocess product.wxs
```

### 2. Test Silent Installation (Recommended)

```powershell
# Elevate to admin and run
$msiPath = "D:\repos\others\dev\GlobalVSTHost\build\GlobalVSTHost-1.0.0-x64.msi"

# Silent install (no UI)
msiexec /i $msiPath /quiet

# Silent install with progress bar
msiexec /i $msiPath /quiet /norestart
```

**Verify installation:**
```powershell
# Check if app exists in Add/Remove Programs
Get-WmiObject Win32_Product | Where-Object { $_.Name -like "*Global VST*" }

# Check if executable was installed
Test-Path "$env:LOCALAPPDATA\JyGlobalVST\JyGlobalVST.exe"

# Check if DLL was installed
Test-Path "$env:LOCALAPPDATA\JyGlobalVST\jyglobalvst_audio_engine.dll"

# Check registry entries
Get-ItemProperty "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\JyGlobalVST" -ErrorAction SilentlyContinue
```

### 3. Test Interactive Installation

```powershell
$msiPath = "D:\repos\others\dev\GlobalVSTHost\build\GlobalVSTHost-1.0.0-x64.msi"
msiexec /i $msiPath
```

A wizard should appear allowing user to customize installation.

### 4. Test Uninstall

```powershell
# Find the GUID (ProductCode) from registry
$uninstallKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\JyGlobalVST"
$guid = (Get-ItemProperty $uninstallKey).PSChildName

# Uninstall silently
msiexec /x $guid /quiet

# Verify removal
Test-Path "$env:LOCALAPPDATA\JyGlobalVST" # Should be gone
```

### 5. Test for Admin vs Non-Admin Users

```powershell
# If you installed as admin, test uninstall as standard user:
# 1. Create a test standard user account
# 2. Log in as that user
# 3. Run: msiexec /x {GUID} /quiet
# 4. Verify it uninstalls cleanly
```

---

## Troubleshooting

### "WiX not found"

CMake message: `WiX Toolset not found. Skipping MSI build.`

**Fix**: Install WiX Toolset from https://github.com/wixtoolset/wix4/releases

### "File not found" during candle/light

Error: `fatal error CNDL0001: The system cannot find the file specified`

**Cause**: Binary paths in `-d` flags don't match actual build output

**Fix**: Verify paths:
```powershell
# Check actual paths
ls build\bin\Release\JyGlobalVST.exe
ls build\lib\Release\jyglobalvst_audio_engine.dll
```

Update the `-d` flags in CMakeLists.txt or manual command to match.

### "Validation-Unattended-Failed" on WinGet

The installer doesn't support silent mode.

**Fix**: Ensure the MSI was built with WiX (it should support silent out of the box). Test locally:
```powershell
msiexec /i GlobalVSTHost-1.0.0-x64.msi /quiet
```

If this hangs or fails, the product.wxs may have an interactive element. Common issues:
- Custom action that shows dialog (removed in our template)
- Invalid UI reference (check WixUI_InstallDir exists)

### "InstallScope=perUser" permissions error

If tests run under admin, per-user install might behave differently.

**Fix**: Test as a standard (non-admin) user:
```powershell
# Open new PowerShell as standard user
Start-Process powershell -Verb RunAs -ArgumentList {
    msiexec /i GlobalVSTHost-1.0.0-x64.msi /quiet
}
```

---

## Next Steps

1. **Install WiX Toolset v4** (if not already installed)
2. **Build the Release binaries** (`cmake --build build --config Release`)
3. **Build the MSI** (`cmake --build build --config Release --target msi_installer`)
4. **Test silent install** (`msiexec /i build\GlobalVSTHost-1.0.0-x64.msi /quiet`)
5. **Verify installation** (check App, registry, files)
6. **Test uninstall** (`msiexec /x {GUID} /quiet`)
7. **Commit to git** once tests pass
8. **Push GitHub release** with the MSI attached

---

## For Windows Store / WinGet Submission

Once the MSI is tested and working:

1. Create a GitHub release tag: `v1.0.0`
2. Compute SHA256 of the MSI:
   ```powershell
   (Get-FileHash "GlobalVSTHost-1.0.0-x64.msi" -Algorithm SHA256).Hash
   ```
3. Attach MSI to the GitHub release
4. Create WinGet manifests with the correct SHA256
5. Submit to microsoft/winget-pkgs

See `../../docs/winget-release-plan.md` for full submission steps.
