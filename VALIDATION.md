# Pocket Pal 0.1.0 validation

Recorded 2026-11-14 UTC. Standalone application, tested against CYDOS 0.4.0
source commit `9194feda710b7ecb48c022ec303ca9a612c09c3e` (`codex/platform-0.4.0`).
No OS source, device setting or firmware image was modified to support this app.

Executed:

- `python tools/build.py --cydos /workspace/cyd-os-040` passed using the actual
  `cydctl` packager and inspector. Final package: **106312 bytes**, 11 files,
  ZIP_STORED, API 1, targets C5/S3, **storage only**. All files fit 64 KiB and the
  package fits the existing 512 KiB/48-file limits. Nine CIMG images are 80×64.
- Native CMake build passed with pinned **Lua 5.4.8, LVGL 9.1.0** and the actual
  CYDOS runtime/bindings and service engine. **ASan and UBSan** were enabled.
- `IDF_PATH=/workspace/deps/esp-idf CYDOS_SOURCE=/workspace/cyd-os-040 python -m
  unittest discover -s tests -v`: **19 tests passed, no skips**.
- Fourteen pure-model tests execute real Lua: feed/clean/full guards, sleep/wake,
  play rewards, decay/remainders, coalesced catch-up bound, automatic naps,
  zero-health recovery, moods/growth, name validation/serialization, journal
  selection/checksums, malformed saves and numeric saturation.
- Two packaging tests check actual archive limits/assets and run the actual
  firmware package preflight compiled for **both C5 and S3**.
- Three UI runs execute actual restricted Lua/LVGL at **320×240**, **240×320** and
  **800×480**. The root reproduces the OS shell's real `height-88` app size and
  12-pixel padding, rather than assuming the full display is available. Tests
  check visible care/rename controls, touch-event callbacks, feeding/sleep/play,
  live detail updates, saved renaming, 600 animation ticks per size, pause/restart,
  one corrupted checkpoint fallback, no network attempts and clean teardown.
- The three preview PNGs are derived from those LVGL framebuffers. The surrounding
  header/Home button are host fixtures; the application rendering is real LVGL.

RTOS scheduling and file IO are host fixtures; touches are LVGL synthetic events.
There is **no physical display/touch/SD/power-loss testing**. The host test does not
claim physical frame rate, animation smoothness or flash/SD reliability. Both OS
board builds were already recorded in the separate 0.4.0 platform validation;
there was no new firmware build for this application-only change.

Needs pause while the app is closed. No reliable wall-clock or background app
service is assumed. Up to 30 seconds of passive progress can be lost on stop;
care actions save immediately. Recovery uses ordinary unencrypted SD checkpoints,
not a storage transaction or a security mechanism. SD failures can stop the app.
Growth changes the age label; the creature retains the same base sprite.

## Hosting

GitHub rejected `createRepository` through the connected integration. The complete
standalone app is therefore hosted on **`apps/pocket-pal`**, an independent app-only
branch of the user's existing CYD-OS repository. It has its own source tree/history
and is not merged into, bundled with, or installed by the operating system.
It can be moved into a dedicated repository later without changing the package ID.
