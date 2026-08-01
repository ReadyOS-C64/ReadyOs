#!/usr/bin/env bash
set -euo pipefail

host="${C64U_HOST:-10.0.0.79}"
text="${1:?usage: c64u_type_text.sh TEXT}"
api="http://${host}/v1"
tmp_dir="${TMPDIR:-/tmp}/c64u_type_text.$$"

mkdir -p "$tmp_dir"
trap 'rm -rf "$tmp_dir"' EXIT

post_bytes() {
  local address="$1"
  local file="$2"
  local hex
  hex="$(xxd -p -c 256 "$file" | tr -d '\n')"
  curl --fail --silent --show-error --max-time 10 \
    -X PUT \
    "${api}/machine:writemem?address=${address}&data=${hex}" >/dev/null
}

wait_keyboard_empty() {
  local sample value
  for _ in $(seq 1 200); do
    sample="$tmp_dir/kb_count.bin"
    if curl --fail --silent --show-error --max-time 5 \
      "${api}/machine:readmem?address=C6&length=1" -o "$sample" >/dev/null 2>&1; then
      value="$(od -An -tu1 "$sample" | tr -d ' ')"
      if [[ "$value" == "0" ]]; then
        return 0
      fi
    fi
    sleep 0.03
  done
  echo "Keyboard buffer did not drain" >&2
  return 1
}

python3 - "$tmp_dir/type.bin" "$text" <<'PY'
import pathlib
import sys

text = sys.argv[2]
pathlib.Path(sys.argv[1]).write_bytes(
    bytes(0x0D if ch in "\r\n" else ord(ch.upper()) for ch in text)
)
PY

size="$(wc -c < "$tmp_dir/type.bin" | tr -d ' ')"
offset=0
while (( offset < size )); do
  dd if="$tmp_dir/type.bin" of="$tmp_dir/chunk.bin" bs=1 skip="$offset" count=1 status=none
  byte_hex="$(xxd -p -c 1 "$tmp_dir/chunk.bin" | tr -d '\n')"
  printf '\000\000' > "$tmp_dir/kbclear.bin"
  wait_keyboard_empty
  post_bytes "C5" "$tmp_dir/kbclear.bin"
  curl --fail --silent --show-error --max-time 10 \
    -X PUT \
    "${api}/machine:writemem?address=277&data=${byte_hex}" >/dev/null
  printf '\001' > "$tmp_dir/count.bin"
  post_bytes "C6" "$tmp_dir/count.bin"
  wait_keyboard_empty
  offset=$(( offset + 1 ))
done
