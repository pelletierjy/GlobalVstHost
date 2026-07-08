# Data Model: JyGlobalVST (System Host)

**Branch**: `001-jyglobalvst-system-host` | **Date**: 2026-06-04

This document defines the entities, their fields, relationships, validation rules, and state transitions. The wire / on-disk serializations are formalized in `contracts/*.json` (JSON Schemas); this file is the conceptual model.

---

## Entity overview

```
Settings ─────────────► PluginScanPaths (list)
   │                          │
   │                          ▼
   │                    PluginScanCache ────► Plugin (catalog entry)
   │                                                │
   ▼                                                ▼
HardwareOutputDevice ◄────── PluginChain ◄──── PluginInstance ──► PluginState (opaque blob)
        ▲                          │
        │                          ▼
        └────────── AudioStream ◄── AudioEngine
                                        │
                                        ▼
                                  LatencyProfile
                                        +
                                  CPUMonitor
```

Persistence destinations (Windows-only paths):
- Roaming: `%AppData%\Roaming\JyGlobalVST\settings.json`
- Local-machine: `%LocalAppData%\JyGlobalVST\{scan-cache.json, autosave.json, window-state.json, endpoint-last.json}`
- User documents: `%UserProfile%\Documents\JyGlobalVST\Presets\*.jvst`

---

## 1. VirtualAudioDevice

The WASAPI render endpoint that JyGlobalVST registers with Windows.

**Fields**:
| Field | Type | Notes |
|---|---|---|
| `endpoint_id` | string (Windows endpoint ID) | Assigned by Windows at driver installation |
| `friendly_name` | string | "JyGlobalVST Virtual Output" (FR-001) |
| `channel_count` | int | Fixed at 2 (stereo-only v1, FR-003) |
| `supported_sample_rates` | int[] | {44100, 48000, 96000, 176400, 192000} (FR-002) |
| `supported_bit_depths` | enum[] | {Int16, Int24, Float32} (FR-002a) |
| `state` | enum | `Active`, `Disabled`, `NotPresent` |

**Validation**:
- `channel_count` MUST equal 2; any other value is a v1 violation (FR-003).
- `endpoint_id` is opaque to JyGlobalVST and never parsed.

**State transitions**:
- `NotPresent → Active`: driver installed and Windows audio service has loaded it.
- `Active → Disabled`: user disabled in Windows Sound settings; the audio engine MUST stop gracefully (no error popup; FR-024).
- `Active → NotPresent`: driver uninstalled.

---

## 2. HardwareOutputDevice

A WASAPI render endpoint owned by physical hardware (DAC, speakers, headphones).

**Fields**:
| Field | Type | Notes |
|---|---|---|
| `endpoint_id` | string | Windows endpoint ID; used for exact match across boots |
| `friendly_name` | string | Used as roaming-friendly identifier (FR-022k) |
| `is_default` | bool | Cached snapshot of Windows default-output flag |
| `is_present` | bool | False after device removal (FR-024) |
| `negotiated_sample_rate` | int | Set when the engine binds to this device |
| `negotiated_bit_depth` | enum | Set when the engine binds |
| `resolution_source` | enum | `EndpointIdMatch`, `FriendlyNameMatch`, `WindowsDefaultFallback` — exposed in device-selector tooltip (FR-022m) |

**State transitions**:
- `Selected → Bound`: engine successfully opens an `IAudioClient3` on this endpoint.
- `Bound → Lost`: Windows raises endpoint removal; engine falls back to current Windows default and notifies the user (FR-024).
- `Lost → Bound`: endpoint reappears (e.g., USB reconnect); engine automatically restores it.

---

## 3. Plugin (catalog entry)

A VST3 plugin discovered during scanning. One per `.vst3` bundle.

**Fields**:
| Field | Type | Notes |
|---|---|---|
| `plugin_uid` | byte[16] | VST3 class UID (Steinberg `TUID`) — primary identifier |
| `name` | string | Plugin display name |
| `vendor` | string | Plugin vendor string |
| `version` | string | Plugin version reported by `getControllerClassId`'s metadata |
| `file_path` | string | Absolute path to the `.vst3` bundle on disk |
| `category` | string | VST3 category (e.g., `Fx|EQ`, `Fx|Dynamics`) |
| `supports_double_precision` | bool | Plugin declares double-precision processing capability |
| `has_editor` | bool | Plugin exposes a native GUI |
| `scan_timestamp` | datetime | When this entry was last validated |
| `is_blocklisted_by_user` | bool | User disabled the plugin from the chooser |

**Validation**:
- `(plugin_uid, vendor, name)` is the resolution tuple for preset re-pointing (FR-022g-2). All three MUST match for a preset slot to bind to a scanned plugin.
- `file_path` is treated as a hint only; the runtime never loads a plugin binary by path alone.

---

## 4. PluginInstance

A live instance of a plugin within a chain. Distinct from the catalog entry: one Plugin → 0..N PluginInstance.

**Fields**:
| Field | Type | Notes |
|---|---|---|
| `instance_id` | uuid | Unique within the running chain |
| `plugin_ref` | Plugin (FK by uid+vendor+name) | The catalog entry this instance was instantiated from |
| `position` | int | 0-based order within the chain |
| `is_bypassed` | bool | Default false |
| `is_failed` | bool | Set true if SEH/C++ exception caught (FR-023); audio bypasses; visible in UI |
| `editor_open` | bool | Whether the plugin's native GUI window is currently shown |
| `last_known_parameters` | map<paramId, float> | Snapshot for re-instantiation after `is_failed` reset |
| `state_chunk` | binary blob | Opaque VST3 state from `getStateInformation` — never introspected |

**Validation**:
- `position` is unique within a `PluginChain` and contiguous (0..N-1) after any mutation.
- `state_chunk` size MUST NOT exceed 16 MB per FR-022g-1.

**State transitions**:
- `Loading → Active`: instance constructed and prepared with current `AudioStream` parameters.
- `Active → Bypassed`: user toggled bypass (audio still flows, processing skipped).
- `Active → Failed`: SEH or C++ exception caught during `processBlock` (FR-023). Stays in chain at same position; audio bypasses.
- `Failed → Active`: user explicitly re-enables the plugin (UI action; engine attempts re-prepare with `last_known_parameters` and `state_chunk`).
- `Active → Removed`: user removed plugin from chain; instance destroyed.

---

## 5. PlaceholderInstance

A chain slot that points to an unresolved plugin (FR-022f).

**Fields**:
| Field | Type | Notes |
|---|---|---|
| `instance_id` | uuid | Unique within the running chain |
| `recorded_plugin_uid` | byte[16] | Identifier the preset referenced |
| `recorded_vendor` | string | Vendor the preset referenced |
| `recorded_name` | string | Plugin name the preset referenced |
| `recorded_path` | string | Path the preset hinted (display only) |
| `position` | int | 0-based order within the chain (preserved) |
| `is_bypassed` | bool | Carried over from preset |
| `pending_state_chunk` | binary blob | Held for re-instantiation when user re-points (FR-022f) |

**Validation**:
- Placeholder slots are audio-transparent (chain skips them).
- Visually greyed out in UI; preserve chain depth indicator count.

**State transitions**:
- `Placeholder → PluginInstance`: user re-points to a scanned plugin; engine instantiates it and applies `pending_state_chunk`.
- `Placeholder → Removed`: user removes the slot.

---

## 6. PluginChain

The ordered processing graph.

**Fields**:
| Field | Type | Notes |
|---|---|---|
| `slots` | (PluginInstance \| PlaceholderInstance)[] | Ordered list |
| `chain_revision` | int (monotonic) | Bumped on any structural mutation; UI uses for diffing |

**Invariants**:
- `slots[i].position == i` after any mutation.
- An empty chain is valid and audio MUST pass through transparently with minimum added latency (FR-008 + edge case "No VST plugins loaded").

**Operations** (all serialize through the lock-free SPSC command queue described in research.md §6):
- `addPlugin(pluginRef, position)` — inserts at position; shifts subsequent slots
- `removeSlot(position)` — removes; shifts subsequent slots
- `move(fromPos, toPos)` — reorders; audio MUST NOT drop out (FR-010)
- `setBypass(position, bool)` — toggles bypass
- `setParameter(position, paramId, value)` — single-parameter update

---

## 7. Preset

A persisted, user-named snapshot of a `PluginChain` plus device/buffer state.

**Fields**:
| Field | Type | Notes |
|---|---|---|
| `schema_version` | int | Currently 1; increment on incompatible change |
| `preset_name` | string | User-supplied; used as default file name |
| `created_at` | datetime | ISO 8601 UTC |
| `updated_at` | datetime | ISO 8601 UTC |
| `target_sample_rate` | int (nullable) | Preferred sample rate at save time; advisory only |
| `target_buffer_size` | int | One of {32, 64, 128, 256, 512, 1024} |
| `target_device_friendly_name` | string (nullable) | Advisory; resolved per FR-022m on load |
| `slots` | PresetSlot[] | See below |

**PresetSlot**:
| Field | Type | Notes |
|---|---|---|
| `position` | int | 0-based |
| `plugin_uid` | byte[16] (hex string in JSON) | Resolution key |
| `plugin_vendor` | string | Resolution key |
| `plugin_name` | string | Resolution key |
| `plugin_path_hint` | string | Display-only hint; NEVER used to load the binary (FR-022g-2) |
| `is_bypassed` | bool | |
| `state_chunk_b64` | string | base64-encoded VST3 state; ≤ 16 MB pre-encoding |

**Validation** (enforced on import per FR-022g-1):
- File size ≤ 50 MB
- Per-`state_chunk_b64` decoded size ≤ 16 MB
- `schema_version` ∈ known set OR migratable (FR-022b)
- No unknown top-level fields (strict on import; tolerant on round-trip save of in-app-edited presets)
- `slots[i].position == i`

**State transitions** (on load):
- All slots resolve → chain is fully populated, audio flows normally.
- Some slots fail to resolve → those become `PlaceholderInstance`; non-modal notification listing missing plugins (FR-022f).

---

## 8. AutoSaveState

Machine-local, ephemeral snapshot persisted on app close.

**Fields**: Same shape as `Preset` minus `preset_name`, `created_at`, `updated_at`. Lives at `%LocalAppData%\JyGlobalVST\autosave.json`. Not visible in the user's preset list (FR-022c).

**Behavior**:
- Written on app close (FR-022c).
- Read on app launch unless corrupted (FR-022d) — corruption silently discards and starts blank.
- The user can override the auto-save by explicitly loading a saved preset before closing (FR-022e); the engine marks an in-memory flag that suppresses the auto-save write at exit.
- No crash detection (FR-022d, clarification #22): no sentinel file, no exit-cause record.

---

## 9. Settings (roaming)

User-portable preferences. Lives at `%AppData%\Roaming\JyGlobalVST\settings.json` (FR-022k).

**Fields**:
| Field | Type | Notes |
|---|---|---|
| `schema_version` | int | Currently 1 |
| `custom_scan_paths` | string[] | Additional directories the user added beyond the default two (FR-005) |
| `disabled_default_paths` | string[] | If user disabled `%ProgramFiles%\Common Files\VST3` or the AppData one |
| `default_buffer_size` | int | One of {32, 64, 128, 256, 512, 1024} |
| `theme` | string | "light" \| "dark" \| "system" |
| `default_hardware_device_friendly_name` | string (nullable) | Used as resolution fallback per FR-022m step 2 |
| `update_check_endpoint_url` | string | Configurable; default points at project's manifest URL |

**Validation**:
- Unknown top-level fields are preserved on save (forward-compatible).
- Invalid `default_buffer_size` falls back to 256 with a non-modal notification.

---

## 10. LocalState (machine-local)

Multiple files under `%LocalAppData%\JyGlobalVST\`. Each is independent so partial corruption of one does not lose the others.

| File | Schema | Notes |
|---|---|---|
| `scan-cache.json` | PluginScanCache | Catalog of discovered plugins; rebuilt on user rescan (FR-005) |
| `autosave.json` | AutoSaveState | See entity 8 |
| `window-state.json` | WindowState (position, size, maximized) | FR-022l |
| `endpoint-last.json` | EndpointSnapshot (endpoint_id, friendly_name, last_bound_at) | FR-022l, used as resolution priority 1 per FR-022m |

**Validation**:
- Each file MUST tolerate absence (first launch).
- Each file MUST tolerate corruption (silently discard, log a non-modal notification, continue with defaults).

---

## 11. AudioStream

The runtime audio flow descriptor. Not persisted; constructed each session.

**Fields**:
| Field | Type | Notes |
|---|---|---|
| `sample_rate` | int | Negotiated with the hardware output endpoint |
| `buffer_size` | int | Current user-selected buffer size |
| `channel_count` | int | 2 (stereo-only v1, FR-003) |
| `internal_format` | enum | Always `Float32` |
| `source_format` | enum | What the virtual endpoint received (Int16, Int24, Float32) |
| `destination_format` | enum | What the hardware endpoint negotiated |
| `is_active` | bool | True while audio is flowing |
| `output_transport` | enum | `Wasapi` or `Asio` — the driver type bound to hardware output |
| `input_transport` | enum | `Wasapi` or `Asio` — the driver type bound to capture input |
| `mixed_mode` | bool | True when `output_transport == Asio` and `input_transport == Wasapi` (FR-015a); input is captured via direct `IAudioClient` thread feeding a lock-free ring buffer consumed by the ASIO callback |

---

## 12. LatencyProfile

Computed per second; shown in UI (FR-019).

**Fields**:
| Field | Type | Notes |
|---|---|---|
| `capture_ms` | float | Virtual-device capture latency component |
| `resample_ms` | float | Format / sample-rate conversion component |
| `plugin_chain_ms` | float | Sum of plugin processing time across chain |
| `output_ms` | float | Hardware output buffer component |
| `total_round_trip_ms` | float | Sum; gates AUDIO-001 (≤ 10 ms) and AUDIO-005 (≤ 20 ms heavy) |
| `last_updated` | timestamp | |

---

## 13. CPUMonitor

Real-time CPU usage of the audio thread (FR-026, AUDIO-002).

**Fields**:
| Field | Type | Notes |
|---|---|---|
| `instantaneous_pct` | float | Most recent buffer's wall-clock / wall-clock-budget × 100 |
| `rolling_1s_pct` | float | 1-second average; this is what's compared to the 5% threshold |
| `xrun_count_session` | int | Count of buffer underruns since session start |
| `warning_active` | bool | True when `rolling_1s_pct ≥ 5%` — UI shows persistent warning (FR-026) |

---

## 14. IpcEnvelope (service-mode only)

Wire framing for tray ↔ service named-pipe messages. Detailed in `contracts/ipc-protocol.md`.

**Fields** (per message):
| Field | Type | Notes |
|---|---|---|
| `protocol_version` | int | Negotiated at connect; v1 = 1 |
| `request_id` | uuid | Set by client; server echoes for response correlation |
| `command` | string enum | e.g., `chain.add`, `chain.remove`, `chain.bypass`, `device.select`, `subscribe.meters` |
| `payload` | object | Command-specific |
| `error` | nullable object | Set on response if the command failed; `{ code, message }` |

---

## Relationships summary

- **Settings 1..* PluginScanPath**: user-configurable scan paths feed the scanner.
- **PluginScanCache 1..* Plugin**: catalog of all known plugins.
- **PluginChain 1..* (PluginInstance | PlaceholderInstance)**: ordered slots.
- **PluginInstance N..1 Plugin**: each instance points back to a catalog entry by `(uid, vendor, name)`.
- **Preset 1..* PresetSlot**: on load, each `PresetSlot` resolves to either a `PluginInstance` (plugin found) or `PlaceholderInstance` (plugin missing) per FR-022f / FR-022g-2.
- **AudioStream 1..1 PluginChain**: the active stream binds to the current chain.
- **AudioStream 1..1 HardwareOutputDevice**: the active stream outputs to one device (FR-013).
- **AudioStream 1..1 VirtualAudioDevice**: the active stream captures from the one virtual endpoint.

---

## State machine — application lifecycle

```
[Cold Start]
   │
   ▼
[Load Settings] ──(corrupt? → defaults + notification)
   │
   ▼
[Load LocalState] ──(missing? → first-launch defaults)
   │
   ▼
[Resolve Hardware Output] (per FR-022m priority chain)
   │
   ▼
[Load Plugin Scan Cache] ──(missing? → trigger scan)
   │
   ▼
[Read AutoSaveState] ──(corrupt? → blank chain)
   │
   ▼
[Bind Audio Engine] ──(failure? → notification, retry with default device)
   │
   ▼
[Running] ◄────────── parameter changes / chain mutations / preset loads
   │
   │ ──[User closes app]──► [Write AutoSaveState] ──► [Persist roaming + local] ──► [Exit]
   │
   │ ──[Hardware lost]──► [Fall back to Windows default] ──► [Running]
   │                          (waits for reconnect; auto-restores)
   │
   ▼
[Exit]
```

No quarantine state, no crash-detection branch (per clarifications #12, #22). The next cold start treats every previous exit identically.
