# Phase 1 Data Model: Built-In Audio Effect Plugins

This feature adds **no new persisted files** and **no changes to existing shared structs** (`PluginCatalogEntry`, `PluginRef`, `ChainSlotSnapshot`, etc. in `src/audio-engine/include/jyglobalvst/types.h` are reused as-is). New entities are engine-internal.

---

## 1. Reserved built-in identities

Each effect gets a fixed 16-byte `PluginUid` from a stable ASCII seed. `vendor = "JyGlobalVST"`, `category = "Fx"`, `file_path = ""` (empty), `has_editor = true`.

| Effect | Display name | ASCII seed (16 bytes) | UID hex (32 chars, lowercase) |
|---|---|---|---|
| Night-time | `Night-time` | `JYGL-NIGHTTIME01` | `4a59474c2d4e49474854494d453031` **+ pad** → see note |
| EQ | `Equalizer` | `JYGL-EQ-BAND0010` | derived from ASCII bytes |

> **Note**: The authoritative constants are defined once in `builtin_ids.h` as the ASCII byte arrays; the hex form is whatever `PluginUidToHexString()` yields from those 16 bytes. Implementation must assert `seed.size() == 16` and that the two UIDs differ. (The hex sample above is illustrative — the header's ASCII array is the source of truth.)

These values MUST remain constant across releases so saved presets/autosave re-resolve. Any change is a breaking preset migration and is out of scope.

---

## 2. `BuiltinEffectDescriptor` (engine-internal)

Static description used to build a `PluginCatalogEntry` and to construct instances.

| Field | Type | Notes |
|---|---|---|
| `uid` | `PluginUid` | Reserved constant (see §1) |
| `name` | `std::string` | Display name |
| `category` | `std::string` | `"Fx"` |
| `factory` | `std::function<std::unique_ptr<juce::AudioPluginInstance>()>` | Constructs a fresh instance |

`BuiltinEffectRegistry` holds the two descriptors and exposes the contract in `contracts/builtin-effects-contract.md`.

---

## 3. Night-time processor — parameters & state

`NightTimeProcessor : juce::AudioPluginInstance`

| ParamId (index) | Name | Type / Range | Default | Meaning |
|---|---|---|---|---|
| 0 | `Preset` | choice {0 Light, 1 Medium, 2 Strong} | 1 Medium | Selects the normalization/limiter tuning table row |
| 1 | `LookAheadMs` | float 0.0 … 10.0 ms | 0.0 | Limiter look-ahead; 0 ⇒ zero added latency |

Enable/disable is the chain's existing per-slot **bypass** (no dedicated parameter).

**Preset tuning table** (concrete values chosen in implementation; shape fixed here):

| Preset | Target loudness | Max upward gain | Attack / Release | Output ceiling |
|---|---|---|---|---|
| Light | higher (gentler) | small | slower | just-below 0 dBFS |
| Medium | mid | moderate | moderate | just-below 0 dBFS |
| Strong | lower (very even) | large | faster pull-down | just-below 0 dBFS |

**State chunk** (`getStateInformation`): `{ "v":1, "preset":<int>, "lookAheadMs":<float> }` (JSON, then written to the `MemoryBlock`). `setStateInformation` tolerates missing keys → defaults.

**Runtime state (preallocated in `prepareToPlay`, not serialized)**: K-weighting biquad coefficients (per current sample rate), short-term loudness ring buffer, current smoothed gain, limiter look-ahead delay line (sized to max 10 ms × channels), limiter envelope.

**Reported latency**: `getLatencySamples()` = `round(lookAheadMs × sampleRate / 1000)`.

---

## 4. EQ processor — parameters & state

`EqProcessor : juce::AudioPluginInstance`

| ParamId (index) | Name | Type / Range | Default | Meaning |
|---|---|---|---|---|
| 0…9 | `BandGain[0..9]` | float −12.0 … +12.0 dB | 0.0 | Peaking-filter gain at fixed center freq for band i |
| 10 | `BassBoost` | float 0.0 … +12.0 dB (or normalized 0..1) | 0.0 | Low-shelf boost amount, independent of bands |

Fixed band centers (Hz), band 0 → 9: **32, 64, 125, 250, 500, 1000, 2000, 4000, 8000, 16000** (final values may be tuned; count = 10 is fixed by FR-013/FR-017).

**Flat/reset action** (FR-015): sets ParamId 0…10 to their defaults (all 0). Exposed as a button in the editor (writes each parameter through the normal path).

**State chunk**: `{ "v":1, "bands":[10 floats dB], "bass":<float> }`.

**Runtime state (preallocated)**: 10 peaking biquad coefficient sets + 1 low-shelf set (per channel, per current sample rate), recomputed in place on parameter change; optional output safety-ceiling state (FR-016). Latency = 0.

---

## 5. Chain / catalog / preset integration (existing structs, new behavior)

- **`PluginCatalogEntry`** — two synthesized entries prepended to `catalog()` output; `file_path` empty distinguishes built-ins programmatically, and (FR-003) the UI may show a "Built-in" badge when `vendor == "JyGlobalVST"` and `file_path` is empty.
- **`ChainSlotSnapshot`** — unchanged; a built-in slot reports its reserved `ref` and (empty) `file_path`. This is sufficient for the UI to detect built-ins for badging.
- **Preset/autosave slot record** — unchanged schema: `{ plugin_uid, vendor, name, is_bypassed, state_chunk_b64 }`. For built-ins, `state_chunk_b64` carries the JSON state from §3/§4.

## 6. Resolution precedence (load path)

On `addPlugin(ref)` and on preset/autosave resolve, resolution order is:

1. `BuiltinEffectRegistry::findByRef(ref)` → if matched, construct via factory (no disk access).
2. Else existing `scan_cache_->findByRef(ref)` → `plugin_loader_->load(file_path)`.
3. Else placeholder (unchanged behavior; onPresetPartialLoad for scanned misses).

Built-ins therefore **never** become placeholders — they are always resolvable (FR-002).
