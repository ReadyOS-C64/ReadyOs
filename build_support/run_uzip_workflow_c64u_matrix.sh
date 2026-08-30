#!/usr/bin/env bash
set -euo pipefail

readyos_root="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
speeds="${UZIP_WORKFLOW_SPEEDS:-1 64}"
matrix_id="$(date +%Y%m%d-%H%M%S)-$$"
matrix_dir="${UZIP_WORKFLOW_MATRIX_OUT_DIR:-$readyos_root/logs/uzip-workflow-matrix/$matrix_id}"
runner_source="${UZIP_WORKFLOW_RUNNER:-$readyos_root/build_support/run_uzip_workflow_c64u.sh}"
runner="$matrix_dir/run_uzip_workflow_c64u.frozen.sh"

mkdir -p "$matrix_dir"
cp "$runner_source" "$runner"
chmod +x "$runner"

expected_uzip="${UZIP_WORKFLOW_EXPECTED_UZIP_SHA256:-$(shasum -a 256 "$readyos_root/bin/uzip.prg" | awk '{print $1}')}"
expected_uzpack="${UZIP_WORKFLOW_EXPECTED_UZPACK_SHA256:-$(shasum -a 256 "$readyos_root/bin/uzpack.prg" | awk '{print $1}')}"
printf 'uzip.prg %s\nuzpack.prg %s\n' "$expected_uzip" "$expected_uzpack" \
  > "$matrix_dir/FROZEN_SHA256.txt"

for speed in $speeds; do
  case "$speed" in
    1|16|64) ;;
    *) echo "invalid uZIP workflow matrix speed: $speed" >&2; exit 64 ;;
  esac
  UZIP_WORKFLOW_SPEED_MHZ="$speed" UZIP_WORKFLOW_OUT_DIR="" \
  UZIP_WORKFLOW_EXPECTED_UZIP_SHA256="$expected_uzip" \
  UZIP_WORKFLOW_EXPECTED_UZPACK_SHA256="$expected_uzpack" \
    /bin/bash "$runner" \
      "$matrix_dir/${speed}mhz.log" "$matrix_dir/${speed}mhz.status"
done

echo "uZIP complete physical workflow matrix passed at: $speeds" | \
  tee "$matrix_dir/PASS.txt"
