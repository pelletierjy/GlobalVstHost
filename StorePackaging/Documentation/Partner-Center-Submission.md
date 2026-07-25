# Partner Center Submission Guide

**For**: JyGlobalVST MSIX Package  
**Audience**: Product Owners, Release Managers

---

> **This walkthrough has never been executed.** It describes the expected Partner Center
> flow, but the exact UI labels and step order change over time — treat it as orientation
> and follow Partner Center's own prompts. Nothing here is a verified transcript.
>
> Submission is currently blocked; see
> [Partner-Center-Checklist.md](Partner-Center-Checklist.md).

---

## Prerequisites

- [ ] Microsoft Account created
- [ ] Partner Center account active (partner.microsoft.com)
- [ ] Publisher identity verified
- [ ] App name reserved in Partner Center
- [ ] **Package rebuilt with the Partner Center identity.** Copy `Identity/@Name`,
      `Identity/@Publisher` and the publisher display name from
      Partner Center → your app → Product management → Product identity, then run
      `Build-MSIX.ps1 -PackageName ... -Publisher ... -PublisherDisplayName ...`.
      The placeholder identity in the repo is rejected at upload.
- [ ] `Validate-Package.ps1` passes on that `.msixbundle`
- [ ] Package is **unsigned** — Partner Center applies the Store signature
- [ ] All materials from Partner-Center-Checklist.md prepared

---

## Step-by-Step Submission

### 1. Sign Into Partner Center

1. Go to https://partner.microsoft.com
2. Sign in with your Microsoft Account
3. Select "Windows & Xbox"
4. Locate your app in the dashboard

### 2. Create New Submission

1. Click your app name
2. Click "Create new submission"
3. Review the submission overview

### 3. Upload Package

1. Go to "Packages" section
2. Click "Browse files"
3. Select your `.msixbundle` file from `build/store-packages/store/`
   (not `build/store-packages/local/` — that one is the self-signed test build)
4. Wait for upload to complete

### 4. Fill App Details

Complete all sections (see Partner-Center-Checklist.md):

**Properties**:
- Title: JyGlobalVST
- Subtitle: (optional)
- Short description (50 chars min)
- Long description (200+ chars)

**Store listings**:
- Category: Audio/Media or Utilities
- Age rating: Complete IARC questionnaire
- Keywords: VST, audio, processor, effects
- Description
- Screenshots (optional but recommended)

**Availability**:
- Pricing: Free
- Markets: Select regions
- Languages: Select languages

### 5. Review Requirements

Partner Center will perform automated checks:
- ✓ Manifest validation
- ✓ File size check
- ✓ Architecture verification
- ✓ Capability declaration review

Address any errors before proceeding.

### 6. Age Ratings (IARC)

1. Answer IARC questionnaire (10-15 questions)
2. Questionnaire assesses content appropriateness
3. Generates age ratings for different regions
4. Takes ~5 minutes to complete

### 7. Submit for Certification

1. Review all information entered
2. Click "Submit for certification"
3. Confirm submission

**Timeline**: I have no verified figure for current certification review times. Partner
Center reports live status for your submission — rely on that rather than any duration
quoted in this repository.

### 8. Monitor Certification

1. Go to "Certification" tab
2. Check status regularly
3. Status will show:
   - "In certification" (Microsoft is reviewing)
   - "Passed certification" (approved, publishing)
   - "Certification failed" (see feedback and re-submit)

### 9. If Rejected

1. Read Microsoft's feedback carefully
2. Identify specific issue
3. Refer to Certification-Reference.md
4. Fix the issue
5. Create new submission with corrected files
6. Re-submit

---

## Common Submission Data

**For JyGlobalVST v1.0.0.0:**

| Field | Value |
|-------|-------|
| App Name | JyGlobalVST |
| Publisher | JyGlobalVST |
| Version | 1.0.0.0 |
| Package Size | ~150 MB (varies) |
| Minimum OS | Windows 10 1909 |
| Architecture | x64 |
| Pricing | Free |
| Category | Audio or Utilities |

---

## Post-Publication

### Monitor the App

1. Check reviews daily for first week
2. Respond to user feedback
3. Monitor for any Microsoft enforcement actions

### Future Updates

For future releases:

1. Update `version-info.txt`
2. Run `Build-MSIX.ps1` with new version
3. Create git tag and push
4. CI/CD creates release
5. Follow same Partner Center submission steps
6. Increment version (never reuse same version)

---

## Troubleshooting

**Package not uploading?**
- Check file size (< 500MB)
- Verify file format (.msixbundle)
- Try different web browser

**Manifest validation fails?**
- Run `Validate-Package.ps1` locally
- Check error message carefully
- Fix manifest and rebuild
- Re-submit

**Certification fails?**
- Read feedback thoroughly
- Refer to Certification-Reference.md
- Fix issue
- Re-submit in new submission

---

## Resources

- Partner Center Help: https://support.microsoft.com/partners/
- App Store Policies: https://docs.microsoft.com/en-us/windows/uwp/publish/store-policies
- MSIX Documentation: https://learn.microsoft.com/windows/msix/

