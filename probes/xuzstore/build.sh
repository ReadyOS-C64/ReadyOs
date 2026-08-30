#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "$0")/../.." && pwd)"
build_dir="$root_dir/build/xuzstore"
run_id="${XUZSTORE_RUN_ID:-}"
volume="${XUZSTORE_VOLUME:-USB1}"

if [[ -z "$run_id" ]]; then
  echo "XUZSTORE_RUN_ID is required for a fresh owned physical run" >&2
  exit 64
fi

mkdir -p "$build_dir"
python3 "$root_dir/build_support/build_xuzstore_config.py" \
  --run-id "$run_id" --volume "$volume" \
  --output "$build_dir/xuzstore_config.h"
python3 "$root_dir/build_support/verify_uci_protocol_contract.py"

cl65 -t c64 -Os \
  -I "$build_dir" -I "$root_dir/src/apps/uzip" \
  -C "$root_dir/probes/xuzstore/probe.cfg" \
  -m "$build_dir/xuzstore.map" \
  -o "$build_dir/xuzstore.prg" \
  "$root_dir/probes/xuzstore/xuzstore.c" \
  "$root_dir/src/apps/uzip/uz_uci.c" \
  "$root_dir/src/apps/uzip/uz_uci_asm.s" \
  "$root_dir/src/apps/uzip/uz_u32.c" \
  "$root_dir/src/apps/uzip/uz_dos.c" \
  "$root_dir/src/apps/uzip/uz_crc32.c" \
  "$root_dir/src/apps/uzip/uz_zip_write.c" \
  "$root_dir/src/apps/uzip/uz_zip_read.c"

echo "$build_dir/xuzstore.prg"
