# StorePackaging Directory

**Overview**: All files and documentation for Microsoft Store MSIX publishing

---

## Quick Start

1. **Build Package**:
   ```powershell
   .\Scripts\Build-MSIX.ps1 -Version 1.0.0.0
   ```

2. **Validate Package**:
   ```powershell
   .\Scripts\Validate-Package.ps1 -PackagePath build\store-packages\JyGlobalVST_1.0.0.0_x64.msixbundle
   ```

3. **Test Installation**:
   ```powershell
   .\Scripts\Test-LocalInstall.ps1 -PackagePath build\store-packages\JyGlobalVST_1.0.0.0_x64.msixbundle
   ```

---

## Directory Structure

```
StorePackaging/
├── AppxManifest.xml                 # Application manifest (Package metadata)
├── JyGlobalVST.msixproj             # Visual Studio project file
├── version-info.txt                 # Current version (single source of truth)
├── Assets/                          # Application icons and logos
│   ├── Square44x44Logo.png          # 44×44 taskbar icon
│   ├── Square50x50Logo.png          # 50×50 tile icon
│   ├── Square150x150Logo.png        # 150×150 medium tile
│   ├── Square310x310Logo.png        # 310×310 large tile
│   └── StoreLogo.png                # 50×50 Store listing logo
├── Scripts/                         # Build and test automation
│   ├── Build-MSIX.ps1               # Main build script
│   ├── Validate-Package.ps1         # Package validation script
│   ├── Test-LocalInstall.ps1        # Local installation test
│   ├── Test-PackageFull.ps1         # Full automated testing
│   ├── Prepare-Release.ps1          # Release preparation
│   └── Run-MSAP.ps1                 # MSAP validation wrapper
└── Documentation/                   # Internal guides and references
    ├── README.md                    # This file
    ├── MSIX-Build-Guide.md          # How to build packages
    ├── Local-Testing-Guide.md       # How to test packages locally
    ├── Partner-Center-Checklist.md  # Pre-submission checklist
    ├── Partner-Center-Submission.md # Step-by-step submission guide
    ├── Capability-Declaration.md    # Why each capability is declared
    ├── Certification-Reference.md   # Common issues & solutions
    ├── Implementation-Notes.md      # Design decisions & constraints
    ├── Version-Management.md        # Version numbering strategy
    └── AppxManifest-Reference.md    # Manifest schema reference
```

---

## Key Files

### AppxManifest.xml
Application configuration for MSIX package. Declares:
- Application identity and version
- Visual assets (icons, logos)
- System capabilities (microphone, registry access, etc.)
- Minimum OS requirements
- Applications and entry points

### JyGlobalVST.msixproj
Visual Studio project file for packaging. Configures:
- x64 architecture (only)
- Build output directory
- Manifest location
- Asset references
- Auto-signing settings

### Scripts
Automated tools for building, validating, and testing packages:
- **Build-MSIX.ps1**: Compiles app and creates package (main entry point)
- **Validate-Package.ps1**: Checks manifest schema and content
- **Test-LocalInstall.ps1**: Tests installation and functionality
- **Prepare-Release.ps1**: Prepares release with version management

---

## Documentation Map

| Document | Purpose | Audience |
|----------|---------|----------|
| [MSIX-Build-Guide.md](MSIX-Build-Guide.md) | How to build MSIX locally | Developers |
| [Local-Testing-Guide.md](Local-Testing-Guide.md) | How to test packages | QA Engineers |
| [Partner-Center-Checklist.md](Partner-Center-Checklist.md) | Pre-submission verification | Release Managers |
| [Partner-Center-Submission.md](Partner-Center-Submission.md) | Step-by-step submission | Product Owners |
| [Capability-Declaration.md](Capability-Declaration.md) | Why each capability is needed | Security Review |
| [Certification-Reference.md](Certification-Reference.md) | Common issues & solutions | Everyone |
| [Implementation-Notes.md](Implementation-Notes.md) | Design decisions | Architects |

---

## Common Tasks

### Build a Package

```powershell
# Update version
Edit StorePackaging\version-info.txt

# Build
.\StorePackaging\Scripts\Build-MSIX.ps1 -Version 1.0.0.0

# Validate
.\StorePackaging\Scripts\Validate-Package.ps1 -PackagePath build\store-packages\JyGlobalVST_1.0.0.0_x64.msixbundle
```

### Test Installation

```powershell
.\StorePackaging\Scripts\Test-LocalInstall.ps1 -PackagePath build\store-packages\JyGlobalVST_1.0.0.0_x64.msixbundle
```

### Prepare Release

```powershell
.\StorePackaging\Scripts\Prepare-Release.ps1 -Version "1.0.0"

# Then create git tag:
git tag v1.0.0.0
git push origin v1.0.0.0

# CI/CD automatically builds and creates release
```

### Submit to Partner Center

See [Partner-Center-Submission.md](Partner-Center-Submission.md) for step-by-step instructions.

---

## Version Management

Version format: `Major.Minor.Build.Revision` (e.g., `1.0.0.0`)

Current version: See `version-info.txt`

Update before each release:
```
1.0.0.0  # Initial release
1.0.1.0  # Bug fixes
1.1.0.0  # New features
2.0.0.0  # Major release
```

See [Version-Management.md](Version-Management.md) for details.

---

## Assets (Icons)

All icon files must be:
- **Format**: PNG with transparency (RGBA)
- **Size**: Exact pixel dimensions (no scaling)
- **Location**: `Assets/` directory
- **Naming**: Match manifest references exactly

Required icons:
- Square44x44Logo.png (44×44)
- Square50x50Logo.png (50×50)
- Square150x150Logo.png (150×150)
- Square310x310Logo.png (310×310)
- StoreLogo.png (50×50)

---

## Troubleshooting

**Build fails?** → Run CMake and Visual Studio Build manually, check prerequisites  
**Manifest error?** → Run Validate-Package.ps1, check against schema  
**Installation fails?** → Run Test-LocalInstall.ps1 for diagnostic output  
**Certification rejected?** → See Certification-Reference.md for solutions  

---

## References

- **MSIX Documentation**: https://learn.microsoft.com/en-us/windows/msix/
- **AppxManifest Schema**: https://learn.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/
- **Partner Center**: https://partner.microsoft.com/
- **Store Policies**: https://docs.microsoft.com/en-us/windows/uwp/publish/store-policies

