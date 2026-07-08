# Microsoft Store Certification Reference

**Common Failure Reasons & Solutions**

---

## Over-Declared Capabilities

**Problem**: App declares capabilities it doesn't use  
**Prevention**: Audit manifest before submission  
**Solution**:
- Review each `<Capability>` element
- Verify code actually uses the capability
- Remove unused capabilities
- Re-submit

**For JyGlobalVST**: Only declare microphone, audioLibrary, documentsLibrary, registryRead/Write

---

## Version Mismatch

**Problem**: Version in manifest doesn't match package filename or Partner Center submission  
**Prevention**: Automate version injection  
**Solution**:
- Ensure AppxManifest.xml version matches `.msixbundle` filename
- Verify version matches Partner Center submission
- Use version-info.txt as single source of truth

---

## Missing Required Assets

**Problem**: Icon files missing or wrong sizes  
**Prevention**: Validate before submission  
**Solution**:
- Ensure all 5 required icon sizes present
- Verify dimensions are exact pixels (no scaling)
- Check PNG format with transparency
- Run validation script before upload

---

## Manifest Schema Errors

**Problem**: AppxManifest.xml doesn't conform to Microsoft schema  
**Prevention**: Validate locally with MSAP  
**Solution**:
- Use MSAP tool to validate manifest
- Check for required elements (Identity, Properties, Dependencies, Capabilities, Applications)
- Verify XML is well-formed (no parsing errors)
- Reference contracts/appx-manifest-schema.md for correct structure

---

## Unsupported Minimum OS

**Problem**: MinVersion less than Windows 10 1909  
**Prevention**: Set correct minimum version  
**Solution**:
- Ensure `<TargetDeviceFamily MinVersion="10.0.18363.0"/>`
- Don't set MinVersion to older Windows versions
- Update if needed and re-submit

---

## Architecture Mismatch

**Problem**: Package declares x86 or ARM64 instead of x64  
**Prevention**: Build for correct architecture  
**Solution**:
- Ensure manifest specifies only x64
- Verify .msixbundle contains x64 package
- For v1, x64 is only supported architecture

---

## Hardcoded Paths

**Problem**: App uses hardcoded paths like `C:\Users\username\...`  
**Prevention**: Use environment variables  
**Solution**:
- Use `%APPDATA%` for roaming settings
- Use `%LOCALAPPDATA%` for machine-local state
- Use `%USERPROFILE%` for Documents
- Don't hardcode user names or paths

**For JyGlobalVST**:
- Settings: `%AppData%\JyGlobalVST\` (roaming)
- Cache: `%LocalAppData%\JyGlobalVST\` (local)
- Presets: `%UserProfile%\Documents\JyGlobalVST\Presets\`

---

## Undeclared Registry Access

**Problem**: App writes to registry without declaring `registryWrite` capability  
**Prevention**: Declare all registry operations  
**Solution**:
- Add `<uap:Capability Name="registryWrite" />`
- Or use file-based settings instead
- Scope access to `HKEY_CURRENT_USER\Software\AppName\`

---

## Undeclared File Access

**Problem**: App accesses files without declaring appropriate capabilities  
**Prevention**: Declare file access  
**Solution**:
- For Documents: `<uap:Capability Name="documentsLibrary" />`
- For Pictures: `<uap:Capability Name="picturesLibrary" />`
- For Downloads: Requires app-specific capability

---

## Missing Privacy Policy

**Problem**: App lacks privacy policy link  
**Prevention**: Create and host privacy policy  
**Solution**:
- Write clear privacy policy explaining data collection
- Host on publicly accessible website
- Provide URL in Partner Center submission
- Update policy if data practices change

---

## Network Activity Without Declaration

**Problem**: App uses internet but doesn't declare it  
**Prevention**: Declare network capabilities  
**Solution**:
- If app needs internet: `<Capability Name="networkInternet" />`
- For local network: `<Capability Name="networkLocalNetworksOrRemoteNetworks" />`
- Document in privacy policy what data is sent where

**Note**: JyGlobalVST v1 doesn't use network in core features

---

## Elevated Privileges Required

**Problem**: App requires admin to run  
**Prevention**: Design for standard user  
**Solution**:
- Don't require admin elevation
- Use HKEY_CURRENT_USER instead of HKEY_LOCAL_MACHINE
- Store user data in AppData folders
- If elevation needed, reconsider design

---

## Certificate Issues

**Problem**: Package signature is invalid or untrusted  
**Prevention**: Use automatic signing  
**Solution**:
- For dev: Let MSIX tool auto-generate test certificate
- For Store submission: Submit unsigned package (Store applies their cert)
- Don't include .pfx files in source
- Verify certificate chain locally before submission

---

## Large Package Size

**Problem**: Package exceeds 500MB  
**Prevention**: Monitor build output size  
**Solution**:
- Exclude unnecessary files from package
- Remove debug symbols from Release build
- Compress large resources
- Use asset compression if available

**For JyGlobalVST**: Typical size 50-200MB

---

## Content Rating Issues

**Problem**: App doesn't match declared content rating  
**Prevention**: Complete IARC questionnaire honestly  
**Solution**:
- Answer all IARC questions accurately
- Review selected age rating
- Update if app features change

---

## Store Listing Incomplete

**Problem**: Missing description, screenshots, or metadata  
**Prevention**: Fill all required fields  
**Solution**:
- Add short description (50+ chars)
- Add long description (200+ chars)
- Include screenshots (optional but recommended)
- Set category correctly
- Provide support email

---

## Testing Checklist Before Submission

- [ ] Run MSAP validation - zero errors
- [ ] Validate manifest schema
- [ ] Test on clean Windows 10/11 VM
- [ ] Install and launch app
- [ ] Test core functionality
- [ ] Verify settings persist
- [ ] Uninstall completely
- [ ] Check for orphaned files

---

## If Rejected

1. Read Microsoft's rejection feedback carefully
2. Identify the specific issue
3. Find solution in this reference
4. Fix and validate locally
5. Re-submit with changes
6. Monitor for re-certification

Typical re-submission turnaround: 24-48 hours

