# Contract: Tray ↔ Service IPC Protocol

**Status**: v1 (service-mode installs only)
**Channel**: Windows Named Pipe
**Pipe name**: `\\.\pipe\JyGlobalVST\v1\<session_id>`
**Authentication**: ACL restricts to interactive user session that owns the SID; server verifies connecting process's session via `GetNamedPipeClientProcessId` → `OpenProcessToken` → `TokenSessionId` (FR-028a)
**Network exposure**: None — named pipes are not exposed on any network interface (FR-028a)

## Wire framing

Each message is length-prefixed JSON:

```
+----------------+------------------------------------------+
| length: u32 LE | UTF-8 JSON body (length bytes)           |
+----------------+------------------------------------------+
```

- `length` is little-endian, unsigned 32-bit, ≤ 4 MiB. Anything larger is a protocol violation and the server MUST close the pipe.
- Body is UTF-8 encoded JSON. Server MUST validate it parses before processing.

## Envelope shape

Every message is an envelope:

```jsonc
{
  "protocol_version": 1,
  "request_id": "550e8400-e29b-41d4-a716-446655440000",   // UUIDv4; client-assigned
  "command": "<command-name>",
  "payload": { /* command-specific */ }
}
```

Responses:

```jsonc
{
  "protocol_version": 1,
  "request_id": "550e8400-e29b-41d4-a716-446655440000",   // echoes request
  "command": "<command-name>",
  "result": { /* command-specific */ },                    // present on success
  "error": null
}
```

On error:

```jsonc
{
  "protocol_version": 1,
  "request_id": "550e8400-e29b-41d4-a716-446655440000",
  "command": "<command-name>",
  "result": null,
  "error": { "code": "<error-code>", "message": "<human-readable>" }
}
```

## Connection lifecycle

1. **Connect**: Tray opens the named pipe (`CreateFileW`). Server accepts.
2. **Negotiate**: Tray sends `hello` with its `protocol_version`. Server replies with the highest mutually supported version; v1 only for this release.
3. **Authenticate**: Server verifies the client's session ID matches the server's session ID (interactive user only). On mismatch, server replies `error.code = "auth.session_mismatch"` and closes.
4. **Operate**: Tray issues commands; server responds.
5. **Disconnect**: Either side closes the pipe. On unexpected disconnect, the audio engine continues running (the service does NOT depend on a connected UI to keep producing audio).

## Commands (v1)

### `hello`

Negotiates protocol version.

**Request**:
```jsonc
{ "command": "hello", "payload": { "client_protocol_version": 1 } }
```

**Response**:
```jsonc
{ "command": "hello", "result": { "server_protocol_version": 1, "session_id": 3 } }
```

### `chain.snapshot`

Returns the current chain state.

**Request payload**: empty `{}`.

**Response result**:
```jsonc
{
  "chain_revision": 42,
  "slots": [
    {
      "instance_id": "uuid",
      "kind": "plugin" | "placeholder",
      "position": 0,
      "is_bypassed": false,
      "is_failed": false,
      "plugin_uid": "...", "plugin_vendor": "...", "plugin_name": "...",
      "recorded_path": "..."   // only for placeholder slots
    }
  ]
}
```

### `chain.add`

Adds a plugin at a position.

**Payload**:
```jsonc
{
  "plugin_uid": "32-char hex",
  "plugin_vendor": "...",
  "plugin_name": "...",
  "position": 2
}
```

**Result**:
```jsonc
{ "chain_revision": 43, "instance_id": "uuid" }
```

**Errors**: `plugin.not_in_cache`, `chain.position_out_of_range`.

### `chain.remove`

**Payload**: `{ "position": 2 }`
**Result**: `{ "chain_revision": 44 }`

### `chain.move`

**Payload**: `{ "from_position": 2, "to_position": 0 }`
**Result**: `{ "chain_revision": 45 }`

### `chain.set_bypass`

**Payload**: `{ "position": 1, "is_bypassed": true }`
**Result**: `{ "chain_revision": 46 }`

### `chain.set_parameter`

**Payload**: `{ "position": 1, "parameter_id": 42, "value": 0.75 }`
**Result**: `{ "chain_revision": 47 }`

### `chain.repoint_placeholder`

Re-points a placeholder slot at a newly resolved plugin (FR-022f).

**Payload**:
```jsonc
{
  "position": 3,
  "plugin_uid": "...",
  "plugin_vendor": "...",
  "plugin_name": "..."
}
```

### `device.list_outputs`

**Result**:
```jsonc
{
  "outputs": [
    {
      "endpoint_id": "{0.0.0.00000000}.{...}",
      "friendly_name": "Speakers (Realtek)",
      "is_default": true,
      "is_present": true
    }
  ]
}
```

### `device.select_output`

**Payload**: `{ "endpoint_id": "..." }`
**Result**: `{ "negotiated_sample_rate": 48000, "negotiated_bit_depth": "Int24" }`

### `buffer.set_size`

**Payload**: `{ "buffer_size": 256 }`
**Result**: `{ "applied": true, "new_latency_ms": 8.3 }`

### `preset.load`

**Payload**: `{ "file_path": "C:\\Users\\...\\Documents\\JyGlobalVST\\Presets\\Gaming.jvst" }`
**Result**:
```jsonc
{
  "chain_revision": 48,
  "missing_plugins": [
    { "plugin_uid": "...", "vendor": "...", "name": "...", "recorded_path": "..." }
  ]
}
```

### `subscribe.meters`

Subscribes the client to a real-time meter stream. Server pushes meter frames at 30 Hz over the same pipe (asynchronous push; client must drain).

Meter frame payload:
```jsonc
{
  "command": "event.meter_frame",
  "payload": {
    "timestamp_us": 123456789,
    "input_peak_l": 0.42,
    "input_peak_r": 0.41,
    "output_peak_l": 0.39,
    "output_peak_r": 0.40,
    "cpu_pct": 3.2,
    "latency_ms": 9.1
  }
}
```

**Note**: 30 Hz × ~150 bytes ≈ 4.5 KB/s; well within pipe capacity. If profiling shows JSON overhead matters, a shared-memory ring is the migration path (see research.md §10 follow-up).

### `event.notification`

Server-pushed event (no `request_id`, no response expected). Used for: plugin failure (FR-023), device lost/restored (FR-024), CPU warning (FR-026), missing plugin on preset load (FR-022f).

```jsonc
{
  "command": "event.notification",
  "payload": {
    "severity": "info" | "warn" | "error",
    "code": "plugin.failed" | "device.lost" | "device.restored" | "cpu.warning" | "preset.partial_load",
    "message": "Plugin 'Pro-Q 3' failed and has been bypassed.",
    "context": { /* code-specific */ }
  }
}
```

## Error codes

| Code | Meaning |
|---|---|
| `auth.session_mismatch` | Connecting client is not in the server's interactive session |
| `protocol.version_mismatch` | Client requested an unsupported protocol version |
| `protocol.invalid_envelope` | Message did not parse as a valid envelope |
| `plugin.not_in_cache` | Referenced plugin is not in the scan cache |
| `chain.position_out_of_range` | Position is not in [0, len(chain)] |
| `device.not_found` | endpoint_id does not match any current device |
| `device.bind_failed` | WASAPI client could not bind to the device |
| `preset.invalid` | Preset file failed schema validation (FR-022g-1) |
| `preset.size_exceeded` | Preset file or state chunk exceeds size caps |
| `internal.audio_engine_error` | Unexpected engine error; non-modal UI notification sent |

## Versioning

- Protocol version is negotiated on `hello`. v1 is the only version in this release.
- Adding fields to existing commands is allowed; clients MUST ignore unknown fields.
- Removing or repurposing fields is a breaking change → new `protocol_version`.
- New commands are allowed without bumping `protocol_version`; older clients receive `protocol.unknown_command` and degrade gracefully.

## User-mode (in-process) parity

In user-mode installs, the tray app hosts the audio engine directly and does not use this IPC channel. The same command set is exposed via an in-process C++ interface defined in `audio-engine-api.md`; the IPC layer is a thin JSON adapter over that interface. UI code MUST NOT depend on whether the engine is in-process or remote (FR-029).
