# Contract: AppxManifest.xml Schema

**Phase**: 1 (Design) | **Date**: 2026-07-05 | **Status**: Complete

This document defines the structure, required fields, and validation rules for the AppxManifest.xml file used in MSIX packaging. It serves as the canonical reference for manifest generation and validation.

---

## Document Purpose

This contract specifies:
- **What fields are required vs. optional** in the manifest
- **What formats and values are valid** for each field
- **What constraints** are enforced during package validation
- **What examples** show correct usage

This allows manifest generators (automated scripts or Visual Studio tooling) to produce correct manifests, and validators (MSAP, Partner Center) to verify them.

---

## AppxManifest.xml Structure

### Root Element: `<Package>`

The top-level element that wraps all manifest content.

```xml
<?xml version="1.0" encoding="utf-8"?>
<Package
  xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10"
  xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10"
  IgnorableNamespaces="uap">

  <!-- Child elements... -->
</Package>
```

**Attributes**:
| Attribute | Required | Value | Notes |
|-----------|----------|-------|-------|
| `xmlns` | Yes | `http://schemas.microsoft.com/appx/manifest/foundation/windows10` | Standard UWP namespace |
| `xmlns:uap` | Yes | `http://schemas.microsoft.com/appx/manifest/uap/windows10` | Extended capabilities namespace |
| `IgnorableNamespaces` | Yes | `uap` | Tells parser to allow unknown `uap:` elements gracefully |

---

## Required Child Elements

All elements below are **REQUIRED** unless marked *[OPTIONAL]*.

### 1. Identity Element

Declares the application's unique identity in the Microsoft Store ecosystem.

```xml
<Identity
  Name="JyGlobalVST"
  Publisher="CN=JyGlobalVST"
  Version="1.0.0.0" />
```

**Child attributes**:
| Attribute | Required | Type | Format | Constraints | Example |
|-----------|----------|------|--------|-------------|---------|
| `Name` | Yes | string | Alphanumeric + hyphens, no spaces | 3-50 chars; must match Partner Center app name reservation | `JyGlobalVST` |
| `Publisher` | Yes | string | X.509 Distinguished Name | Format: `CN=CompanyName[,O=Org,C=Country]` | `CN=JyGlobalVST` |
| `Version` | Yes | version quad | `Major.Minor.Build.Revision` | Each component: integer 0-65535; must increase for each submission | `1.0.0.0` |

**Validation Rules**:
- ✓ `Name` must match the app name registered in Partner Center
- ✓ `Version` must be strictly greater than any previously published version
- ✓ `Publisher` CN component is required; O and C are optional but recommended
- ✓ No whitespace before or after attribute values

**Example** (correct):
```xml
<Identity
  Name="JyGlobalVST"
  Publisher="CN=JyGlobalVST,O=JyGlobalVST,C=US"
  Version="1.0.0.0" />
```

**Invalid examples**:
```xml
<!-- ✗ Name doesn't match Partner Center registration -->
<Identity Name="GlobalVST" Publisher="CN=JyGlobalVST" Version="1.0.0.0" />

<!-- ✗ Version not incremented -->
<Identity Name="JyGlobalVST" Publisher="CN=JyGlobalVST" Version="0.9.9.9" />

<!-- ✗ Version has invalid format (non-integer component) -->
<Identity Name="JyGlobalVST" Publisher="CN=JyGlobalVST" Version="1.0.0.0-alpha" />
```

---

### 2. Properties Element

Declares visual and descriptive properties displayed in Start menu, Settings, and Microsoft Store.

```xml
<Properties>
  <DisplayName>JyGlobalVST</DisplayName>
  <PublisherDisplayName>JyGlobalVST</PublisherDisplayName>
  <Logo>Assets/StoreLogo.png</Logo>
</Properties>
```

**Child elements**:
| Element | Required | Type | Constraints | Example |
|---------|----------|------|-------------|---------|
| `DisplayName` | Yes | string | 1-256 chars; shown in Start menu and Store | `JyGlobalVST` |
| `PublisherDisplayName` | Yes | string | 1-256 chars; company/publisher name | `JyGlobalVST` |
| `Logo` | Yes | file path | Relative path; must point to 50×50 PNG icon | `Assets/StoreLogo.png` |

**Validation Rules**:
- ✓ All three child elements are required (no optional sub-elements)
- ✓ Paths are case-sensitive on Windows; must match exactly
- ✓ `Logo` file must exist in the package at specified path
- ✓ `Logo` file must be exactly 50×50 pixels, PNG format, with transparency

**Example** (correct):
```xml
<Properties>
  <DisplayName>JyGlobalVST</DisplayName>
  <PublisherDisplayName>JyGlobalVST</PublisherDisplayName>
  <Logo>Assets/StoreLogo.png</Logo>
</Properties>
```

---

### 3. Dependencies Element

Declares the target operating system version and device family.

```xml
<Dependencies>
  <TargetDeviceFamily
    Name="Windows.Desktop"
    MinVersion="10.0.18363.0"
    MaxVersionTested="10.0.26200.0" />
</Dependencies>
```

**Attributes of `TargetDeviceFamily`**:
| Attribute | Required | Type | Format | Constraints | Example |
|-----------|----------|------|--------|-------------|---------|
| `Name` | Yes | enum | Fixed value | Must be `Windows.Desktop` (only option for desktop apps) | `Windows.Desktop` |
| `MinVersion` | Yes | version quad | `Major.Minor.Build.Revision` | Minimum supported OS; recommend >= 10.0.18363.0 (Win 10 1909) | `10.0.18363.0` |
| `MaxVersionTested` | Yes | version quad | `Major.Minor.Build.Revision` | Latest OS tested during development; use current Windows 11 version | `10.0.26200.0` |

**Validation Rules**:
- ✓ `MinVersion` must be >= 10.0.18363.0 (Windows 10 1909) per specification
- ✓ `MaxVersionTested` should be current Windows 11 build number
- ✓ `MaxVersionTested` must be >= `MinVersion`
- ✓ `Name` is always `Windows.Desktop` for desktop applications

**Version Reference**:
| OS | Build | Version |
|----|-------|---------|
| Windows 10 1909 | 18363 | 10.0.18363.0 |
| Windows 10 21H2 | 19044 | 10.0.19044.0 |
| Windows 11 (initial) | 22000 | 10.0.22000.0 |
| Windows 11 23H2 | 22621 | 10.0.22621.0 |
| Windows 11 current (2026) | 26200 | 10.0.26200.0 |

**Example** (correct):
```xml
<Dependencies>
  <TargetDeviceFamily
    Name="Windows.Desktop"
    MinVersion="10.0.18363.0"
    MaxVersionTested="10.0.26200.0" />
</Dependencies>
```

---

### 4. Capabilities Element

Declares what system resources the application can access (microphone, registry, files, etc.).

```xml
<Capabilities>
  <Capability Name="microphone" />
  <Capability Name="audioLibrary" />
  <uap:Capability Name="documentsLibrary" />
  <uap:Capability Name="registryRead" />
  <uap:Capability Name="registryWrite" />
</Capabilities>
```

**Valid Capability Names** (for audio/VST applications):

| Capability Name | Namespace | Purpose | Required for JyGlobalVST | Justification |
|-----------------|-----------|---------|--------------------------|---------------|
| `microphone` | implicit (base) | Audio input capture | ✓ Yes | WASAPI loopback capture |
| `audioLibrary` | implicit (base) | Audio file library access | Optional | For future file-based audio I/O |
| `documentsLibrary` | `uap:` | User Documents folder | ✓ Yes | Preset file storage, user guide |
| `registryRead` | `uap:` | Read user registry hive | ✓ Yes | Read app settings (optional but common) |
| `registryWrite` | `uap:` | Write user registry hive | ✓ Yes | Persist app settings (optional but common) |

**Validation Rules**:
- ✓ Each declared capability must correspond to actual code paths in the application
- ✓ Over-declaring capabilities may cause Store rejection
- ✓ Under-declaring capabilities may cause runtime permission failures
- ✓ Capability names are case-sensitive
- ✓ Capabilities using `uap:` prefix MUST have `xmlns:uap="..."` declared at root `<Package>` level

**Example** (correct; minimal):
```xml
<Capabilities>
  <Capability Name="microphone" />
  <Capability Name="audioLibrary" />
  <uap:Capability Name="documentsLibrary" />
</Capabilities>
```

**Example** (extended; for full feature set):
```xml
<Capabilities>
  <Capability Name="microphone" />
  <Capability Name="audioLibrary" />
  <uap:Capability Name="documentsLibrary" />
  <uap:Capability Name="registryRead" />
  <uap:Capability Name="registryWrite" />
</Capabilities>
```

---

### 5. Applications Element

Declares the application entry point and visual presentation.

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

**Application Element Attributes**:
| Attribute | Required | Type | Format | Constraints | Example |
|-----------|----------|------|--------|-------------|---------|
| `Id` | Yes | string | Alphanumeric + underscores | 1-256 chars; becomes part of AUMID | `JyGlobalVSTApp` |
| `StartPage` | Yes | file path | Relative path to .exe | Must be the main application executable | `JyGlobalVST.exe` |

**VisualElements Sub-attributes**:
| Attribute | Required | Type | Constraints | Example |
|-----------|----------|------|-------------|---------|
| `DisplayName` | Yes | string | 1-256 chars | `JyGlobalVST` |
| `Square150x150Logo` | Yes | file path | Path to 150×150 PNG icon | `Assets/Square150x150Logo.png` |
| `Square44x44Logo` | Yes | file path | Path to 44×44 PNG icon | `Assets/Square44x44Logo.png` |
| `Description` | Yes | string | 1-256 chars; brief app description | `VST audio processor for Windows` |
| `BackgroundColor` | Yes | enum | `transparent` or hex color (e.g., `#FFFFFF`) | `transparent` |

**Validation Rules**:
- ✓ `StartPage` EXE file must exist in the package root (not in subdirectories)
- ✓ All icon files referenced in `VisualElements` must exist
- ✓ Icon files must have exact dimensions specified (no rounding)
- ✓ `Id` value becomes part of Application User Model ID (AUMID); keep alphanumeric + underscores only
- ✓ At least one `Application` element required; multiple apps possible (but rare)

**Example** (correct):
```xml
<Applications>
  <Application
    Id="JyGlobalVSTApp"
    StartPage="JyGlobalVST.exe">
    <uap:VisualElements
      DisplayName="JyGlobalVST"
      Square150x150Logo="Assets/Square150x150Logo.png"
      Square44x44Logo="Assets/Square44x44Logo.png"
      Description="System audio VST host for Windows"
      BackgroundColor="transparent" />
  </Application>
</Applications>
```

---

## Optional Extensions

Extensions declare additional capabilities (e.g., file associations, protocols). For v1 of JyGlobalVST, extensions are *NOT required*.

```xml
<!-- Example: File association (optional, not needed for v1) -->
<Extensions>
  <uap:Extension Category="windows.fileTypeAssociation">
    <uap:FileTypeAssociation Name="jvst">
      <uap:SupportedFileTypes>
        <uap:FileType>.jvst</uap:FileType>
      </uap:SupportedFileTypes>
    </uap:FileTypeAssociation>
  </uap:Extension>
</Extensions>
```

**For v1**: Omit the `<Extensions>` element entirely.

---

## Complete Example Manifest

```xml
<?xml version="1.0" encoding="utf-8"?>
<Package
  xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10"
  xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10"
  IgnorableNamespaces="uap">

  <Identity
    Name="JyGlobalVST"
    Publisher="CN=JyGlobalVST,O=JyGlobalVST"
    Version="1.0.0.0" />

  <Properties>
    <DisplayName>JyGlobalVST</DisplayName>
    <PublisherDisplayName>JyGlobalVST</PublisherDisplayName>
    <Logo>Assets/StoreLogo.png</Logo>
  </Properties>

  <Dependencies>
    <TargetDeviceFamily
      Name="Windows.Desktop"
      MinVersion="10.0.18363.0"
      MaxVersionTested="10.0.26200.0" />
  </Dependencies>

  <Capabilities>
    <Capability Name="microphone" />
    <Capability Name="audioLibrary" />
    <uap:Capability Name="documentsLibrary" />
    <uap:Capability Name="registryRead" />
    <uap:Capability Name="registryWrite" />
  </Capabilities>

  <Applications>
    <Application
      Id="JyGlobalVSTApp"
      StartPage="JyGlobalVST.exe">
      <uap:VisualElements
        DisplayName="JyGlobalVST"
        Square150x150Logo="Assets/Square150x150Logo.png"
        Square44x44Logo="Assets/Square44x44Logo.png"
        Description="System audio VST host and effects processor"
        BackgroundColor="transparent" />
    </Application>
  </Applications>

</Package>
```

---

## Validation Checklist

Use this checklist when verifying a generated manifest:

- [ ] XML is well-formed (can be parsed without errors)
- [ ] Root element is `<Package>` with required xmlns attributes
- [ ] `<Identity>` element present with Name, Publisher, Version
- [ ] `<Properties>` element present with DisplayName, PublisherDisplayName, Logo
- [ ] `<Dependencies>` element present with TargetDeviceFamily
- [ ] `<Capabilities>` element present with at least `microphone` declared
- [ ] `<Applications>` element present with at least one `Application` child
- [ ] All file paths in manifest point to files that exist in the package
- [ ] Version is strictly greater than previously published version
- [ ] All icon files are PNG format with transparency
- [ ] All icon files have exact pixel dimensions required
- [ ] `StartPage` executable file exists in package
- [ ] No typos in capability names or attribute values

---

## References

- **Microsoft AppxManifest.xml Schema**: https://learn.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/
- **Capabilities Reference**: https://learn.microsoft.com/en-us/windows/uwp/packaging/app-capability-declarations
- **XML Best Practices**: https://learn.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/element-identity

