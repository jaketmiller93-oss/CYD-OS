# Downloadable artifacts

Use the GitHub release assets, not build caches, for ready-to-install files.

[Current 0.3.2 release](https://github.com/jaketmiller93-oss/CYD-OS/releases/tag/v0.3.2)

- `cyd-os-0.3.2-nm_cyd_c5-ota.bin`: current credential-free C5 application image.
- Its `.json` sidecar: board/version, size, file and ESP image SHA-256.
- `cyd-os-update.zip`: one-time USB bootstrap for a pre-OTA C5 installation.
- `cyd-os-source.zip`: complete source snapshot.
- `hello.cydapp`: runnable example Lua package.
- `CYD-OS-Project-Reference.md`: comprehensive context for another chat.

Archived assets from the same development thread are included for reference:
`cyd-os-0.3.1-nm_cyd_c5-ota.bin` and metadata are the earlier OTA exercise;
`cyd-connection-manager-update.zip` is the superseded 0.2 source updater.
Use the current 0.3.2 files for normal setup. Historical Git commit histories
are not reconstructed: this repository begins with the complete current snapshot.

These are project/build artifacts, not a backup of the physical device's SD card,
its private configuration or original factory firmware.
