#!/usr/bin/env bash
set -euo pipefail

readyos_root="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
speeds="${XUZEXTRACT_SPEEDS:-1 16 64}"
matrix_id="$(date +%Y%m%d-%H%M%S)-$$"
matrix_dir="${XUZEXTRACT_MATRIX_OUT_DIR:-$readyos_root/logs/xuzextract-matrix/$matrix_id}"
runner_source="${XUZEXTRACT_RUNNER:-$readyos_root/build_support/run_xuzextract_c64u.sh}"
runner="$matrix_dir/run_xuzextract_c64u.frozen.sh"

mkdir -p "$matrix_dir"
cp "$runner_source" "$runner"
chmod +x "$runner"

for speed in $speeds; do
  case "$speed" in
    1|16|64) ;;
    *) echo "invalid xuzextract matrix speed: $speed" >&2; exit 64 ;;
  esac
  XUZEXTRACT_SPEED_MHZ="$speed" XUZEXTRACT_OUT_DIR="" \
    /bin/bash "$runner" \
      "$matrix_dir/${speed}mhz.log" "$matrix_dir/${speed}mhz.status"
done

echo "XUZEXTRACT physical matrix passed at: $speeds" | tee "$matrix_dir/PASS.txt"
