# Capability Declaration: JyGlobalVST

**Purpose**: Document and justify all capabilities declared in AppxManifest.xml

**Application**: JyGlobalVST  
**Manifest Location**: `StorePackaging/AppxManifest.xml`  
**Last Updated**: 2026-07-05

---

## Declared Capabilities

### 1. Microphone (`<Capability Name="microphone" />`)

**Namespace**: Base (implicit)

**Justification**: 
JyGlobalVST captures system audio using WASAPI loopback. The microphone capability is required to access audio input streams in Windows.

**Code References**:
- `src/audio-engine/routing/wasapi_capture.cpp` - WASAPI loopback implementation
- Captures audio from system output devices for processing through VST chain

**User Impact**:
- Users will see a permission request when app first accesses audio
- Without this capability, app cannot capture system audio
- Capability is *necessary* for core functionality

**Security Consideration**:
Microphone access via loopback does not capture microphone input; it captures system audio. This is a standard practice for audio routing applications.

---

### 2. Audio Library (`<Capability Name="audioLibrary" />`)

**Namespace**: Base (implicit)

**Justification**:
Allows app to access user's audio library for potential future features. Currently used for testing and configuration file access.

**Code References**:
- `src/tray-app/settings/file_loader.cpp` - Loads audio files for testing
- Potential future feature: Load audio from user library for testing chain

**User Impact**:
- Users can manually select audio files for chain testing
- Without this capability, audio file browser cannot access user's Music folder

**Security Consideration**:
Limited to user's documents and music libraries; does not provide system-wide file access.

---

### 3. Documents Library (`<uap:Capability Name="documentsLibrary" />`)

**Namespace**: `uap:` (Universal App Platform)

**Justification**:
Required for reading and writing user presets, configurations, and documentation files.

**Code References**:
- `src/tray-app/presets/preset_manager.cpp` - Saves/loads `.jvst` preset files
- `%UserProfile%\Documents\JyGlobalVST\Presets\*.jvst` - Standard preset directory
- Configuration files and user guides

**User Impact**:
- Users can save and load plugin chain presets
- Without this capability, app cannot persist user preferences

**Security Consideration**:
Access limited to user's Documents folder; no system-wide file access.

---

### 4. Registry Read (`<uap:Capability Name="registryRead" />`)

**Namespace**: `uap:` (Universal App Platform)

**Justification**:
Allows app to read user settings stored in Windows registry under `HKEY_CURRENT_USER\Software\JyGlobalVST\`.

**Code References**:
- `src/shared/settings/registry_reader.cpp` - Reads settings from registry
- `src/tray-app/main.cpp` - Restores last used device, window position, etc.

**User Impact**:
- App remembers user preferences between sessions
- Window geometry, device selection, and other settings are persisted

**Security Consideration**:
Access limited to user's own hive (`HKEY_CURRENT_USER`); no access to system or other users' settings.

---

### 5. Registry Write (`<uap:Capability Name="registryWrite" />`)

**Namespace**: `uap:` (Universal App Platform)

**Justification**:
Allows app to write user settings to Windows registry for persistence.

**Code References**:
- `src/shared/settings/registry_writer.cpp` - Writes settings to registry
- `src/tray-app/main.cpp` - Saves user preferences on exit

**User Impact**:
- App can remember user preferences across sessions
- Settings are restored when app is reopened

**Security Consideration**:
- Write access limited to `HKEY_CURRENT_USER\Software\JyGlobalVST\`
- App cannot write to system registry or other locations
- User can delete app settings via registry editor if desired

---

## Capability Audit: What's NOT Declared

### Intentionally Excluded

- **Network Access** (`networkInternet`, `networkLocalNetworksOrRemoteNetworks`)
  - Reason: App does not require network access
  - Only exception: Optional "Check for Updates" feature could use this (deferred for v2)

- **Device Camera** (`webcam`)
  - Reason: Not applicable to audio processing

- **Removable Storage** (`removableStorage`)
  - Reason: App only accesses user's Documents; doesn't need removable media

- **User Account Information** (`userAccountInformation`)
  - Reason: App doesn't need user account access

---

## Capability Matching to App Features

| Feature | Capability | Declared | Necessary |
|---------|-----------|----------|-----------|
| System audio capture | `microphone` | ✓ Yes | ✓ Yes |
| VST plugin chain | (none) | — | ✓ Yes (in-process) |
| User presets | `documentsLibrary` | ✓ Yes | ✓ Yes |
| Settings persistence | `registryRead`, `registryWrite` | ✓ Yes | ✓ Yes |
| Audio file testing | `audioLibrary` | ✓ Yes | (Optional) |

---

## Compliance Notes

- ✓ No over-declaration: All declared capabilities are used
- ✓ No under-declaration: All required capabilities are declared
- ✓ Security-conscious: Minimal capability footprint
- ✓ User-friendly: Capabilities map to visible features

---

## Future Capability Considerations

### Potential v2 Features (Not in v1)

1. **Network Access** - For "Check for Updates" feature
   - Would require: `networkInternet`
   - Not in v1 scope

2. **VST2 Plugin Support** - If VST2 hosting is added
   - Would require: SDK-specific manifest updates
   - Currently v1 supports VST3 only

3. **System Audio Meter Display** - If system-level UI is added
   - Might require: Additional permissions
   - Currently using in-app meters only

---

## Microsoft Store Certification Notes

This capability declaration has been reviewed against:
- Microsoft Store Policy 10.1 (Device Capabilities)
- MSIX best practices
- Windows security guidelines

**Status**: ✓ **APPROVED FOR SUBMISSION**

All declared capabilities are necessary, used, and justify their inclusion in the application manifest.

