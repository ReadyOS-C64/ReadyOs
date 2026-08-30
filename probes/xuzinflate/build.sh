#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "$0")/../.." && pwd)"
build_dir="$root_dir/build/xuzinflate"
run_id="${XUZINFLATE_RUN_ID:-}"
volume="${XUZINFLATE_VOLUME:-USB1}"
fixture_dir="${XUZINFLATE_FIXTURE_DIR:-$build_dir/fixtures}"

if [[ -z "$run_id" ]]; then
  echo "XUZINFLATE_RUN_ID is required for a fresh owned physical run" >&2
  exit 64
fi

mkdir -p "$build_dir"
python3 "$root_dir/build_support/build_xuzinflate_config.py" \
  --run-id "$run_id" --volume "$volume" \
  --fixture-dir "$fixture_dir" \
  --output "$build_dir/xuzinflate_config.h"
python3 "$root_dir/build_support/verify_uci_protocol_contract.py"

cl65 -t c64 -Os -DUZ_DOS_INFLATE_PROBE_MINIMAL \
  -I "$build_dir" -I "$root_dir/src/apps/uzip" \
  -C "$root_dir/probes/xuzinflate/probe.cfg" \
  -m "$build_dir/xuzinflate.map" \
  -o "$build_dir/xuzinflate.prg" \
  "$root_dir/probes/xuzinflate/xuzinflate.c" \
  "$root_dir/probes/xuzinflate/xuzinflate_stack.s" \
  "$root_dir/src/apps/uzip/uz_uci.c" \
  "$root_dir/src/apps/uzip/uz_uci_asm.s" \
  "$root_dir/src/apps/uzip/uz_u32.c" \
  "$root_dir/src/apps/uzip/uz_dos.c" \
  "$root_dir/src/apps/uzip/uz_crc32.c" \
  "$root_dir/src/apps/uzip/uz_inflate6502.c" \
  "$root_dir/src/apps/uzip/uz_inflate6502_asm.s" \
  "$root_dir/src/apps/uzip/uz_inflate6502_boundary.s"

python3 - "$build_dir/xuzinflate.map" <<'PY'
import re
import sys

text = open(sys.argv[1], encoding="utf-8").read()
def segment(name):
    match = re.search(
        rf"^{name}\s+([0-9A-F]+)\s+([0-9A-F]+)\s+([0-9A-F]+)\s",
        text, re.M,
    )
    if not match:
        raise SystemExit(f"xuzinflate map has no {name} segment")
    return tuple(int(value, 16) for value in match.groups())

def symbol(name):
    match = re.search(rf"(?:^|\s){name}\s+([0-9A-F]+)\s", text, re.M)
    if not match:
        raise SystemExit(f"xuzinflate map has no {name} symbol")
    return int(match.group(1), 16)

bss_start, bss_end, _ = segment("BSS")
ro_start, ro_end, _ = segment("RODATA")
data_start, data_end, _ = segment("DATA")
init_start, init_end, _ = segment("INIT")
job_start, job_end, _ = segment("JOB_CODE")
job_ro_start, job_ro_end, _ = segment("JOB_RODATA")
job_bss_start, job_bss_end, _ = segment("JOB_BSS")
if bss_end >= 0x2FFF:
    raise SystemExit(f"xuzinflate low BSS ${bss_start:04X}-${bss_end:04X} reaches guard")
high_start = min(
    ro_start, data_start, init_start, job_start, job_ro_start, job_bss_start
)
high_end = max(
    ro_end, data_end, init_end, job_end, job_ro_end, job_bss_end
)
if high_start != 0xB001 or high_end >= 0xC400:
    raise SystemExit(
        f"xuzinflate job image ${high_start:04X}-${high_end:04X} "
        "overlaps dictionary guard or stack"
    )
main_start = symbol("__MAIN_START__")
main_size = symbol("__MAIN_SIZE__")
if main_start + main_size != 0xD000:
    raise SystemExit(
        f"xuzinflate crt0 stack top is ${main_start + main_size:04X}, not $D000"
    )
print(
    f"xuzinflate low BSS ends ${bss_end:04X}; "
    f"job ends ${high_end:04X}; cc65 stack top $D000/floor $C400"
)
PY

echo "$build_dir/xuzinflate.prg"
