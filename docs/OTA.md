# Wireless OS updates (0.3.0+)

One USB bootstrap is required when upgrading from 0.2.0: the bootloader must gain
ESP-IDF rollback support, and the running OS must gain the authenticated updater.
The partition layout stays unchanged. This bootstrap does not erase NVS or SD.
Afterward, normal CYD OS updates use Safari on an iPad connected to the device's
AP or the same LAN. Internet access is not required to install a downloaded image.

## Existing NM-CYD-C5 installation

Download and extract `cyd-os-update.zip` into Downloads on the Debian computer.
It creates `~/Downloads/cyd-os-update`. Close the serial monitor with Ctrl+].
Connect the C5 with its usual USB data cable. Run:

```bash
bash ~/Downloads/cyd-os-update/install_c5_update.sh
```

The installer targets `~/cyd-work/cyd-os`, `/dev/ttyACM0`, and the existing
`build-device-c5/sdkconfig`. Optional positional arguments override source and port:

```bash
bash ~/Downloads/cyd-os-update/install_c5_update.sh /path/to/cyd-os /dev/ttyACM0
```

It verifies payload checksums, backs up changed sources and the private sdkconfig,
updates sources including the display HALs, enables rollback/UI fonts, then builds
and checks the application version/descriptor before flashing. Device Wi-Fi,
deployment token and compiled touch calibration values stay in sdkconfig; app
sources stay in place. Backups live in `.os-update-backup-*` with private directory
permissions. Keep that backup private. The script stops if any step fails.

The script activates `~/esp/esp-idf/export.sh` when present; ESP-IDF 5.5.4 is required.
No command belongs in the serial monitor: run it in the normal Debian terminal.
If the flash tool cannot connect, use BOOT + RESET, release BOOT after connection.
After “Hash of data verified”, let it reset or press RESET once with BOOT released.
Never run `erase-flash` as part of this update.

Check the new circular home menu and **Settings → Device access & OS updates**.
Wait roughly 10 seconds for startup confirmation. Refresh Safari to load the new
embedded dashboard. The existing token is imported into NVS once; later firmware
builds cannot silently replace it. The working bootstrap calibration is also saved
into NVS immediately, before the first generic OTA update.

## Updating from the iPad afterward

1. Save the board-specific CYD OS **application `.bin`** into Files while online.
   The included `cyd-os-0.3.2-nm_cyd_c5-ota.bin` adds remembered dashboard access.
   Install it wirelessly on 0.3.0/0.3.1; the current USB bootstrap also installs 0.3.2.
2. Connect to CYDAP (or use a shared LAN), then open the CYD address in Safari.
   The normal AP address is `http://192.168.4.1`.
3. Sign in with the device token, open **OS update**, choose the `.bin`, then
   **Upload & verify OS**. Keep device power connected.
4. After verification, choose **Restart into update**. Rejoin CYDAP if iPad Wi-Fi
   changes networks during reboot. Safari waits for the device's startup checks.
5. Verify the running version and “startup confirmed”. Nothing on the SD is moved
   or erased by OS updates. Changes to compiled app APIs still require compatible apps.

Uploading never selects the new image by itself. Cancel keeps the current image
selected. A transfer interrupted before restart leaves the current OS selected.
If a restart request's response is lost, reconnect and check status rather than
assuming it failed. Starting an upload overwrites the inactive slot, so it replaces
any older fallback image in that slot.

The ChatGPT app creates/downloads files; Safari transfers them to the device.
ChatGPT does not directly connect to a private CYD address. No account/cloud service
or desktop is required for this transfer after bootstrap. Building a new native OS
binary still requires ESP-IDF on a computer or build service; the ESP32 cannot
compile its own native OS. Lua app packages remain a separate, lightweight workflow.

## Validation and rollback

The authenticated raw upload endpoint streams to the **inactive 7 MiB slot**. It
checks ESP chip, 16 MiB header, CYD project, exact board profile, supported update
protocol/settings schemas, partition-layout hash and required rollback features
before opening that slot. ESP-IDF checks the complete image/chip/checksum/appended
SHA-256 before the image is staged. Only an explicit restart selects it. This is
integrity and compatibility checking, not publisher-signature verification.

A new pending image must initialize core/NVS/access/device settings, board, API,
network service and UI. UI/network heartbeats must stay fresh (under 2.5 seconds)
for 10 continuous seconds. Startup has a 45-second deadline. It does not require
internet or an SD card. On failure, a pending image asks ESP-IDF to mark it invalid
and reboot into the previous confirmed image. Bootloader rollback also covers a
reset before confirmation. Watchdog/power recovery is still needed for a hard hang
that prevents the health task from running. Software checks cannot validate actual
pixel colors, touch accuracy or RF performance.

Manual **Restore previous OS** is offered when ESP-IDF has a confirmed fallback;
the operation additionally checks its CYD compatibility descriptor. It retains SD
files and NVS. A USB bootstrap normally has no compatible fallback yet; one is
created by a successful wireless update. An OS upload holds a maintenance gate,
stops Lua, and blocks app launches/installs, folder mutations, display changes and
network reconfiguration until staging is cancelled or the device restarts.

Protocol 1 preserves NVS schemas `cyd_wifi` v1, `cyd_access` token,
`cyd_device` display v1, and SD folder catalog v1. Future schema changes must preserve
backward readability or explicitly change/reject compatibility. OTA does not change
partition tables, bootloaders or eFuses. Incompatible layout/bootloader migrations
require a deliberate USB update. Unrelated firmware and merged images are rejected.

## Building future images

Activate ESP-IDF, then from the CYD source folder:

```bash
python scripts/build_ota.py --board nm_cyd_c5 --version 0.3.3
```

Use `--board crowpanel_s3` only for confirmed DIS01943A V1.1–V1.3 hardware.
The builder uses a separate build directory, blanks all initial Wi-Fi/AP/token
fields, validates the embedded descriptor and complete ESP image digest, and emits
an application `.bin` plus a JSON file with image/file SHA-256 values into `dist`.
It never borrows a user's device sdkconfig. Use the actual changed source tree;
a version number by itself adds no new functionality.

For serious recovery failures, see [RECOVERY.md](RECOVERY.md). Hardware acceptance
is still required before relying on automatic rollback in the field.

## Remember dashboard access (0.3.2)

“Remember this device” is checked by default. After a successful sign-in, Safari
stores the token locally and signs in automatically on later visits at the same
address. Existing tab-only tokens migrate automatically on a refresh, without
retyping. Uncheck the option to keep access limited to the current tab instead.
Settings → Forget saved access clears both persistent and tab-only credentials.

Use Safari consistently and bookmark `http://192.168.4.1` when using CYDAP. A LAN
IP, `cyd.local`, a different port or another browser is a different storage
origin; it does not inherit the token. Clearing website data, closing private
browsing or replacing the token on the CYD can require another sign-in. An invalid
saved token is removed and the login form returns. A temporary connection/service
failure keeps saved access for the next reconnect. If browser storage is blocked,
the current page still works and explains that access could not be remembered.
The token remains required by the API; it is never embedded into the public page.
