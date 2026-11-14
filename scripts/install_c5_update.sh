#!/usr/bin/env bash
# One-time USB bootstrap; subsequent application image updates use Safari.
set -euo pipefail
bundle_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
project_dir=${1:-"$HOME/cyd-work/cyd-os"}
serial_port=${2:-/dev/ttyACM0}
project_dir=$(realpath -- "$project_dir")
if [[ -f "$HOME/esp/esp-idf/export.sh" ]]; then
    source "$HOME/esp/esp-idf/export.sh"
fi
command -v idf.py >/dev/null || { echo 'Activate ESP-IDF 5.5.4, then run this installer again.' >&2; exit 1; }
python3 "$bundle_dir/apply_os_update.py" "$project_dir"
cd -- "$project_dir"
python3 - <<'PY'
from pathlib import Path
if '#define CYD_VERSION "0.3.2"' not in Path('components/cyd_common/include/cyd.h').read_text():
    raise SystemExit('Wrong source version; not flashing')
PY
idf.py -B build-device-c5 -D CYD_BOARD=nm_cyd_c5 -D IDF_TARGET=esp32c5 \
    -D "SDKCONFIG=$project_dir/build-device-c5/sdkconfig" -D SDKCONFIG_DEFAULTS=sdkconfig.defaults build
python3 - <<'PY'
from pathlib import Path
cfg=Path('build-device-c5/sdkconfig').read_text()
if '\nCONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y\n' not in '\n'+cfg:
    raise SystemExit('Rollback is not enabled; not flashing')
app=Path('build-device-c5/cyd-os.bin').read_bytes()
if app[48:80].split(b'\0')[0]!=b'0.3.2' or app[288:304].rstrip(b'\0')!=b'CYD-OS-UPDATE':
    raise SystemExit('Unexpected firmware version/update descriptor; not flashing')
PY
idf.py -B build-device-c5 -p "$serial_port" flash
echo 'USB bootstrap finished. Press Reset if necessary, then wait for startup confirmation.'
echo 'Open Settings -> Device access & OS updates, or refresh Safari on the CYD AP.'
