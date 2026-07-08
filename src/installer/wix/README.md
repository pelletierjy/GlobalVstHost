# WiX MSI Installer — Quick Reference

## What This Does

Builds a Windows MSI installer that:
- ✅ Installs to `%LocalAppData%\JyGlobalVST\` (no admin required)
- ✅ Registers in Add/Remove Programs
- ✅ Creates Start Menu & Desktop shortcuts
- ✅ Auto-launches on user login (via Run registry key)
- ✅ Supports silent install: `msiexec /i GlobalVSTHost-1.0.0-x64.msi /quiet`
- ✅ Supports clean uninstall
- ✅ Upgrades automatically when new version detected

## Files

| File | Purpose |
|------|---------|
| `product.wxs` | WiX source (main installer definition) — edit this for customization |
| `CMakeLists.txt` | CMake integration (auto-compiles & links MSI) |
| `license.rtf` | License text shown during installation |
| `test-msi.ps1` | Automated test script (install + verify + uninstall) |
| `SETUP.md` | Detailed build & test guide |
| `IMAGES.md` | How to add custom branding images |

## Quick Build

**Prerequisites:**
- WiX Toolset v4: https://github.com/wixtoolset/wix4/releases
- Release binaries: `cmake --build build --config Release`

**Build:**
```powershell
cmake --build build --config Release --target msi_installer
```

**Output:** `build/GlobalVSTHost-1.0.0-x64.msi`

## Quick Test

```powershell
.\test-msi.ps1 -Test Silent
```

This:
1. Silently installs the MSI
2. Verifies all files and registry entries
3. Silently uninstalls
4. Verifies complete removal

**Expected result:** All tests pass ✓

## WiX Customization

Common changes in `product.wxs`:

```xml
<!-- Change install location -->
<Directory Id="JYGLOBALVSTFOLDER" Name="JyGlobalVST" />
<!-- → change Name to your preferred folder -->

<!-- Remove auto-launch -->
<!-- <ComponentRef Id="AutoLaunchComponent" /> -->

<!-- Remove desktop shortcut -->
<!-- <ComponentRef Id="DesktopShortcutComponent" /> -->

<!-- Add branding images -->
<WixVariable Id="WixUIBannerBmp" Value="banner.bmp" />
<WixVariable Id="WixUIDialogBmp" Value="dialog.bmp" />
```

See `product.wxs` for full documentation.

## For WinGet Submission

Once the MSI is tested:

1. **Create GitHub release:**
   ```powershell
   git tag v1.0.0
   git push origin v1.0.0
   gh release create v1.0.0 build/GlobalVSTHost-1.0.0-x64.msi
   ```

2. **Get SHA256:**
   ```powershell
   (Get-FileHash "build\GlobalVSTHost-1.0.0-x64.msi" -Algorithm SHA256).Hash
   ```

3. **Create WinGet manifest** (see parent README and release plan)

4. **Submit to microsoft/winget-pkgs** (see release plan)

## Troubleshooting

| Problem | Solution |
|---------|----------|
| "WiX not found" | Install from https://github.com/wixtoolset/wix4/releases |
| "File not found" | Build Release: `cmake --build build --config Release` |
| Silent install hangs | Check test with `/quiet` flag; WiX MSI should support it by default |
| Uninstall fails | Test as standard (non-admin) user; check permissions on `%LocalAppData%` |

See `SETUP.md` for full troubleshooting guide.

## What Next?

1. **Install WiX Toolset v4** (one-time, ~5 min)
2. **Build Release binaries** (CMake)
3. **Run `cmake --build ... --target msi_installer`** (WiX builds MSI)
4. **Run `.\test-msi.ps1 -Test Silent`** (verify it works)
5. **Push to GitHub release + WinGet** (see release plan)

---

**Total time to working MSI:** ~30 minutes (first time, including WiX install)

**Total time per release:** ~5 minutes (build + test)
