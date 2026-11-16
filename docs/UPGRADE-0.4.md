# Install and roll back CYDOS 0.4.0

Primary artifact: `cyd-os-0.4.0-nm_cyd_c5-ota.bin`, generated with the existing
board-specific `scripts/build_ota.py` and validated for chip, board, protocol,
partition hash, version, complete ESP image SHA-256 and credential-free build config.
It is a prepared update for the existing NM-CYD-C5 profile, **not hardware-validated**
in this workspace. Do not use a `build-check-*` binary as a device release. The
secondary S3 result is a real firmware compile check, not acceptance of a physical
CrowPanel revision. No hardware was flashed or configured during this upgrade.

If already running the 0.3.x OTA-capable OS:

1. Keep the current source/private device configuration and an SD backup. Confirm
   your board/profile and displayed running version. Keep the device powered.
2. Download the exact C5 `.bin` to the iPad Files app while online.
3. Join CYDAP or a shared LAN and open the existing dashboard in Safari. Retained
   browser access/token behavior remains; use the same browser/address.
4. OS update → choose the `.bin` → Upload & verify. Review the staged version.
5. Restart into update, reconnect to CYDAP if needed, and wait for startup
   confirmation (normally about 10 seconds after services are ready).
6. Refresh Safari to obtain the updated package permission-inspection flow.
7. Inspect/package/upload the generic demo separately if desired. The OS does not
   install it automatically, and no application package is required for OS boot.

A pre-OTA 0.2 installation still requires the original one-time USB bootstrap
process, now built from the current source with the updated config helper. Nothing
in 0.4.0 requires erasing flash, reformatting SD, provisioning encryption or changing
eFuses. Never overwrite a real device sdkconfig with tests/sdkconfig.compile.

Existing Wi-Fi profiles/AP credentials, OS token, display calibration/brightness/
rotation, clock offset, SD apps/data/catalog and OTA partition layout are retained.
The app-credential namespace is additive. New permissions use API version 1;
existing storage-only/permission-free apps remain compatible. Some formerly
unrestricted Lua metatable/finalizer behavior is intentionally rejected to close
a hook bypass; this is documented in LUA-API-0.4.md.

## Rollback

The original A/B startup checks and 45-second startup deadline remain. A failed
pending boot attempts the existing ESP-IDF rollback path. A reset before a pending
image is confirmed is also covered by the bootloader. If the new OS confirms but
you need the previous compatible confirmed OS, use the existing Restore previous
OS control/dashboard when available. Starting another OTA upload overwrites the
inactive fallback slot, so do not assume a historical fallback remains forever.

An older OS does not recognize the new network/mqtt/credentials permissions, so
new-capability app revisions may not be listed/runnable after rollback. Their SD
files and new NVS credential records are not deleted. Avoid reorganizing those
new app identities under an older OS: its legacy catalog cleanup may discard
placements for identities it cannot recognize. Old API 1 apps still use the
same storage paths. Roll forward to a compatible OS before running new apps again.
Do not automatically downgrade app manifests by removing permissions.

USB recovery remains necessary for damaged bootloaders/partition tables, both
unusable slots, some hard hangs or physical faults. See RECOVERY.md. Software
validation does not promise physical power-loss recovery, radio/TLS interoperability
or touch/backlight behavior; those require a controlled device acceptance pass.

## Reproducing artifacts and tests

Activate ESP-IDF 5.5.4, install a native C compiler/CMake and fetch the pinned IDF
components via configure/build. Then:

```bash
python -m unittest discover -s tests -v
bash scripts/build_check.sh nm_cyd_c5
bash scripts/build_check.sh crowpanel_s3
python scripts/build_ota.py --board nm_cyd_c5 --version 0.4.0
cydctl inspect apps/platform_demo
cydctl pack apps/platform_demo -o platform-demo.cydapp
```

The native Lua/LVGL test builds the actual runtime bindings with AddressSanitizer
and UBSan. Loopback network tests require OpenSSL development headers/libraries and
the openssl command. Optional browser tests require Playwright/system Chromium.
Check test output for skips rather than assuming these dependencies were present.
The validation record lists the exact runs performed for this checkout.
