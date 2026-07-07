#!/usr/bin/env bash
set -euo pipefail

repo="$(cd "$(dirname "$0")/.." && pwd)"
host="${C64U_HOST:-10.0.0.79}"
image="${1:?usage: run_readyos_boot_c64u_rest.sh <local-d81> <remote-name> <out-dir>}"
remote_name="${2:?usage: run_readyos_boot_c64u_rest.sh <local-d81> <remote-name> <out-dir>}"
out_dir="${3:?usage: run_readyos_boot_c64u_rest.sh <local-d81> <remote-name> <out-dir>}"
api="http://${host}/v1"
remote_root="${C64U_REMOTE_DIR:-USB1}"
remote="${remote_root}/${remote_name}"
tmp_dir="${out_dir}/tmp"
connect_wait_s="${C64U_CONNECT_WAIT_S:-300}"
skip_upload="${C64U_SKIP_UPLOAD:-0}"
skip_config="${C64U_SKIP_CONFIG:-0}"
clear_reu="${READYOS_CLEAR_REU:-0}"
clear_reu_banks="${READYOS_CLEAR_REU_BANKS:-256}"
machine_reboot="${C64U_MACHINE_REBOOT:-0}"
mkdir -p "$out_dir" "$tmp_dir"
log="${out_dir}/run.log"
: > "$log"

run() {
  echo "+ $*" >> "$log"
  "$@" >> "$log" 2>&1
}

wait_for_http() {
  local deadline now
  deadline=$((SECONDS + connect_wait_s))
  while (( SECONDS <= deadline )); do
    if curl --fail --silent --show-error --max-time 5 \
      "${api}/drives" -o "${tmp_dir}/drives.json" >> "$log" 2>&1; then
      echo "REST ready after ${SECONDS}s" >> "$log"
      return 0
    fi
    sleep 3
  done
  now="$(date)"
  echo "REST not reachable after ${connect_wait_s}s at ${now}" >> "$log"
  return 1
}

wait_for_ftp() {
  local deadline now
  deadline=$((SECONDS + connect_wait_s))
  while (( SECONDS <= deadline )); do
    if curl --fail --silent --show-error --max-time 8 \
      "ftp://${host}/${remote_root}/" --user anonymous:anonymous@ \
      -o "${tmp_dir}/ftp-list.txt" >> "$log" 2>&1; then
      echo "FTP ready after ${SECONDS}s" >> "$log"
      return 0
    fi
    sleep 3
  done
  now="$(date)"
  echo "FTP not reachable after ${connect_wait_s}s at ${now}" >> "$log"
  return 1
}

delete_remote_image() {
  echo "delete remote /${remote}" >> "$log"
  curl --silent --show-error --max-time 20 \
    -Q "DELE /${remote}" "ftp://${host}/" --user anonymous:anonymous@ \
    >> "$log" 2>&1 || true
}

verify_remote_image() {
  run curl --fail --silent --show-error --max-time 20 \
    "ftp://${host}/${remote_root}/" --user anonymous:anonymous@ \
    -o "${tmp_dir}/ftp-list-after-upload.txt"
  if ! grep -F -i -q "$remote_name" "${tmp_dir}/ftp-list-after-upload.txt"; then
    echo "remote image not found after upload: ${remote_name}" >> "$log"
    return 1
  fi
}

reset_machine() {
  local label="$1"
  local action="reset"
  if [[ "$machine_reboot" == "1" ]]; then
    action="reboot"
  fi
  echo "${action} ${label}" >> "$log"
  run curl --fail --silent --show-error --max-time 10 -X PUT "${api}/machine:${action}"
  sleep "${READYOS_RESET_WAIT_S:-3}"
  run curl --fail --silent --show-error --max-time 10 -X PUT "${api}/machine:resume"
  sleep "${READYOS_RESUME_WAIT_S:-7}"
}

clear_reu_data() {
  python3 - "${tmp_dir}/clear_reu.prg" "$clear_reu_banks" <<'PY'
import pathlib
import sys

banks = int(sys.argv[2], 0)
if not 1 <= banks <= 256:
    raise SystemExit("READYOS_CLEAR_REU_BANKS must be 1..256")

prg = bytearray([
    0x01, 0x08, 0x0B, 0x08, 0x0A, 0x00, 0x9E, 0x32,
    0x30, 0x36, 0x31, 0x00, 0x00, 0x00, 0x78, 0xA9,
    0x00, 0x8D, 0x0A, 0xDF, 0xA2, 0x20, 0x8E, 0x1E,
    0x08, 0xA0, 0x00, 0xA9, 0x00, 0x99, 0x00, 0x20,
    0xC8, 0xD0, 0xFA, 0xE8, 0xE0, 0x30, 0xD0, 0xEE,
    0xA9, 0x00, 0x8D, 0x79, 0x08, 0xAD, 0x79, 0x08,
    0x8D, 0x06, 0xDF, 0xA9, 0x00, 0x8D, 0x7A, 0x08,
    0xA9, 0x00, 0x8D, 0x02, 0xDF, 0xA9, 0x20, 0x8D,
    0x03, 0xDF, 0xA9, 0x00, 0x8D, 0x04, 0xDF, 0xAD,
    0x7A, 0x08, 0x8D, 0x05, 0xDF, 0xAD, 0x79, 0x08,
    0x8D, 0x06, 0xDF, 0xA9, 0x00, 0x8D, 0x07, 0xDF,
    0xA9, 0x10, 0x8D, 0x08, 0xDF, 0xA9, 0x90, 0x8D,
    0x01, 0xDF, 0x18, 0xAD, 0x7A, 0x08, 0x69, 0x10,
    0x8D, 0x7A, 0x08, 0xD0, 0xCB, 0xEE, 0x79, 0x08,
    0xAD, 0x79, 0x08, 0xCD, 0x7B, 0x08, 0xD0, 0xB5,
    0x58, 0x60, 0x00, 0x00, 0x00,
])
prg[-1] = banks & 0xFF
pathlib.Path(sys.argv[1]).write_bytes(prg)
PY
  echo "clear REU banks=${clear_reu_banks}" >> "$log"
  run curl --fail --silent --show-error --max-time 30 \
    -H "Content-Type: application/octet-stream" \
    --data-binary "@${tmp_dir}/clear_reu.prg" \
    "${api}/runners:run_prg"
  sleep "${READYOS_CLEAR_REU_WAIT_S:-20}"
}

post_bytes() {
  local address="$1"
  local file="$2"
  run curl --fail --silent --show-error --max-time 10 \
    -X PUT --data-binary "@${file}" \
    "${api}/machine:writemem?address=${address}"
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
  for _ in $(seq 1 250); do
    sample="${tmp_dir}/kb_count.bin"
    if curl --fail --silent --show-error --max-time 5 \
      "${api}/machine:readmem?address=C6&length=1" -o "$sample" >> "$log" 2>&1; then
      value="$(od -An -tu1 "$sample" | tr -d ' ')"
      if [[ "$value" == "0" ]]; then
        return 0
      fi
    fi
    sleep 0.03
  done
  echo "keyboard buffer did not drain" >> "$log"
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
    run curl --fail --silent --show-error --max-time 10 \
      -X PUT "${api}/machine:writemem?address=00C5&data=0000"
    run curl --fail --silent --show-error --max-time 10 \
      -X PUT "${api}/machine:writemem?address=0277&data=${byte_hex}"
    run curl --fail --silent --show-error --max-time 10 \
      -X PUT "${api}/machine:writemem?address=00C6&data=01"
    wait_keyboard_empty
    offset=$((offset + 1))
  done
}

type_key() {
  local byte_hex
  byte_hex="$(printf '%02x' "$1")"
  wait_keyboard_empty
  run curl --fail --silent --show-error --max-time 10 \
    -X PUT "${api}/machine:writemem?address=00C5&data=0000"
  run curl --fail --silent --show-error --max-time 10 \
    -X PUT "${api}/machine:writemem?address=0277&data=${byte_hex}"
  run curl --fail --silent --show-error --max-time 10 \
    -X PUT "${api}/machine:writemem?address=00C6&data=01"
  wait_keyboard_empty
}

select_editor() {
  type_key 19
  type_key 17
  type_key 17
}

select_menu_downs() {
  local count="$1"
  type_key 19
  for _ in $(seq 1 "$count"); do
    type_key 17
  done
}

select_relative_downs() {
  local count="$1"
  if (( count <= 0 )); then
    return 0
  fi
  for _ in $(seq 1 "$count"); do
    type_key 17
  done
}

capture_screen() {
  local label="$1"
  local dir="${out_dir}/${label}"
  rm -rf "$dir"
  C64U_HOST="$host" /bin/bash "$repo/build_support/capture_uci_dma_probe_c64u.sh" "$dir" >> "$log" 2>&1
}

screen_has() {
  local label="$1"
  local text="$2"
  grep -q "$text" "${out_dir}/${label}/screen.txt"
}

screen_matches() {
  local label="$1"
  local pattern="$2"
  grep -Eq "$pattern" "${out_dir}/${label}/screen.txt"
}

wait_for_loaded_marker() {
  local label="$1"
  local app_text="$2"
  local count="$3"
  for i in $(seq 1 "$count"); do
    if capture_screen "$label"; then
      if screen_matches "$label" "${app_text}.*\\*"; then
        echo "screen matched loaded marker for ${app_text} at poll ${i}" >> "$log"
        return 0
      fi
    fi
    sleep 1
  done
  return 1
}

wait_for_screen() {
  local label="$1"
  local text="$2"
  local count="$3"
  for i in $(seq 1 "$count"); do
    if capture_screen "$label"; then
      if screen_has "$label" "$text"; then
        echo "screen matched ${text} at poll ${i}" >> "$log"
        return 0
      fi
    fi
    sleep 1
  done
  return 1
}

wait_for_screen_re() {
  local label="$1"
  local pattern="$2"
  local count="$3"
  for i in $(seq 1 "$count"); do
    if capture_screen "$label"; then
      if screen_matches "$label" "$pattern"; then
        echo "screen matched regex ${pattern} at poll ${i}" >> "$log"
        return 0
      fi
    fi
    sleep 1
  done
  return 1
}

wait_for_loader_done() {
  local label_prefix="$1"
  local count="$2"
  local fail_on_disk="${3:-0}"
  local context="${4:-loader}"
  local failure_file="${5:-${label_prefix}_disk_loading_failure.txt}"
  local label
  local saw_dma=0
  local saw_disk=0

  for i in $(seq 1 "$count"); do
    label="${label_prefix}_${i}"
    if capture_screen "$label"; then
      if screen_has "$label" "DMA LOADING"; then
        saw_dma=1
        echo "${context} saw DMA LOADING at poll ${i}" >> "$log"
      fi
      if screen_has "$label" "FAIL" || screen_has "$label" "INCOMPLETE LOAD"; then
        echo "${context} saw load failure at poll ${i}" >> "$log"
        cp "${out_dir}/${label}/screen.txt" "${out_dir}/${label_prefix}_load_failure.txt" 2>/dev/null || true
        return 3
      fi
      if screen_has "$label" "DISK LOADING"; then
        saw_disk=1
        echo "${context} saw DISK LOADING at poll ${i}" >> "$log"
        if [[ "$fail_on_disk" == "1" ]]; then
          cp "${out_dir}/${label}/screen.txt" "${out_dir}/${failure_file}" 2>/dev/null || true
          return 2
        fi
      fi
      if screen_has "$label" "PRESS ANY KEY"; then
        if [[ "${READYOS_EXPECT_DISK_LOADING:-0}" != "0" && "$saw_disk" != "1" ]]; then
          echo "${context} did not see expected DISK LOADING before completion" >> "$log"
          return 4
        fi
        echo "${context} done at poll ${i}; saw_dma=${saw_dma}" >> "$log"
        return 0
      fi
    fi
    sleep 1
  done
  return 1
}

wait_for_loadall_done() {
  wait_for_loader_done "$1" "$2" "${3:-0}" "loadall" "loadall_disk_loading_failure.txt"
}

wait_for_app_or_disk() {
  local label_prefix="$1"
  local expected_text="$2"
  local count="$3"
  local fail_on_disk="${4:-0}"
  local context="${5:-app-load}"
  local failure_file="${6:-${label_prefix}_disk_loading_failure.txt}"
  local label
  local saw_dma=0
  local saw_disk=0

  for i in $(seq 1 "$count"); do
    label="${label_prefix}_${i}"
    if capture_screen "$label"; then
      if screen_has "$label" "DMA LOADING"; then
        saw_dma=1
        echo "${context} saw DMA LOADING at poll ${i}" >> "$log"
      fi
      if screen_has "$label" "FAIL" || screen_has "$label" "INCOMPLETE LOAD"; then
        echo "${context} saw load failure at poll ${i}" >> "$log"
        cp "${out_dir}/${label}/screen.txt" "${out_dir}/${label_prefix}_load_failure.txt" 2>/dev/null || true
        return 3
      fi
      if screen_has "$label" "DISK LOADING"; then
        saw_disk=1
        echo "${context} saw DISK LOADING at poll ${i}" >> "$log"
        if [[ "$fail_on_disk" == "1" ]]; then
          cp "${out_dir}/${label}/screen.txt" "${out_dir}/${failure_file}" 2>/dev/null || true
          return 2
        fi
      fi
      if screen_has "$label" "$expected_text"; then
        if [[ "${READYOS_EXPECT_DISK_LOADING:-0}" != "0" && "$saw_disk" != "1" ]]; then
          echo "${context} did not see expected DISK LOADING before ${expected_text}" >> "$log"
          return 4
        fi
        echo "${context} matched ${expected_text} at poll ${i}; saw_dma=${saw_dma}" >> "$log"
        return 0
      fi
    fi
    sleep 1
  done
  return 1
}

readyshell_overlay_smoke() {
  if ! wait_for_screen "readyshell_prompt_wait" "RUN: CAT" 120; then
    capture_screen "readyshell_prompt_failure"
    echo "READYOS_READYSHELL_PROMPT_FAIL" | tee "${out_dir}/status"
    exit 1
  fi

  type_text $'VER\r'
  if ! wait_for_screen "readyshell_ver_wait" "VERSION 0.2" 90; then
    capture_screen "readyshell_ver_failure"
    echo "READYOS_READYSHELL_VER_FAIL" | tee "${out_dir}/status"
    exit 1
  fi

  type_text $'LST "RSHELP"\r'
  if ! wait_for_screen "readyshell_lst_wait" "NAME:RSHELP" 120; then
    capture_screen "readyshell_lst_failure"
    echo "READYOS_READYSHELL_LST_FAIL" | tee "${out_dir}/status"
    exit 1
  fi

  type_text $'VER\r'
  if ! wait_for_screen "readyshell_ver_after_lst_wait" "VERSION 0.2" 90; then
    capture_screen "readyshell_ver_after_lst_failure"
    echo "READYOS_READYSHELL_VER_AFTER_LST_FAIL" | tee "${out_dir}/status"
    exit 1
  fi

  type_text $'CAT "RSHELP" ! TOP 1\r'
  if ! wait_for_screen "readyshell_cat_wait" "READYSHELL QUICK REF" 120; then
    capture_screen "readyshell_cat_failure"
    echo "READYOS_READYSHELL_CAT_FAIL" | tee "${out_dir}/status"
    exit 1
  fi

  type_text $'VER\r'
  if ! wait_for_screen "readyshell_ver_after_cat_wait" "VERSION 0.2" 90; then
    capture_screen "readyshell_ver_after_cat_failure"
    echo "READYOS_READYSHELL_VER_AFTER_CAT_FAIL" | tee "${out_dir}/status"
    exit 1
  fi

  capture_screen "readyshell_overlay_final"
}

echo "host=${host}" >> "$log"
echo "connect_wait_s=${connect_wait_s}" >> "$log"
echo "skip_upload=${skip_upload}" >> "$log"
echo "skip_config=${skip_config}" >> "$log"
echo "clear_reu=${clear_reu}" >> "$log"
echo "machine_reboot=${machine_reboot}" >> "$log"
if [[ "$skip_upload" == "1" ]]; then
  wait_for_http
else
  wait_for_ftp
  delete_remote_image
  run curl --fail --silent --show-error --ftp-create-dirs \
    -T "$image" "ftp://${host}/${remote}" --user anonymous:anonymous@
  verify_remote_image
  wait_for_http
fi

if [[ "$skip_config" != "1" ]]; then
  cat > "${tmp_dir}/config_a.json" <<JSON
{"Drive A Settings":{"Drive":"Enabled","Drive Type":"1581","Drive Bus ID":8}}
JSON
  run curl --fail --silent --show-error --max-time 10 \
    -H "Content-Type: application/json" \
    --data-binary "@${tmp_dir}/config_a.json" \
    "${api}/configs"
fi

if [[ "$clear_reu" == "1" ]]; then
  clear_reu_data
fi

reset_machine "pre-mount"
run curl --fail --silent --show-error --max-time 10 \
  -X PUT "${api}/drives/a:mount?image=%2F${remote_root}%2F${remote_name}&type=d81&mode=readwrite"
reset_machine "post-mount"
wait_for_screen "basic_after_reset" "READY" 30

type_text $'LOAD"BOOT",8\r'
sleep 45
capture_screen "after_boot_load"
type_text $'RUN\r'
sleep "${READYOS_BOOT_INITIAL_WAIT_S:-90}"

if [[ "${READYOS_BOOT_ACTION:-}" == "autorun-editor" ]]; then
  if ! wait_for_screen "editor_wait" "EDITOR:" 240; then
    capture_screen "editor_failure"
    echo "READYOS_AUTORUN_EDITOR_FAIL" | tee "${out_dir}/status"
    exit 1
  fi
  type_key 2
  if ! wait_for_screen "launcher_after_editor_wait" "READY OS" 120; then
    capture_screen "launcher_after_editor_failure"
    echo "READYOS_AUTORUN_EDITOR_RETURN_FAIL" | tee "${out_dir}/status"
    exit 1
  fi
  if [[ "${READYOS_EXPECT_DMA:-1}" != "0" ]]; then
    if ! wait_for_screen "dma_on_wait" "DMA:ON" 60; then
      capture_screen "dma_on_failure"
      echo "READYOS_AUTORUN_EDITOR_DMA_ON_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
  fi
  capture_screen "autorun_editor_final"
  echo "READYOS_AUTORUN_EDITOR_PASS" | tee "${out_dir}/status"
  exit 0
fi

if wait_for_screen "launcher_wait" "READY OS" 180; then
  capture_screen "launcher_final"
  if [[ "${READYOS_EXPECT_DMA_READY:-0}" != "0" ]]; then
    if ! wait_for_screen_re "launcher_dma_ready_wait" "DMA:(YES|ON)" 60; then
      capture_screen "launcher_dma_ready_failure"
      echo "READYOS_DMA_READY_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
  fi
  if [[ "${READYOS_EXPECT_DMA_OFF:-0}" != "0" ]]; then
    if ! wait_for_screen "launcher_dma_off_wait" "DMA:NO" 60; then
      capture_screen "launcher_dma_off_failure"
      echo "READYOS_DMA_OFF_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
  fi
  if [[ -n "${READYOS_EXPECT_NOTICE_RE:-}" ]]; then
    if ! wait_for_screen_re "launcher_notice_wait" "${READYOS_EXPECT_NOTICE_RE}" 60; then
      capture_screen "launcher_notice_failure"
      echo "READYOS_NOTICE_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
  fi
  if [[ "${READYOS_BOOT_ACTION:-}" == "editor-direct" ]]; then
    select_editor
    type_key 13
    # C64U REST screen/memory polling can stall active KERNAL disk loads.
    if [[ -n "${READYOS_QUIET_AFTER_APP_ENTER_S:-}" ]]; then
      sleep "${READYOS_QUIET_AFTER_APP_ENTER_S}"
    fi
    if ! wait_for_screen "editor_wait" "EDITOR:" 120; then
      capture_screen "editor_failure"
      echo "READYOS_EDITOR_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    capture_screen "editor_final"
    echo "READYOS_EDITOR_DIRECT_PASS" | tee "${out_dir}/status"
    exit 0
  fi

  if [[ "${READYOS_BOOT_ACTION:-}" == "editor-direct-dma-return" ]]; then
    select_editor
    type_key 13
    # C64U REST screen/memory polling can stall active KERNAL disk loads.
    if [[ -n "${READYOS_QUIET_AFTER_APP_ENTER_S:-}" ]]; then
      sleep "${READYOS_QUIET_AFTER_APP_ENTER_S}"
    fi
    fail_on_disk=0
    if [[ "${READYOS_EXPECT_DMA:-1}" != "0" ]]; then
      fail_on_disk="${READYOS_FAIL_ON_DISK_LOADING_WHEN_DMA:-1}"
    fi
    if wait_for_app_or_disk "editor_direct_watch" "EDITOR:" 180 "$fail_on_disk" \
      "editor-direct" "editor_direct_disk_loading_failure.txt"; then
      editor_direct_watch_rc=0
    else
      editor_direct_watch_rc=$?
    fi
    if [[ "$editor_direct_watch_rc" == "2" ]]; then
      capture_screen "editor_direct_disk_loading_failure"
      echo "READYOS_EDITOR_DIRECT_DISK_LOADING_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    if [[ "$editor_direct_watch_rc" != "0" ]]; then
      capture_screen "editor_failure"
      echo "READYOS_EDITOR_DIRECT_DMA_RETURN_EDITOR_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    type_key 2
    if ! wait_for_screen "launcher_after_editor_wait" "READY OS" 120; then
      capture_screen "launcher_after_editor_failure"
      echo "READYOS_EDITOR_DIRECT_DMA_RETURN_LAUNCHER_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    if [[ "${READYOS_EXPECT_DMA:-1}" != "0" ]]; then
      if ! wait_for_screen "dma_on_wait" "DMA:ON" 60; then
        capture_screen "dma_on_failure"
        echo "READYOS_EDITOR_DIRECT_DMA_RETURN_DMA_ON_FAIL" | tee "${out_dir}/status"
        exit 1
      fi
      if ! wait_for_loaded_marker "editor_direct_loaded_marker_wait" "EDITOR" 30; then
        capture_screen "editor_direct_loaded_marker_failure"
        echo "READYOS_EDITOR_DIRECT_LOADED_MARKER_FAIL" | tee "${out_dir}/status"
        exit 1
      fi
    fi
    capture_screen "editor_direct_dma_return_final"
    echo "READYOS_EDITOR_DIRECT_DMA_RETURN_PASS" | tee "${out_dir}/status"
    exit 0
  fi

  if [[ "${READYOS_BOOT_ACTION:-}" == "loadall-editor" ||
        "${READYOS_BOOT_ACTION:-}" == "loadall-editor-any" ||
        "${READYOS_BOOT_ACTION:-}" == "loadall-readyshell-overlay-smoke" ]]; then
    type_key 133
    fail_on_disk=0
    if [[ "${READYOS_EXPECT_DMA:-1}" != "0" ]]; then
      fail_on_disk="${READYOS_FAIL_ON_DISK_LOADING_WHEN_DMA:-1}"
    fi
    if wait_for_loadall_done "loadall_watch" "${READYOS_LOADALL_WATCH_POLLS:-360}" "$fail_on_disk"; then
      loadall_watch_rc=0
    else
      loadall_watch_rc=$?
    fi
    if [[ "$loadall_watch_rc" == "2" ]]; then
      capture_screen "loadall_disk_loading_failure"
      echo "READYOS_LOADALL_DISK_LOADING_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    if [[ "$loadall_watch_rc" == "3" ]]; then
      capture_screen "loadall_reported_failure"
      echo "READYOS_LOADALL_REPORTED_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    if [[ "$loadall_watch_rc" != "0" ]]; then
      capture_screen "loadall_failure"
      echo "READYOS_LOADALL_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    type_key 13
    if [[ "${READYOS_BOOT_ACTION:-}" == "loadall-editor" ||
          "${READYOS_BOOT_ACTION:-}" == "loadall-readyshell-overlay-smoke" ]]; then
      if ! wait_for_screen "dma_on_wait" "DMA:ON" 60; then
        capture_screen "dma_on_failure"
        echo "READYOS_DMA_ON_FAIL" | tee "${out_dir}/status"
        exit 1
      fi
      if ! wait_for_loaded_marker "editor_loaded_marker_wait" "EDITOR" 30; then
        capture_screen "editor_loaded_marker_failure"
        echo "READYOS_EDITOR_LOADED_MARKER_FAIL" | tee "${out_dir}/status"
        exit 1
      fi
    else
      if ! wait_for_screen "loadall_done_wait" "READY OS" 60; then
        capture_screen "loadall_done_failure"
        echo "READYOS_LOADALL_DONE_FAIL" | tee "${out_dir}/status"
        exit 1
      fi
    fi
    if [[ "${READYOS_BOOT_ACTION:-}" == "loadall-readyshell-overlay-smoke" ]]; then
      select_menu_downs "${READYOS_SELECT_DOWNS:-3}"
      type_key 13
      if ! wait_for_screen "readyshell_wait" "READYOS READYSHELL" 180; then
        capture_screen "readyshell_failure"
        echo "READYOS_LOADALL_READYSHELL_FAIL" | tee "${out_dir}/status"
        exit 1
      fi
      readyshell_overlay_smoke
      capture_screen "readyshell_final"
      echo "READYOS_LOADALL_READYSHELL_OVERLAY_PASS" | tee "${out_dir}/status"
      exit 0
    fi
    select_editor
    type_key 13
    if ! wait_for_screen "editor_wait" "EDITOR:" 120; then
      capture_screen "editor_failure"
      echo "READYOS_EDITOR_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    capture_screen "editor_final"
    if [[ "${READYOS_BOOT_ACTION:-}" == "loadall-editor" ]]; then
      echo "READYOS_LOADALL_EDITOR_PASS" | tee "${out_dir}/status"
    else
      echo "READYOS_LOADALL_EDITOR_ANY_PASS" | tee "${out_dir}/status"
    fi
    exit 0
  fi

  if [[ "${READYOS_BOOT_ACTION:-}" == "manifest-sidetris" ]]; then
    type_key 135
    if ! wait_for_screen "manifest_dialog_wait" "BROWSE APP MANIFEST" 120; then
      capture_screen "manifest_dialog_failure"
      echo "READYOS_MANIFEST_DIALOG_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    select_relative_downs "${READYOS_MANIFEST_DOWNS:-4}"
    type_key 13
    fail_on_disk=0
    if [[ "${READYOS_EXPECT_DMA:-1}" != "0" ]]; then
      fail_on_disk="${READYOS_FAIL_ON_DISK_LOADING_WHEN_DMA:-1}"
    fi
    if wait_for_loader_done "manifest_load_watch" 180 "$fail_on_disk" \
      "manifest-load" "manifest_disk_loading_failure.txt"; then
      manifest_load_watch_rc=0
    else
      manifest_load_watch_rc=$?
    fi
    if [[ "$manifest_load_watch_rc" == "2" ]]; then
      capture_screen "manifest_disk_loading_failure"
      echo "READYOS_MANIFEST_DISK_LOADING_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    if [[ "$manifest_load_watch_rc" != "0" ]]; then
      capture_screen "manifest_load_failure"
      echo "READYOS_MANIFEST_LOAD_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    if ! grep -ERq "OK|LOADED TO REU" "${out_dir}"/manifest_load_watch_*/screen.txt 2>/dev/null; then
      capture_screen "manifest_load_not_ok"
      echo "READYOS_MANIFEST_LOAD_NOT_OK" | tee "${out_dir}/status"
      exit 1
    fi
    type_key 13
    if [[ "${READYOS_EXPECT_DMA:-1}" != "0" ]]; then
      if ! wait_for_screen "manifest_dma_on_wait" "DMA:ON" 60; then
        capture_screen "manifest_dma_on_failure"
        echo "READYOS_MANIFEST_DMA_ON_FAIL" | tee "${out_dir}/status"
        exit 1
      fi
    fi
    type_key 13
    if ! wait_for_screen "sidetris_wait" "SIDETRIS" 120; then
      capture_screen "sidetris_failure"
      echo "READYOS_MANIFEST_SIDETRIS_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    capture_screen "sidetris_final"
    echo "READYOS_MANIFEST_SIDETRIS_PASS" | tee "${out_dir}/status"
    exit 0
  fi

  if [[ "${READYOS_BOOT_ACTION:-}" == "editor-load-selected" ]]; then
    select_editor
    type_key 134
    fail_on_disk=0
    if [[ "${READYOS_EXPECT_DMA:-1}" != "0" ]]; then
      fail_on_disk="${READYOS_FAIL_ON_DISK_LOADING_WHEN_DMA:-1}"
    fi
    if wait_for_loader_done "load_selected_watch" "${READYOS_LOAD_SELECTED_WATCH_POLLS:-120}" "$fail_on_disk" \
      "editor-load-selected" "load_selected_disk_loading_failure.txt"; then
      load_selected_watch_rc=0
    else
      load_selected_watch_rc=$?
    fi
    if [[ "$load_selected_watch_rc" == "2" ]]; then
      capture_screen "load_selected_disk_loading_failure"
      echo "READYOS_LOAD_SELECTED_DISK_LOADING_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    if [[ "$load_selected_watch_rc" == "3" ]]; then
      capture_screen "load_selected_reported_failure"
      echo "READYOS_LOAD_SELECTED_REPORTED_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    if [[ "$load_selected_watch_rc" != "0" ]]; then
      capture_screen "load_selected_failure"
      echo "READYOS_LOAD_SELECTED_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    if ! grep -R -q "OK" "${out_dir}"/load_selected_watch_*/screen.txt 2>/dev/null; then
      capture_screen "load_selected_not_ok"
      echo "READYOS_LOAD_SELECTED_NOT_OK" | tee "${out_dir}/status"
      exit 1
    fi
    type_key 13
    if [[ "${READYOS_EXPECT_DMA:-1}" != "0" ]]; then
      if ! wait_for_screen "dma_on_wait" "DMA:ON" 60; then
        capture_screen "dma_on_failure"
        echo "READYOS_DMA_ON_FAIL" | tee "${out_dir}/status"
        exit 1
      fi
      if ! wait_for_loaded_marker "editor_loaded_marker_wait" "EDITOR" 30; then
        capture_screen "editor_loaded_marker_failure"
        echo "READYOS_EDITOR_LOADED_MARKER_FAIL" | tee "${out_dir}/status"
        exit 1
      fi
    else
      if ! wait_for_screen "load_selected_done_wait" "READY OS" 60; then
        capture_screen "load_selected_done_failure"
        echo "READYOS_LOAD_SELECTED_DONE_FAIL" | tee "${out_dir}/status"
        exit 1
      fi
    fi
    type_key 13
    if ! wait_for_screen "editor_wait" "EDITOR:" 120; then
      capture_screen "editor_failure"
      echo "READYOS_EDITOR_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    capture_screen "editor_final"
    echo "READYOS_EDITOR_LOAD_SELECTED_PASS" | tee "${out_dir}/status"
    exit 0
  fi

  if [[ "${READYOS_BOOT_ACTION:-}" == "app-load-selected-marker" ]]; then
    selected_downs="${READYOS_SELECT_DOWNS:-2}"
    selected_marker="${READYOS_EXPECT_MARKER:-EDITOR}"
    select_menu_downs "$selected_downs"
    type_key 134
    fail_on_disk=0
    if [[ "${READYOS_EXPECT_DMA:-1}" != "0" ]]; then
      fail_on_disk="${READYOS_FAIL_ON_DISK_LOADING_WHEN_DMA:-1}"
    fi
    if wait_for_loader_done "load_selected_watch" 180 "$fail_on_disk" \
      "app-load-selected-marker" "load_selected_disk_loading_failure.txt"; then
      load_selected_watch_rc=0
    else
      load_selected_watch_rc=$?
    fi
    if [[ "$load_selected_watch_rc" == "2" ]]; then
      capture_screen "load_selected_disk_loading_failure"
      echo "READYOS_LOAD_SELECTED_DISK_LOADING_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    if [[ "$load_selected_watch_rc" == "3" ]]; then
      capture_screen "load_selected_reported_failure"
      echo "READYOS_LOAD_SELECTED_REPORTED_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    if [[ "$load_selected_watch_rc" != "0" ]]; then
      capture_screen "load_selected_failure"
      echo "READYOS_LOAD_SELECTED_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    if ! grep -R -q "OK" "${out_dir}"/load_selected_watch_*/screen.txt 2>/dev/null; then
      capture_screen "load_selected_not_ok"
      echo "READYOS_LOAD_SELECTED_NOT_OK" | tee "${out_dir}/status"
      exit 1
    fi
    type_key 13
    if [[ "${READYOS_EXPECT_DMA:-1}" != "0" ]]; then
      if ! wait_for_screen "dma_on_wait" "DMA:ON" 60; then
        capture_screen "dma_on_failure"
        echo "READYOS_DMA_ON_FAIL" | tee "${out_dir}/status"
        exit 1
      fi
    fi
    if ! wait_for_loaded_marker "loaded_marker_wait" "$selected_marker" 30; then
      capture_screen "loaded_marker_failure"
      echo "READYOS_LOADED_MARKER_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    capture_screen "load_selected_marker_final"
    echo "READYOS_LOAD_SELECTED_MARKER_PASS" | tee "${out_dir}/status"
    exit 0
  fi

  if [[ "${READYOS_BOOT_ACTION:-}" == "readyshell-enter-smoke" ]]; then
    select_menu_downs "${READYOS_SELECT_DOWNS:-3}"
    type_key 13
    fail_on_disk=0
    if [[ "${READYOS_EXPECT_DMA:-1}" != "0" ]]; then
      fail_on_disk="${READYOS_FAIL_ON_DISK_LOADING_WHEN_DMA:-1}"
    fi
    if wait_for_app_or_disk "enter_watch" "${READYOS_EXPECT_TEXT:-READYOS READYSHELL}" 180 \
      "$fail_on_disk" "readyshell-enter" "enter_disk_loading_failure.txt"; then
      enter_watch_rc=0
    else
      enter_watch_rc=$?
    fi
    if [[ "$enter_watch_rc" == "2" ]]; then
      capture_screen "enter_disk_loading_failure"
      echo "READYOS_READYSHELL_ENTER_DISK_LOADING_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    if [[ "$enter_watch_rc" != "0" ]]; then
      capture_screen "app_failure"
      echo "READYOS_READYSHELL_ENTER_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    readyshell_overlay_smoke
    capture_screen "app_final"
    echo "READYOS_READYSHELL_ENTER_SMOKE_PASS" | tee "${out_dir}/status"
    exit 0
  fi

  if [[ "${READYOS_BOOT_ACTION:-}" == "readyshell-load-selected" || "${READYOS_BOOT_ACTION:-}" == "readyshell-overlay-smoke" || "${READYOS_BOOT_ACTION:-}" == "readybasic-load-selected" ]]; then
    if [[ "${READYOS_BOOT_ACTION:-}" == "readyshell-load-selected" || "${READYOS_BOOT_ACTION:-}" == "readyshell-overlay-smoke" ]]; then
      select_menu_downs "${READYOS_SELECT_DOWNS:-3}"
      expected_text="${READYOS_EXPECT_TEXT:-READYOS READYSHELL}"
      if [[ "${READYOS_BOOT_ACTION:-}" == "readyshell-overlay-smoke" ]]; then
        pass_status="READYOS_READYSHELL_OVERLAY_SMOKE_PASS"
      else
        pass_status="READYOS_READYSHELL_LOAD_SELECTED_PASS"
      fi
    else
      select_menu_downs "${READYOS_SELECT_DOWNS:-6}"
      expected_text="${READYOS_EXPECT_TEXT:-READYBASIC}"
      pass_status="READYOS_READYBASIC_LOAD_SELECTED_PASS"
    fi
    type_key 134
    fail_on_disk=0
    if [[ "${READYOS_EXPECT_DMA:-1}" != "0" ]]; then
      fail_on_disk="${READYOS_FAIL_ON_DISK_LOADING_WHEN_DMA:-1}"
    fi
    if wait_for_loader_done "load_selected_watch" 180 "$fail_on_disk" \
      "${READYOS_BOOT_ACTION:-load-selected}" "load_selected_disk_loading_failure.txt"; then
      load_selected_watch_rc=0
    else
      load_selected_watch_rc=$?
    fi
    if [[ "$load_selected_watch_rc" == "2" ]]; then
      capture_screen "load_selected_disk_loading_failure"
      echo "READYOS_LOAD_SELECTED_DISK_LOADING_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    if [[ "$load_selected_watch_rc" != "0" ]]; then
      capture_screen "load_selected_failure"
      echo "READYOS_LOAD_SELECTED_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    if ! grep -R -q "OK" "${out_dir}"/load_selected_watch_*/screen.txt 2>/dev/null; then
      capture_screen "load_selected_not_ok"
      echo "READYOS_LOAD_SELECTED_NOT_OK" | tee "${out_dir}/status"
      exit 1
    fi
    type_key 13
    if [[ "${READYOS_EXPECT_DMA:-1}" != "0" ]]; then
      if ! wait_for_screen "dma_on_wait" "DMA:ON" 60; then
        capture_screen "dma_on_failure"
        echo "READYOS_DMA_ON_FAIL" | tee "${out_dir}/status"
        exit 1
      fi
    else
      if ! wait_for_screen "load_selected_done_wait" "READY OS" 60; then
        capture_screen "load_selected_done_failure"
        echo "READYOS_LOAD_SELECTED_DONE_FAIL" | tee "${out_dir}/status"
        exit 1
      fi
    fi
    type_key 13
    if ! wait_for_screen "app_wait" "$expected_text" 180; then
      capture_screen "app_failure"
      echo "READYOS_APP_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    if [[ "${READYOS_BOOT_ACTION:-}" == "readyshell-overlay-smoke" ]]; then
      readyshell_overlay_smoke
    fi
    capture_screen "app_final"
    echo "$pass_status" | tee "${out_dir}/status"
    exit 0
  fi
  echo "READYOS_BOOT_PASS" | tee "${out_dir}/status"
  exit 0
fi

capture_screen "failure_final"
curl --fail --silent --show-error --max-time 10 \
  "${api}/machine:readmem?address=0&length=65536" -o "${out_dir}/mem_failure.bin" >> "$log" 2>&1 || true
echo "READYOS_BOOT_FAIL" | tee "${out_dir}/status"
exit 1
