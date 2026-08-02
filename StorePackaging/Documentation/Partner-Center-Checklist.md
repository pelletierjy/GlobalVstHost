# Microsoft Partner Center Submission Checklist

**Application**: JyGlobalVST  
**Version**: 1.0.0.0  
**Checklist reviewed against code**: 2026-07-24

Use this checklist to prepare all materials for Microsoft Partner Center submission.

> **Current status as of 2026-07-24: one hard blocker left.**
>
> **Blocking:** the **IARC age rating** has not been completed. It is mandatory under policy
> 11.11 and can only be done inside Partner Center during submission.
>
> **Done:** Partner Center identity (`JYPSolutions.GlobalVSTHost`), package builds and
> validates clean in CI with that identity, privacy policy live at
> https://pelletierjy.github.io/GlobalVstHost/privacy-policy.html, support email, category,
> description, search terms, certification notes, and three 1920x1080 screenshots.
>
> **Recommended before submitting, not blocking:** capture a fourth screenshot of the
> Auto Volume Leveller / Compressor editor (needs a rebuild — the old capture showed a since-fixed mojibake bug),
> walk the six-step test in Listing/certification-notes.md against the installed package, and
> confirm preset save/load still works now that `documentsLibrary` has been removed.
>
> **Researched, residual risk:** no Store policy prohibits a packaged full-trust app from
> loading user-supplied VST3 plugins, and MSIX does not block it technically (verified by
> loading three real plugins inside the package container). But no precedent for a VST host
> on the Store as MSIX was found, so expect a reviewer question. See
> [Certification-Reference.md](Certification-Reference.md).

---

## Account & Registration (One-Time Setup)

- [ ] Microsoft Account created
- [ ] Partner Center account set up (partner.microsoft.com)
- [ ] Publisher identity verified
- [ ] Payment method configured (if not free app)
- [ ] Developer certificate obtained (if required for your region)

---

## App Registration (One-Time per App)

- [ ] App name reserved in Partner Center
- [ ] App name matches manifest `<Identity Name="..."/>`
- [ ] Primary category selected
- [ ] Secondary category (optional)
- [ ] Age rating completed (IARC questionnaire)
- [ ] Privacy policy URL provided and live
- [ ] Support email configured
- [ ] Support URL configured (if applicable)

---

## Package & Manifest Validation

- [ ] Version number format: `Major.Minor.Build.Revision` (e.g., 1.0.0.0)
- [ ] Version number incremented from previous release
- [ ] AppxManifest.xml validates without errors
- [ ] `Identity` carries the real Partner Center values, not the placeholders
- [ ] `.msixbundle` generated in `build/store-packages/store/`
- [ ] Package is **unsigned** (Partner Center signs Store submissions)
- [ ] Architecture: x64 only
- [ ] Minimum OS: Windows 10 1909 (10.0.18363.0) or later

Current package size is ~3.5 MB, well inside any Store limit. Confirm the current maximum
against Partner Center if that ever changes materially.

---

## Icons & Assets

- [ ] **Square44x44Logo.png** - 44×44 pixels, PNG with transparency
- [ ] **Square50x50Logo.png** - 50×50 pixels, PNG with transparency
- [ ] **Square150x150Logo.png** - 150×150 pixels, PNG with transparency
- [ ] **Square310x310Logo.png** - 310×310 pixels, PNG with transparency
- [ ] **StoreLogo.png** - 50×50 pixels, PNG with transparency
- [ ] All icon files referenced in manifest exist
- [ ] All icons are PNG format (no JPG, BMP, etc.)
- [ ] All icons have transparency (RGBA, not RGB)
- [ ] Store screenshots prepared (if available)
- [ ] Store description images prepared (if available)

---

## Capabilities & Declarations

The manifest declares exactly two capabilities. See
[Capability-Declaration.md](Capability-Declaration.md) for the code justification of each.

- [ ] `rescap:runFullTrust` - mandatory for a packaged Win32 desktop app
- [ ] `DeviceCapability microphone` - WASAPI loopback capture. **Requirement unverified**:
      capture may work under `runFullTrust` alone. Be ready to explain to a reviewer that
      loopback records system output, not microphone input
- [x] `uap:documentsLibrary` - **removed 2026-07-24** as redundant under `runFullTrust`

Do not add `registryRead` or `registryWrite` — those are not real MSIX capability names.
`HKEY_CURRENT_USER` access under full trust needs no declaration.

### Additional
- [ ] No over-declared capabilities (minimizes rejection risk)
- [ ] Each declared capability has code justification
- [ ] Capability-to-feature mapping documented

---

## System Requirements & Compatibility

- [ ] **Minimum OS**: Windows 10 version 1909 (build 18363) or later
- [ ] **Maximum OS tested**: Windows 11 current version
- [ ] **Architecture**: x64 only (32-bit and ARM64 out of scope)
- [ ] **Processor requirement**: No special requirements
- [ ] **RAM requirement**: Reasonable for desktop audio application (≥ 4GB recommended)
- [ ] **Storage requirement**: ~250MB for installation + user presets

---

## Store Listing & Description

- [x] **Short name**: Global VST Host (matches the reserved name and manifest DisplayName)
- [ ] **Long description**: Comprehensive feature overview (200+ chars)
  - Describe core functionality
  - Mention VST3 plugin support
  - Note system audio processing capability
- [ ] **Release notes**: Feature overview and version info
- [ ] **Keywords**: Relevant search terms (VST, audio, processor, effects, host)
- [x] **Category**: primary `Music`, secondary `Multimedia design > Music production`.
      There is no "Audio" category — see Listing/store-listing.md
- [ ] **Content rating**: Complete IARC form

---

## Submission Content

- [ ] **Version number** matches manifest
- [ ] **Release date**: Set to current date or future date
- [ ] **Pricing tier**: Free
- [ ] **Availability**: 
  - [ ] Select regions (all regions recommended)
  - [ ] Select languages (English initially recommended)
- [ ] **Age group**: set by the IARC questionnaire; expect the lowest available rating
- [ ] **Content declaration**: Mark applicable content (if any)

---

## Pre-Submission Validation

- [ ] `Validate-Package.ps1` passes with zero errors on the `.msixbundle` you will upload
- [ ] Manifest schema validation passed (implicit — MakeAppx fails the pack otherwise)
- [ ] Local installation test passed (Windows 10/11) using a **self-signed local** build;
      the Store package is unsigned and will not sideload
- [ ] Application launch test passed
- [ ] Plugin chain exercised with real VST3 plugins
- [ ] Settings and presets persist across restart
- [ ] Uninstall verification passed

---

## Microsoft Store Certification Readiness

### Common Rejection Reasons to Avoid

- [ ] **Over-declared capabilities**: Only request what app actually uses
- [ ] **Missing minimum OS declaration**: Manifest specifies 10.0.18363.0 or higher
- [ ] **Icon size mismatches**: All icons are exact pixel dimensions
- [ ] **Missing required elements**: Identity, Properties, Dependencies, Applications present
- [ ] **Hardcoded paths**: App uses environment variables (AppData, ProgramFiles, etc.)
- [ ] **Undeclared registry access**: All registry ops declared in manifest
- [ ] **Undeclared file access**: All file operations declared in capabilities
- [ ] **Network activity without disclosure**: Document in privacy policy

---

## Upload & Publication Steps

1. **Sign into Partner Center** (partner.microsoft.com)
2. **Select your app** from dashboard
3. **Create new submission**
4. **Upload package**: Select `.msixbundle` file
5. **Enter submission details**:
   - Pricing & availability
   - Properties
   - Age ratings
   - Store listings
6. **Add supplementary content** (screenshots, descriptions)
7. **Submit for certification**
8. **Monitor review status** in Partner Center (no reliable published turnaround figure)
9. **Respond to feedback** if rejections occur
10. **Publish** when approved

---

## Post-Submission

- [ ] Monitor Partner Center daily during certification
- [ ] Respond promptly to any certification feedback
- [ ] Re-submit with fixes if rejected
- [ ] Update app version in Partner Center when submitting updates
- [ ] Keep release notes current in Partner Center
- [ ] Monitor app reviews and ratings

---

## Troubleshooting Checklist

**If rejected for "capability over-declaration"**:
- Review each declared capability
- Check if app code actually uses the capability
- Remove unused capabilities
- Re-submit

**If rejected for "missing minimum OS"**:
- Verify manifest specifies MinVersion 10.0.18363.0 or later
- Update if needed and re-submit

**If rejected for "icon issues"**:
- Verify all icon dimensions are exact pixels
- Ensure all icons are PNG format with transparency
- Check that all icons exist in manifest references

**If rejected for "manifest errors"**:
- Run `Validate-Package.ps1` locally
- Rebuild — MakeAppx reports the offending line and reason on any schema violation
- Re-submit with an incremented version

---

## Certification Timeline

- **Upload**: Immediate
- **Auto-validation**: 30 minutes - 2 hours
- **Human review**: 24-48 hours (typical)
- **Publication**: If approved, live within 1-24 hours
- **Rejection feedback**: Usually within above timeline

---

## Resources

- [Microsoft App Store Policies](https://docs.microsoft.com/en-us/windows/uwp/publish/store-policies)
- [MSIX Packaging Tool Documentation](https://docs.microsoft.com/en-us/windows/msix/packaging-tool/create-an-msix-overview)
- [AppxManifest.xml Schema Reference](https://docs.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/)
- [Partner Center Documentation](https://docs.microsoft.com/en-us/windows/apps/publish/)

---

## Sign-Off

- [ ] All checklist items reviewed
- [ ] All materials prepared and validated
- [ ] Ready for Partner Center submission
- [ ] Authorized submitter confirms readiness

**Prepared by**: ________________  
**Date**: ________________  
**Notes**: ________________________________________________________________

