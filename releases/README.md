# Firmware, setup files and examples

All downloads are stored in the repository so they remain accessible through the
GitHub connection even when the release-asset endpoint is unavailable.

## Current files

Open [the 0.3.2 downloads folder](v0.3.2).

- [C5 OTA application image](v0.3.2/cyd-os-0.3.2-nm_cyd_c5-ota.bin)
- [Image metadata and hashes](v0.3.2/cyd-os-0.3.2-nm_cyd_c5-ota.bin.json)
- [One-time USB bootstrap ZIP](v0.3.2/cyd-os-update.zip)
- [Complete source snapshot ZIP](v0.3.2/cyd-os-source.zip)
- [Hello Lua app package](v0.3.2/hello.cydapp)
- [Comprehensive project reference](v0.3.2/CYD-OS-Project-Reference.md)
- [Artifact checksums](v0.3.2/SHA256SUMS.txt)

In GitHub's file view, use **Download raw file** to save a binary/ZIP. Upload OS
`.bin` files in the CYD OS update page, and `.cydapp` files in the Apps page.
The bootstrap ZIP is for pre-OTA installations; ordinary compatible OS updates
use the `.bin`. These images target the exact NM-CYD-C5 profile, not CrowPanel.

## Archived files

The folder also retains the earlier 0.3.1 C5 OTA image/metadata and the superseded
0.2 connection-manager updater. Use the 0.3.2 files for normal setup. Historical
Git histories are not reconstructed: this repository starts from the complete
current source snapshot.

The files are project artifacts, not a live SD backup, private device sdkconfig
or original factory firmware. Hardware acceptance of the latest features remains
pending; software checks are described in docs/VALIDATION.md.
