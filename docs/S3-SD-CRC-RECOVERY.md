# CrowPanel S3 SD startup recovery

Installed after explicit user approval: S3 0.5.1 booted, the CRC-enable retry was
accepted, storage initialization succeeded, and startup confirmed the firmware
at 12.339 seconds. A 120-second capture showed no resets, panics or SD errors.
The verified inactive-slot install preserved NVS, bootloader, partition table and
the previous application. C5 was not flashed. On-device app and paired-update
acceptance remain pending. The following describes the diagnosis and safeguards.

The user confirmed S1=1/S0=1, FAT32 and insertion before startup. A passive
115200-baud USB capture identified the installed S3 application as 0.5.0,
ELF prefix 463a69017. Its initial SD mount fails before filesystem access:

```
sdmmc_init_spi_crc: sdmmc_send_cmd_crc_on_off returned 0x106
sdmmc_card_init failed (0x106).
SD unavailable; shell remains usable
```

ESP-IDF 5.5.4 aborts after the first rejected CMD59. Espressif's newer driver
retries this particular error once after 10 ms. This release backports that
behavior through an S3-only linker wrapper, leaving the installed SDK intact.
CRC stays enabled; the second failure propagates. Timeout and CRC corruption
errors do not receive this workaround. SD pins, selector and clock are unchanged.

Sources: [newer Espressif driver](https://github.com/espressif/esp-idf/blob/master/components/sdmmc/sdmmc_sd.c),
[Espressif issue 15450](https://github.com/espressif/esp-idf/issues/15450),
[Espressif issue 18496](https://github.com/espressif/esp-idf/issues/18496).
This matched the observed failure and the retry subsequently succeeded on this
card. Reformatting cannot repair this command error.

The candidate is 0.5.1 and also includes the prepared native-app timing fix,
S3 Local logs and deferred paired startup. The C5 image is unchanged. The separate
C5 immediate reset on connected power-up remains undiagnosed.

## One-time USB recovery boundary

The installed paired updater stages its container on the failing S3 SD card.
USB installation required separate authorization: the initial USB connection was
authorized only for passive logging. That diagnosis used no write/reset commands.
The user then explicitly approved the inactive-slot installation reported above.

After authorization, verify S3 chip/security state, the installed partition table,
both OTA selection records and the selected running image before writing. Stop
on any unexpected encryption, secure boot, layout or unconfirmed selection.
Privately back up NVS and OTA metadata. Program only the inactive application,
verify its full readback hash, then write the other OTA metadata sector selecting
the image as NEW. Preserve the current VALID selection sector and application.
The bootloader must retain pending verification and normal rollback protection.
Verify NVS and partition-table hashes unchanged before restarting. Never use a
merged image or a guessed application address on this existing OTA installation.

`scripts/ota_recovery_plan.py` is an offline, tested metadata planner, not a
flasher. It refuses absent, ambiguous, unconfirmed or exhausted selection state.
It requires independent partition-table and active-image verification by its
caller. It does not open ports. Do not blindly use otatool's generic switch
operation: it retains the old state and rewrites both selection sectors.

After installation, capture startup, check the CMD59 retry result, SD mount and
local boot confirmation. If mounting still fails, retain the new error instead
of repeatedly formatting or changing wiring. App and paired OTA acceptance remain.
