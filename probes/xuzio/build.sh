#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "$0")/../.." && pwd)"
build_dir="$root_dir/build/xuzio"
run_id="${XUZIO_RUN_ID:-}"
volume="${XUZIO_VOLUME:-USB1}"

if [[ -z "$run_id" ]]; then
  echo "XUZIO_RUN_ID is required and must name a fresh owned physical run" >&2
  exit 64
fi

mkdir -p "$build_dir"
python3 "$root_dir/build_support/build_xuzio_config.py" \
  --run-id "$run_id" --volume "$volume" \
  --output "$build_dir/xuzio_config.h"
python3 "$root_dir/build_support/verify_uci_protocol_contract.py"

cl65 -t c64 -Os \
  -I "$build_dir" \
  -I "$root_dir/src/apps/uzip" \
  -C "$root_dir/probes/xuzio/probe.cfg" \
  -m "$build_dir/xuzio.map" \
  -o "$build_dir/xuzio.prg" \
  "$root_dir/probes/xuzio/xuzio.c" \
  "$root_dir/src/apps/uzip/uz_uci.c" \
  "$root_dir/src/apps/uzip/uz_uci_asm.s" \
  "$root_dir/src/apps/uzip/uz_u32.c" \
  "$root_dir/src/apps/uzip/uz_dos.c" \
  "$root_dir/src/apps/uzip/uz_crc32.c"

echo "$build_dir/xuzio.prg"
