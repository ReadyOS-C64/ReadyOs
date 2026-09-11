#!/usr/bin/env bash
set -uo pipefail

host="${C64U_HOST:-10.0.0.79}"
out_dir="${1:-}"
log="${2:-/tmp/xuzdeflate_live_capture.log}"
status_file="${3:-/tmp/xuzdeflate_live_capture.status}"

if [[ -z "$out_dir" ]]; then
  echo "output directory required" > "$log"
  echo 64 > "$status_file"
  exit 64
fi

mkdir -p "$out_dir"
: > "$log"
rc=0

# This is a read-only post-timeout capture. It intentionally runs only after
# the codec plan's finite REST-silent interval has ended; it never supplies
# pacing to the codec and does not mutate the Ultimate or its storage.
if ! curl --fail --silent --show-error --max-time 90 \
  "http://${host}/v1/machine:readmem?address=0&length=65536" \
  -o "$out_dir/memory-0000-ffff.bin" >> "$log" 2>&1; then
  rc=1
fi
if [[ -f "$out_dir/memory-0000-ffff.bin" ]]; then
  shasum -a 256 "$out_dir/memory-0000-ffff.bin" >> "$log" 2>&1
  dd if="$out_dir/memory-0000-ffff.bin" of="$out_dir/zp-0002-001b.bin" \
    bs=1 skip=2 count=26 status=none
  dd if="$out_dir/memory-0000-ffff.bin" of="$out_dir/state-033c-06ff.bin" \
    bs=1 skip=828 count=964 status=none
  dd if="$out_dir/memory-0000-ffff.bin" of="$out_dir/coord-a000-afff.bin" \
    bs=1 skip=40960 count=4096 status=none
  dd if="$out_dir/memory-0000-ffff.bin" of="$out_dir/phase-b000-c3ff.bin" \
    bs=1 skip=45056 count=5120 status=none
  dd if="$out_dir/memory-0000-ffff.bin" of="$out_dir/stack-c400-c5ff.bin" \
    bs=1 skip=50176 count=512 status=none
fi

echo "$rc" > "$status_file"
exit "$rc"
