# Feature Specification: JyGlobalVST (System Host)

**Feature Branch**: `001-jyglobalvst-system-host`

**Created**: 2026-06-04

**Status**: Draft

**Input**: User description: "JyGlobalVST — a Windows application that creates a virtual audio output device, routes all system audio through a VST plugin chain, and outputs to a selected hardware device with low latency. Replaces complex chains involving Virtual Audio Cable, Voicemeeter, and LightHost with a single lightweight FXSound-style app for gamers, audiophiles, streamers, and home studio users."

## Clarifications

### Session 2026-06-04

- Q: Should JyGlobalVST support VST2, VST3, or both? → A: VST3 only at launch; VST2 support roadmapped for a later release if user demand warrants.
- Q: When a VST3 plugin misbehaves (hard crash, hang, memory corruption), what should JyGlobalVST do? → A: In-process hosting with structured exception handling (SEH) — catch common plugin exceptions in the audio thread, automatically bypass the failing plugin, continue audio flow through the rest of the chain. Accepts that memory corruption and audio-thread hangs cannot be fully recovered.
- Q: What format should JyGlobalVST presets use? → A: JSON manifest (plugin list, order, bypass state, metadata, schema version) with base64-encoded VST3 state chunks embedded. Human-readable, diff-able, hand-editable for chain order/plugin paths; plugin internal state remains opaque per VST3 spec.
- Q: When the selected hardware output disappears mid-playback, what should JyGlobalVST do? → A: Auto-fall-back to the current Windows default output device, show a non-modal notification, and automatically resume on the preferred device when it reconnects. Mirrors native Windows audio behavior so users do not experience unexpected silence.
- Q: What diagnostic/logging strategy should JyGlobalVST use? → A: No persistent logging at all. No disk logs, no telemetry, no crash reports, no network transmission. Errors surface only through in-app UI notifications during the active session. Maximum privacy; support will rely on user-reproducible steps and screenshots rather than diagnostic files.
- Q: When should JyGlobalVST be active and processing system audio? → A: Hybrid approach: default is user-mode app that auto-launches on user login and runs in system tray. Advanced installer option allows opt-in Windows Service mode (runs at boot, pre-login audio support). Both modes supported; default avoids service complexity for most users.
- Q: Should JyGlobalVST auto-save the active plugin chain and device selections when the user closes the application? → A: Yes, auto-save on close. Store current chain state (plugins, order, parameters, device, buffer size) to a hidden auto-save file. Restore on next launch. User can undo auto-save by explicitly opening a saved preset before closing.
- Q: When CPU usage approaches or exceeds the 5% budget, what should JyGlobalVST do beyond notifying the user? → A: Warn only (passive notification). Display a non-modal notification "CPU approaching limit; consider increasing buffer size." User retains full control over whether to respond. Do not auto-increase buffer or auto-bypass plugins.
- Q: What is the practical maximum number of plugins in a single chain? → A: No hard limit. UI supports any number (scrollable list with depth indicator). Test validation covers up to 10-plugin chains (99th percentile use case). Users loading 20+ plugins are power users who understand latency/CPU tradeoffs.
- Q: What bit depth(s) should JyGlobalVST support internally and from external sources? → A: Support 16-bit, 24-bit, and 32-bit (float) at all sample rates. Extend sample rate support up to 176.4 kHz and 192 kHz (beyond initial 44.1/48/96 kHz). Process internally at 32-bit float; convert source and destination formats as needed.
- Q: When loading a preset that references a VST3 plugin no longer installed at the recorded path, what should JyGlobalVST do? → A: Load partially — instantiate plugins that resolve; for each missing plugin, insert a greyed-out placeholder slot preserving its position, bypass state, and remembered state chunk; show a non-modal notification listing what is missing; user can re-point the placeholder to a relocated plugin or remove it.
- Q: After a plugin hard-crashes the host (memory corruption / audio-thread hang), what should JyGlobalVST do on next launch regarding that plugin? → A: No quarantine. Always reload the last saved state on next launch; the user is responsible for identifying and removing crashing plugins manually. No persistent quarantine list, no auto-skip, no "re-enable" flow.
- Q: How should JyGlobalVST handle multichannel (5.1/7.1) source audio in v1? → A: Advertise the virtual device as stereo-only; Windows / source apps perform any required downmix upstream before audio reaches JyGlobalVST; the virtual device never receives more than 2 channels.
- Q: What driver-signing and install model should the virtual audio device use? → A: WHQL-signed user-mode virtual audio driver (modern WaveRT virtual endpoint or AVStream/APO) shipped inside a single signed MSI/MSIX. One admin-elevation prompt on install, no reboot required, full removal on uninstall. A valid code-signing certificate (EV preferred for WHQL attestation) is a release prerequisite.
- Q: What is the plugin-scan UX (default paths, blocking vs. background, cancellation, progress)? → A: On first launch and any user-triggered rescan, JyGlobalVST pre-seeds the two standard Windows VST3 paths (`%ProgramFiles%\Common Files\VST3` and `%LocalAppData%\Programs\Common\VST3`). Scan runs on a background thread with a visible progress indicator showing current path and plugin count, the UI remains responsive, the user can cancel, and validated plugins appear incrementally in the chooser as they are discovered.
- Q: In service-mode installs, what is the runtime relationship between the Windows Service (audio engine) and the user-facing UI? → A: Service + tray app. The Windows Service hosts the audio engine and runs at boot (enabling pre-login audio). A separate tray application is also installed and auto-launches on user login; it provides the UI, meters, and chain editor and communicates with the service via local IPC (named pipe / RPC). The tray app does not host the audio engine in service-mode; it is a client of the service. In user-mode (default) installs, the tray app hosts the audio engine in-process and no service is installed.
- Q: Where are saved presets stored on disk, and how do users share/import them? → A: Individual `.jvst` files under `%UserProfile%\Documents\JyGlobalVST\Presets\`. Files are visible, portable, drag-droppable into the app, and cloud-sync friendly (OneDrive/Drive) without JyGlobalVST owning sync logic. Auto-save state lives separately in `%LocalAppData%\JyGlobalVST\` (machine-local, ephemeral, not synced).
- Q: What happens when a second instance of the tray app is launched while one is already running? → A: Single-instance per user session enforced via a session-scoped named mutex. The second launch detects the running tray app, focuses/restores its main window, then exits. Multi-user / Fast-User-Switching / RDP scenarios each get one independent tray-app instance per session. In service-mode installs, the Windows Service itself is single-instance machine-wide by Windows service semantics; multiple per-session tray apps may connect to it concurrently via IPC.
- Q: Where do app-level settings (custom scan paths, default buffer, theme, default device, window geometry) live, and how is portable vs machine-local state separated? → A: Split storage. `%AppData%\Roaming\JyGlobalVST\settings.json` holds user-portable preferences (custom scan paths, default buffer size, theme, default hardware device by friendly name). `%LocalAppData%\JyGlobalVST\` holds machine-specific state (window geometry, last hardware device by Windows endpoint ID, plugin scan cache, auto-save chain state). Allows users to carry preferences to a new machine without polluting roaming with geometry or endpoint-ID data.
- Q: How do users discover and install updates given the no-telemetry / no-network constitutional stance? → A: User-initiated in-app update check. An "About → Check for updates…" menu item, only when explicitly clicked, performs a single HTTPS GET to a known version-manifest endpoint and displays the available version + download link. No background polling, no launch-time check, no identifying payload, no transmission of error/diagnostic data. The MSI/MSIX download and install remain explicit user actions. The no-telemetry rule is interpreted as banning transmission of diagnostic/usage data, not banning explicit user-initiated lookups.
- Q: What is the accessibility and localization scope for v1? → A: English-only at v1 (UI strings are not externalized for translation); the UI MUST be fully operable via keyboard alone (tab order, focus indicators, accelerators for all primary actions) and all interactive controls MUST expose accessible names/roles to Windows screen readers (UI Automation / MSAA). Formal WCAG certification is not pursued, but blind/low-vision users can operate the app. Multi-language localization is deferred to a future release.
- Q: How should imported preset files (drag-drop or file-picker) be validated before they touch the running chain? → A: Validated import (no sandbox process). Imported `.jvst` files MUST pass strict JSON schema validation (reject unknown top-level fields, enforce maximum file size and per-state-chunk size). The plugin path recorded in the preset is treated as a *hint only*: the host re-resolves each referenced plugin against the scanned-plugin cache by VST3 identifier (UID + vendor + name), and only loads the matching scanned plugin. State chunks remain opaque to the host and are passed to the plugin per the VST3 spec. No probe/sandbox process is required — the existing in-process SEH plugin-crash isolation (FR-023) covers runtime failure.
- Q: On launch after an unclean shutdown (host process crashed mid-session), should JyGlobalVST detect the prior crash and signal the user before silently restoring auto-save state? → A: No crash detection. Next launch always restores the last auto-saved state with no notification, regardless of whether the previous session ended cleanly or by crash. Consistent with the prior "no quarantine" decision (clarification #12) and the no-persistent-logging / no-telemetry posture: the host adds no mechanism (no sentinel file, no exit-cause record) to distinguish clean shutdown from crash. The user is solely responsible for identifying crash-prone plugins by observation and removing them manually. Accepts the risk of a silent crash loop if a recently added plugin is consistently fatal.
- Q: Is MIDI input / parameter control in scope for v1? → A: No MIDI in v1 — explicit out-of-scope. The host does not enumerate MIDI input devices, does not route MIDI events into the VST3 chain, and provides no host-level MIDI-learn for plugin parameters. Plugins requiring MIDI input will load but receive no MIDI events. Rationale: JyGlobalVST is positioned as a system-audio processor (post-mix DSP on already-rendered Windows audio), not a DAW. MIDI integration is roadmapped as a future enhancement and v1 architecture must not block adding it later, but no v1 task delivers it.
- Q: Can the chain feed multiple hardware output endpoints simultaneously (fan-out / mirror) in v1? → A: Single hardware output only in v1 — explicit. The chain feeds exactly one user-selected Windows audio output endpoint at a time. Switching outputs is a one-click change in the device selector; no simultaneous fan-out, no mirror endpoint, no independent processing chains. Multi-output (mirror or N-way fan-out) is documented as out-of-scope for v1 and a roadmapped future enhancement. v1 architecture must remain compatible with adding it later (no contracts that would block per-endpoint resampling or fan-out routing in a later release).

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Apply VST processing to all system audio (Priority: P1)

A gamer or audiophile installs JyGlobalVST, selects it as their default Windows audio output, loads a room-correction VST plugin (e.g., IK Multimedia ARC X), and selects their hardware speakers/headphones as the output destination. From that moment, all system audio — games, music, video calls, browser media — flows through the VST chain and out to their hardware with low latency.

**Why this priority**: This is the entire product value proposition. Without it, no other feature matters. It directly replaces the user's current fragile multi-tool chain.

**Independent Test**: After install, set JyGlobalVST as the Windows default output, load one VST plugin, play any audio source (Spotify, YouTube, a game). Confirm audio is processed by the plugin and reaches hardware output without dropouts.

**Acceptance Scenarios**:

1. **Given** JyGlobalVST is installed and running, **When** the user opens Windows Sound settings, **Then** JyGlobalVST appears as an available audio output device
2. **Given** the user selects JyGlobalVST as default output, **When** any application plays audio, **Then** that audio is captured by JyGlobalVST and routed through the loaded VST chain
3. **Given** a VST plugin is loaded and a hardware output is selected, **When** audio plays, **Then** the processed audio is audible on the selected hardware with no dropouts
4. **Given** the system is playing audio, **When** the user measures round-trip latency, **Then** total latency is under 20 ms (target < 10 ms)

---

### User Story 2 - Manage a chain of VST plugins (Priority: P2)

A home studio user wants to layer multiple VST plugins: a room-correction plugin first, then a parametric EQ, then a limiter. They open JyGlobalVST, scan their VST directories, drag plugins into a chain in their preferred order, and can bypass individual plugins for A/B comparison.

**Why this priority**: Single-plugin support (P1) covers the most common use case, but advanced users — the secondary audience — need chains. This unlocks pro-audio use cases without DAW complexity.

**Independent Test**: Load three plugins into the chain in a specific order, play audio, verify processing order is correct. Bypass the middle plugin and confirm audio bypasses it but still flows through the others.

**Acceptance Scenarios**:

1. **Given** the user has VST plugins installed, **When** they trigger a plugin scan, **Then** JyGlobalVST lists all detected VST3 plugins
2. **Given** plugins are scanned, **When** the user adds plugins to the chain, **Then** the chain processes audio in the displayed order
3. **Given** a plugin is in the chain, **When** the user toggles its bypass, **Then** audio bypasses that plugin while continuing through the rest of the chain
4. **Given** plugins are in the chain, **When** the user reorders them, **Then** processing order updates immediately without audio interruption

---

### User Story 3 - Save and recall presets (Priority: P3)

A streamer wants different audio chains for different scenarios: one for gaming (heavy bass boost, voice clarity EQ), one for music listening (room correction, mild compression), one for streaming (de-esser, limiter). They configure each chain and save it as a named preset they can recall in one click.

**Why this priority**: Quality-of-life feature that increases stickiness but isn't core. Users can manually rebuild chains without it; presets just save time.

**Independent Test**: Configure a chain, save it as "Gaming Preset", change the chain entirely, save as "Music Preset", switch between the two presets, verify the chain rebuilds correctly each time.

**Acceptance Scenarios**:

1. **Given** a configured plugin chain, **When** the user saves it as a named preset, **Then** the preset persists across application restarts
2. **Given** saved presets exist, **When** the user selects a preset, **Then** the plugin chain rebuilds to match the preset including plugin order, parameters, and bypass states
3. **Given** a preset is loaded, **When** the user modifies the chain, **Then** they can save it as an update to the current preset or as a new preset

---

### User Story 4 - Monitor audio levels and latency in real time (Priority: P3)

A user troubleshooting audio issues wants visibility into what's happening: input level meters showing audio is flowing in, output level meters confirming audio is leaving, and a latency readout to verify performance is acceptable for their use case (gaming requires lower latency than music).

**Why this priority**: Helps diagnose issues but not required for normal operation. Users with working audio rarely look at meters.

**Independent Test**: Play audio of varying volumes and confirm input/output meters respond appropriately. Adjust buffer size and confirm the latency readout updates accordingly.

**Acceptance Scenarios**:

1. **Given** audio is playing, **When** the user views the main UI, **Then** input and output level meters show real-time signal levels
2. **Given** audio is flowing, **When** the user views the latency indicator, **Then** it displays current round-trip latency in milliseconds
3. **Given** the user changes the buffer size, **When** the change takes effect, **Then** the latency display updates to reflect the new value

---

### Edge Cases

- **Sample rate / bit depth mismatch**: What happens when source audio is 48 kHz 24-bit but hardware is 44.1 kHz 16-bit (or any other combination)? System must automatically resample and convert bit depth without glitching, using 32-bit float as internal working format.
- **Plugin crash (catchable exception)**: What happens when a loaded VST3 plugin throws an exception mid-playback? System catches the exception via SEH, marks the plugin as failed in the chain UI, bypasses it, and continues audio flow. User sees a non-modal notification.
- **Plugin hard failure (memory corruption / audio-thread hang)**: What happens when a plugin causes uncatchable memory corruption or hangs the audio thread? Host may crash. On next launch, JyGlobalVST reloads the auto-saved chain state exactly as it was (no automatic quarantine, no auto-skip). The user is responsible for identifying and removing repeatedly-crashing plugins manually via the chain UI.
- **Hardware output device removed**: What happens when the user unplugs the selected output (e.g., USB DAC, Bluetooth headphones)? System detects the removal, automatically routes audio to the current Windows default output device, displays a non-modal notification, and automatically restores output to the preferred device when it reconnects.
- **No VST plugins loaded**: What happens when JyGlobalVST is selected as output but no plugins are in the chain? Audio must pass through transparently (no processing, no added latency beyond minimum).
- **CPU overload**: What happens when a heavy plugin chain exceeds CPU budget? System must detect xruns/dropouts and warn the user with a suggestion to increase buffer size.
- **Plugin GUI crash**: What happens when a plugin's GUI window crashes but the audio processing continues? Audio must continue; only the GUI is unavailable until reload.
- **System sleep/wake**: What happens when Windows sleeps and wakes? JyGlobalVST must reinitialize cleanly without requiring user intervention.
- **Multiple audio applications**: What happens when multiple apps play audio simultaneously through JyGlobalVST? All streams must mix correctly and pass through the same VST chain.
- **Preset references a missing plugin**: What happens when a loaded preset references a plugin whose file is no longer present at the recorded path (different machine, plugin uninstalled or moved)? System loads the preset partially, instantiating resolvable plugins and inserting greyed-out placeholders for missing ones (preserving chain position, bypass state, and stored state chunk). A non-modal notification lists the missing plugins; the user can re-point each placeholder to a relocated file or remove it.

## Requirements *(mandatory)*

### Functional Requirements

**Virtual Audio Device**

- **FR-001**: System MUST register a virtual audio output device that is visible and selectable in Windows Sound settings
- **FR-002**: System MUST support sample rates of 44.1 kHz, 48 kHz, 96 kHz, 176.4 kHz, and 192 kHz on the virtual device
- **FR-002a**: System MUST support bit depths of 16-bit integer, 24-bit integer, and 32-bit float; process internally at 32-bit float and convert source/destination formats as needed
- **FR-003**: System MUST advertise the virtual audio device as stereo-only (2-channel) in the initial release. Source applications and the Windows audio engine are responsible for any required downmix from multichannel (5.1, 7.1, etc.) before audio reaches the virtual device. The virtual device MUST NOT accept or expose any non-stereo endpoint configuration in v1
- **FR-004**: System MUST capture all audio routed to the virtual device for processing

**VST Hosting**

- **FR-005**: System MUST scan for VST3 plugins on a background thread (never blocking the UI). On first launch and on any user-triggered rescan, the scan set MUST include — pre-seeded by default — the two standard Windows VST3 paths: `%ProgramFiles%\Common Files\VST3` and `%LocalAppData%\Programs\Common\VST3`. Users MUST be able to add, remove, or disable additional directories. The scan UI MUST show a progress indicator displaying the current directory and a running plugin count, MUST allow the user to cancel mid-scan, and MUST surface validated plugins in the chooser incrementally as they are discovered (not only on scan completion)
- **FR-006**: System MUST load and instantiate VST3 plugins selected by the user
- **FR-007**: System MUST display each loaded plugin's native GUI in a window the user can show/hide
- **FR-008**: System MUST allow the user to construct a chain of multiple plugins in a specified order; no hard limit on chain length. UI MUST display a chain depth indicator and remain usable with 10+ plugins (scrollable list)
- **FR-009**: System MUST allow the user to bypass individual plugins without removing them from the chain
- **FR-010**: System MUST allow the user to reorder plugins in the chain without audio interruption
- **FR-011**: System MUST allow the user to remove plugins from the chain

**Audio Routing**

- **FR-012**: System MUST route captured audio through the configured plugin chain in order
- **FR-013**: System MUST output processed audio to exactly one user-selected hardware output device at a time. Simultaneous fan-out / mirror to multiple endpoints is explicitly out of scope for v1. Switching to a different hardware output is a one-click change in the device selector and MUST complete without restarting the application
- **FR-013a**: System architecture MUST NOT introduce contracts (data structures, IPC schemas, preset schema fields, etc.) that would block adding multi-output fan-out in a later release. Specifically, the audio routing layer should treat the single output as the v1 instance of a potentially N-way output set, not as a globally hardcoded singleton
- **FR-013b**: System MUST expose the post-chain processed audio as a virtual ASIO driver endpoint (ASIO multi-client), allowing ASIO-only DAWs and professional audio applications to consume the processed signal. The ASIO driver MUST share the same audio engine output buffer as the WASAPI hardware output path, MUST support sample rates {44.1, 48, 96, 176.4, 192} kHz and buffer sizes {32, 64, 128, 256, 512, 1024} samples, and MUST present a stereo (2-channel) output. The ASIO driver MUST be registerable and unregisterable on Windows without requiring a system reboot. The ASIO driver is a separate driver-level component from the WaveRT virtual endpoint; both may be installed simultaneously
- **FR-014**: System MUST automatically negotiate sample rates and resample when the source and destination differ
- **FR-015**: System MUST allow the user to select buffer size from a defined range (e.g., 128, 256, 512, 1024 samples)
- **FR-015a**: System MUST support using different driver types for input and output simultaneously. Specifically, the user MUST be able to select an ASIO device for hardware output while capturing input from a non-ASIO (WASAPI) device, and audio MUST flow seamlessly through the plugin chain.

**User Interface**

- **FR-016**: System MUST provide a single-window UI showing the plugin chain, device selectors, and meters
- **FR-017**: System MUST provide a hardware output device selector populated with available Windows audio devices
- **FR-018**: System MUST display real-time input and output level meters
- **FR-019**: System MUST display current round-trip latency in milliseconds
- **FR-019a**: All UI views (main window, chain editor, device selector, meters, About dialog, notifications) MUST be fully operable using the keyboard alone. Every interactive control MUST be reachable via Tab navigation in a logical reading order, MUST display a visible focus indicator when focused, and primary actions (load preset, save preset, bypass plugin, open plugin GUI, scan plugins) MUST have keyboard accelerators
- **FR-019b**: All interactive controls and meters MUST expose accessible names, roles, and current values via Windows UI Automation (or MSAA fallback) so that screen readers (Narrator, NVDA, JAWS) can announce them. Dynamic state changes (plugin added/removed, bypass toggled, device disconnect, CPU warning) MUST raise UI Automation notifications. Formal WCAG certification is not a v1 requirement
- **FR-019c**: All user-facing strings in v1 are English-only. The application is NOT required to externalize strings for translation in v1; multi-language localization is explicitly deferred to a later release
- **FR-020**: System MUST allow the user to save the current plugin chain configuration as a named preset
- **FR-021**: System MUST allow the user to load a saved preset, replacing the current chain
- **FR-022**: Preset files MUST persist across application restarts and MUST be stored as individual files (one preset per file) under `%UserProfile%\Documents\JyGlobalVST\Presets\` with the `.jvst` extension. The folder MUST be created on first launch if it does not exist
- **FR-022a**: Preset files MUST use a JSON manifest format containing plugin list, plugin identification metadata (VST3 UID + vendor + name), order, bypass states, and a schema version field, with each plugin's VST3 internal state stored as opaque base64-encoded binary chunks (state chunks are NOT introspected or partially deserialized by the host; they are passed verbatim to the plugin per VST3 spec)
- **FR-022g**: System MUST support importing preset files via (a) drag-and-drop of a `.jvst` file onto the application window, and (b) an in-app "Import Preset…" file picker. Imported presets are copied into the user's Presets folder; on name collision, the user MUST be prompted to overwrite, rename, or cancel
- **FR-022g-1**: Before an imported preset is loaded into the active chain or written to the Presets folder, System MUST validate it against a strict JSON schema: (a) reject the file if it contains unknown top-level fields, (b) enforce a hard maximum file size (e.g., 50 MB) and a per-state-chunk size cap (e.g., 16 MB) to prevent decompression/denial-of-service abuse, (c) verify the schema version is recognized (or migratable per FR-022b). On validation failure, the import MUST be rejected with a non-modal notification explaining which check failed; no partial state is committed and no chain modification occurs
- **FR-022g-2**: The plugin file path recorded inside an imported preset MUST be treated as a hint only. For each referenced plugin, System MUST resolve the plugin against the local scanned-plugin cache by VST3 identifier (UID + vendor + plugin name); the host MUST NOT load a plugin binary directly from the recorded path. If no scanned plugin matches the identifier, the slot becomes a placeholder per FR-022f. VST3 state chunks remain opaque and are passed to the resolved plugin per the VST3 spec; the host MUST NOT introspect, modify, or partially deserialize them
- **FR-022h**: System MUST provide an "Export Preset…" action that saves the currently selected preset to a user-chosen location, and a "Reveal in Explorer" action that opens the Presets folder in Windows Explorer
- **FR-022i**: System MUST NOT implement its own cloud sync. Users who place their Presets folder inside a cloud-synced location (OneDrive, Google Drive, Dropbox, etc.) are responsible for sync; JyGlobalVST MUST tolerate eventual-consistency behaviors (e.g., a preset file appearing or disappearing between scans) without crashing
- **FR-022b**: System MUST handle preset files from older schema versions by migrating known fields and preserving unknown fields; if migration is impossible, the user MUST be warned and the preset MUST NOT silently corrupt
- **FR-022c**: System MUST auto-save the current active chain state (all plugins in chain with order, bypass states, parameters, selected hardware output device, selected buffer size) on application close to a single auto-save file under `%LocalAppData%\JyGlobalVST\` (machine-local, not user-synced). The auto-save file is NOT a preset and MUST NOT appear in the user's preset list
- **FR-022d**: System MUST restore the auto-saved state on application launch if the auto-save file is valid and not corrupted; if corruption is detected, silently discard the auto-save and start with a blank chain. System MUST NOT distinguish clean shutdown from prior unclean termination (host process crash): the auto-save is restored identically in both cases, with no crash-detection notification, no sentinel file, and no quarantine of any previously loaded plugin. Identifying and removing crash-prone plugins is solely the user's responsibility
- **FR-022e**: User can override auto-save behavior by explicitly loading a saved preset before closing the application; the next launch will restore that preset instead of the auto-save
- **FR-022f**: When loading a preset that references one or more plugins that cannot be resolved (missing file, moved, or uninstalled), System MUST load the preset partially: instantiate every plugin that resolves and, for each unresolved plugin, insert a greyed-out placeholder slot in its original chain position preserving bypass state and the stored VST3 state chunk. System MUST display a non-modal notification listing each missing plugin (display name + recorded path). User MUST be able to (a) re-point a placeholder to a relocated plugin file, or (b) remove the placeholder from the chain. Audio MUST flow through the resolved plugins, skipping placeholders, without dropouts.
- **FR-022j**: The tray application MUST enforce single-instance behavior per user session using a session-scoped named mutex. If a second instance is launched within the same user session, it MUST focus/restore the existing instance's main window (sending a bring-to-foreground IPC message if needed) and then exit cleanly without claiming the virtual device. Distinct user sessions (Fast-User-Switching, RDP) MUST each be allowed their own independent tray-app instance. In service-mode installs, multiple per-session tray instances MAY connect concurrently to the single machine-wide service via the IPC channel defined in FR-028a
- **FR-022k**: System MUST persist user-portable preferences — custom plugin scan paths added or disabled by the user (extending FR-005), default buffer size, theme/color scheme, and default hardware output device identified by its Windows friendly name — to `%AppData%\Roaming\JyGlobalVST\settings.json`. This file MUST be JSON, schema-versioned, and tolerant of unknown fields (forward-compatible)
- **FR-022l**: System MUST persist machine-specific state — main window geometry (position, size, maximized state), the last successfully bound hardware output device identified by its Windows endpoint ID, the plugin scan cache, and the auto-save chain state from FR-022c — under `%LocalAppData%\JyGlobalVST\`. Roaming this state to another machine MUST NOT be required and MUST NOT cause failure if absent on first launch
- **FR-022m**: On startup the system MUST resolve the active hardware output device in this priority order: (1) endpoint-ID match from local state (FR-022l); (2) if that endpoint is unavailable, friendly-name match from roaming settings (FR-022k); (3) otherwise the current Windows default output. The resolution path used MUST be visible in the device selector tooltip or About/diagnostics surface so users can tell why a particular device was chosen
- **FR-022n**: System MUST provide an "About → Check for updates…" menu item that, only when explicitly invoked by the user, performs a single HTTPS GET against a configured version-manifest endpoint and displays the latest available version, release notes summary, and a download link. The system MUST NOT poll for updates on launch, on a timer, or in the background, MUST NOT send any identifying or diagnostic payload (no version-of-installed, no user ID, no machine ID, no usage data — only the default HTTPS GET that the platform performs), and MUST surface any failure (offline, DNS, TLS, 4xx/5xx) only as an in-session UI notification per the no-persistent-logging rule
- **FR-022o**: The actual update package (MSI/MSIX) download and install MUST remain explicit user actions (click the download link, run the installer). The application MUST NOT auto-download or auto-install updates

**Stability & Resilience**

- **FR-023**: System MUST catch structured exceptions and C++ exceptions raised by plugins during audio processing, automatically bypass the failing plugin, display an in-app non-modal notification identifying the failed plugin, and continue audio flow through remaining plugins in the chain. Memory corruption and audio-thread hangs are explicit non-goals for recovery. No persistent log entry is written to disk.
- **FR-024**: System MUST detect when the selected hardware output device is removed mid-playback, automatically switch the audio stream to the current Windows default output device, display a non-modal notification identifying which device was lost and which device is now in use, and automatically restore output to the preferred device when it reconnects. If the removed device IS itself the current Windows default output (no remaining fallback target), System MUST display a non-modal notification explaining that no output is currently available, MUST continue silent, and MUST automatically resume on either (a) the preferred device when it reconnects, or (b) any other output the user selects in the device selector
- **FR-025**: System MUST handle Windows sleep/wake cycles without requiring user intervention
- **FR-026**: System MUST continuously monitor CPU usage during audio processing; when CPU usage approaches or exceeds the 5% budget threshold, display a persistent non-modal warning in the UI ("CPU approaching limit; consider increasing buffer size"). Do not auto-increase buffer size or auto-bypass plugins; user retains full control over response

**Installation & Deployment**

- **FR-027**: Default (user-mode) installation MUST install a single tray application that hosts the audio engine in-process, auto-launches on user login, and runs in the system tray. No Windows Service is installed in this mode.
- **FR-028**: Installer MUST provide an optional advanced flag that additionally installs the audio engine as a Windows Service running at boot (enabling pre-login audio for lock-screen video, fast-startup gaming, etc.). In service-mode installs, the tray application is still installed and still auto-launches on user login, but it does NOT host the audio engine in-process; instead it acts as a UI client of the service.
- **FR-028a**: In service-mode installs, the tray application MUST communicate with the audio-engine service via a local-only IPC channel (e.g., named pipe or local RPC). The IPC channel MUST be restricted to the interactive user session on the same machine, MUST NOT expose any network-listening endpoint, and MUST authenticate the connecting user before exposing chain-edit or device-selection operations.
- **FR-028b**: User-mode and service-mode installs MUST be mutually exclusive on a given machine. Switching modes requires running the installer again; the installer MUST detect the existing mode and offer to convert (uninstall the service or install it) while preserving auto-saved chain state and saved presets.
- **FR-029**: Both installation modes MUST be independently testable and maintained; service mode is optional but must not introduce degradation in user-mode operation. The tray application's UI behavior MUST be identical in both modes (the user cannot tell from the UI whether the audio engine is in-process or in the service, except via an "About" / diagnostics surface).
- **FR-030**: The virtual audio device MUST be implemented as a WHQL-signed user-mode driver (modern WaveRT virtual endpoint or AVStream/APO model) packaged inside a single signed MSI/MSIX installer. Installation MUST require exactly one administrator-elevation prompt, MUST NOT require a system reboot to register the endpoint, and uninstall MUST fully remove the virtual device from Windows Sound settings
- **FR-031**: All shipped binaries (installer, driver, host application, helper service when service-mode is selected) MUST be signed with a valid Authenticode certificate; the driver MUST additionally carry Microsoft WHQL / attestation signing as required for Windows 10 1909+ and Windows 11 to load it without test-signing mode
- **FR-032**: The ASIO virtual driver MUST be built and shipped as a separate driver binary (e.g., `jyglobalvst_asio.dll`) installed alongside the WaveRT driver. It MUST implement the Steinberg ASIO 2.3 host API (or a compatible subset sufficient for major DAWs). It MUST register with Windows as an ASIO driver via the standard `HKLM\SOFTWARE\ASIO` registry key so that DAWs can enumerate it without custom configuration. The ASIO driver MUST NOT require a separate user-mode service process; it MUST communicate with the audio engine output buffer via a shared-memory ring buffer or equivalent zero-copy mechanism. The ASIO driver MUST support multiple concurrent ASIO clients (multi-client) when buffer parameters match, or MUST gracefully reject mismatched open attempts with a descriptive error code

### Key Entities

- **Virtual Audio Device**: A WASAPI-compatible audio endpoint registered with Windows; appears in Sound settings; captures all audio routed to it
- **Plugin**: A VST3 instance with associated metadata (name, vendor, format, file path), parameter state, bypass state, and GUI window. A Plugin slot may also exist in a **placeholder** state — chain position is held but the underlying VST3 file is unresolved (missing on disk); placeholders are audio-bypassed, visually distinct, and retain the stored state chunk so they can be reconstituted when the user re-points to the relocated file
- **Plugin Chain**: An ordered sequence of plugins; defines audio processing flow from input to output
- **Hardware Output Device**: A user-selected Windows audio output (DAC, speakers, headphones); receives processed audio from the chain. May be WASAPI or ASIO (FR-015a).
- **Hardware Input Device**: A user-selected Windows audio input (microphone, loopback, virtual cable); provides audio to the chain. In mixed mode (FR-015a), input may be WASAPI while output is ASIO.
- **Mixed-Driver Mode**: An internal engine state where ASIO is used for hardware output and WASAPI is used for capture input. Audio flows via a lock-free ring buffer between the WASAPI capture thread and the ASIO output callback.
- **Preset**: A persisted snapshot of a plugin chain configuration stored as an individual `.jvst` JSON file under `%UserProfile%\Documents\JyGlobalVST\Presets\` with a schema version, a list of plugins (path, format, vendor, identifier), chain order, per-plugin bypass state, and base64-encoded VST3 state chunks; user-named, shareable (drag-drop / file copy), and reloadable. Distinct from the auto-save state, which lives under `%LocalAppData%\JyGlobalVST\` and is machine-local
- **Audio Stream**: The continuous flow of audio samples from virtual device → chain → hardware output (and, when the ASIO driver is active, simultaneously to the ASIO virtual output); characterized by sample rate, buffer size, channel count
- **Latency Profile**: Current round-trip latency measurement, broken down by component (capture, processing, output)

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: New users can install JyGlobalVST, load their first VST plugin, and route system audio through it in under 2 minutes from first launch
- **SC-002**: Users replace at least three previously-used tools (Virtual Audio Cable, Voicemeeter, LightHost) with JyGlobalVST alone for their daily audio workflow
- **SC-003**: 90% of users successfully load their target VST plugin (e.g., ARC X) without consulting documentation
- **SC-004**: Users report acceptable audio quality (no perceptible dropouts) during typical mixed-use sessions of 4+ hours
- **SC-005**: Application memory footprint stays under 200 MB during normal operation with a typical plugin chain

### Audio-Specific Success Criteria *(required)*

- **AUDIO-001**: Round-trip latency measured and ≤ 10 ms (WASAPI capture → VST chain → hardware output) with default buffer size, single plugin
- **AUDIO-002**: CPU usage ≤ 5% on a modern multi-core system (Intel i5 8th gen / Ryzen 5 3000 series or newer) during normal playback with a typical 3-plugin chain
- **AUDIO-003**: Zero audio dropouts during 12-hour continuous playback test
- **AUDIO-004**: Graceful handling of sample rate mismatches between 44.1 kHz, 48 kHz, 96 kHz, 176.4 kHz, and 192 kHz — no audible glitches during transitions
- **AUDIO-005**: Worst-case round-trip latency under heavy plugin chain (5+ plugins) remains under 20 ms (gaming-acceptable)
- **AUDIO-006**: ASIO virtual output latency overhead (engine output buffer → ASIO callback) ≤ 1 ms at all supported buffer sizes. ASIO driver MUST report correct latency values via `ASIOGetLatencies`

## Assumptions

- Users are running Windows 10 (1909+) or Windows 11; older Windows versions out of scope
- Users have administrator rights for initial installation of the virtual audio device
- Users have at least one VST3 plugin installed in standard plugin directories (or know the path); VST2 plugins are not supported in v1
- WASAPI shared mode is sufficient for the default experience; exclusive mode is a later enhancement
- Stereo (2-channel) audio covers > 95% of target user audio sources; multichannel (5.1/7.1) is a future addition. Because JyGlobalVST advertises a stereo-only endpoint in v1, Windows and source applications perform any necessary downmix upstream — JyGlobalVST itself never receives more than 2 channels of input
- Users provide their own VST plugins; JyGlobalVST bundles no built-in DSP
- Modern multi-core CPU is assumed for performance targets; JyGlobalVST is not intended for very old hardware
- ASIO output with WASAPI input (mixed-driver mode) is supported in v1 (FR-015a). ASIO multi-client support (multiple applications simultaneously using the same ASIO driver) is out of scope. ASIO input paired with WASAPI output is not supported in v1
- MIDI input and MIDI parameter control are out of scope for v1. The host does not enumerate MIDI devices, does not route MIDI events into the VST3 chain, and offers no host-level MIDI-learn. Plugins that require MIDI input will load but receive no events. MIDI is a documented future enhancement; v1 architecture must remain compatible with adding it later (no contracts that would block per-plugin MIDI bus routing in a later release)
- Multiple simultaneous hardware outputs (fan-out / mirror routing) are out of scope for v1. The chain feeds exactly one Windows audio output endpoint at a time (FR-013). Users wanting dual-monitoring (e.g., speakers + headphones at once) must rely on OS-level tools. Multi-output is roadmapped; v1 architecture must remain compatible with adding it later (FR-013a)
- macOS and Linux are out of scope for v1
- Multi-language localization (translated UI strings) is out of scope for v1; the app ships English-only. Accessibility for blind/low-vision users via screen readers and full keyboard operability IS in scope for v1 (see FR-019a / FR-019b)
- Plugin scanning is user-initiated, not automatic on every launch (avoids long startup times)
- No persistent logging, no telemetry, no crash reports, no transmission of diagnostic or usage data to remote endpoints; errors and events surface only as in-session UI notifications. The single exception is a strictly user-initiated update check (FR-022n) which performs one HTTPS GET against a version-manifest endpoint when the user clicks "Check for updates…" and sends no identifying payload. Apart from that one explicit lookup, JyGlobalVST performs no outbound network activity

## Dependencies

- Windows audio subsystem (WASAPI shared mode at minimum)
- User-installed VST plugins (third-party; JyGlobalVST does not ship plugins)
- Driver signing requirements for Windows virtual audio device installation: JyGlobalVST targets a WHQL-signed user-mode virtual audio driver (WaveRT virtual endpoint or AVStream/APO) distributed in a signed MSI/MSIX. A valid Authenticode code-signing certificate (EV preferred for WHQL attestation) is a hard release prerequisite
- ASIO virtual driver requires Steinberg ASIO SDK 2.3 (or compatible headers) for driver-side ASIO host API implementation. Licensing: Steinberg ASIO SDK is provided under a proprietary license that permits redistribution of the driver binary; the SDK headers themselves are not redistributed in source form. Legal review required before shipping
- User-selected hardware output device must be functional and supported by Windows
