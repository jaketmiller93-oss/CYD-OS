#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
case "${1:-crowpanel_s3}" in
    crowpanel_s3) board=crowpanel_s3; target=esp32s3 ;;
    nm_cyd_c5) board=nm_cyd_c5; target=esp32c5 ;;
    *) echo 'Usage: scripts/build_check.sh [crowpanel_s3|nm_cyd_c5]' >&2; exit 2 ;;
esac
# Isolated, synthetic build-only configuration. No physical validation implied.
idf.py -B "build-check-$board" -D "CYD_BOARD=$board" -D "IDF_TARGET=$target" \
  -D "SDKCONFIG=$PWD/build-check-$board/sdkconfig" \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;tests/sdkconfig.compile' build
