# Tasks: Microsoft Store Publication via MSIX Packaging

**Input**: Design documents from `specs/009-microsoft-store-publish/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, quickstart.md, contracts/appx-manifest-schema.md

**Organization**: Tasks organized by user story to enable independent implementation and testing.

---

## Format: `[ID] [P?] [Story] Description`

- **[ID]**: Task identifier (T001, T002, etc.)
- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: User story label (US1, US2, US3, US4)
- **Description**: Clear action with exact file paths

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create project structure and baseline files for MSIX packaging

- [ ] T001 Create `StorePackaging/` directory structure per plan.md at repository root
- [ ] T002 Create `StorePackaging/Scripts/` subdirectory for build automation scripts
- [ ] T003 Create `StorePackaging/Documentation/` subdirectory for internal guides
- [ ] T004 [P] Create empty `StorePackaging/Assets/` subdirectory for icon files
- [ ] T005 Create `.gitignore` entries for `StorePackaging/Assets/*.png` and `build/store-packages/` output directory
- [ ] T006 Copy `specs/009-microsoft-store-publish/plan.md` reference to project README.md (feature tracking section)

**Checkpoint**: Directory structure ready; ready for foundational tasks

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Create core files and templates that ALL user stories depend on

**⚠️ CRITICAL**: No user story work can proceed until this phase is complete

- [ ] T007 Create AppxManifest.xml template at `StorePackaging/AppxManifest.xml` with required structure from contracts/appx-manifest-schema.md
  - Include Identity element with placeholders for Name, Publisher, Version
  - Include Properties element with DisplayName, PublisherDisplayName, Logo path
  - Include Dependencies element with TargetDeviceFamily (Min: 10.0.18363.0, Max: 10.0.26200.0)
  - Include Capabilities element with: microphone, audioLibrary, documentsLibrary, registryRead, registryWrite
  - Include Applications element with StartPage="JyGlobalVST.exe" and visual assets

- [ ] T008 Create JyGlobalVST.msixproj file at `StorePackaging/JyGlobalVST.msixproj` (Visual Studio project file)
  - Reference AppxManifest.xml
  - Configure x64 architecture only
  - Set output directory to `build/store-packages/`
  - Configure auto-signing settings (test certificate for dev)

- [ ] T009 [P] Create placeholder PNG icon files in `StorePackaging/Assets/`:
  - `Square44x44Logo.png` (44×44, placeholder with app name)
  - `Square50x50Logo.png` (50×50, placeholder with app name)
  - `Square150x150Logo.png` (150×150, placeholder with app name)
  - `Square310x310Logo.png` (310×310, placeholder with app name)
  - `StoreLogo.png` (50×50, placeholder with app name)
  - Note: Placeholders are sufficient for initial build; final assets to be created in US3

- [ ] T010 Create `StorePackaging/Scripts/Build-MSIX.ps1` PowerShell script that:
  - Accepts parameters: -Version (required), -Configuration (Release/Debug), -Architecture (x64)
  - Validates that CMake and Visual Studio are available
  - Calls `cmake -B build -A x64` to configure build directory
  - Calls `cmake --build build --config Release` to compile application
  - Validates that `build/bin/JyGlobalVST.exe` exists
  - Calls MSIX Packaging Tool (or Visual Studio) to create package
  - Outputs `.msixbundle` to `build/store-packages/JyGlobalVST_{Version}_x64.msixbundle`
  - Runs MSAP validation (if available) and reports results
  - Includes error handling and progress logging

- [ ] T011 Create `StorePackaging/Scripts/Validate-Package.ps1` PowerShell script that:
  - Accepts path to .msixbundle file as parameter
  - Validates manifest schema using XSD validator
  - Checks that all icon files referenced in manifest exist
  - Verifies AppxManifest.xml is well-formed XML
  - Reports any validation errors
  - Usable in CI/CD pipelines

- [ ] T012 Create `build/store-packages/` output directory and add to .gitignore (MSIX packages should not be committed)

- [ ] T013 Create version configuration file at `StorePackaging/version-info.txt` with initial version 1.0.0.0

**Checkpoint**: Build automation scaffolding complete; ready for user story implementation

---

## Phase 3: User Story 1 - Package the application for Microsoft Store distribution (Priority: P1) 🎯 MVP

**Goal**: Create a valid MSIX package that can be installed and tested locally

**Independent Test**: Execute `Build-MSIX.ps1 -Version 1.0.0.0 -Configuration Release`; verify .msixbundle is generated and passes `Validate-Package.ps1` validation

### Implementation for User Story 1

- [ ] T014 [US1] Update `StorePackaging/AppxManifest.xml`:
  - Replace placeholders with actual values from `CLAUDE.md` (app name, publisher)
  - Verify version matches `StorePackaging/version-info.txt`
  - Verify all icon paths point to existing files in Assets/
  - Verify StartPage path is correct relative to package root

- [ ] T015 [US1] Implement icon asset generation or sourcing:
  - Either: Create high-quality icon source (512×512 or larger)
  - Or: Use automated tool to generate all sizes from single source
  - Update all files in `StorePackaging/Assets/` with final icons
  - Verify all icons are PNG format with transparency (no JPG or solid colors)
  - Verify dimensions match manifest icon element requirements exactly

- [ ] T016 [P] [US1] Update `Build-MSIX.ps1` to:
  - Validate AppxManifest.xml exists and is well-formed
  - Inject version number from `version-info.txt` into manifest
  - Inject architecture (x64) into manifest
  - Copy AppxManifest.xml to output directory for reference

- [ ] T017 [US1] Create `StorePackaging/Scripts/Test-LocalInstall.ps1` script that:
  - Takes path to .msixbundle as parameter
  - Installs package locally using `Add-AppxPackage`
  - Verifies application appears in `Get-AppxPackage` list
  - Attempts to launch application
  - Checks system tray icon appears (or expected process starts)
  - Reports success/failure

- [ ] T018 [US1] Create initial GitHub Actions or Azure Pipelines workflow at `.github/workflows/store-package.yml` that:
  - Triggers on release tag (e.g., `v1.0.0`)
  - Checks out source code
  - Runs `Build-MSIX.ps1` with tag version
  - Runs `Validate-Package.ps1` on output
  - Runs `Test-LocalInstall.ps1` on output (optional, requires Windows runner)
  - Archives .msixbundle as artifact

- [ ] T019 [US1] Execute build locally:
  - Run `.\StorePackaging\Scripts\Build-MSIX.ps1 -Version 1.0.0.0 -Configuration Release`
  - Verify no errors; .msixbundle exists in `build/store-packages/`
  - Run `.\StorePackaging\Scripts\Validate-Package.ps1 -PackagePath build/store-packages/JyGlobalVST_1.0.0.0_x64.msixbundle`
  - Verify validation passes

**Checkpoint**: User Story 1 complete - valid MSIX package is generated, validated, and can be installed locally

---

## Phase 4: User Story 2 - Automate MSIX build as part of the release pipeline (Priority: P1)

**Goal**: Integrate MSIX build into CI/CD so releases are automatic and reliable

**Independent Test**: Trigger CI workflow via tag; verify .msixbundle appears in artifacts; verify it passes validation automatically

### Implementation for User Story 2

- [ ] T020 [US2] Enhance CI/CD workflow at `.github/workflows/store-package.yml`:
  - Add step to download and cache MSIX Packaging Tool (if not pre-installed)
  - Add step to download and cache MSAP validation tool
  - Run `Build-MSIX.ps1` and capture output
  - Run `Validate-Package.ps1` and parse results
  - Fail workflow if validation errors occur
  - Store .msixbundle and validation report as artifacts
  - Add metadata: version, architecture, build time, validation status

- [ ] T021 [US2] Create `StorePackaging/Scripts/Prepare-Release.ps1` script that:
  - Extracts version from git tag (e.g., `v1.0.0` → `1.0.0.0`)
  - Updates `StorePackaging/version-info.txt` with new version
  - Validates that version is higher than any previous release (read from git history)
  - Calls `Build-MSIX.ps1` with new version
  - Returns exit code 0 on success, non-zero on failure

- [ ] T022 [US2] Integrate `Prepare-Release.ps1` into release process:
  - Add step to CI/CD that calls this script
  - Verify script runs on Windows runner
  - Verify script output is captured and logged
  - Add post-step to commit version bump (if approved)

- [ ] T023 [US2] Create local build script at `build-and-test.ps1` (repository root) that:
  - Wraps the full build process for developer convenience
  - Calls `Build-MSIX.ps1` with version from command line
  - Calls `Validate-Package.ps1` automatically
  - Provides clear output: success (✓) or failure (✗)
  - Usable by developers: `.\build-and-test.ps1 -Version 1.0.0.1`

- [ ] T024 [US2] Add build instructions to `CLAUDE.md`:
  - Document command to build MSIX: `.\build-and-test.ps1 -Version <version>`
  - Document output location: `build/store-packages/`
  - Document validation command: `.\StorePackaging\Scripts\Validate-Package.ps1 -PackagePath <path>`

- [ ] T025 [US2] Test CI/CD workflow:
  - Create a test release tag locally (e.g., `v1.0.1`)
  - Push tag to repository
  - Monitor CI workflow execution
  - Verify .msixbundle is generated and available as artifact
  - Verify validation passes in CI logs

**Checkpoint**: User Story 2 complete - MSIX builds are automated and reliable in CI/CD; no manual packaging required

---

## Phase 5: User Story 3 - Prepare submission materials for Microsoft Partner Center (Priority: P2)

**Goal**: Generate all metadata, documentation, and checklists needed for Partner Center submission

**Independent Test**: Collect all artifacts from build output; verify each matches Partner Center requirements from research.md; verify no missing or malformed files

### Implementation for User Story 3

- [ ] T026 [US3] Create `StorePackaging/Documentation/Partner-Center-Checklist.md`:
  - List all required fields for Partner Center submission
  - Fields: App name, description, category, pricing, regions, keywords, system requirements
  - Provide template values for each field based on JyGlobalVST project
  - Include version numbering scheme (1.0.0.0 format)
  - Include capability declarations checklist
  - Include icon asset requirements checklist

- [ ] T027 [US3] Create `StorePackaging/Documentation/Capability-Declaration.md`:
  - Document each capability declared in AppxManifest.xml
  - List: microphone, audioLibrary, documentsLibrary, registryRead, registryWrite
  - Explain why each capability is needed
  - Reference code paths that use each capability
  - Include justification for Partner Center submission

- [ ] T028 [US3] Create asset inventory document at `StorePackaging/Assets/README.md`:
  - List all icon files and their purposes
  - Document dimensions, format, transparency requirements
  - Provide installation checklist for Partner Center
  - Map each file to manifest element (e.g., `Square44x44Logo.png` → Applications/VisualElements/@Square44x44Logo)

- [ ] T029 [US3] Create version management guide at `StorePackaging/Documentation/Version-Management.md`:
  - Document version numbering scheme (Major.Minor.Build.Revision)
  - Explain how to increment versions for releases
  - Provide instructions to update `version-info.txt` before releases
  - Include validation rules (version must increase, no gaps)

- [ ] T030 [US3] Create `StorePackaging/Documentation/AppxManifest-Reference.md`:
  - Copy relevant sections from `contracts/appx-manifest-schema.md`
  - Include complete manifest example with all required elements
  - Document each element and attribute
  - Include validation rules
  - Provide troubleshooting for common manifest errors

- [ ] T031 [P] [US3] Create submission materials generation script at `StorePackaging/Scripts/Prepare-Submission.ps1`:
  - Takes version as input
  - Collects: .msixbundle, AppxManifest.xml, all icon files
  - Generates manifest copy with final version number
  - Creates submission package directory: `build/store-submission/v{version}/`
  - Copies all required files to submission directory
  - Generates file listing and checksum report
  - Outputs ready-for-upload package

- [ ] T032 [US3] Create README for Partner Center at `StorePackaging/Documentation/SUBMISSION-README.md`:
  - One-page overview of submission process
  - Prerequisites: Partner Center account, app name reserved
  - Step-by-step: How to upload, what fields to fill
  - Expected timeline: 24-48 hours for certification
  - Common rejection reasons and how to avoid them
  - Support resources: links to Microsoft documentation

**Checkpoint**: User Story 3 complete - all Partner Center submission materials are generated and organized; ready for upload

---

## Phase 6: User Story 4 - Test MSIX package locally before Partner Center submission (Priority: P2)

**Goal**: Provide documented procedures to validate MSIX packages on Windows systems before Partner Center submission

**Independent Test**: Follow all test scenarios in quickstart.md on a clean Windows 10/11 system; verify all tests pass

### Implementation for User Story 4

- [ ] T033 [US4] Implement `Test-LocalInstall.ps1` scenario tests:
  - Scenario 1: Manifest validation (Parse XML, verify schema)
  - Scenario 2: Installation on clean system
  - Scenario 3: Application launch and basic functionality
  - Scenario 4: Settings persistence (save and restore)
  - Scenario 5: Uninstallation and cleanup
  - Each scenario has clear pass/fail criteria

- [ ] T034 [US4] Create `StorePackaging/Scripts/Validate-Manifest.ps1`:
  - Extracts AppxManifest.xml from .msixbundle (rename to .zip, extract)
  - Parses XML and validates schema
  - Checks all required elements are present
  - Checks all referenced files exist in package
  - Reports any schema violations
  - Usable in automated testing pipelines

- [ ] T035 [US4] Create manual testing guide at `StorePackaging/Documentation/Local-Testing-Guide.md`:
  - Reference: specs/009-microsoft-store-publish/quickstart.md (Part A)
  - Provide step-by-step instructions for each test scenario
  - Include expected outcomes for each test
  - Include troubleshooting guide for common failures
  - Provide pass/fail checklist

- [ ] T036 [US4] Create `StorePackaging/Scripts/Test-PackageFull.ps1`:
  - Comprehensive test script that runs all scenarios in sequence
  - Takes .msixbundle path as input
  - Runs: manifest validation, install, launch, functionality check, uninstall
  - Generates test report with pass/fail for each scenario
  - Returns exit code 0 if all tests pass, 1 if any fail
  - Usable in CI/CD pipelines or manual testing

- [ ] T037 [US4] Create MSAP validation wrapper at `StorePackaging/Scripts/Run-MSAP.ps1`:
  - Checks if MSAP (Microsoft Store App Certification Kit) is installed
  - If installed: Runs MSAP validation and parses results
  - If not installed: Provides download link and instructions
  - Generates report with all validation results
  - Highlights any errors (must fix) vs. warnings (should review)

- [ ] T038 [US4] Create `StorePackaging/Documentation/Certification-Reference.md`:
  - Based on research.md findings
  - List common certification failure reasons
  - For each: What causes it, how to prevent it, how to fix it
  - Examples: Over-declared capabilities, missing assets, version mismatches
  - Provides validation commands to catch issues before Partner Center

- [ ] T039 [US4] Test all validation scripts:
  - Build a test package using T019
  - Run each validation script on the test package
  - Verify each script produces correct results
  - Verify error handling works (non-zero exit codes on failure)
  - Document any dependencies (MSAP installation, etc.)

**Checkpoint**: User Story 4 complete - comprehensive local testing procedures are documented and automated; developers can validate packages before submission

---

## Phase 7: Documentation & Polish

**Purpose**: Final documentation, guides, and cross-cutting improvements

- [ ] T040 [P] Create internal guide at `StorePackaging/Documentation/MSIX-Build-Guide.md`:
  - Quick-start: One-command build procedure
  - Detailed: Step-by-step explanation of build process
  - Troubleshooting: Common build errors and solutions
  - References: Links to design documents, scripts, and resources

- [ ] T041 [P] Create `StorePackaging/Documentation/IMPLEMENTATION-NOTES.md`:
  - Assumptions made during implementation
  - Known limitations (e.g., x64 only, Windows 10+ only)
  - Future improvements (VST2 support, ARM64 support, other regions/languages)
  - Maintenance notes (how to update for new releases)

- [ ] T042 [P] Update project README.md:
  - Add section: "Microsoft Store Availability"
  - Include download link (placeholder until first submission)
  - Link to MSIX-Build-Guide.md for developers

- [ ] T043 [P] Create `CHANGELOG.md` entry:
  - Document new feature: Microsoft Store packaging
  - Include build process, submission process
  - List supported platforms: Windows 10 1909+, Windows 11
  - Credit: MSIX Packaging Tool, Visual Studio

- [ ] T044 Add comprehensive index to `StorePackaging/Documentation/README.md`:
  - Overview of all documentation files
  - Quick-start links for: building, testing, submitting
  - Developer guide, QA guide, product owner guide

- [ ] T045 [P] Clean up placeholder assets:
  - Review all icon files created in T009
  - Verify final icons (from T015) are in place
  - Remove any temporary test files
  - Add .gitignore rules for generated artifacts

- [ ] T046 Run full integration test:
  - Execute end-to-end process: build → validate → test → submission prep
  - Verify all scripts work together without errors
  - Verify output is organized as expected
  - Document results in test log

- [ ] T047 Commit all Store Packaging files to git:
  - Add all files in `StorePackaging/` directory
  - Add `.github/workflows/store-package.yml` for CI/CD
  - Add updates to `CLAUDE.md`, README.md, CHANGELOG.md
  - Commit message: `store(packaging): add Microsoft Store MSIX packaging infrastructure`

- [ ] T048 Create GitHub issue template for Store releases:
  - Template for release checklist
  - Links to build guide, testing guide, submission guide
  - Checklist items: version bump, build, validate, test, submit

**Checkpoint**: Documentation complete; codebase ready for Microsoft Store submission

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: No dependencies - can start immediately
- **Phase 2 (Foundational)**: Depends on Phase 1 - **MUST COMPLETE before user stories**
- **Phase 3 (US1)**: Depends on Phase 2
- **Phase 4 (US2)**: Depends on Phase 2 (can run parallel to US1 after Phase 2)
- **Phase 5 (US3)**: Depends on Phase 2 (can run parallel to US1/US2 after Phase 2)
- **Phase 6 (US4)**: Depends on Phase 2 (can run parallel to other stories after Phase 2)
- **Phase 7 (Polish)**: Depends on all user stories being complete

### User Story Dependencies

| Story | Can Start After | Blocking | Notes |
|-------|-----------------|----------|-------|
| US1 (P1) | Phase 2 | None | MVP - provides basic MSIX functionality |
| US2 (P1) | Phase 2 | None | Can run parallel to US1; enhances US1 with CI/CD |
| US3 (P2) | Phase 2 | None | Can run parallel; generates submission materials |
| US4 (P2) | Phase 2 | None | Can run parallel; validates packages from US1 |

**All user stories can proceed in parallel after Phase 2 is complete.**

### Parallel Opportunities

- **Phase 1**: All [P] marked tasks can run in parallel
- **Phase 2**: T009 (icon placeholders) can run parallel to T007, T008, T010, T011
- **US1 → US2 → US3 → US4**: After Phase 2, all stories can be worked on by different team members simultaneously
- **Within Phase 3**: T016 (icon generation) can run parallel to T014 (manifest updates)
- **Within Phase 5**: T027, T028, T029, T030 (documentation) can run in parallel

---

## Parallel Example: Full Pipeline (All 4 User Stories Concurrent)

```
After Phase 2 completion:

Thread 1: US1                    Thread 2: US2                 Thread 3: US3                  Thread 4: US4
T014 (Manifest)                 T020 (CI/CD workflow)        T026 (Checklist)               T033 (Test scenarios)
  ↓                               ↓                            ↓                              ↓
T015 (Icons)                    T021 (Release script)        T027 (Capabilities)           T034 (Manifest validation)
  ↓                               ↓                            ↓                              ↓
T016 (Update Build)            T022 (Integration)           T028 (Asset inventory)        T035 (Testing guide)
  ↓                               ↓                            ↓                              ↓
T017 (Test script)             T023 (Local script)          T029 (Version guide)          T036 (Full test script)
  ↓                               ↓                            ↓                              ↓
T018 (GitHub workflow)         T024 (Build docs)           T030 (Manifest reference)     T037 (MSAP wrapper)
  ↓                               ↓                            ↓                              ↓
T019 (Build locally)           T025 (Test workflow)        T031 (Submission script)      T038 (Cert reference)
                                                                ↓                             ↓
                                                            T032 (Submission README)      T039 (Test all scripts)

All 4 threads complete → Merge to Phase 7 for polish
```

---

## Implementation Strategy

### MVP Scope: Complete User Story 1 Only

1. ✅ Complete Phase 1: Setup (estimated: 2-4 hours)
2. ✅ Complete Phase 2: Foundational (estimated: 8-12 hours)
3. ✅ Complete Phase 3: User Story 1 (estimated: 8-12 hours)
4. 🎯 **STOP HERE FOR MVP**: Test US1 independently
5. 🎯 **SHIP MVP**: Valid MSIX package can be built and validated locally

**MVP Timeline**: 18-28 hours elapsed
**MVP Deliverable**: Build script, validated .msixbundle, local testing capability

### Incremental Delivery (All 4 Stories)

1. Phase 1 + Phase 2: Foundation (CRITICAL - must complete first)
2. + Phase 3 (US1): Basic packaging → Demo/Deploy MVP
3. + Phase 4 (US2): CI/CD automation → Easier releases
4. + Phase 5 (US3): Submission materials → Ready for Store
5. + Phase 6 (US4): Testing procedures → Quality confidence
6. + Phase 7: Documentation & polish → Production-ready

**Full Timeline**: 40-60 hours elapsed (depending on parallel execution and team size)

### Parallel Team Strategy (3-4 Developers)

1. Developer A + B: Complete Phase 1 + Phase 2 together
2. After Phase 2 done:
   - Developer A: US1 + US2 (packaging + CI/CD)
   - Developer B: US3 + US4 (submission + testing)
   - Developer C (if available): Documentation and Polish
3. All integrate and merge → Phase 7 polish
4. Final validation → Ready for submission

---

## Quality Gates & Checkpoints

| Checkpoint | Verify | Success Criteria |
|-----------|--------|------------------|
| Phase 1 end | Directory structure | All directories created; files organized per plan.md |
| Phase 2 end | Build automation | Build-MSIX.ps1 runs without errors; produces .msixbundle |
| US1 end | Package creation | Valid .msixbundle generated; passes validation; installs locally |
| US2 end | CI/CD integration | GitHub Actions workflow triggers; builds package automatically |
| US3 end | Submission ready | All Partner Center materials generated; checklist complete |
| US4 end | Testing procedures | Local testing works; all test scenarios pass |
| Phase 7 end | Documentation complete | Guides written; developers can reproduce process |

---

## Task Completion Notes

**Total Task Count**: 48 tasks

**Breakdown by Phase**:
- Phase 1 (Setup): 6 tasks
- Phase 2 (Foundational): 7 tasks
- Phase 3 (US1): 6 tasks
- Phase 4 (US2): 6 tasks
- Phase 5 (US3): 7 tasks
- Phase 6 (US4): 7 tasks
- Phase 7 (Polish): 9 tasks

**Breakdown by Priority**:
- US1 (P1): 6 tasks
- US2 (P1): 6 tasks
- US3 (P2): 7 tasks
- US4 (P2): 7 tasks
- Shared/Polish: 22 tasks

**Parallelizable Tasks**: 15 marked [P]

**MVP (User Story 1 Only)**: 13 tasks (Phase 1 + Phase 2 + Phase 3)

---

## Notes

- Each task has exact file paths for implementation
- Tasks follow strict checkbox + ID format for tracking
- User story labels (US1, US2, US3, US4) enable independent testing
- [P] marking identifies parallel opportunities
- Each checkpoint validates one complete user story
- Polish phase can proceed after any user story is complete
- See plan.md for technical architecture details
- See spec.md for user stories and acceptance criteria
- See quickstart.md for validation procedures and Partner Center submission guide

