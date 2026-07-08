# Feature Specification: Microsoft Store Publication via MSIX Packaging

**Feature Branch**: `009-microsoft-store-publish`

**Created**: 2026-07-05

**Status**: Draft

**Input**: Integrate all necessary steps, files, scripts, and configurations to enable publishing the JyGlobalVST application to the Microsoft Store using the lowest-cost path: MSIX packaging with automatic signing and free hosting via the Microsoft Store, with no paid certificates required.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Package the application for Microsoft Store distribution (Priority: P1)

As an application maintainer, I need to prepare JyGlobalVST for Microsoft Store submission. Currently the application is only distributed via manual download. I want to package it using MSIX format with automatic signing, eliminating the need for expensive code-signing certificates. The packaging process should be automated so it runs reliably as part of the build pipeline without manual intervention.

**Why this priority**: This is the primary objective of the feature — the entire scope depends on successfully creating a distributable MSIX package with automatic signing.

**Independent Test**: Can be verified by executing the build automation, confirming a valid .msixbundle is generated, and validating its manifest and signature using Microsoft tools.

**Acceptance Scenarios**:

1. **Given** the application source code is ready, **When** the automated build process runs, **Then** a complete MSIX package (.msix) or bundle (.msixbundle) is generated without requiring external certificates or manual signing steps.
2. **Given** a generated MSIX package, **When** validated against Microsoft Store requirements using official tools, **Then** the package passes validation including manifest integrity, architecture support, and capability declarations.
3. **Given** an MSIX package exists, **When** installed on a clean Windows system, **Then** the application installs successfully without administrative privileges required.
4. **Given** the application is installed via MSIX, **When** the user launches it, **Then** it runs identically to the traditional EXE installation, with all features functional.

---

### User Story 2 - Automate MSIX build as part of the release pipeline (Priority: P1)

As a CI/CD pipeline administrator, I need the MSIX packaging to be fully automated so that creating a release candidate requires only triggering a build job, not manual packaging steps. The automation should handle all stages: building the application, creating the package manifest, generating the final bundle, and validating it locally before it's ready for Partner Center submission.

**Why this priority**: Without automation, the process becomes a manual bottleneck and introduces errors. Automation ensures consistent, repeatable builds for every release candidate.

**Independent Test**: Can be verified by executing the CI workflow multiple times and confirming each produces an identical valid package without manual intervention.

**Acceptance Scenarios**:

1. **Given** a configured CI workflow exists, **When** the workflow is triggered for a release tag, **Then** it automatically builds the application in Release configuration without prompts or external input.
2. **Given** the application has been built, **When** the workflow continues, **Then** it automatically generates an AppxManifest.xml file and packages the application into MSIX format using no external certificates.
3. **Given** an MSIX package is generated, **When** the workflow reaches validation, **Then** it runs local package tests (installation, launch, feature verification) before marking the package as ready for submission.
4. **Given** the workflow completes successfully, **When** a team member reviews the output, **Then** they can locate the ready-to-submit .msixbundle and all supporting metadata (manifest, icons, version info) in a well-organized output directory.

---

### User Story 3 - Prepare submission materials for Microsoft Partner Center (Priority: P2)

As a product owner preparing for store launch, I need all the supplementary materials that Microsoft Partner Center requires to be already generated and documented. This includes the version number scheme, all required icon sizes, capability declarations, and a clear checklist of what's needed for certification. The goal is to reach Partner Center with all dependencies resolved so submission is a straightforward upload.

**Why this priority**: Partner Center submission requires exact formats and careful declarations; having automated generation and clear documentation reduces rejection risk.

**Independent Test**: Can be verified by collecting all generated materials, comparing them against Microsoft Store's official requirements checklist, and confirming zero formatting or completeness issues.

**Acceptance Scenarios**:

1. **Given** the automated build completes, **When** the submission materials are collected, **Then** the version number follows the required format (e.g., 1.0.0.0) and matches throughout the manifest and package metadata.
2. **Given** application assets exist, **When** they are processed for the Store, **Then** all required icon sizes are generated (44×44, 50×50, 150×150, 310×310, and any other Microsoft Store sizes) in PNG format with transparency.
3. **Given** the application manifest is generated, **When** Partner Center validates it, **Then** all required capability declarations (audio capture, file access, registry access, etc.) are correctly specified based on the application's actual needs.
4. **Given** all materials are ready, **When** a checklist is consulted, **Then** it provides a clear, itemized list of files and configuration values to enter in Partner Center, reducing submission errors.

---

### User Story 4 - Test MSIX package locally before Partner Center submission (Priority: P2)

As a QA engineer, I need to verify that the generated MSIX package installs and runs correctly on a clean system before it's submitted to Partner Center. I should have a documented procedure to test the package locally and identify issues early, catching certification failures before they happen on the Store side.

**Why this priority**: Local validation catches obvious failures (missing dependencies, broken registry declarations, etc.) before costly Partner Center reviews.

**Independent Test**: Can be verified by following the documented test procedure on a fresh Windows environment and confirming all test cases pass.

**Acceptance Scenarios**:

1. **Given** an MSIX package exists, **When** the test procedure is executed on a clean Windows 10 or 11 system, **Then** the package installs successfully without requiring admin rights and without errors.
2. **Given** the application has been installed via MSIX, **When** it is launched from the Start menu or Windows search, **Then** it starts without errors and the system tray icon appears.
3. **Given** the application is running from an MSIX installation, **When** the user interacts with all major UI flows (plugin loading, chain configuration, audio routing), **Then** all features work identically to the traditional installer version.
4. **Given** the MSIX package is installed, **When** it is uninstalled via Windows Settings, **Then** it removes cleanly without leaving orphaned files or registry entries.

---

### Edge Cases

- **Version numbering consistency**: Version numbers must be identical across AppxManifest.xml, package metadata, and the application binary itself.
- **Architecture support**: The package must correctly declare x64 support (32-bit and ARM64 are explicitly out of scope per project guidelines).
- **Dependency declarations**: All minimum Windows versions (Windows 10 1909+) must be correctly specified.
- **Asset scaling**: Icons and logos must render correctly at all required sizes and DPI scales.
- **Capability over-declaration**: The manifest must not request more capabilities than the application actually uses (rejection risk), but must not under-declare (functionality breaks).
- **Registry persistence**: Application settings stored in the registry must be accessible from the MSIX-installed application (may require manifest adjustments for isolation-aware paths).
- **UAC and admin elevation**: The application must not require elevation, but any auto-save or monitoring features must work in a standard user context.
- **Certificate chain**: Even with automatic signing, the signature chain must be valid and the package must pass Microsoft's validation tools.

---

## Requirements *(mandatory)*

### Functional Requirements

#### Project Structure & Packaging Setup

- **FR-001**: The project MUST include a dedicated `StorePackaging/` directory containing all packaging-related files and configurations.
- **FR-002**: The StorePackaging directory MUST contain the `.msixproj` file used by Microsoft's build tools to package the application.
- **FR-003**: A complete and valid `AppxManifest.xml` MUST be generated or maintained in the StorePackaging directory, conforming to Microsoft's current schema and all store requirements.
- **FR-004**: All application icons and logos MUST be stored in the StorePackaging directory at the required sizes (44×44, 50×50, 150×150, 310×310, 620×300, and any other store-specified dimensions).
- **FR-005**: The StorePackaging directory MUST include a subdirectory for build scripts and automation configuration.

#### Build Automation

- **FR-006**: A PowerShell script or CI workflow MUST exist that automates the complete MSIX build process (application compilation, manifest generation, packaging, bundling) without requiring manual certificate management.
- **FR-007**: The build automation MUST use automatic signing provided by MSIX Packaging Tool or Visual Studio, with NO requirement for external .pfx certificate files or purchases.
- **FR-008**: The automation script MUST handle architecture-specific packaging (x64 only, per project scope) and generate both individual .msix and .msixbundle formats.
- **FR-009**: The build process MUST output the final .msixbundle to a clearly documented location (e.g., `build/store-packages/`) for Partner Center submission.
- **FR-010**: The automation MUST include a local validation step using Microsoft's package validation tools before marking the package as ready for submission.

#### Manifest & Capability Configuration

- **FR-011**: The AppxManifest.xml MUST declare all capabilities required by the application (e.g., microphone access via loopback, audio output, registry access for settings persistence).
- **FR-012**: Capability declarations MUST be minimal — the manifest MUST NOT request capabilities the application does not use.
- **FR-013**: The manifest MUST correctly specify the minimum supported Windows version (10.0.17763 or later per requirement; 10.0.19041 for Windows 10 1909).
- **FR-014**: The manifest MUST declare the application as x64 (AMD64) only, with no 32-bit ARM64 targets.
- **FR-015**: Extension declarations (if any) MUST be correctly configured based on the application's VST plugin hosting requirements.

#### Asset Management

- **FR-016**: All icon and logo assets MUST be automatically generated or validated to the exact pixel dimensions required by Microsoft Store (no upscaling or downscaling at upload time).
- **FR-017**: Icons MUST be in PNG format with transparency support (no JPG or BMP).
- **FR-018**: A visual asset inventory MUST be maintained documenting which files are used where in the manifest and Partner Center submission.

#### Local Testing & Validation

- **FR-019**: A documented procedure MUST exist that describes how to test an MSIX package on a clean Windows system.
- **FR-020**: The testing procedure MUST include steps to verify installation (without admin rights), launch, basic functionality, and uninstallation.
- **FR-021**: The build automation MUST include an optional local test step that can be run in CI or manually to detect obvious issues before Partner Center submission.
- **FR-022**: A checklist MUST exist that lists all Partner Center submission requirements (fields, metadata, files) so that submission becomes straightforward data entry.

#### Documentation

- **FR-023**: A comprehensive internal guide MUST exist that explains how to generate the MSIX package using the automation.
- **FR-024**: The guide MUST document how to test the package locally.
- **FR-025**: The guide MUST explain how to submit the package to Partner Center, including required fields and expected data values.
- **FR-026**: The guide MUST list common certification errors and how to avoid them (e.g., over-declared capabilities, missing minimum OS declarations, architecture mismatches).

### Non-Functional Requirements

- **NFR-001**: The build automation MUST run in under 10 minutes for a clean build on a standard developer machine.
- **NFR-002**: No external dependencies beyond MSIX Packaging Tool or Visual Studio (both freely available on Windows) may be required.
- **NFR-003**: The packaging process MUST be reproducible — running it twice with the same source code MUST produce bit-identical outputs (deterministic builds).

---

## Success Criteria *(mandatory)*

### Packaging & Build

- [x] MSIX package is generated automatically with zero manual certificate handling required
- [x] Generated package passes Microsoft's official validation tools (MSAP, etc.) without errors
- [x] Package can be installed on Windows 10 (1909+) and Windows 11 (x64) without admin elevation
- [x] Installed application functions identically to the traditional EXE installer version
- [x] Version number format is consistent across all package metadata (1.0.0.0 or higher)

### Submission Readiness

- [x] All required icon sizes are generated and ready for Partner Center upload
- [x] Capability declarations are complete, accurate, and minimal (no over-declaration)
- [x] AppxManifest.xml passes schema validation and Microsoft Store's automated checks
- [x] A checklist of all Partner Center fields exists and is ready to be filled in
- [x] Documentation clearly describes the step-by-step submission process

### Verification & Quality

- [x] Local testing procedure validates installation, launch, and basic functionality
- [x] Build automation completes without manual intervention or prompts
- [x] Package output is clearly organized and ready for archive or distribution
- [x] No paid certificates or third-party services are required at any stage
- [x] Build process is repeatable and produces consistent outputs

---

## Key Entities

- **MSIX Package**: A signed, distributable application package in Microsoft's packaging format.
- **AppxManifest.xml**: XML configuration file declaring application metadata, capabilities, extensions, and visual assets.
- **StorePackaging Directory**: Project folder containing all store-related files: manifest, icons, build scripts, documentation.
- **Partner Center**: Microsoft's cloud platform for app submission, distribution, and monitoring.
- **.msixbundle**: A bundle of MSIX packages supporting multiple architectures (in this case, just x64).
- **Capability Declarations**: Manifest entries requesting system access (audio input/output, registry, etc.).

---

## Assumptions

- **Windows development environment assumed**: Builds will run on Windows with Visual Studio, MSIX Packaging Tool, or equivalent tooling available.
- **x64-only packaging**: Per project scope, only x64 architecture is packaged; 32-bit and ARM64 are out of scope.
- **No pre-existing paid certificate**: The organization does not have and does not plan to purchase a code-signing certificate; automatic signing via MSIX tools is the standard path.
- **Standard Windows application behavior**: The application follows Windows conventions (settings storage, registry, file paths) that are compatible with MSIX sandboxing. If compatibility issues arise, the manifest is adjusted; major application redesign is out of scope.
- **Build environment already has Visual Studio or MSIX Packaging Tool**: The automation assumes these tools are installed and available in the build environment.
- **Incremental feature releases**: The first submission will establish the baseline; future releases will update version numbers and re-run the same automation.
- **No cross-platform distribution**: The feature is scoped to Windows Store only; macOS or Linux distributions are out of scope.
- **Microsoft Store account exists**: An organization account on Microsoft Partner Center already exists and is ready for submission.

