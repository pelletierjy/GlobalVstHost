# Data Model: Microsoft Store MSIX Packaging

**Phase**: 1 (Design) | **Date**: 2026-07-05 | **Status**: Complete

This document defines the structure, validation rules, and relationships for all entities involved in MSIX packaging for Microsoft Store publication.

---

## 1. MSIX Package Structure

### Entity: MSIX Package

A signed, compressed archive containing the application binary, dependencies, metadata, and digital signature, ready for distribution via Microsoft Store.

**Fields**:
| Field | Type | Required | Format | Notes |
|-------|------|----------|--------|-------|
| `filename` | string | Yes | `.msix` or `.msixbundle` | Naming convention: `JyGlobalVST_{Version}.msix` |
| `version` | version | Yes | `Major.Minor.Build.Revision` | E.g., `1.0.0.0`; must be integer parts |
| `architecture` | enum | Yes | `x64` only | Per spec, v1 supports x64 only |
| `signature_valid` | boolean | Yes | — | Verified by MSAP or Windows API |
| `size_bytes` | integer | Yes | — | Typical: 50-500MB for desktop apps |
| `manifest_validated` | boolean | Yes | — | Validated against AppxManifest schema |
| `created_at` | timestamp | Yes | ISO 8601 | Build timestamp |

**Relationships**:
- `contains` → AppxManifest.xml (1:1)
- `contains` → IconAsset objects (1:many; typically 4-6 icons)
- `contains` → Application binary + dependencies (1:1)
- `signed_by` → Certificate (1:1; auto-generated for dev, Microsoft-signed for Store)

**Validation Rules**:
- ✓ Package must contain exactly one AppxManifest.xml
- ✓ All icon references in manifest must have corresponding file in package
- ✓ Application entry point (executable path) must exist in package
- ✓ Version number must be higher than any previously submitted version
- ✓ Size must not exceed Microsoft Store limits (500MB initial; typically apps are 50-200MB)
- ✓ Signature must be valid (MSAP checks this)

**State Transitions**:
```
[Generated] → [Validated] → [Signed] → [Submitted] → [Certified] → [Published]
```

---

## 2. AppxManifest.xml

### Entity: Manifest Document

The core XML configuration file declaring application metadata, capabilities, extensions, and asset references.

**Root Element**: `<Package>` (following UWP AppxManifest schema)

### Key Metadata Elements

#### Identity Element

```xml
<Identity
  Name="JyGlobalVST"
  Publisher="CN=JyGlobalVST"
  Version="1.0.0.0" />
```

| Attribute | Required | Type | Format | Notes |
|-----------|----------|------|--------|-------|
| `Name` | Yes | string | alphanumeric + hyphens | Must be unique in Microsoft Store; reserved during app creation |
| `Publisher` | Yes | string | `CN=CompanyName` format | Distinguished Name; can be auto-generated |
| `Version` | Yes | version | `Major.Minor.Build.Revision` | All parts must be integers 0-65535 |

**Validation Rules**:
- ✓ Name must match app name reserved in Partner Center
- ✓ Version must be sequential (no gaps, no decrease)
- ✓ Publisher CN format must be valid X.509 Distinguished Name

#### Properties Element

```xml
<Properties>
  <DisplayName>JyGlobalVST</DisplayName>
  <PublisherDisplayName>JyGlobalVST</PublisherDisplayName>
  <Logo>Assets/StoreLogo.png</Logo>
</Properties>
```

| Element | Required | Type | Notes |
|---------|----------|------|-------|
| `DisplayName` | Yes | string | Displayed in Start menu, Settings, Store |
| `PublisherDisplayName` | Yes | string | Company/publisher name shown in Store |
| `Logo` | Yes | file path | 50×50 PNG; must exist in Assets |

#### Applications Element

```xml
<Applications>
  <Application
    Id="JyGlobalVSTApp"
    StartPage="JyGlobalVST.exe">
    <uap:VisualElements
      DisplayName="JyGlobalVST"
      Square150x150Logo="Assets/Square150x150Logo.png"
      Square44x44Logo="Assets/Square44x44Logo.png"
      Description="VST audio processor for Windows"
      BackgroundColor="transparent" />
  </Application>
</Applications>
```

| Attribute | Required | Type | Format | Notes |
|-----------|----------|------|--------|-------|
| `Id` | Yes | string | alphanumeric + underscores | Unique within manifest; AUMID component |
| `StartPage` | Yes | file path | Path to .exe | Must be relative path in package |

#### Capabilities Element

```xml
<Capabilities>
  <Capability Name="microphone" />
  <Capability Name="audioLibrary" />
  <uap:Capability Name="documentsLibrary" />
  <uap:Capability Name="registryRead" />
  <uap:Capability Name="registryWrite" />
</Capabilities>
```

**Declared Capabilities for JyGlobalVST**:

| Capability | Purpose | Required for Feature | Namespace |
|-----------|---------|----------------------|-----------|
| `microphone` | Loopback audio capture | Yes | `cap:` (implicit) |
| `audioLibrary` | Audio file access | Recommended | `cap:` |
| `documentsLibrary` | Settings file persistence | Yes | `uap:` |
| `registryRead` | Read HKEY_CURRENT_USER\Software | Maybe | `uap:` |
| `registryWrite` | Write HKEY_CURRENT_USER\Software | Maybe | `uap:` |

**Validation Rules**:
- ✓ Each declared capability must have a functional justification in the application
- ✓ Do not declare unused capabilities (Microsoft Store validates this)
- ✓ Capability names must match official UWP capability list (Microsoft-defined)
- ✓ Some capabilities require a `uap:` XML namespace prefix (defined at root element)

#### Dependencies Element (Optional)

```xml
<Dependencies>
  <TargetDeviceFamily Name="Windows.Desktop" MinVersion="10.0.18363.0" MaxVersionTested="10.0.26200.0" />
</Dependencies>
```

| Attribute | Required | Format | Notes |
|-----------|----------|--------|-------|
| `MinVersion` | Yes | `Major.Minor.Build.Revision` | Windows 10 1909 = 10.0.18363.0 |
| `MaxVersionTested` | Yes | `Major.Minor.Build.Revision` | Windows 11 latest = 10.0.26200.0 (as of 2026-07) |

**Validation Rules**:
- ✓ MinVersion must be >= 10.0.18363.0 (Windows 10 1909)
- ✓ MaxVersionTested should be current Windows 11 version
- ✓ These versions must match application's actual compatibility

---

## 3. Icon Assets

### Entity: Icon Asset

A PNG image file used for application branding at various sizes and contexts.

**Fields**:
| Field | Type | Required | Format | Constraints |
|-------|------|----------|--------|-------------|
| `filename` | string | Yes | `.png` | Stored in Assets/ subdirectory |
| `purpose` | enum | Yes | See table below | Context where icon is used |
| `width_px` | integer | Yes | Integer | Exact match required |
| `height_px` | integer | Yes | Integer | Exact match required |
| `dpi_aware` | boolean | Yes | — | True; assets should scale correctly |
| `has_transparency` | boolean | Yes | — | True; must have alpha channel |

**Icon Inventory** (Complete List for v1):

| Filename | Purpose | Dimensions | DPI | Required |
|----------|---------|-----------|-----|----------|
| `Square44x44Logo.png` | Taskbar, context menu | 44×44 | 100% | ✓ Yes |
| `Square50x50Logo.png` | Tile icon | 50×50 | 100% | ✓ Yes |
| `Square150x150Logo.png` | Medium tile | 150×150 | 100% | ✓ Yes |
| `Square310x310Logo.png` | Large tile | 310×310 | 100% | ✓ Yes |
| `StoreLogo.png` | Microsoft Store listing | 50×50 | 100% | ✓ Yes |
| `Wide310x150Logo.png` | Wide tile (optional) | 310×150 | 100% | ✗ Optional |
| `SplashScreen.png` | Splash/launch screen (optional) | 620×300 | 100% | ✗ Optional |

**Validation Rules**:
- ✓ All dimensions must be exact pixel matches (no rounding)
- ✓ Format must be PNG (not JPG, BMP, or other)
- ✓ All images must have transparency (RGBA, not RGB-only)
- ✓ All images must be referenced in AppxManifest.xml
- ✓ File paths in manifest must match actual file paths in Assets/

**Best Practices**:
- Design single source icon (e.g., 512×512 at 300 DPI)
- Generate all variants using automated asset generator
- Test rendering at 100%, 125%, 150%, 200% DPI scaling
- Verify transparency on both light and dark backgrounds

---

## 4. Build Artifact

### Entity: Build Output

The compiled application binary and all runtime dependencies, ready to be packaged into MSIX.

**Fields**:
| Field | Type | Required | Notes |
|-------|------|----------|-------|
| `exe_path` | file path | Yes | Relative path in package: `bin/JyGlobalVST.exe` |
| `exe_size_bytes` | integer | Yes | Typical: 5-50MB for desktop app |
| `dependencies` | list[file path] | Yes | JUCE runtime libs, Windows system DLLs (usually auto-included) |
| `config` | enum | Yes | Must be "Release"; Debug builds not shipped |
| `architecture` | enum | Yes | Must be "x64" |
| `timestamp` | timestamp | Yes | Build timestamp (for debugging) |

**Validation Rules**:
- ✓ EXE must be built in Release configuration (no Debug symbols in distribution)
- ✓ EXE must target x64 architecture only
- ✓ EXE must be digitally signable (no invalid PE headers)
- ✓ All runtime dependencies must be included or declared as system DLLs
- ✓ No unsigned executables (Windows Defender will flag them)

---

## 5. Build Artifact (Continued): Package Output

### Entity: Package Output Bundle

The final .msixbundle file (or .msix in v1) produced by the MSIX build process.

**Directory Structure** (Expected Output):

```
build/
├── store-packages/
│   ├── JyGlobalVST_1.0.0.0_x64.msix          # Individual package
│   ├── JyGlobalVST_1.0.0.0_x64.msixbundle    # Bundle (can support multiple archs)
│   ├── JyGlobalVST_1.0.0.0_x64.appxsym       # Symbol file (optional, for debugging)
│   └── AppxManifest.xml                      # Manifest copy (for reference)
└── ...
```

**Fields**:
| Field | Type | Format | Notes |
|-------|------|--------|-------|
| `bundle_filename` | string | `.msixbundle` | Standard extension |
| `version` | version | `Major.Minor.Build.Revision` | Matches manifest version |
| `architecture` | enum | `x64` | Only one arch in v1 |
| `size_bytes` | integer | — | Typical: 50-200MB |
| `validation_status` | enum | "PASS" / "FAIL" | Result of MSAP validation |
| `ready_for_submission` | boolean | — | True only after all checks pass |

**Validation Rules**:
- ✓ Filename must follow convention: `JyGlobalVST_{Version}_{Architecture}.msixbundle`
- ✓ Bundle must contain at least one .msix package
- ✓ Version in bundle filename must match manifest version
- ✓ Bundle must pass MSAP validation (zero errors)
- ✓ Bundle must be unsigned (Partner Center will sign for distribution)

---

## 6. Manifest Validation Schema

### Entity: Manifest Validation Rules

Business logic and constraints enforced during manifest generation and validation.

**Version Numbering**:
```
Rule: version_format
  Pattern: ^(\d{1,5})\.(\d{1,5})\.(\d{1,5})\.(\d{1,5})$
  Example: 1.0.0.0, 1.2.3.4
  Min: 0.0.0.0
  Max: 65535.65535.65535.65535

Rule: version_increment
  Previous version: 1.0.0.0
  New version: 1.0.0.1 ✓ (allowed)
  New version: 1.0.0.0 ✗ (rejected; must be higher)
  New version: 0.9.9.9 ✗ (rejected; must be higher)
```

**Asset Path Validation**:
```
Rule: asset_path_format
  Requirement: Paths must be relative, forward-slash separated
  Example: Assets/Square44x44Logo.png ✓
  Invalid: \Assets\Icon.png ✗ (backslashes not allowed)
  Invalid: C:\Assets\Icon.png ✗ (absolute paths not allowed)
```

**Capability Validation**:
```
Rule: declared_capabilities_must_be_used
  If manifest declares <Capability Name="microphone" />
    Then application must actually use microphone features
    Validation method: Code review + functional testing
    
Rule: no_undeclared_capability_usage
  If application uses audio APIs
    Then capability must be declared
    Validation method: Runtime permission checks on Windows
```

**Entry Point Validation**:
```
Rule: startup_page_must_exist
  If Applications/Application/@StartPage = "JyGlobalVST.exe"
    Then file must exist in package at root or bin/ subdirectory
    Status: FAIL if exe not found
    Status: FAIL if exe name has typos or case mismatch
```

---

## 7. Relationships & Cardinality

```
┌─────────────────────────────────────────────────┐
│          MSIX Package (1)                       │
├─────────────────────────────────────────────────┤
│  • Version: 1.0.0.0                             │
│  • Architecture: x64                            │
│  • Signature: Validated                         │
└──────────────┬──────────────────────────────────┘
               │
        ┌──────┴──────┬───────────┬──────────────┐
        │             │           │              │
        ▼             ▼           ▼              ▼
   Manifest(1)  Icons(4-6)  Binary(1)  Dependencies(many)
   
┌─────────────────────┐
│  AppxManifest.xml   │
├─────────────────────┤
│ • Identity(1)       │ ──→ Publisher CN, Name
│ • Properties(1)     │ ──→ DisplayName, Logo ref
│ • Applications(1)   │ ──→ Entry point, Visual Assets
│ • Capabilities(0..*) │ ──→ microphone, registryWrite, etc
│ • Dependencies(1)   │ ──→ MinVersion, MaxVersionTested
└─────────────────────┘

┌──────────────────────────┐
│  Icon Assets (4-6 items) │
├──────────────────────────┤
│ Square44x44Logo.png      │
│ Square150x150Logo.png    │
│ Square310x310Logo.png    │
│ StoreLogo.png            │
│ (and optional extras)    │
└──────────────────────────┘
```

---

## 8. State Transitions & Lifecycle

```
Development Lifecycle:
┌──────────────┐
│  Source Code │ (C++ + CMake)
└──────┬───────┘
       │ compile (Debug)
       ▼
┌──────────────────┐
│  Debug Build     │ (For developer testing)
│  (Optional)      │
└──────┬───────────┘
       │ compile (Release)
       ▼
┌──────────────────┐
│  Release Binary  │ (JyGlobalVST.exe + deps)
│  + Assets        │ (Icons, manifest template)
└──────┬───────────┘
       │ package (MSIX Packaging Tool)
       ▼
┌──────────────────────────┐
│  Unsigned MSIX Package   │ (For Partner Center)
│  (.msix or .msixbundle)  │
└──────┬───────────────────┘
       │ validate (MSAP tool)
       ├─→ [FAIL] ──→ fix manifest ──→ [retry]
       │
       └─→ [PASS]
           │
           ▼
┌──────────────────────────┐
│  Signed MSIX Package     │ (By Microsoft Store)
│  (Ready for distribution)│
└──────┬───────────────────┘
       │ download (from Store)
       ▼
┌──────────────────────────┐
│  Installed App           │ (On user machine)
│  (HKEY_LOCAL_MACHINE)    │
└──────────────────────────┘
```

---

## 9. Data Validation & Constraints Summary

| Entity | Constraint | Validation Method | Enforced By |
|--------|-----------|-------------------|------------|
| Version | `Major.Minor.Build.Revision` format | Regex match | Manifest schema |
| Version | Sequential (no decreases) | Compare to previous | Partner Center |
| Manifest | Valid XML schema | XSD validator | MSAP |
| Manifest | All declared capabilities used | Code review | Store team |
| Icons | Exact pixel dimensions | File metadata check | MSAP |
| Icons | PNG format with transparency | File magic bytes | Build script |
| Entry point | EXE exists in package | File presence check | Package validation |
| Publisher | Valid X.509 DN format | DN parser | Manifest parser |
| Architecture | x64 only (v1) | PE header check | Build script |

---

## Next Steps

1. **Phase 1b**: Create contracts/appx-manifest-schema.md with detailed XML examples
2. **Phase 1c**: Create quickstart.md with validation procedures
3. **Phase 2**: Generate tasks.md with implementation work items
4. **Phase 3**: Implement scaffolding (create StorePackaging/ directory, add Build-MSIX.ps1 script)

