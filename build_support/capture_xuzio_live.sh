#!/usr/bin/env bash
set -euo pipefail

host="${C64U_HOST:-10.0.0.79}"
out_dir="${1:?output directory required}"

mkdir -p "$out_dir"
curl --fail --silent --show-error --max-time 10 \
  "http://${host}/v1/machine:readmem?address=C000&length=128" \
  -o "$out_dir/result-c000.bin"
curl --fail --silent --show-error --max-time 10 \
  "http://${host}/v1/machine:readmem?address=0400&length=1000" \
  -o "$out_dir/screen-0400.bin"
xxd -g1 "$out_dir/result-c000.bin" > "$out_dir/result-c000.hex"
python3 "$(dirname "$0")/decode_c64_screen.py" \
  "$out_dir/screen-0400.bin" --output "$out_dir/screen.txt"
printf 'done\n' > "$out_dir/status"
