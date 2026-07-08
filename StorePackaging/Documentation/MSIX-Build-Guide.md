# MSIX Build Guide

**Quick Start**: `.\StorePackaging\Scripts\Build-MSIX.ps1 -Version 1.0.0.0 -Configuration Release`

---

## Overview

This guide explains how to build MSIX packages for JyGlobalVST application ready for Microsoft Store distribution.

## Prerequisites

- Windows 10 or Windows 11
- CMake 3.22 or later
- Visual Studio 2022 Community Edition (or Build Tools)
- Windows SDK (included with VS 2022)
- MSIX Packaging Tool (optional, for advanced packaging)

## Build Process

### Step 1: Update Version

Edit `StorePackaging/version-info.txt`:
```
1.0.1.0
```

### Step 2: Run Build Script

```powershell
cd D:\repos\others\dev\GlobalVSTHost
.\StorePackaging\Scripts\Build-MSIX.ps1 -Version 1.0.1.0 -Configuration Release
```

### Step 3: Validate Package

```powershell
.\StorePackaging\Scripts\Validate-Package.ps1 -PackagePath build\store-packages\JyGlobalVST_1.0.1.0_x64.msixbundle
```

### Step 4: Test Installation (Optional)

```powershell
.\StorePackaging\Scripts\Test-LocalInstall.ps1 -PackagePath build\store-packages\JyGlobalVST_1.0.1.0_x64.msixbundle
```

## Output

Built packages are saved to: `build/store-packages/`

- `JyGlobalVST_1.0.1.0_x64.msix` - Individual package
- `JyGlobalVST_1.0.1.0_x64.msixbundle` - Bundle (ready for Store)
- `AppxManifest.xml` - Manifest copy (reference)

## Troubleshooting

| Issue | Solution |
|-------|----------|
| CMake not found | Install CMake from cmake.org |
| Visual Studio not found | Install VS 2022 Community |
| Version format invalid | Use format: Major.Minor.Build.Revision |
| Build fails | Run `cmake --build build --config Release` manually |

## Release Process

1. Update version in `version-info.txt`
2. Run build script
3. Validate package
4. Test locally
5. Create git tag: `git tag v1.0.1.0`
6. Push tag: `git push origin v1.0.1.0`
7. CI automatically builds and creates release

See [Partner Center Submission Guide](Partner-Center-Submission.md) for next steps.

