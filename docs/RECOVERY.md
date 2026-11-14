# Recovery and OS updates

Lua packages never replace the bootloader, partition table or native firmware.
Nothing auto-runs at boot. If a script fails, Home / Stop requests cancellation
and the shell returns after cleanup. If a script blocks in C/SD I/O, a stop may
time out; the runtime refuses a second app until cleanup completes. Press RESET
in that case. Remove a malfunctioning SD card with power off if needed; the
shell can start with SD unavailable. No package can be installed without the SD.

CYD OS 0.3.0 implements authenticated A/B OS updates, startup validation and ESP-IDF
rollback. See [OTA.md](OTA.md) for the one-time USB bootstrap and iPad workflow.
A bad pending boot attempts to revert; a confirmed compatible previous OS can also
be restored manually. Neither path repairs damaged partition tables/bootloaders,
both broken app slots, corrupt NVS, hardware faults or SD filesystem damage.

For firmware that prevents normal boot, use the board’s ESP32 ROM serial downloader:

1. Use a known USB **data** cable and sufficient power. Identify the serial port
   (`ls /dev/serial/by-id/` or `dmesg`). Close any program holding that port.
2. With the manufacturer's BOOT control held (GPIO0 low on the CrowPanel; GPIO28 low on NM-CYD-C5), press/release RESET
   (EN), then release BOOT. This is the ROM download path; it does not require the
   OS or touchscreen to start. Check the exact board’s controls and schematic.
3. From a correctly configured CYD source tree and ESP-IDF 5.5.4 environment,
   on C5 run `idf.py -B build-device-c5 -p /dev/ttyACM0 flash monitor` with
   your actual build directory/port. On S3 use its separate target build directory.
   This restores the project bootloader, partition table and application together.
4. Verify flash/PSRAM identification, SD diagnostics, and shell before resuming
   wireless app deployments.

USB recovery requires physical access and is not tested here. No multi-press BOOT
application gesture is claimed to recover broken native firmware. The BOOT pins remain reserved; C5 uses GPIO28, not the CrowPanel GPIO0.
See HARDWARE-C5.md for its dedicated build and flash instructions. A factory flash erase is destructive to NVS and is
not required for normal application recovery.

Never upload arbitrary `merged.bin`, Bruce images, a foreign bootloader or a
foreign partition table into these OTA slots. Conventional projects may not
support this RGB panel or flash layout. Testing them is a separate, deliberate
USB-flashing workflow with backups, not a guaranteed return-to-CYD feature.
