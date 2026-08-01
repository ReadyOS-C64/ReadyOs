#!/usr/bin/env bash
set -euo pipefail

repo="$(cd "$(dirname "$0")/.." && pwd)"
host="${C64U_HOST:-10.0.0.79}"
remote_image="${C64U_REMOTE_IMAGE:-USB1/readyos.d81}"
log="${1:-/tmp/uci_timing_probe_c64u_rest.log}"
status="${2:-/tmp/uci_timing_probe_c64u_rest.status}"

case "$remote_image" in
  USB0/*|USB1/*) ;;
  *) echo "C64U_REMOTE_IMAGE must start with USB0/ or USB1/" >&2; exit 64 ;;
esac

api="http://${host}/v1"
remote_root="${remote_image%%/*}"
build_image="${repo}/build/uci_timing/readyos.d81"
capture_dir="/tmp/uci_timing_probe_c64u_rest_capture"
tmp_dir="/tmp/uci_timing_probe_c64u_rest_tmp"

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

wait_for_url() {
  local label="$1"
  local url="$2"
  local deadline=$((SECONDS + 300))
  echo "Waiting for ${label}: ${url}" >> "$log"
  while [ "$SECONDS" -lt "$deadline" ]; do
    if curl --fail --silent --show-error --max-time 5 "$url" >/dev/null 2>>"$log"; then
      echo "${label} reachable" >> "$log"
      return 0
    fi
    sleep 3
  done
  echo "${label} not reachable" >> "$log"
  return 1
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

capture_probe() {
  rm -rf "$capture_dir"
  mkdir -p "$capture_dir"
  run curl --fail --silent --show-error --max-time 10 \
    "${api}/machine:readmem?address=0400&length=1000" \
    -o "$capture_dir/screen_0400.bin"
  run curl --fail --silent --show-error --max-time 10 \
    "${api}/machine:readmem?address=3000&length=192" \
    -o "$capture_dir/probe_results_3000.bin"
  run curl --fail --silent --show-error --max-time 10 \
    "${api}/drives" \
    -o "$capture_dir/drives.json"
  python3 "$repo/build_support/decode_c64_screen.py" \
    "$capture_dir/screen_0400.bin" \
    --output "$capture_dir/screen.txt" >> "$log" 2>&1
  xxd -g 1 -c 16 "$capture_dir/probe_results_3000.bin" \
    > "$capture_dir/probe_results_3000.hex.txt"
}

wait_for_basic_ready() {
  local label="$1"
  local allow_loading="${2:-0}"
  for _ in $(seq 1 30); do
    if capture_probe; then
      if grep -q "READY" "$capture_dir/screen.txt"; then
        if [[ "$allow_loading" == "1" ]] \
          || { ! grep -q "SEARCHING" "$capture_dir/screen.txt" \
            && ! grep -q "LOADING" "$capture_dir/screen.txt"; }; then
          echo "BASIC ready after ${label}" >> "$log"
          return 0
        fi
      fi
    fi
    sleep 1
  done
  echo "BASIC did not become ready after ${label}" >> "$log"
  cat "$capture_dir/screen.txt" >> "$log" 2>/dev/null || true
  return 1
}

reboot_to_basic() {
  local label="$1"
  run curl --fail --silent --show-error --max-time 10 -X PUT "${api}/machine:reset"
  sleep 3
  run curl --fail --silent --show-error --max-time 10 -X PUT "${api}/machine:resume"
  sleep 7
  wait_for_basic_ready "$label"
}

{
  date
  echo "C64U host: ${host}"
  echo "Remote image: /${remote_image}"
} > "$log"

cd "$repo"
run /bin/bash probes/uci_timing/build.sh

wait_for_url "C64U FTP" "ftp://anonymous:anonymous%40@${host}/${remote_root}/"
run curl --max-time 20 --quote "DELE /${remote_image}" \
  "ftp://anonymous:anonymous%40@${host}/" || true
run curl --ftp-create-dirs --max-time 90 \
  -T "$build_image" \
  "ftp://anonymous:anonymous%40@${host}/${remote_image}"

wait_for_url "C64U REST" "${api}/drives"
reboot_to_basic "pre-mount reset"

cat > "$tmp_dir/config_a.json" <<JSON
{
  "Drive A Settings": {"Drive":"Enabled","Drive Type":"1581","Drive Bus ID":8}
}
JSON
if ! run curl --fail --silent --show-error --max-time 10 \
  -H "Content-Type: application/json" \
  --data-binary "@${tmp_dir}/config_a.json" \
  "${api}/configs"; then
  echo "Drive A config POST failed; continuing with explicit mount type=d81" >> "$log"
fi

run curl --fail --silent --show-error --max-time 10 \
  -X PUT \
  "${api}/drives/a:mount?image=%2F${remote_image//\//%2F}&type=d81&mode=readwrite"

reboot_to_basic "post-mount reset"

type_text $'LOAD"UTIME",8\r'
echo "Quiet wait for UTIME disk LOAD to finish" >> "$log"
sleep "${UTIME_LOAD_WAIT_S:-120}"
wait_for_basic_ready "after UTIME LOAD" 1
type_text $'RUN\r'

for _ in $(seq 1 "${UTIME_RUN_WAIT_S:-240}"); do
  if capture_probe; then
    if grep -q "PROBE DONE" "$capture_dir/screen.txt"; then
      break
    fi
    if grep -q "config .*failed" "$capture_dir/screen.txt"; then
      break
    fi
    if grep -q "uci timing probe" "$capture_dir/screen.txt"; then
      echo "Timing probe visible, waiting for completion" >> "$log"
    fi
  fi
  sleep 1
done

cat "$capture_dir/screen.txt" >> "$log" 2>/dev/null || true
cat "$capture_dir/probe_results_3000.hex.txt" >> "$log" 2>/dev/null || true

if ! run python3 "$repo/build_support/analyze_uci_timing_probe_run.py" \
  "$capture_dir" --json-output "$repo/build/uci_timing/latest_result.json"; then
  echo "Timing probe did not report success" >> "$log"
  exit 4
fi

echo "Capture: $capture_dir" >> "$log"
echo "OK UCI timing" >> "$log"
