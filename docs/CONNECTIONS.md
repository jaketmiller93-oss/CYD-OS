# Touchscreen connection manager — CYD OS 0.2

In 0.3.x, open **Settings → Connections**. In 0.2 it was a home-screen button
(previously labelled Connect). No terminal,
SD card or firmware rebuild is needed to change Wi-Fi after this one-time OS update.
The manager supports the C5's 320×240 panel and the CrowPanel's 800×480 panel.
Use vertical scrolling to reach the other controls. Home remains available below
the manager and its keyboard.

## Join a network or phone hotspot

1. Tap **Scan nearby networks**. Scanning runs on the network task while touch/UI
   stay active. Up to 16 distinct named networks appear, with signal strength and
   open/secured status. The C5 requests both supported bands; S3 uses 2.4 GHz.
2. Select the SSID. Tap **Enter Wi-Fi password**. For a hidden SSID, use
   **Join hidden / other network**, then enter its name and password.
3. The keyboard's checkmark accepts an entry. Its keyboard/hide icon cancels.
   **Clear** empties the current field. Password input is masked.
4. Tap **Connect and save on success**. An IP address confirms DHCP succeeded.
   Allow up to 15 seconds before a failed attempt is reported.
5. Saved networks appear under **Saved networks**; reconnect with **Connect using
   saved password**. At most five profiles are retained. Forget an unwanted profile
   before adding a sixth; profiles are never silently evicted.

A new/changed password is saved only after a matching DHCP event for the current
connection attempt. A failed password leaves the old saved profile intact. At boot
and after loss of connection, the last successful network is tried first, then
other saved networks. Unavailable networks take approximately 15 seconds to time
out, with a 3-second retry interval. A failed manual attempt leaves 30 seconds for
correction before automatic retries resume. Joining and forgetting are explicit
physical UI actions; neither exposes passwords through HTTP.

**Disconnect Wi-Fi** retains saved credentials and stays disconnected after a
reboot. **Reconnect saved Wi-Fi** resumes automatic connection attempts.

## Use the C5's access point

Under **CYD access point settings**:

- Edit the AP name and its **12–63 character** WPA2 password. Tap **Save AP settings**.
- Tap **Start CYD access point** to keep an AP available alongside station Wi-Fi.
- Tap **Use CYD AP only** to stop station Wi-Fi and run the protected local AP.
- Tap **Show password for your iPad** to view the password on the physical screen.
- Tap **Stop CYD access point** when it is no longer needed.

If no initial AP password was configured, a random 16-character password is
created and retained. The default AP name is CYD-OS. This password is distinct
from the dashboard deployment token. The manager shows the **station IP and AP
IP separately**. The AP is normally 192.168.4.1; use the displayed address.

Changing mode/AP settings restarts the radio and can disconnect clients; wait for
reconnection. A station connection can move the AP to its channel. Scanning from
AP-only mode temporarily enables the station interface and can briefly interrupt
AP clients. **There is no NAT/internet forwarding**: AP+station mode does not turn
the C5 into an internet router. For a Wi-Fi-only iPad, either join the same phone
hotspot as the C5, or switch between the phone hotspot (ChatGPT) and CYD AP (local
uploads). The same deployment-token authentication applies on both interfaces.

## Settings and migration

Five SSID/password profiles, last successful network, station enable state, AP
name/password and AP enable state live in a versioned NVS blob in internal flash.
The SD card is not involved. NVS commits are atomic at the blob level. A storage
failure is shown on screen; a connection may work without its credentials being
saved. This build does not enable flash encryption; treat device flash/config
backups as private. Passwords are excluded from public status, HTTP and core logs.

On the first boot of 0.2, the existing build-time station credentials are imported
once, so the working LAN connection is retained. After that, touchscreen settings
are authoritative. Rebuilding with a different sdkconfig SSID does not overwrite
saved profiles. An initially configured AP password is imported too; otherwise
a generated password is available in physical AP settings. A device with no valid
initial station profile starts a protected AP, so it can be configured locally.

Enterprise/EAP, WEP, captive-portal login, WPS and internet routing are not
implemented. The dashboard can report station/AP status but cannot change Wi-Fi
credentials remotely in this release.

## Install into the working Linux source folder

Download `cyd-connection-manager-update.zip` into Linux Downloads. Stop the serial
monitor with Ctrl+] before flashing. From a normal terminal:

```bash
python3 -m zipfile -e ~/Downloads/cyd-connection-manager-update.zip ~/cyd-work
python3 ~/cyd-work/cyd-connection-manager-update/apply_update.py ~/cyd-work/cyd-os
cd ~/cyd-work/cyd-os
source ~/esp/esp-idf/export.sh
idf.py -B build-device-c5 \
  -D CYD_BOARD=nm_cyd_c5 \
  -D IDF_TARGET=esp32c5 \
  -D SDKCONFIG="$PWD/build-device-c5/sdkconfig" \
  -p /dev/ttyACM0 flash monitor
```

Use the actual serial port. The installer verifies payload hashes and backs up
existing files into a private `.connection-manager-backup-*` folder. It preserves
`build-device-c5/sdkconfig`, all app source, the SD card and board/touch calibration.
It does not erase NVS. This is a native OS feature, so adding it requires this one
firmware flash; future network changes use the touchscreen. If the board remains
in download mode after successful flashing, press RESET with BOOT released.

## Validation and physical acceptance

The network service is exercised natively with radio, NVS and RTOS test doubles:
profile migration, successful/failed DHCP, stale events, failed NVS commits,
reboot persistence, bounded profile storage, AP-only/AP+station transitions,
scan deduplication, disconnect/forget behavior and fallback are tested. These
are software tests, not radio tests. LVGL pages/keyboards are also rendered at
320×240 and 800×480, including bounds and keyboard accept/cancel checks.

On the actual C5, verify:

1. The existing LAN reconnects after updating; tap Connections and check its IP.
2. Scan, select the phone hotspot, enter its password, and confirm the new IP.
3. Reboot and confirm it reconnects without re-entering the password.
4. Enter an incorrect password; verify a clear failure, retained saved credentials,
   and successful reconnection using the saved profile.
5. Start the CYD AP, reveal its password, join it from the iPad, and open the
   displayed AP address in Safari. Authenticate and deploy/run Hello again.
6. Test AP-only mode, stop/start AP, then reconnect to the saved home network.
7. Forget a spare saved profile and verify it stays removed after reboot.

This new feature still requires these physical tests; no radio or on-device
keyboard validation is claimed from the build environment.
