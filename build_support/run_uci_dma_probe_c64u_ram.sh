#!/usr/bin/env bash
set -euo pipefail

repo="$(cd "$(dirname "$0")/.." && pwd)"
host="${C64U_HOST:-10.0.0.79}"
remote_dir="${C64U_REMOTE_DIR:-USB1}"
image_type="${PROBE_IMAGE_TYPE:-d64}"
log="${1:-/tmp/uci_dma_probe_c64u_ram.log}"
status="${2:-/tmp/uci_dma_probe_c64u_ram.status}"

case "$remote_dir" in
  USB0|USB1) ;;
  *) echo "C64U_REMOTE_DIR must be USB0 or USB1" >&2; exit 64 ;;
esac

case "$image_type" in
  d64)
    image_name="UCI40.D64"
    expected_version="40"
    expected_title="UCI DOS REU PROBE V40"
    ;;
  d81)
    image_name="UCI41.D81"
    expected_version="41"
    expected_title="UCI DOS REU PROBE V41"
    ;;
  *) echo "PROBE_IMAGE_TYPE must be d64 or d81" >&2; exit 64 ;;
esac

api="http://${host}/v1"
remote_image="${remote_dir}/${image_name}"
build_image="${repo}/build/uci_dma_probe/uci_dma_probe.${image_type}"
build_prg="${repo}/build/uci_dma_probe/uci_dma_probe.prg"
capture_dir="/tmp/uci_dma_probe_c64u_ram_${image_type}_capture"
tmp_dir="/tmp/uci_dma_probe_c64u_ram_${image_type}_tmp"

rm -f "$log" "$status"
rm -rf "$capture_dir" "$tmp_dir"
mkdir -p "$tmp_dir"

finish() {
  local rc=$?
  echo "$rc" > "$status"
  echo "EXIT $rc" >> "$log"
  exit "$rc"
}
trap finish EXIT

run() {
  echo "+ $*" >> "$log"
  "$@" >> "$log" 2>&1
}

post_hex() {
  local address="$1"
  local data="$2"
  run curl --fail --silent --show-error --max-time 10 \
    -X PUT \
    "${api}/machine:writemem?address=${address}&data=${data}"
}

type_text() {
  python3 - "$tmp_dir/type.hex" "$1" <<'PY'
import pathlib
import sys

text = sys.argv[2]
data = bytes(0x0D if ch in "\r\n" else ord(ch.upper()) for ch in text)
pathlib.Path(sys.argv[1]).write_text(data.hex())
PY
  local hex
  hex="$(cat "$tmp_dir/type.hex")"
  post_hex "00C5" "0000"
  post_hex "0277" "$hex"
  printf "%02x" $(( ${#hex} / 2 )) > "$tmp_dir/type_count.hex"
  post_hex "00C6" "$(cat "$tmp_dir/type_count.hex")"
}

write_probe_prg() {
  python3 - "$build_prg" "$tmp_dir/chunks.tsv" "$tmp_dir/basic_ptrs.hex" <<'PY'
import pathlib
import sys

prg = pathlib.Path(sys.argv[1]).read_bytes()
load = prg[0] | (prg[1] << 8)
body = prg[2:]
end = load + len(body)
chunk = 64
lines = []
for offset in range(0, len(body), chunk):
    address = load + offset
    data = body[offset : offset + chunk].hex()
    lines.append(f"{address:04X}\t{data}")
pathlib.Path(sys.argv[2]).write_text("\n".join(lines) + "\n")
ptr = bytes([end & 0xFF, end >> 8])
pathlib.Path(sys.argv[3]).write_text((ptr * 4).hex())
print(f"PRG load=${load:04X} end=${end:04X} bytes={len(body)}")
PY

  while IFS=$'\t' read -r address data; do
    [[ -n "$address" ]] || continue
    post_hex "$address" "$data"
  done < "$tmp_dir/chunks.tsv"
  post_hex "002D" "$(cat "$tmp_dir/basic_ptrs.hex")"
}

{
  date
  echo "C64U host: ${host}"
  echo "Image type: ${image_type}"
  echo "Remote image: /${remote_image}"
} > "$log"

cd "$repo"
run env PROBE_IMAGE_TYPE="$image_type" /bin/bash probes/uci_dma/build.sh
run curl --ftp-create-dirs --max-time 90 \
  -T "$build_image" \
  "ftp://anonymous:anonymous%40@${host}/${remote_image}"

run curl --fail --silent --show-error --max-time 10 -X PUT "${api}/machine:reset"
sleep 3
run curl --fail --silent --show-error --max-time 10 -X PUT "${api}/machine:resume"
sleep 8

write_probe_prg >> "$log" 2>&1
type_text $'RUN\r'

for _ in $(seq 1 120); do
  if C64U_HOST="$host" /bin/bash "$repo/build_support/capture_uci_dma_probe_c64u.sh" "$capture_dir" >> "$log" 2>&1; then
    if grep -q "PROBE DONE" "$capture_dir/screen.txt"; then
      break
    fi
    if grep -q "$expected_title" "$capture_dir/screen.txt"; then
      echo "Probe visible, waiting for completion" >> "$log"
    fi
  fi
  sleep 1
done

cat "$capture_dir/screen.txt" >> "$log" 2>/dev/null || true

if ! grep -q "PROBE DONE" "$capture_dir/screen.txt"; then
  echo "Probe did not reach PROBE DONE" >> "$log"
  exit 3
fi

run python3 "$repo/build_support/analyze_uci_dma_probe_run.py" \
  "$capture_dir" --expect success --version "$expected_version"

echo "Capture: $capture_dir" >> "$log"
echo "OK ${image_type}" >> "$log"
