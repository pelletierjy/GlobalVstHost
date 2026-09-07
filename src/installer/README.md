# Installer & Distribution

This directory contains the Windows MSI installer and WinGet package manifest templates for Global VST Host.

## Contents

### `wix/` — WiX Installer Project

**Files:**
- `product.wxs` — WiX source file (main installer definition)
- `CMakeLists.txt` — CMake integration for automated MSI build
- `license.rtf` — License text displayed during installation
- `SETUP.md` — Step-by-step build and test guide
- `test-msi.ps1` — PowerShell script for automated testing
- `IMAGES.md` — Instructions for adding custom branding images

**Quick Start:**
```powershell
# Prerequisites: Install WiX Toolset v4
# https://github.com/wixtoolset/wix4/releases

# Build the MSI
cmake --build build --config Release --target msi_installer

# Test silent installation
.\src\installer\wix\test-msi.ps1 -Test Silent
```

**Output:** `build/GlobalVSTHost-1.1.0-x64.msi`

### `wix/product.wxs` — Component Map

The MSI installs to `%LocalAppData%\JyGlobalVST\`:

| Component | Files | Purpose |
|-----------|-------|---------|
| **Main Executable** | `JyGlobalVST.exe` | Tray application (entry point) |
| **Audio Engine** | `jyglobalvst_audio_engine.dll` | Real-time audio DSP library |
| **Start Menu Shortcut** | (registry) | Windows Start menu entry |
| **Desktop Shortcut** | (registry) | Desktop shortcut (optional) |
| **Auto-Launch** | (registry Run key) | Auto-start on user login |

Scope: **Per-user** (no admin required, installed to `%LocalAppData%`)

## Build Requirements

### For Local Testing

- **Windows 10 1909+** or **Windows 11** (x64)
- **WiX Toolset v4** (https://github.com/wixtoolset/wix4/releases)
- **CMake 3.24+**
- **MSVC** (cl.exe)

### For WinGet / Store Release

Same as above, plus:
- Code-signing certificate (optional but recommended)
- GitHub release hosting with HTTPS
- WinGet account (free)

## Workflow

### Phase 1: Build & Test (Local)

```powershell
# 1. Configure and build Release
cmake -B build -A x64
cmake --build build --config Release

# 2. Build MSI
cmake --build build --config Release --target msi_installer

# 3. Test (automated)
.\src\installer\wix\test-msi.ps1 -Test Silent

# Expected output:
# ✓ Silent install succeeded
# ✓ Executable found
# ✓ Audio engine DLL found
# ✓ Registry entry found
# ✓ Silent uninstall succeeded
# ✓ Executable removed
```

### Phase 2: GitHub Release

Once tests pass:

```powershell
# 1. Tag release
git tag v1.1.0
git push origin v1.1.0

# 2. Create GitHub release manually, or script it:
gh release create v1.1.0 `
  --title "Global VST Host v1.1.0" `
  --notes "Initial release with driverless audio capture and built-in effects" `
  build/GlobalVSTHost-1.1.0-x64.msi

# 3. Compute SHA256 for WinGet manifest
(Get-FileHash "build\GlobalVSTHost-1.1.0-x64.msi" -Algorithm SHA256).Hash
```

### Phase 3: WinGet Submission

See the main release plan for full WinGet submission steps. Key points:

1. Create 3 YAML manifest files (version, installer, locale)
2. Use the MSI URL from your GitHub release
3. Include the SHA256 hash
4. Fork microsoft/winget-pkgs and create a PR
5. Azure Pipeline validates automatically
6. Approved within 1–7 days (typically 1–3)

## Customization (v1→v2)

For future versions, you may want to customize:

- **Installer images**: `banner.bmp` (493×58) and `dialog.bmp` (493×312)
  - See `IMAGES.md` for tools and instructions
- **License text**: Update `license.rtf` with your actual license
- **Install location**: Change `JYGLOBALVSTFOLDER` path in `product.wxs`
- **Auto-launch behavior**: Comment out the `AutoLaunchComponent` in `product.wxs` if not desired
- **Shortcuts**: Add/remove Start Menu or Desktop shortcut components

## Troubleshooting

See `wix/SETUP.md` for detailed troubleshooting guide.

Common issues:
- **WiX not found** → Install from https://github.com/wixtoolset/wix4/releases
- **File not found** → Verify Release build exists (`build\bin\Release\JyGlobalVST.exe`)
- **Silent install fails** → Check MSI supports `/quiet` flag (should by default)
- **Uninstall issues** → Test as standard (non-admin) user

## Next Steps

1. ✅ WiX project scaffolded (`product.wxs`, CMakeLists.txt)
2. ⬜ Install WiX Toolset v4
3. ⬜ Build and test MSI locally
4. ⬜ Create GitHub release
5. ⬜ Create WinGet manifests
6. ⬜ Submit to microsoft/winget-pkgs

See the main project README and release plan for full context.
