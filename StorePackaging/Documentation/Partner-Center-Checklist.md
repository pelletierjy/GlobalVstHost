# Microsoft Partner Center Submission Checklist

**Application**: JyGlobalVST  
**Version**: 1.0.0.0  
**Date**: 2026-07-05

Use this checklist to prepare all materials for Microsoft Partner Center submission.

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
- [ ] `.msixbundle` file generated successfully
- [ ] Package size < 500MB (typical: 50-200MB)
- [ ] Architecture: x64 only
- [ ] Minimum OS: Windows 10 1909 (10.0.18363.0) or later

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

### Audio Capabilities
- [ ] `microphone` - Declared and justified (WASAPI loopback audio capture)
- [ ] `audioLibrary` - Declared if app accesses audio files

### File & Storage
- [ ] `documentsLibrary` - Declared for preset/configuration storage
- [ ] `registryRead` - Declared if app reads settings from registry
- [ ] `registryWrite` - Declared if app persists settings to registry

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

- [ ] **Short name**: JyGlobalVST (50 chars or less)
- [ ] **Long description**: Comprehensive feature overview (200+ chars)
  - Describe core functionality
  - Mention VST3 plugin support
  - Note system audio processing capability
- [ ] **Release notes**: Feature overview and version info
- [ ] **Keywords**: Relevant search terms (VST, audio, processor, effects, host)
- [ ] **Category**: Audio (or appropriate category)
- [ ] **Content rating**: Complete IARC form

---

## Submission Content

- [ ] **Version number** matches manifest
- [ ] **Release date**: Set to current date or future date
- [ ] **Pricing tier**: Free
- [ ] **Availability**: 
  - [ ] Select regions (all regions recommended)
  - [ ] Select languages (English initially recommended)
- [ ] **Age group**: Select appropriate rating (typically 12+ or 16+)
- [ ] **Content declaration**: Mark applicable content (if any)

---

## Pre-Submission Validation

- [ ] MSAP validation passed (zero errors)
- [ ] Manifest schema validation passed
- [ ] Local installation test passed (Windows 10/11)
- [ ] Application launch test passed
- [ ] Basic functionality verified
- [ ] Uninstall verification passed
- [ ] No errors in validation script output

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
8. **Monitor review status**: Typically 24-48 hours
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
- Run MSAP validation locally
- Fix any schema errors
- Validate XML structure
- Re-submit

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

