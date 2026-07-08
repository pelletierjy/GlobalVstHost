# Implementation Plan: Microsoft Store Publication via MSIX Packaging

**Branch**: `009-microsoft-store-publish` | **Date**: 2026-07-05 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/009-microsoft-store-publish/spec.md`

## Summary

Enable the JyGlobalVST application for distribution through the Microsoft Store by implementing MSIX packaging with automatic code signing (no paid certificates required) and providing complete Partner Center submission materials and documentation. This feature involves creating a `StorePackaging/` directory structure with MSIX project configuration, AppxManifest.xml, asset management, build automation, and comprehensive documentation for local testing and Partner Center submission procedures.

## Technical Context

**Language/Version**: C++20 (primary), PowerShell 7+ (build automation)

**Primary Dependencies**: 
- MSIX Packaging Tool (free, Microsoft-provided)
- Visual Studio 2022 Community Edition or Build Tools (free)
- CMake 3.22+ (already in use)
- Windows SDK (included with Visual Studio)

**Storage**: File-based (assets, manifests stored in StorePackaging/ directory)

**Testing**: 
- Manual installation testing (Windows 10 1909+, Windows 11)
- Microsoft's package validation tools (MSAP, Store validation API)
- PowerShell-based validation scripts

**Target Platform**: Windows only (x64 architecture; 32-bit and ARM64 explicitly out of scope)

**Project Type**: Desktop application (GUI + system tray + VST plugin host)

**Performance Goals**: 
- Build time: < 10 minutes for complete MSIX generation on standard developer machine
- Package size: Optimized with no unnecessary bloat (typical desktop app range: 50-500MB)
- Installation time: < 2 minutes on modern hardware
- Launch time: Identical to traditional EXE installation (< 5 seconds)

**Constraints**: 
- No external paid certificates allowed (automatic signing only)
- Must support Windows 10 1909+ and Windows 11
- Installation must not require administrative elevation
- Settings persistence must work within MSIX sandbox constraints
- Package must pass Microsoft's automated Store certification checks

**Scale/Scope**: 
- Single application (JyGlobalVST tray application)
- One target architecture (x64 only)
- Store submission for one region/language initially (English, en-US)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### Applicability to JyGlobalVST Constitution

The Microsoft Store publishing feature is **NOT audio-critical** and does **NOT affect core audio components** defined in the JyGlobalVST constitution. Therefore, the following audio-specific principles do not apply:

❌ **Excluded**: Principle II (Performance Requirements - latency/CPU/stability for audio)
❌ **Excluded**: Principle III (Audio Task Sequencing) 
❌ **Excluded**: Principle V (Real-Time Code Documentation)
❌ **Excluded**: Audio Performance Standards (latency validation, buffer management, plugin compatibility)

### Principles That Apply

✓ **Principle I (Spec Hierarchy & Component Isolation)**: This feature is a **deployment-layer feature**, isolated from audio components. It does not interact with WASAPI, VST hosting, or DSP routing. No audio component dependencies.

✓ **Principle IV (Naming & Conventions)**: Branch naming follows kebab-case convention (`009-microsoft-store-publish`). Commits will include "store:" prefix (e.g., `store(packaging): add AppxManifest.xml`).

✓ **Principle VI (Dependencies & Build Order)**: This feature is **parallel to audio development** — it does not block audio work and audio work does not block it. Build order: core audio components must stabilize before Store submission, but packaging infrastructure can be built concurrently.

✓ **Development Workflow (Code Review Gate)**: Store-related PRs require verification that package validation passes and submission materials are complete. No latency review needed; instead: manifest validation, icon/asset verification, documentation completeness.

✓ **Governance**: This plan is subordinate to the constitution. Any future Store-related changes (e.g., new capabilities, minimum OS bump) must re-check this gate.

### Conclusion

**GATE PASSED**: Feature does not violate constitution. Component isolation is maintained (deployment layer is separate from audio). Naming and governance principles apply.

## Project Structure

### Documentation (this feature)

```text
specs/009-microsoft-store-publish/
├── plan.md                      # This file
├── spec.md                       # Feature specification
├── research.md                   # Phase 0: dependency and requirements research
├── data-model.md                 # Phase 1: MSIX manifest schema, asset structure
├── quickstart.md                 # Phase 1: validation checklist and submission guide
├── contracts/                    # Phase 1: AppxManifest schema, icon spec
│   └── appx-manifest-schema.md   # Required fields, capability declarations
├── checklists/
│   └── requirements.md           # Specification quality checklist
└── tasks.md                      # Phase 2: implementation tasks (generated by /speckit-tasks)
```

### Source Code (repository root)

```text
StorePackaging/                              # New directory
├── AppxManifest.xml                        # Application manifest (auto-generated or template)
├── JyGlobalVST.msixproj                    # MSIX project file for Visual Studio
├── Assets/                                 # Logos, icons, and splash images
│   ├── Square44x44Logo.png                 # 44×44 tile icon
│   ├── Square50x50Logo.png                 # 50×50 tile icon
│   ├── Square150x150Logo.png               # 150×150 medium tile
│   ├── Square310x310Logo.png               # 310×310 large tile
│   ├── Wide310x150Logo.png                 # 310×150 wide tile (optional)
│   ├── StoreLogo.png                       # 50×50 store logo
│   └── SplashScreen.png                    # 620×300 splash (optional)
├── Scripts/                                # Build and validation automation
│   └── Build-MSIX.ps1                      # PowerShell script to build package
└── Documentation/                          # Internal guides
    ├── MSIX-Build-Guide.md                 # How to run the build script
    ├── Local-Testing-Procedure.md          # How to test .msix locally
    └── Partner-Center-Submission.md        # Step-by-step Partner Center guide
```

**Structure Decision**: Single integrated structure. The StorePackaging/ directory is a sibling to src/, tests/, and CMakeLists.txt, containing all Store-specific assets and automation. No separate backend/frontend needed (the application is monolithic). Assets are co-located with the manifest for clarity. PowerShell scripts are kept in Scripts/ subdirectory for easy execution from the build pipeline.

## Complexity Tracking

> **Note**: No constitution violations. No complexity justification needed.

This feature is orthogonal to the core audio system. Build automation and packaging are standard desktop application deployment patterns.

---

## Phase 0: Research & Clarifications

*All NEEDS CLARIFICATION items from spec.md have been resolved. Phase 0 research focuses on* **best practices** *and* **dependency investigation**:

### Research Tasks

1. **MSIX Packaging Best Practices for Desktop Applications**
   - Investigate: icon sizing requirements, manifest schema current version, capability declaration patterns
   - Deliverable: best-practices summary in research.md

2. **Automatic Code Signing in MSIX**
   - Investigate: MSIX Packaging Tool signing behavior, Visual Studio integration, certificate chain validation
   - Deliverable: signing workflow options and recommendation in research.md

3. **Windows Store Certification Requirements & Common Rejections**
   - Investigate: official Microsoft Store policies, common certification failures, capability declaration pitfalls
   - Deliverable: certification requirements checklist in research.md

4. **Partner Center Configuration & Submission Workflow**
   - Investigate: account setup, submission process, testing requirements, review timeline
   - Deliverable: submission workflow outline in research.md

5. **Local MSIX Testing Tools & Procedures**
   - Investigate: Add-AppxPackage PowerShell cmdlet, package validation tools, uninstallation cleanup
   - Deliverable: local testing procedure outline in research.md

---

## Phase 1: Design & Contracts

### 1. Data Model (data-model.md)

Extract entities from feature spec and document:

- **MSIX Manifest Schema**
  - Required elements: Package, Identity, Properties, Applications, Capabilities, Extensions
  - Version number field and format (e.g., 1.0.0.0)
  - Capability declarations (microphone, audio, registry access, etc.)
  - Minimum Windows version specification

- **Asset Inventory**
  - Icon sizes and purposes (tile, store logo, splash)
  - Filename conventions and relationships to manifest
  - Format requirements (PNG with transparency)

- **Build Artifact Structure**
  - .msix package contents and validation
  - .msixbundle structure (single architecture in v1)
  - Output directory layout and file naming

### 2. Interface Contracts (contracts/)

Create **AppxManifest.xml schema documentation** at `contracts/appx-manifest-schema.md`:

- Required and optional XML elements
- Capability declaration format and options
- Identity element structure (Publisher, Name, Version)
- Applications element (entry point specification)
- Example manifest snippet with common patterns
- Validation rules (e.g., version must match package version)

### 3. Quickstart Guide (quickstart.md)

Document a runnable validation checklist:

**Prerequisites**:
- Windows 10 1909+ or Windows 11 (x64)
- Latest .msixbundle file (from build output)
- Administrator PowerShell access (for package installation only, not daily use)

**Setup Steps**:
1. Obtain the .msixbundle file from build output
2. (Optional) Run manifest validation using Microsoft tools

**Validation Scenarios**:
1. **Install Package**: PowerShell script to install .msixbundle, verify success
2. **Launch Application**: Verify application starts and system tray icon appears
3. **Test Plugin Chain**: Add plugins, save/load presets, verify persistence
4. **Uninstall Package**: PowerShell script to cleanly remove package
5. **Manifest Validation**: Run MSAP or Store submission API to validate manifest

**Expected Outcomes**: All scenarios pass, no errors logged, application behaves identically to traditional installer.

### 4. Agent Context Update

Update the plan reference in `CLAUDE.md` (if needed) or create a new section pointing to this plan:

```markdown
**Active Feature: Microsoft Store Publication (009)**
- Current plan: [specs/009-microsoft-store-publish/plan.md](specs/009-microsoft-store-publish/plan.md)
- Spec: [specs/009-microsoft-store-publish/spec.md](specs/009-microsoft-store-publish/spec.md)
```

---

## Implementation Approach

### Phase Breakdown

| Phase | Deliverable | Duration | Owner |
|-------|-------------|----------|-------|
| **Phase 0** | research.md, all clarifications resolved | 1-2 days | Research Agent |
| **Phase 1** | data-model.md, contracts/, quickstart.md | 3-5 days | Design Review |
| **Phase 2** | tasks.md (via /speckit-tasks) | 1 day | Task Generator |
| **Phase 3** | Implementation (via /speckit-implement) | 10-15 days | Dev Team |
| **Phase 4** | Testing & Validation | 3-5 days | QA + Dev |
| **Phase 5** | Partner Center Submission | 1-2 days | Product Owner |

### Key Milestones

1. ✅ Specification complete (2026-07-05)
2. ⏳ Research & design artifacts (Phase 0-1)
3. ⏳ Implementation tasks defined (Phase 2)
4. ⏳ StorePackaging directory scaffolded
5. ⏳ Build automation (Build-MSIX.ps1) created
6. ⏳ AppxManifest.xml generated and validated
7. ⏳ Local testing procedure validated
8. ⏳ Partner Center submission ready

---

## External References

- **Microsoft MSIX Documentation**: https://learn.microsoft.com/en-us/windows/msix/
- **AppxManifest.xml Schema**: https://learn.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/
- **Microsoft Store Certification Requirements**: https://learn.microsoft.com/en-us/windows/uwp/publish/
- **MSIX Packaging Tool**: https://learn.microsoft.com/en-us/windows/msix/packaging-tool/create-an-msix-overview
- **Partner Center**: https://partner.microsoft.com/

---

## Next Steps

1. Execute Phase 0: Research & Clarifications (research.md generation)
2. Execute Phase 1: Design & Contracts (generates research.md, data-model.md, contracts/, quickstart.md)
3. Run `/speckit-tasks` to generate detailed implementation tasks (tasks.md)
4. Review tasks with team and begin implementation via `/speckit-implement`
