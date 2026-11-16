# S3 raw application — 0.5.1

`UPLOAD-THIS.bin` is a raw OTA application, not a merged USB image. It has no
universal USB address: inspect the device's actual inactive OTA slot before
recovery. Preserve NVS, bootloader, partition table and rollback protection.
Normal connected 0.5 updates use the paired container at the release root.

This exact S3 image was installed and startup-confirmed during SD recovery.
