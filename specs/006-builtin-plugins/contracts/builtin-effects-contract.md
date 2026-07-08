# Contract: Built-In Effects Registry & Integration

This feature adds no new **external/IPC** surface. The `IAudioEngine` contract (`src/audio-engine/include/jyglobalvst/audio_engine.h`) is unchanged. This document specifies the new **engine-internal** contract and the behavioral guarantees the existing public methods must uphold for built-in effects.

---

## 1. `BuiltinEffectRegistry` (new, engine-internal)

```cpp
namespace jyglobalvst::engine {

class BuiltinEffectRegistry
{
public:
    // Catalog entries for all built-in effects (reserved UID, vendor "JyGlobalVST",
    // empty file_path, has_editor = true). Stable order.
    std::vector<PluginCatalogEntry> entries() const;

    // True if the ref (by reserved UID; fallback vendor+name) is a built-in.
    bool isBuiltin(const PluginRef& ref) const;

    // Construct a fresh instance for a built-in ref, or nullptr if not a built-in.
    // Never touches disk. Returns an AudioPluginInstance so it slots into
    // PluginInstance/PluginChain exactly like a loaded VST3.
    std::unique_ptr<juce::AudioPluginInstance> create(const PluginRef& ref) const;
};

}  // namespace jyglobalvst::engine
```

**Guarantees**
- `entries()` returns exactly the built-ins (2 for v1), deterministic order, no I/O.
- `isBuiltin` / `create` match on reserved `PluginUid` first, then `(vendor,name)`.
- `create` is pure/allocating-on-control-thread only; the returned instance performs **no** allocation in `processBlock`.

---

## 2. Behavioral contract on existing `IAudioEngine` methods

| Method | Added guarantee for built-ins |
|---|---|
| `catalog()` | Result = `registry.entries()` followed by `scan_cache_->plugins()`. Built-ins always present, even with an empty/absent scan cache (FR-002). |
| `addPlugin(ref,pos)` | If `registry.isBuiltin(ref)`, instance comes from `registry.create(ref)` (no `plugin_loader_`), then normal `addSlot`. Never yields a placeholder for a built-in. |
| `setParameter(pos,param,val)` | Unchanged path; `param` is the built-in's parameter index (data-model §3/§4). |
| `openEditor(pos)` / `closeEditor(pos)` | Unchanged path; built-in `createEditor()` returns its custom editor, wrapped by `PluginEditorWindow`. Settings appear only in this window (FR-004a). |
| `savePreset` / `loadPreset` / `restoreChain` | Slot serialized/deserialized with reserved `plugin_uid` + base64 state chunk. On load, the resolve callback consults `registry` before the scan cache (data-model §6). |
| `setBypass`, `moveSlot`, `removeSlot`, `snapshotChain` | Fully unchanged — built-in slots behave identically to scanned slots (FR-004). |

---

## 3. Per-effect `AudioProcessor` obligations (RT + editor contract)

Both `NightTimeProcessor` and `EqProcessor` MUST:

- Subclass `juce::AudioPluginInstance`; implement `fillInPluginDescription` with the reserved UID/name/vendor.
- Allocate **all** DSP state in `prepareToPlay` (coefficients, delay lines, ring buffers) sized to the given sample rate / block size / channel count; **never** allocate, lock, log, or do I/O in `processBlock`.
- Recompute biquad coefficients **in place** into preallocated storage when a parameter changes (no reference-counted coefficient allocation on the audio thread).
- Expose parameters by index per data-model §3/§4; honor `get/setStateInformation` with the documented JSON schema (version-tolerant).
- Provide `createEditor()` returning a custom editor (preset selector + look-ahead for Night-time; 10 band sliders + bass amount + flat/reset for EQ). `hasEditor()` = true.
- Night-time: report `getLatencySamples()` matching the active look-ahead; EQ: 0.
- Tolerate `processBlock` being called with the slot bypassed/failed via the existing chain logic (chain skips bypassed/failed slots; no special handling required inside the processor).

---

## 4. Contract test obligations

- **Registry**: `entries()` contains both built-ins with reserved UIDs and empty `file_path`; `isBuiltin`/`create` resolve by UID; UIDs are distinct and stable (golden hex).
- **Catalog precedence**: `catalog()` lists built-ins even when the scan cache is empty.
- **Add/never-placeholder**: `addPlugin(builtinRef,pos)` returns a non-null `InstanceId` with no scan cache present.
- **Preset round-trip**: save → new engine → load re-resolves both built-ins with identical parameter state (state-chunk equality after `setStateInformation`).
- **RT-safety**: audit that `processBlock` of each effect contains no allocation/lock/IO/log (T106/T107-style static check + review).
