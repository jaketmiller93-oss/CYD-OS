# Firmware, setup files and examples

Artifacts are kept in the repository for access through the GitHub connection.
In GitHub’s file view, use **Download raw file** for binary and ZIP downloads.

## 0.4.0 prepared upgrade — hardware acceptance pending

[0.4.0 downloads](v0.4.0) accompany the reusable Lua platform source. These have
host validation and real firmware builds; they have **not been tested on hardware**.
Review [exact results and limitations](../docs/VALIDATION-0.4.md) before installation.

- [C5 board-specific OTA image](v0.4.0/cyd-os-0.4.0-nm_cyd_c5-ota.bin)
- [Image metadata](v0.4.0/cyd-os-0.4.0-nm_cyd_c5-ota.bin.json)
- [Complete source snapshot](v0.4.0/cyd-os-source.zip)
- [One-time USB bootstrap](v0.4.0/cyd-os-update.zip), for pre-OTA installations
- [Optional generic platform demo](v0.4.0/platform-demo.cydapp), installed separately
- [Checksums](v0.4.0/SHA256SUMS.txt)
- [Installation and rollback](../docs/UPGRADE-0.4.md)

Upload the exact C5 `.bin` using the existing authenticated OS update page. App
`.cydapp` packages use the Apps uploader and permission preflight. These are
separate operations. There is no distributed S3 update: its firmware was built
for compile/link verification only, pending physical board acceptance.

## Previous 0.3.2 files

[0.3.2 downloads](v0.3.2) are unchanged, including the earlier C5 OTA, metadata,
bootstrap/source ZIPs, Hello package, project reference and checksums. That folder
also retains earlier 0.3.1 and 0.2 artifacts for historical reference. Keep a
compatible previous image/source and SD backup before upgrading.

Artifacts are not a live SD backup, private device configuration, original factory
firmware or evidence of hardware acceptance. Do not use compile-check binaries
as device updates.
