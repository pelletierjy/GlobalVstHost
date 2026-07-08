# Microsoft Store Publication Plan: JyGlobalVST

**Objective**: Publish JyGlobalVST to Microsoft Store  
**Current Status**: Implementation complete, ready for publication phase  
**Target Completion**: 2-3 weeks from start  
**Owner**: [Your Name]

---

## Phase 1: Pre-Publication Preparation (Days 1-3)

### 1.1 Icon Design & Assets

**Task**: Replace placeholder icons with professional designs

**Steps**:
1. [ ] Design or commission professional app icons
   - Recommended: 512×512 source image (high quality)
   - Style: Match JyGlobalVST branding
   - Resources:
     - Canva (design templates)
     - Fiverr/Upwork (freelance designers)
     - Local graphic designer

2. [ ] Generate all required sizes from source
   - [ ] Square44x44Logo.png (44×44)
   - [ ] Square50x50Logo.png (50×50)
   - [ ] Square150x150Logo.png (150×150)
   - [ ] Square310x310Logo.png (310×310)
   - [ ] StoreLogo.png (50×50)
   
   **Tools** (free options):
   - ImageMagick (command-line)
   - GIMP (free editor)
   - Online resizers (ezgif.com, convertio.co)

3. [ ] Replace placeholder files
   ```powershell
   # Remove old placeholders
   Remove-Item StorePackaging\Assets\*.png
   
   # Copy new icons
   Copy-Item "path\to\new\icons\*" StorePackaging\Assets\
   ```

4. [ ] Verify all icons
   - [ ] PNG format with transparency
   - [ ] Exact dimensions (no scaling)
   - [ ] All 5 required sizes present
   - [ ] Professional appearance

**Time Estimate**: 8-16 hours (depending on design vs. commissioning)  
**Responsible**: Design/Marketing team

---

### 1.2 Final Build & Validation

**Task**: Build final release package and validate thoroughly

**Steps**:
1. [ ] Update version to 1.0.0.0 in `StorePackaging/version-info.txt`

2. [ ] Build MSIX package
   ```powershell
   cd D:\repos\others\dev\GlobalVSTHost
   .\StorePackaging\Scripts\Build-MSIX.ps1 -Version 1.0.0.0 -Configuration Release
   ```

3. [ ] Validate package
   ```powershell
   .\StorePackaging\Scripts\Validate-Package.ps1 -PackagePath build\store-packages\JyGlobalVST_1.0.0.0_x64.msixbundle
   ```
   - Expected: ✅ VALIDATION PASSED

4. [ ] Test on clean Windows systems
   ```powershell
   .\StorePackaging\Scripts\Test-LocalInstall.ps1 -PackagePath build\store-packages\JyGlobalVST_1.0.0.0_x64.msixbundle
   ```
   - Expected: ✅ INSTALLATION TEST PASSED

5. [ ] Verify manifest
   - [ ] All required elements present
   - [ ] Version matches (1.0.0.0)
   - [ ] Icons referenced and present
   - [ ] Capabilities correctly declared

6. [ ] Document test results
   - [ ] Test date: _______________
   - [ ] Tester: _______________
   - [ ] Windows 10/11 version tested: _______________
   - [ ] All tests passed: ✓ Yes / ☐ No

**Time Estimate**: 4-6 hours  
**Responsible**: QA/Release Manager

---

### 1.3 Documentation Review

**Task**: Review and finalize all documentation

**Steps**:
1. [ ] Review Partner-Center-Checklist.md
   - [ ] All items applicable
   - [ ] Contact info updated
   - [ ] Privacy policy ready (see below)

2. [ ] Prepare privacy policy
   - [ ] Create or update privacy policy document
   - [ ] Host on public URL (required for Store submission)
   - [ ] Link: _________________________________
   - [ ] Document what data app collects (if any)
   - [ ] For v1: Likely "No data collection" (good!)

3. [ ] Prepare app description for Store
   - [ ] Short description (50+ chars): _____________________________
   - [ ] Long description (200+ chars): ____________________________
   - [ ] Key features listed
   - [ ] VST3 plugin support mentioned
   - [ ] Windows 10/11 compatibility noted

4. [ ] Create/update release notes
   - [ ] v1.0.0.0 release notes: __________________________________
   - [ ] Key features for v1
   - [ ] Known limitations (if any)
   - [ ] System requirements

**Time Estimate**: 2-4 hours  
**Responsible**: Product Owner / Marketing

---

## Phase 2: Partner Center Setup (Days 4-5)

### 2.1 Account & Publisher Registration

**Task**: Set up Partner Center account and verify publisher

**Steps**:
1. [ ] Create Microsoft Account (if needed)
   - Email: _________________________________
   - Password: [Secure it!]

2. [ ] Sign up for Partner Center
   - Go to: https://partner.microsoft.com
   - Select "Windows & Xbox"
   - Complete registration form

3. [ ] Complete publisher verification
   - [ ] Upload business verification documents (if required)
   - [ ] Wait for verification email (24-48 hours typically)
   - [ ] Verify email when received

4. [ ] Configure account settings
   - [ ] Set up payment method (required for paid apps; free apps may skip)
   - [ ] Add team members (if needed)
   - [ ] Configure notifications

**Time Estimate**: 2-4 hours (plus 1-2 days for verification)  
**Responsible**: Account Owner / Legal

---

### 2.2 App Registration

**Task**: Reserve app name and register in Partner Center

**Steps**:
1. [ ] Go to Partner Center dashboard
   - URL: https://partner.microsoft.com

2. [ ] Create new app submission
   - Click "Create new app"
   - Select "Reserve a name"
   - Enter: **JyGlobalVST**
   - Confirm name is available and reserve it

3. [ ] Verify name reservation
   - [ ] App name confirmed as: **JyGlobalVST**
   - [ ] Unique identifier assigned: _________________
   - [ ] Name matches manifest Identity/@Name

4. [ ] Configure basic app info
   - [ ] Category: Audio / Utilities
   - [ ] Subcategory: (if applicable)
   - [ ] Publisher name: JyGlobalVST
   - [ ] Contact email: _________________________________

**Time Estimate**: 1-2 hours  
**Responsible**: Product Owner

---

## Phase 3: Submission Preparation (Days 6-8)

### 3.1 Complete Partner Center Listing

**Task**: Fill in all Partner Center submission details

**Steps**:
1. [ ] Sign in to Partner Center
   - Account: _________________________________

2. [ ] Start new submission for JyGlobalVST
   - Click app name from dashboard
   - Click "Create new submission"

3. [ ] Fill "Packages & availability" section
   - [ ] Upload `.msixbundle` file from `build/store-packages/`
   - [ ] Verify version shows as: 1.0.0.0
   - [ ] Verify architecture shows as: x64
   - [ ] Set pricing to: Free

4. [ ] Fill "Properties" section
   - [ ] Title: **JyGlobalVST**
   - [ ] Subtitle: (optional) System audio VST host
   - [ ] Short description: (50+ chars) _________________________
   - [ ] Long description: (200+ chars) _________________________
   - [ ] Search terms/keywords: VST, audio, processor, effects, host
   - [ ] Copyright/legal info: [Your copyright notice]
   - [ ] Support contact email: _________________________________
   - [ ] Support website: _________________________________

5. [ ] Fill "Store listings" section
   - [ ] Description (Store page): _________________________________
   - [ ] Category: Audio (or Utilities)
   - [ ] Subcategory: (if available)
   - [ ] Age rating: Complete IARC questionnaire (see below)
   - [ ] Add screenshots (optional but recommended)
     - [ ] Screenshot 1 (app main window)
     - [ ] Screenshot 2 (plugin chain editor)
     - [ ] Screenshot 3 (settings/preferences)

6. [ ] Complete IARC age rating questionnaire
   - Go to IARC section
   - Answer 10-15 questions about app content
   - Questions cover: violence, language, content rating
   - For JyGlobalVST: Likely all "No" (audio tool, no content)
   - Receive age ratings for different regions

7. [ ] Fill "Availability" section
   - [ ] Pricing tier: Free
   - [ ] Markets: Select all regions initially (or choose specific regions)
   - [ ] Languages: English (US/UK)
   - [ ] Device family: Windows 10/11 (x64)

8. [ ] Add system requirements
   - [ ] Minimum OS: Windows 10 version 1909
   - [ ] Processor: x64
   - [ ] RAM: 4 GB recommended
   - [ ] Storage: 300 MB free space
   - [ ] Graphics: (if applicable)

9. [ ] Add legal documents
   - [ ] Privacy policy URL: _________________________________
   - [ ] Terms of service: (optional) _________________________________
   - [ ] License agreement: (optional)

**Time Estimate**: 4-6 hours  
**Responsible**: Product Owner / Marketing

---

### 3.2 Review & Final Checks

**Task**: Verify all information before submission

**Steps**:
1. [ ] Use Partner-Center-Checklist.md
   - Work through entire checklist
   - [ ] All items verified
   - [ ] No missing information
   - [ ] All required fields completed

2. [ ] Validate manifest locally one more time
   ```powershell
   .\StorePackaging\Scripts\Validate-Package.ps1 -PackagePath build\store-packages\JyGlobalVST_1.0.0.0_x64.msixbundle
   ```
   - Expected: ✅ VALIDATION PASSED

3. [ ] Review Partner Center submission preview
   - [ ] All text appears correct in Store preview
   - [ ] Icons display properly
   - [ ] Description formatting is correct
   - [ ] No typos or grammar errors

4. [ ] Final sign-off
   - [ ] Approver name: _________________________________
   - [ ] Approval date: _________________________________
   - [ ] All items verified and approved

**Time Estimate**: 1-2 hours  
**Responsible**: Product Owner / QA Lead

---

## Phase 4: Submit to Microsoft Store (Day 9)

### 4.1 Create Submission

**Task**: Submit package to Microsoft Store for certification

**Steps**:
1. [ ] Go to Partner Center
   - App: JyGlobalVST
   - Status: Should show "Ready to submit"

2. [ ] Review submission summary
   - [ ] Package version: 1.0.0.0
   - [ ] File size: ________ MB
   - [ ] All icons present
   - [ ] All metadata complete

3. [ ] Click "Submit for certification"
   - Confirm submission
   - Note the submission ID: _________________________________

4. [ ] Monitor initial validation (30 min - 2 hours)
   - Partner Center runs automated checks
   - If errors appear: Fix and re-submit
   - If passes: Moves to human certification

**Time Estimate**: 1 hour (plus wait time)  
**Responsible**: Release Manager

---

## Phase 5: Monitor Certification (Days 9-11)

### 5.1 Track Certification Progress

**Task**: Monitor app through Microsoft's certification process

**Steps**:
1. [ ] Check Partner Center daily
   - Go to JyGlobalVST app
   - Check "Certification" tab
   - Look for status updates

2. [ ] Understand certification statuses
   - **In certification**: Microsoft is reviewing (24-48 hours typical)
   - **Passed certification**: Approved! Will publish automatically
   - **Certification failed**: Issues to fix (see below)

3. [ ] If certification passes
   - [ ] App approved
   - [ ] Submission status: "Passed certification"
   - [ ] Publishing date: _____________ (usually within 1 hour)
   - [ ] Proceed to Phase 6

4. [ ] If certification fails
   - [ ] Read failure feedback carefully
   - [ ] Note specific issues: _________________________________
   - [ ] Refer to `StorePackaging/Documentation/Certification-Reference.md`
   - [ ] Fix issues locally
   - [ ] Rebuild package if needed
   - [ ] Create new submission with fixes
   - [ ] Re-submit for certification
   - [ ] Wait 24-48 hours again

**Time Estimate**: Daily checks (10 min each)  
**Responsible**: Release Manager

---

## Phase 6: Publication & Launch (Day 11-12)

### 6.1 Verify Publication

**Task**: Confirm app is live on Microsoft Store

**Steps**:
1. [ ] Check Partner Center
   - Status should show: "Published"
   - Publication date: _________________________________

2. [ ] Verify on Microsoft Store
   - Go to: https://www.microsoft.com/store/apps
   - Search for: "JyGlobalVST"
   - [ ] App appears in search results
   - [ ] App description shows correctly
   - [ ] Icons display properly
   - [ ] Screenshots visible
   - [ ] Free download button present

3. [ ] Install from Store (final test)
   - [ ] Download and install from Store
   - [ ] Verify app launches correctly
   - [ ] Verify all features work
   - [ ] Check system tray integration

4. [ ] Get store link
   - Store URL: https://www.microsoft.com/store/apps/...
   - Share this link publicly!

**Time Estimate**: 1-2 hours  
**Responsible**: Release Manager / QA

---

### 6.2 Announce Publication

**Task**: Notify users and stakeholders

**Steps**:
1. [ ] Create announcement
   - Template:
     ```
     🎉 JyGlobalVST is now available on Microsoft Store!
     
     Download here: [Store Link]
     
     v1.0.0.0 - Initial Release
     - System audio VST host
     - VST3 plugin support
     - Windows 10/11 compatible
     ```

2. [ ] Publish announcements
   - [ ] Website/blog post
   - [ ] Email announcement (if mailing list exists)
   - [ ] Social media (if applicable)
   - [ ] GitHub releases page

3. [ ] Update documentation
   - [ ] Update project README.md with Store link
   - [ ] Update CLAUDE.md with publication date
   - [ ] Create release notes in GitHub

4. [ ] Monitor initial feedback
   - [ ] Watch Store reviews
   - [ ] Monitor ratings
   - [ ] Respond to user feedback
   - [ ] Track download numbers

**Time Estimate**: 1-2 hours  
**Responsible**: Marketing / Product Owner

---

## Phase 7: Post-Publication Support (Ongoing)

### 7.1 Monitoring & Support

**Task**: Monitor app health and user feedback

**Daily (First Week)**:
- [ ] Check Store reviews
- [ ] Note any issues or feedback
- [ ] Monitor download statistics
- [ ] Respond to user reviews

**Weekly (First Month)**:
- [ ] Review ratings trend
- [ ] Track user feedback themes
- [ ] Monitor for any certification issues
- [ ] Plan any hotfixes if needed

**Monthly (Ongoing)**:
- [ ] Generate usage reports
- [ ] Plan next feature release
- [ ] Monitor app performance metrics
- [ ] Prepare v1.1 updates if needed

### 7.2 Future Releases

**For v1.1+ releases**, follow this simplified workflow:

1. [ ] Update version in `StorePackaging/version-info.txt`
2. [ ] Build: `Build-MSIX.ps1 -Version 1.0.1.0`
3. [ ] Validate: `Validate-Package.ps1`
4. [ ] Test: `Test-LocalInstall.ps1`
5. [ ] Create git tag: `git tag v1.0.1.0`
6. [ ] Push tag: `git push origin v1.0.1.0`
7. [ ] CI/CD automatically builds and creates release
8. [ ] Go to Partner Center → Create new submission
9. [ ] Upload new .msixbundle
10. [ ] Update version notes
11. [ ] Submit for certification
12. [ ] Wait 24-48 hours for approval
13. [ ] Publish!

**Time Estimate**: 2-3 hours per release  
**Responsible**: Release Manager

---

## Timeline Summary

| Phase | Duration | Start Date | End Date | Status |
|-------|----------|-----------|----------|--------|
| Phase 1: Pre-Publication | 3 days | Day 1 | Day 3 | ⏳ |
| Phase 2: Partner Center Setup | 2-3 days | Day 4 | Day 5-6 | ⏳ |
| Phase 3: Submission Prep | 3 days | Day 6 | Day 8 | ⏳ |
| Phase 4: Submit | 1 day | Day 9 | Day 9 | ⏳ |
| Phase 5: Certification | 2-3 days | Day 9 | Day 11 | ⏳ |
| Phase 6: Publication | 1 day | Day 11 | Day 12 | ⏳ |
| **Total** | **12-14 days** | | | |

**Critical Path**: If all goes smoothly, 12 days from start to publication  
**With Issues**: Add 3-7 days (for certification failures/fixes)

---

## Contingency Plans

### If Icon Design Delays

**Backup**: Use AI-generated icons or basic geometric designs
- Recommended: Microsoft Designer or similar
- Create 512×512 source
- Regenerate sizes
- Continue to next phase

### If Certification Fails

**Steps**:
1. Read feedback carefully
2. Refer to Certification-Reference.md
3. Fix specific issue
4. Rebuild package
5. Create new submission
6. Re-submit (typical turnaround: 24-48 hours)

**Common Issues** (see Certification-Reference.md):
- Over-declared capabilities → Remove unused ones
- Missing assets → Verify all icons present
- Version mismatch → Rebuild with correct version
- Manifest errors → Run Validate-Package.ps1 locally

### If Partner Center Registration Delays

**Backup**: Use existing business account if available
- Check if your organization already has Partner Center account
- Use existing verified publisher identity
- Speeds up approval process

---

## Sign-Off Checklist

- [ ] All icons designed and validated
- [ ] Final build tested and validated
- [ ] Privacy policy prepared and hosted
- [ ] Partner Center account ready
- [ ] App name reserved
- [ ] All metadata prepared
- [ ] Submission complete and verified
- [ ] Submitted to Microsoft Store
- [ ] Certification complete
- [ ] App published and verified
- [ ] Users notified
- [ ] Feedback monitoring active

---

## Key Contacts & Resources

**Microsoft Support**:
- Partner Center Help: https://support.microsoft.com/partners/
- Store Policies: https://docs.microsoft.com/windows/uwp/publish/store-policies

**Internal Docs**:
- Build Guide: `StorePackaging/Documentation/MSIX-Build-Guide.md`
- Testing Guide: `StorePackaging/Documentation/Local-Testing-Guide.md`
- Checklist: `StorePackaging/Documentation/Partner-Center-Checklist.md`
- Troubleshooting: `StorePackaging/Documentation/Certification-Reference.md`

**Key Files**:
- Package Output: `build/store-packages/JyGlobalVST_1.0.0.0_x64.msixbundle`
- Manifest: `StorePackaging/AppxManifest.xml`
- Build Script: `StorePackaging/Scripts/Build-MSIX.ps1`
- Validation Script: `StorePackaging/Scripts/Validate-Package.ps1`

---

## Notes & Additional Info

**Important Reminders**:
1. ✓ Icon design is THE critical path item (start immediately)
2. ✓ Partner Center registration can take 24-48 hours
3. ✓ Certification typically takes 24-48 hours
4. ✓ You can submit without perfect icons (Store accepts improvements)
5. ✓ Monitor reviews daily for first week
6. ✓ Respond to user feedback professionally

**Success Factors**:
- Professional icons → Better download rates
- Clear description → Better user expectations
- Quick certification response → Faster publication
- Good support → Better reviews and ratings

---

**Good luck! You've got this! 🚀**

For questions, refer to documentation in `StorePackaging/Documentation/` directory.

