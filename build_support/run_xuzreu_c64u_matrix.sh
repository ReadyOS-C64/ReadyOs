#!/usr/bin/env bash
set -euo pipefail

readyos_root="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
speeds="${XUZREU_SPEEDS:-1 16 64}"
matrix_id="$(date +%Y%m%d-%H%M%S)-$$"
matrix_dir="${XUZREU_MATRIX_OUT_DIR:-$readyos_root/logs/xuzreu-matrix/$matrix_id}"

mkdir -p "$matrix_dir"
for speed in $speeds; do
  case "$speed" in
    1|16|64) ;;
    *) echo "invalid xuzreu matrix speed: $speed" >&2; exit 64 ;;
  esac
  XUZREU_SPEED_MHZ="$speed" XUZREU_OUT_DIR="" \
    /bin/bash "$readyos_root/build_support/run_xuzreu_c64u.sh" \
      "$matrix_dir/${speed}mhz.log" "$matrix_dir/${speed}mhz.status"
done

echo "XUZREU physical matrix passed at: $speeds" | tee "$matrix_dir/PASS.txt"
