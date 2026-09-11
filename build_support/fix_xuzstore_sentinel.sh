#!/usr/bin/env bash
set -euo pipefail

host="${C64U_HOST:-10.0.0.79}"
tmp_dir="$(mktemp -d /tmp/xuzstore-sentinel.XXXXXX)"
trap 'rm -rf "$tmp_dir"' EXIT

curl --fail --silent --show-error --max-time 10 \
  "http://${host}/v1/machine:readmem?address=C000&length=128" \
  -o "$tmp_dir/result.bin"
python3 - "$tmp_dir/result.bin" "$tmp_dir/sentinel.bin" <<'PY'
import pathlib
import sys

result = pathlib.Path(sys.argv[1]).read_bytes()
if result[:4] != b"XZS1" or result[5] != 1 or result[6] != 9 or result[7] != 0:
    raise SystemExit("refusing sentinel fix: xuzstore result is not stage-9 pass")
text = "XUZSTORE FINISHED PASS"
pathlib.Path(sys.argv[2]).write_bytes(
    bytes(0x20 if char == " " else ord(char) - 0x40 for char in text)
)
PY

hex="$(xxd -p -c 256 "$tmp_dir/sentinel.bin" | tr -d '\n')"
curl --fail --silent --show-error --max-time 10 -X PUT \
  "http://${host}/v1/machine:writemem?address=0518&data=${hex}"
curl --fail --silent --show-error --max-time 10 \
  "http://${host}/v1/machine:readmem?address=0518&length=22" \
  -o "$tmp_dir/readback.bin"
cmp -s "$tmp_dir/sentinel.bin" "$tmp_dir/readback.bin"
echo "xuzstore uppercase screen sentinel installed"
