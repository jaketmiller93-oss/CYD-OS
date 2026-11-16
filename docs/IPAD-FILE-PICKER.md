# iPad upload picker fix (C5 0.4.1)

The 0.4.0 dashboard uses accept=.cydapp,application/zip for apps and
accept=.bin,application/octet-stream for OS updates. iPadOS can map these hints
to restrictive native document types and disable files that should be selectable.
The observed greyed-out picker occurs before upload and is separate from the
previous Pocket Pal upload/reset report.

Remove accept entirely from both inputs. Do not replace it with */*: that value
has its own documented WebKit picker issues. Keep package/firmware labels, size
checks, authenticated upload, app preflight/permissions confirmation and all
on-device chip/board/layout/hash/package validators unchanged. The picker is a
selection hint, not the format validator. ZIP-named app packages are uploaded as
bytes, with no extraction or recompression; the original manifest remains inside.

This C5 patch is based directly on PR #1 / 0.4.0 (9194fed), without the experimental
pairing branch or S3 display changes. Four initial credential fields are blank in
the generated OTA build. Use the existing CYD-OS updater; do not flash an app-only
OTA binary at zero. No device firmware/settings are changed by preparing this patch.

Immediate app workaround: download the byte-identical Calculator upload ZIP, save
it locally under On My iPad and choose the ZIP itself without extracting it. If a
file provider or picker still disables it, the temporary Safari bookmarklet in
the accompanying instructions removes accept on the currently open dashboard.
It only changes this page until refresh, and sends no requests or credentials.

Browser tests assert both controls have no accept filter and exercise the upload
flow with a custom-extension application/octet-stream file. Physical iPad document
picker behavior cannot be tested by setting files through Playwright; no physical
iPad or C5 installation validation is claimed by those checks. The earlier upload
flash/reset report remains unresolved.
