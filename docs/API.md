# CYDOS Lua API 1

For the additive 0.4.0 platform APIs and their quotas, read [LUA-API-0.4.md](LUA-API-0.4.md)
and [SECURITY-0.4.md](SECURITY-0.4.md). The core contract below remains unless
explicitly extended there.

## Package contract

A `.cydapp` is a ZIP_STORED archive, with `manifest.json` and the entry script at
its root. Use `cydctl pack`, which writes deterministic timestamps and regular-file
attributes. Archive comments, compression, encryption, data descriptors, ZIP64,
symlinks, device/directory entries, duplicate paths and file/directory collisions
are rejected. Local headers, central records, offsets, lengths and CRC32 must
agree. Paths contain lowercase ASCII letters, digits, `_`, `-`, `.`, and `/`.
Segments must not start or end with a dot. No empty, absolute, parent, backslash,
colon, NUL or uppercase paths are accepted (avoids FAT case aliases).

Limits: 512 KiB uploaded archive, 48 files, 64 KiB each, path under 96 bytes,
segment up to 64 bytes, at most four directory levels, manifest up to 4096
bytes and eight JSON nesting levels. `package.cydapp` is reserved for
the retained original upload. The entry point must exist and be nonempty.

Required manifest fields:

```json
{
  "package_id": "com.local.hello",
  "name": "Hello CYD",
  "version": "0.1.0",
  "api_version": 1,
  "entry_point": "main.lua",
  "target": ["esp32s3"],
  "permissions": ["storage"]
}
```

`package_id` is a lowercase reverse-domain string up to 64 bytes. Name is up to
64 UTF-8 bytes, version up to 32. Version is a display string, not a sortable
semantic-version constraint. The current device must appear in `target`.
Permissions are `storage`, `network`, `mqtt`, `credentials` and `documents` (an empty list is allowed). Unknown
permissions fail installation. Duplicate manifest fields fail validation.
The `documents` permission requires C5 0.4.4c or S3 0.4.3s and grants only the
shared Documents area; see [OFFLINE-ESSENTIALS.md](OFFLINE-ESSENTIALS.md).
An optional `icon` is a safe package-relative path; rendering custom icons is a
future feature. Additional metadata fields are retained but unused.

## Lua contract

The entry point runs once, creates its widgets, and returns. The runtime then
waits for events. Do not put a permanent polling loop in the entry point.
One foreground app is supported; starting another waits for the old app to stop.
No suspension or background work is exposed.

- `cyd.ui.create_screen()` returns a handle for the OS-owned application container.
  Multiple calls return that same container, not independently navigable screens.
- `cyd.ui.size()` returns the usable application-container width and height in
  pixels. Use it to adapt an app to the CrowPanel or smaller C5 screen.
- `screen:add_label{ text=..., x=20, y=20 }` creates a wrapping label, returns a
  widget handle. `label:set_text(text)` changes a label. Text is limited to 1024
  bytes, with no embedded NUL. The default label width adapts to the board’s display width.
- `screen:add_button{ text=..., x=20, y=80, on_click=function() ... end }` creates
  a 180×52 button. Callbacks run serially on the Lua task. Widget `set_text` changes
  labels only; button text mutation is not exposed in this MVP.
- `cyd.ui.show(screen)` validates the handle. Starting an app shows its container
  automatically; this call does not replace the shell root or Home control.
- `cyd.storage.write(basename, bytes)` and `.read(basename)` require `storage`.
  Paths are lowercase basenames only; per-file data is capped at 16 KiB. Read
  returns bytes or `nil` on absence/read failure. Writes raise an error on failure.
  Data lives in `/sdcard/cydos/data/<package_id>` across revisions and reboots.
  Writes flush and sync; application data writes are not transactional.
- `cyd.log.info(text)` and `print(...)` enter the device’s bounded log ring.
- `cyd.time.uptime_ms()` returns monotonic uptime milliseconds as a Lua integer. It is not wall-clock time.

Base/string/table/math libraries are loaded. `io`, `os`, `package`, `debug`,
coroutines, `load`, `loadfile`, `dofile`, `collectgarbage`, `pcall` and `xpcall` are
not exposed. Protected calls are unavailable so ordinary Lua cannot swallow
cancellation/budget errors indefinitely. Only textual Lua source is loaded.
UI objects are capped at 96 user widgets; buttons also allocate one child label.
The Lua allocator is capped at 1 MiB. Every 1000 Lua instructions, a hook checks
cancellation and a 500 ms wall-clock execution budget, and yields one RTOS tick.
Each entry point/callback gets a fresh budget. Callbacks share eight slots, with two reserved from ordinary traffic for network
completion/state. Excess clicks/messages may be dropped; input changes and timer
repetitions coalesce. No scripts run on the LVGL task.

Application finalizers are disabled and metatables protected; C-call nesting is
capped at 32. An entry-point return is followed by `running`. Entry or callback errors produce
`crashed` and release widgets, handlers, registry references and Lua allocations.
Home requests stop; normal stop waits up to 2.5 seconds. A timed-out stop prevents
another app starting. Hooks cannot interrupt arbitrary C or blocked filesystem
operations; this is not a secure process sandbox or a hard real-time guarantee.

## HTTP contract

Every `/api/*` route requires `Authorization: Bearer <configured-token>`.
The dashboard HTML is public but contains no credentials/device data.
There is no cookie auth, token URL parameter, CORS policy, or unauthenticated API.
Use HTTP only on your trusted protected local network; TLS is not implemented
on the device. Status, logs, stored source packages and downloads require auth too.

| Method | Path | Result |
|---|---|---|
| GET | `/api/status` | model, target, firmware, IP, Wi-Fi, SD, heap, PSRAM, free storage, app lifecycle/error, credential-free network status |
| GET | `/api/versions` | array of revision, package_id, name, version |
| POST | `/api/packages` | raw `.cydapp`, 201 `{revision,status:"installed"}` |
| POST | `/api/run?revision=<id>` | 202 `{status:"starting"}`; poll status to confirm |
| POST | `/api/stop` | 200 lifecycle snapshot, or 503 if stop times out |
| DELETE | `/api/versions?revision=<id>` | delete inactive revision; running/starting revision returns 409 |
| GET | `/api/download?revision=<id>` | original stored ZIP bytes |
| GET | `/api/logs` | array of up to 64 bounded log lines with uptime/context |

Errors return JSON `{error:"..."}`, with 400 validation, 401 auth, 404 absent
revision, 409 state conflict, or 503 timeout. A successful upload means installed,
not running; a 202 run response means scheduled, not successful startup. `cydctl`
checks these distinctions and uses JSON/nonzero exit status for failures.

Revision IDs are random 16-character lowercase hex strings. The project directory
is keyed by revision rather than display name, preventing metadata-to-path
injection. All versions are listed by package ID/name; ordering is unspecified.
Deleting a revision never deletes `/data/<package_id>`. There is no pruning or
automatic launch/rollback. Keep a known-good revision and run it explicitly.


## Connection manager status (0.2)

`/api/status` now also includes `network`: `ready`, `scanning`, `state`,
`station_ssid`, `station_ip`, `ap_enabled`, `ap_ssid`, `ap_ip`, `message` and
`saved_count`. It does not expose any password or deployment token. Network
configuration is a physical touchscreen operation, not a new remote-write API.
The Lua API/package version remains 1; existing apps and deployment commands work
unchanged. See CONNECTIONS.md for NVS migration and AP/phone-hotspot usage.

## OS, device settings and app folders (0.3.0)

All routes below require `Authorization: Bearer <device token>`. Tokens are stored
in NVS and shown/rotated only through the physical device UI. JSON mutation bodies
are bounded to 768 bytes/eight nesting levels. Credential fields are never returned
in status JSON. Settings mutations enqueue work; poll GET to observe completion.

| Route | Body / result |
| --- | --- |
| `GET /api/projects` | `{folders:[{id,name}],projects:[{package_id,name,folder,versions:[...]}],catalog_error?}`; empty `folder` means root |
| `POST /api/projects` | `{"action":"create","name":"Tools"}`; or `rename` + `folder,name`; `delete` + `folder`; `move` + `package_id,folder` (empty folder returns to root). Returns catalog. |
| `GET /api/device` | brightness, rotation (0–3), UTC offset minutes, clock validity/text, display mode, remaining preview seconds, message |
| `POST /api/device` | `brightness` + integer `value` (10–100), `rotation` + integer `value` (0–3), `confirm`, `revert`, `reset_display`, or `clock` + integer `epoch,offset` (-720…840 minutes). Returns 202; poll for worker result. |
| `GET /api/os` | OS state, running/candidate versions, board, slots, received/total, startup confirmation, rollback availability, ESP image SHA-256, message and image size limit |
| `POST /api/os/update` | Raw application `.bin`, Content-Length required, 512 bytes…7 MiB. Streams/validates inactive slot; returns 201 `ready`; does not select it. |
| `POST /api/os/reboot` | No body; select the verified staged OS and restart asynchronously; 202 |
| `POST /api/os/cancel` | No body; discard staged selection intent, release maintenance; current OS stays selected |
| `POST /api/os/rollback` | No body; restart into a compatible previously confirmed OS, when available; 202 |

OS states are `booting`, `idle`, `uploading`, `verifying`, `ready`, `rebooting`,
`failed`. Display modes are 0 normal, 1 orientation preview, 2 calibration targets,
3 calibration preview. Start calibration on the physical screen. Keep/revert works
from either interface; previews time out independently of the browser. Firmware
uploads close a failed request connection to discard unread binary data. A response
lost during restart is ambiguous: reconnect and check GET status. See [OTA.md](OTA.md).

The legacy `/api/versions`, `/api/packages`, run/stop/download/delete endpoints
keep their existing contracts. Filesystem paths are internal; folder IDs are logical
metadata and are never interpolated as user-supplied filesystem paths.
