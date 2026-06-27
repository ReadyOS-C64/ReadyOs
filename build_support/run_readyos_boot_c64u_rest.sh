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
if [[ "$skip_upload" == "1" ]]; then
  wait_for_http
else
  wait_for_ftp
  run curl --fail --silent --show-error --ftp-create-dirs \
    -T "$image" "ftp://${host}/${remote}" --user anonymous:anonymous@
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

run curl --fail --silent --show-error --max-time 10 -X PUT "${api}/machine:reset"
sleep 2
run curl --fail --silent --show-error --max-time 10 \
  -X PUT "${api}/drives/a:mount?image=%2F${remote_root}%2F${remote_name}&type=d81&mode=readwrite"
run curl --fail --silent --show-error --max-time 10 -X PUT "${api}/machine:reset"
sleep 2
wait_for_screen "basic_after_reset" "READY" 30

type_text $'LOAD"BOOT",8\r'
sleep 45
capture_screen "after_boot_load"
type_text $'RUN\r'
sleep "${READYOS_BOOT_INITIAL_WAIT_S:-90}"

if wait_for_screen "launcher_wait" "READY OS" 180; then
  capture_screen "launcher_final"
  if [[ "${READYOS_EXPECT_DMA_READY:-0}" != "0" ]]; then
    if ! wait_for_screen_re "launcher_dma_ready_wait" "DMA:(YES|ON)" 60; then
      capture_screen "launcher_dma_ready_failure"
      echo "READYOS_DMA_READY_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
  fi
  if [[ "${READYOS_BOOT_ACTION:-}" == "editor-direct" ]]; then
    select_editor
    type_key 13
    if ! wait_for_screen "editor_wait" "EDITOR:" 120; then
      capture_screen "editor_failure"
      echo "READYOS_EDITOR_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    capture_screen "editor_final"
    echo "READYOS_EDITOR_DIRECT_PASS" | tee "${out_dir}/status"
    exit 0
  fi

  if [[ "${READYOS_BOOT_ACTION:-}" == "loadall-editor" || "${READYOS_BOOT_ACTION:-}" == "loadall-editor-any" ]]; then
    type_key 133
    sleep "${READYOS_LOADALL_QUIET_WAIT_S:-240}"
    if ! wait_for_screen "loadall_wait" "PRESS ANY KEY" 240; then
      capture_screen "loadall_failure"
      echo "READYOS_LOADALL_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    type_key 13
    if [[ "${READYOS_BOOT_ACTION:-}" == "loadall-editor" ]]; then
      if ! wait_for_screen "dma_on_wait" "DMA:ON" 60; then
        capture_screen "dma_on_failure"
        echo "READYOS_DMA_ON_FAIL" | tee "${out_dir}/status"
        exit 1
      fi
    else
      if ! wait_for_screen "loadall_done_wait" "READY OS" 60; then
        capture_screen "loadall_done_failure"
        echo "READYOS_LOADALL_DONE_FAIL" | tee "${out_dir}/status"
        exit 1
      fi
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

  if [[ "${READYOS_BOOT_ACTION:-}" == "editor-load-selected" ]]; then
    select_editor
    type_key 134
    sleep "${READYOS_LOAD_SELECTED_QUIET_WAIT_S:-20}"
    if ! wait_for_screen "load_selected_wait" "PRESS ANY KEY" 120; then
      capture_screen "load_selected_failure"
      echo "READYOS_LOAD_SELECTED_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    if ! screen_has "load_selected_wait" "OK"; then
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
    if ! wait_for_screen "editor_wait" "EDITOR:" 120; then
      capture_screen "editor_failure"
      echo "READYOS_EDITOR_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    capture_screen "editor_final"
    echo "READYOS_EDITOR_LOAD_SELECTED_PASS" | tee "${out_dir}/status"
    exit 0
  fi

  if [[ "${READYOS_BOOT_ACTION:-}" == "readyshell-enter-smoke" ]]; then
    select_menu_downs "${READYOS_SELECT_DOWNS:-3}"
    type_key 13
    sleep "${READYOS_ENTER_QUIET_WAIT_S:-5}"
    capture_screen "enter_after_quiet"
    if ! screen_has "enter_after_quiet" "READYOS READYSHELL" &&
       ! screen_has "enter_after_quiet" "LOADING TO REU"; then
      echo "READYOS_READYSHELL_ENTER_EARLY_STATE_UNEXPECTED" >> "$log"
    fi
    if ! wait_for_screen "app_wait" "${READYOS_EXPECT_TEXT:-READYOS READYSHELL}" 180; then
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
    sleep "${READYOS_LOAD_SELECTED_QUIET_WAIT_S:-20}"
    if ! wait_for_screen "load_selected_wait" "PRESS ANY KEY" 180; then
      capture_screen "load_selected_failure"
      echo "READYOS_LOAD_SELECTED_FAIL" | tee "${out_dir}/status"
      exit 1
    fi
    if ! screen_has "load_selected_wait" "OK"; then
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
