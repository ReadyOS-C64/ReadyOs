#!/usr/bin/env bash
set -euo pipefail

repo="$(cd "$(dirname "$0")/.." && pwd)"
host="${C64U_HOST:-10.0.0.79}"
remote_dir="${C64U_REMOTE_DIR:-USB1}"
image_type="${PROBE_IMAGE_TYPE:-d64}"
log="${1:-/tmp/uci_dma_probe_c64u_rest.log}"
status="${2:-/tmp/uci_dma_probe_c64u_rest.status}"

case "$remote_dir" in
  USB0|USB1) ;;
  *) echo "C64U_REMOTE_DIR must be USB0 or USB1" >&2; exit 64 ;;
esac

case "$image_type" in
  d64)
    image_upper="D64"
    image_name="UCI40.D64"
    drive_type="1541"
    expected_version="40"
    expected_title="UCI DOS REU PROBE V40"
    ;;
  d81)
    image_upper="D81"
    image_name="UCI41.D81"
    drive_type="1581"
    expected_version="41"
    expected_title="UCI DOS REU PROBE V41"
    ;;
  *) echo "PROBE_IMAGE_TYPE must be d64 or d81" >&2; exit 64 ;;
esac

api="http://${host}/v1"
remote_image="${remote_dir}/${image_name}"
build_image="${repo}/build/uci_dma_probe/uci_dma_probe.${image_type}"
capture_dir="/tmp/uci_dma_probe_c64u_rest_${image_type}_capture"
tmp_dir="/tmp/uci_dma_probe_c64u_rest_${image_type}_tmp"
screen_probe_dir="/tmp/uci_dma_probe_c64u_rest_${image_type}_screen"

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

post_bytes() {
  local address="$1"
  local file="$2"
  local hex
  hex="$(xxd -p -c 256 "$file" | tr -d '\n')"
  run curl --fail --silent --show-error --max-time 10 \
    -X PUT \
    "${api}/machine:writemem?address=${address}&data=${hex}"
}

write_byte_file() {
  python3 - "$1" "$2" <<'PY'
import pathlib
import sys

pathlib.Path(sys.argv[1]).write_bytes(bytes([int(sys.argv[2], 0) & 0xFF]))
PY
}

wait_keyboard_empty() {
  local sample value
  for _ in $(seq 1 200); do
    sample="$tmp_dir/kb_count.bin"
    if curl --fail --silent --show-error --max-time 5 \
      "${api}/machine:readmem?address=C6&length=1" -o "$sample" >> "$log" 2>&1; then
      value="$(od -An -tu1 "$sample" | tr -d ' ')"
      if [[ "$value" == "0" ]]; then
        return 0
      fi
    fi
    sleep 0.03
  done
  echo "Keyboard buffer did not drain" >> "$log"
  return 1
}

type_text() {
  python3 - "$tmp_dir/type.bin" "$1" <<'PY'
import pathlib
import sys

text = sys.argv[2]
pathlib.Path(sys.argv[1]).write_bytes(
    bytes(0x0D if ch in "\r\n" else ord(ch.upper()) for ch in text)
)
PY

  local size offset byte_hex
  size="$(wc -c < "$tmp_dir/type.bin" | tr -d ' ')"
  offset=0
  while (( offset < size )); do
    dd if="$tmp_dir/type.bin" of="$tmp_dir/chunk.bin" bs=1 skip="$offset" count=1 status=none
    byte_hex="$(xxd -p -c 1 "$tmp_dir/chunk.bin" | tr -d '\n')"
    printf '\000\000' > "$tmp_dir/kbclear.bin"
    wait_keyboard_empty
    post_bytes "C5" "$tmp_dir/kbclear.bin"
    run curl --fail --silent --show-error --max-time 10 \
      -X PUT \
      "${api}/machine:writemem?address=277&data=${byte_hex}"
    write_byte_file "$tmp_dir/count.bin" 1
    post_bytes "C6" "$tmp_dir/count.bin"
    wait_keyboard_empty
    offset=$(( offset + 1 ))
  done
}

capture_screen_probe() {
  rm -rf "$screen_probe_dir"
  C64U_HOST="$host" /bin/bash "$repo/build_support/capture_uci_dma_probe_c64u.sh" "$screen_probe_dir" >> "$log" 2>&1
}

wait_for_basic() {
  local label="$1"
  local allow_stale_load="${2:-0}"
  for _ in $(seq 1 30); do
    if capture_screen_probe; then
      if grep -q "READY" "$screen_probe_dir/screen.txt"; then
        if [[ "$allow_stale_load" == "1" ]] \
          || { ! grep -q "SEARCHING" "$screen_probe_dir/screen.txt" \
            && ! grep -q "LOADING" "$screen_probe_dir/screen.txt"; }; then
          echo "BASIC ready after ${label}" >> "$log"
          return 0
        fi
      fi
    fi
    sleep 1
  done
  echo "BASIC did not become ready after ${label}" >> "$log"
  cat "$screen_probe_dir/screen.txt" >> "$log" 2>/dev/null || true
  return 1
}

reboot_to_basic() {
  local label="$1"
  run curl --fail --silent --show-error --max-time 10 -X PUT "${api}/machine:reset"
  sleep 3
  run curl --fail --silent --show-error --max-time 10 -X PUT "${api}/machine:resume"
  sleep 7
  wait_for_basic "$label"
}

{
  date
  echo "C64U host: ${host}"
  echo "Image type: ${image_type}"
  echo "Remote image: /${remote_image}"
} > "$log"

cd "$repo"
run env PROBE_IMAGE_TYPE="$image_type" /bin/bash probes/uci_dma/build.sh

run curl --ftp-create-dirs --max-time 60 \
  -T "$build_image" \
  "ftp://anonymous:anonymous%40@${host}/${remote_image}"

reboot_to_basic "pre-mount reset"

cat > "$tmp_dir/config_a.json" <<JSON
{
  "Drive A Settings": {"Drive":"Enabled","Drive Type":"${drive_type}","Drive Bus ID":8}
}
JSON
if ! run curl --fail --silent --show-error --max-time 10 \
  -H "Content-Type: application/json" \
  --data-binary "@${tmp_dir}/config_a.json" \
  "${api}/configs"; then
  echo "Drive A config POST failed; continuing with explicit mount type=${image_type}" >> "$log"
fi

run curl --fail --silent --show-error --max-time 10 \
  -X PUT \
  "${api}/drives/a:mount?image=%2F${remote_image//\//%2F}&type=${image_type}&mode=readwrite"

reboot_to_basic "post-mount reset"

type_text $'LOAD"PROBE",8\r'
echo "Quiet wait for disk LOAD to finish" >> "$log"
sleep 45
wait_for_basic "after LOAD" 1
type_text $'RUN\r'

for _ in $(seq 1 90); do
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

cat "$capture_dir/screen.txt" >> "$log"

if ! grep -q "PROBE DONE" "$capture_dir/screen.txt"; then
  echo "Probe did not reach PROBE DONE" >> "$log"
  exit 3
fi

run python3 "$repo/build_support/analyze_uci_dma_probe_run.py" \
  "$capture_dir" --expect success --version "$expected_version" \
  --expect-plain fail --expect-copy-ui fail

echo "Capture: $capture_dir" >> "$log"
echo "OK ${image_upper}" >> "$log"
