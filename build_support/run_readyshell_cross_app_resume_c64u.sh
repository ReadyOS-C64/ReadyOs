#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
READYOS_ROOT="${READYOS_ROOT:-$(cd "$SCRIPT_DIR/.." && pwd)}"
if [ -n "${VICE_TASKS_ROOT:-}" ]; then
  VICE_TOOL_ROOT="$(cd "$VICE_TASKS_ROOT" && pwd)"
  HARNESS_REPO="$(cd "$VICE_TOOL_ROOT/../.." && pwd)"
else
  HARNESS_REPO="${VICE_TASKS_REPO:-$READYOS_ROOT/../agenticdevharness}"
  HARNESS_REPO="$(cd "$HARNESS_REPO" && pwd)"
  VICE_TOOL_ROOT="$HARNESS_REPO/tools/vice_tasks_dotnet"
fi
PROJECT="$VICE_TOOL_ROOT/src/ViceTasks.Binary/ViceTasks.Binary.csproj"
PLAN="${READYSHELL_C64U_PLAN:-$SCRIPT_DIR/readyshell_cross_app_resume_c64u.generated.yaml}"
CONFIG_SRC="${READYSHELL_C64U_CONFIG_SRC:-$SCRIPT_DIR/readyshell_cross_app_resume_c64u.generated.ini}"
REPEAT_COUNT="${READYSHELL_CROSS_APP_REPEAT:-1}"
C64U_HOST="${C64U_HOST:-10.0.0.79}"
C64U_REMOTE_ROOT="${C64U_REMOTE_ROOT:-USB1/automation/vice_tasks_dotnet}"
C64U_REMOTE_D81="${C64U_REMOTE_D81:-readyos-readyshell-c64u.d81}"
C64U_IMAGE_PATH_CONFIG="${C64U_IMAGE_PATH_CONFIG:-}"
C64U_CAPTURE_VIDEO="${C64U_CAPTURE_VIDEO:-false}"
C64U_CONNECT_WAIT_S="${C64U_CONNECT_WAIT_S:-300}"
READYSHELL_C64U_PREMOUNT_REST="${READYSHELL_C64U_PREMOUNT_REST:-1}"
READYSHELL_C64U_ASSUME_MOUNTED="${READYSHELL_C64U_ASSUME_MOUNTED:-0}"
READYSHELL_C64U_BOOT_LOAD_WAIT_S="${READYSHELL_C64U_BOOT_LOAD_WAIT_S:-45}"
READYSHELL_C64U_KEY_DELAY_S="${READYSHELL_C64U_KEY_DELAY_S:-0.25}"
READYSHELL_C64U_POLL_S="${READYSHELL_C64U_POLL_S:-1.0}"
READYSHELL_C64U_POST_RUN_QUIET_S="${READYSHELL_C64U_POST_RUN_QUIET_S:-120}"
READYSHELL_C64U_APP_LOAD_QUIET_S="${READYSHELL_C64U_APP_LOAD_QUIET_S:-90}"

keys() {
  python3 - "$1" <<'PY'
import sys
s = sys.argv[1]
out = []
for ch in s:
    code = ord(ch)
    if 0x61 <= code <= 0x7a:
        code -= 0x20
    out.append(str(code))
print(",".join(out))
PY
}

emit_type_step() {
  local id="$1"
  local text="$2"
  local post="${3:-0.8}"
  cat >>"$PLAN" <<YAML
  - id: $id
    type: input.sequence
    params:
      keys: [$(keys "$text")]
      inter_key_delay_s: $READYSHELL_C64U_KEY_DELAY_S
      post_delay_s: $post
YAML
}

emit_key_step() {
  local id="$1"
  local key_list="$2"
  local delay="${3:-$READYSHELL_C64U_KEY_DELAY_S}"
  local post="${4:-1.0}"
  cat >>"$PLAN" <<YAML
  - id: $id
    type: input.sequence
    params:
      keys: [$key_list]
      inter_key_delay_s: $delay
      post_delay_s: $post
YAML
}

emit_wait_step() {
  local id="$1"
  local text="$2"
  local timeout="${3:-60}"
  local label="${4:-}"
  local pre_delay="${5:-}"
  cat >>"$PLAN" <<YAML
  - id: $id
    type: screen.wait_contains
    params:
      text: "$text"
      wait_timeout_s: $timeout
      poll_s: $READYSHELL_C64U_POLL_S
YAML
  if [ -n "$pre_delay" ]; then
    cat >>"$PLAN" <<YAML
      pre_delay_s: $pre_delay
YAML
  fi
  if [ -n "$label" ]; then
    cat >>"$PLAN" <<YAML
      capture_label: $label
YAML
  fi
}

emit_assert_step() {
  local id="$1"
  local text="$2"
  cat >>"$PLAN" <<YAML
  - id: $id
    type: assert.screen
    params:
      contains: "$text"
YAML
}

emit_capture_step() {
  local id="$1"
  local label="$2"
  cat >>"$PLAN" <<YAML
  - id: $id
    type: screen.capture
    params:
      label: $label
YAML
}

emit_readyshell_ver() {
  local label="$1"
  emit_type_step "readyshell_ver_$label" $'VER\r' "1.5"
  emit_assert_step "assert_readyshell_ver_$label" "version 0.2"
}

emit_readyshell_lst_overlay() {
  local label="$1"
  emit_type_step "readyshell_lst_$label" $'LST "RSHELP"\r' "2.0"
  emit_wait_step "wait_readyshell_lst_result_$label" "BLOCKS" "90"
  emit_type_step "readyshell_ver_after_lst_$label" $'VER\r' "1.5"
  emit_assert_step "assert_readyshell_ver_after_lst_$label" "version 0.2"
}

emit_readyshell_cat_overlay() {
  local label="$1"
  emit_type_step "readyshell_cat_$label" $'CAT "RSHELP" ! TOP 1\r' "2.0"
  emit_capture_step "capture_readyshell_cat_$label" "readyshell_cat_$label"
  emit_wait_step "wait_readyshell_cat_result_$label" "readyshell quick ref" "90"
  emit_type_step "readyshell_ver_after_cat_$label" $'VER\r' "1.5"
  emit_assert_step "assert_readyshell_ver_after_cat_$label" "version 0.2"
}

wait_for_c64u_rest() {
  local deadline
  local probe
  if [ "${READYSHELL_C64U_WAIT_FOR_DEVICE:-1}" = "0" ]; then
    return 0
  fi
  probe="$(mktemp)"
  deadline=$((SECONDS + C64U_CONNECT_WAIT_S))
  while (( SECONDS <= deadline )); do
    if curl --fail --silent --show-error --max-time 5 \
      "http://${C64U_HOST}/v1/drives" -o "$probe" >/dev/null 2>&1; then
      rm -f "$probe"
      return 0
    fi
    sleep 3
  done
  rm -f "$probe"
  echo "C64U REST not reachable after ${C64U_CONNECT_WAIT_S}s at ${C64U_HOST}" >&2
  return 1
}

wait_for_c64u_ftp() {
  local deadline
  local probe
  if [ "${READYSHELL_C64U_WAIT_FOR_DEVICE:-1}" = "0" ]; then
    return 0
  fi
  probe="$(mktemp)"
  deadline=$((SECONDS + C64U_CONNECT_WAIT_S))
  while (( SECONDS <= deadline )); do
    if curl --fail --silent --show-error --max-time 8 \
      "ftp://${C64U_HOST}/${C64U_REMOTE_ROOT}/" \
      --user anonymous:anonymous@ -o "$probe" >/dev/null 2>&1; then
      rm -f "$probe"
      return 0
    fi
    sleep 3
  done
  rm -f "$probe"
  echo "C64U FTP not reachable after ${C64U_CONNECT_WAIT_S}s at ${C64U_HOST}" >&2
  return 1
}

cd "$READYOS_ROOT"
PROFILE_CATALOG_SRC="$(python3 build_support/readyos_profiles.py catalog-source --profile precog-d81)"
if [ -z "$C64U_IMAGE_PATH_CONFIG" ]; then
  C64U_IMAGE_PATH_CONFIG="$(python3 - "$C64U_REMOTE_ROOT" "$C64U_REMOTE_D81" <<'PY'
import sys
root = sys.argv[1].strip().strip("/")
name = sys.argv[2].strip().strip("/")
print(("/" + "/".join(part for part in (root, name) if part)).lower())
PY
)"
fi
python3 - "$PROFILE_CATALOG_SRC" "$CONFIG_SRC" "$C64U_IMAGE_PATH_CONFIG" <<'PY'
import pathlib
import sys

src = pathlib.Path(sys.argv[1])
dst = pathlib.Path(sys.argv[2])
image_path = sys.argv[3]
lines = src.read_text(encoding="utf-8").splitlines()
out = []
for line in lines:
    if line.startswith("c64u_image_path="):
        out.append(f"c64u_image_path={image_path}")
    else:
        out.append(line)
dst.write_text("\n".join(out) + "\n", encoding="utf-8")
PY
PUBLIC_VERSION_TEXT="$(python3 build_support/update_build_version.py --current)"
PUBLIC_VERSION="${PUBLIC_VERSION_TEXT%[A-Z]}"
if [ "${READYSHELL_C64U_SKIP_BUILD:-0}" != "1" ]; then
  RUN_VERSION_TEXT="$(python3 build_support/update_build_version.py --next)"
  make -B \
    BUILD_SUPPORT_DIR=build_support \
    PROFILE=precog-d81 \
    LAUNCHER_DMA_LOAD=1 \
    READYOS_VERSION_TEXT="$RUN_VERSION_TEXT" \
    READYOS_CONFIG_SRC="$CONFIG_SRC" \
    profile
fi

if [ -n "${READYSHELL_C64U_D81:-}" ]; then
  D81="$READYSHELL_C64U_D81"
else
  D81="$(ls -t Releases/$PUBLIC_VERSION/precog-d81/*.d81 | head -1)"
fi

PLAN_D81="$D81"
PLAN_REMOTE_D81="$C64U_REMOTE_ROOT/$C64U_REMOTE_D81"
if [ "$READYSHELL_C64U_ASSUME_MOUNTED" = "1" ]; then
  PLAN_D81=""
  PLAN_REMOTE_D81=""
elif [ "$READYSHELL_C64U_PREMOUNT_REST" = "1" ]; then
  wait_for_c64u_ftp
  curl --fail --silent --show-error --ftp-create-dirs \
    -T "$D81" "ftp://${C64U_HOST}/${PLAN_REMOTE_D81}" \
    --user anonymous:anonymous@
  wait_for_c64u_rest
  curl --fail --silent --show-error --max-time 10 \
    -H "Content-Type: application/json" \
    --data-binary '{"Drive A Settings":{"Drive":"Enabled","Drive Type":"1581","Drive Bus ID":8}}' \
    "http://${C64U_HOST}/v1/configs" >/dev/null
  curl --fail --silent --show-error --max-time 10 -X PUT \
    "http://${C64U_HOST}/v1/machine:reset" >/dev/null
  sleep 2
  curl --fail --silent --show-error --max-time 10 -X PUT \
    "http://${C64U_HOST}/v1/drives/a:mount?image=%2F${PLAN_REMOTE_D81//\//%2F}&type=d81&mode=readwrite" >/dev/null
  PLAN_D81=""
  PLAN_REMOTE_D81=""
fi

cat >"$PLAN" <<YAML
version: 1
kind: ultimate_task_plan
plan_id: readyshell_cross_app_resume_c64u
run_mode: ultimate64
global_defaults:
  retry_policy:
    max_attempts: 2
    backoff_ms: 500
    jitter: false
  timeouts:
    launch_s: 180
    step_s: 300
    read_s: 5
  artifact_policy:
    capture_screen: true
    capture_state: true
    capture_dump: false
  ultimate:
    host: $C64U_HOST
    password: null
    ftp_user: anonymous
    ftp_password: anonymous@
    remote_root: $C64U_REMOTE_ROOT
    disk8: "$PLAN_D81"
    disk9: null
    autostart_prg: null
    drive_a_bus_id: 8
    drive_b_bus_id: 9
    drive_a_type: '1581'
    drive_b_type: '1581'
    drive_a_enabled: true
    drive_b_enabled: false
    mount_mode: readwrite
    boot_drive: 8
    capture_video_stream: $C64U_CAPTURE_VIDEO
    stream_port: 11000
    stream_timeout_s: 4.0
    default_speed_mhz: 1
    warp_equivalent_mhz: 8
steps:
  - id: launch_d81
    type: ultimate.launch
    params:
      boot_mode: disk
      drives:
        - slot: a
          bus_id: 8
          drive_type: '1581'
          enabled: true
          disk: "$PLAN_D81"
          remote_disk: "$PLAN_REMOTE_D81"
          image_type: d81
          mount_mode: readwrite
      boot_drive: 8
      boot_command: "LOAD\\"BOOT\\",8\\r"
      reset_before_boot: true
      post_reset_delay_s: 3.0
      boot_inter_chunk_delay_s: 0.25
  - id: wait_basic_loading_boot
    type: screen.wait_contains
    params:
      text: "LOADING"
      wait_timeout_s: 60
      poll_s: $READYSHELL_C64U_POLL_S
      capture_label: basic_loading_boot
  - id: run_readyos_booter
    type: input.sequence
    params:
      pre_delay_s: $READYSHELL_C64U_BOOT_LOAD_WAIT_S
      keys: [$(keys $'RUN\r')]
      inter_key_delay_s: $READYSHELL_C64U_KEY_DELAY_S
      post_delay_s: 1.0
  - id: wait_launcher_initial
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 360
      poll_s: $READYSHELL_C64U_POLL_S
      pre_delay_s: $READYSHELL_C64U_POST_RUN_QUIET_S
      capture_label: launcher_initial_c64u
  - id: select_editor_from_launcher_initial
    type: input.sequence
    params:
      keys: [19,17,17,13]
      inter_key_delay_s: $READYSHELL_C64U_KEY_DELAY_S
      post_delay_s: 2.0
  - id: wait_editor_initial
    type: screen.wait_contains
    params:
      text: "F1:CPY"
      wait_timeout_s: 300
      poll_s: $READYSHELL_C64U_POLL_S
      pre_delay_s: $READYSHELL_C64U_APP_LOAD_QUIET_S
      capture_label: editor_initial_c64u
YAML

for i in $(seq 1 "$REPEAT_COUNT"); do
cat >>"$PLAN" <<YAML
  - id: editor_touch_$i
    type: input.sequence
    params:
      keys: [$(keys $'readyshell probe editor leg\r')]
      inter_key_delay_s: $READYSHELL_C64U_KEY_DELAY_S
      post_delay_s: 0.8
  - id: ctrl_b_editor_to_launcher_$i
    type: input.sequence
    params:
      keys: [2]
      inter_key_delay_s: $READYSHELL_C64U_KEY_DELAY_S
      post_delay_s: 1.0
  - id: wait_launcher_after_editor_$i
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 60
      poll_s: $READYSHELL_C64U_POLL_S
      capture_label: launcher_after_editor_$i
  - id: move_editor_to_readybasic_$i
    type: input.sequence
    params:
      keys: [17,17,17,17,13]
      inter_key_delay_s: $READYSHELL_C64U_KEY_DELAY_S
      post_delay_s: 2.0
  - id: wait_readybasic_from_editor_$i
    type: screen.wait_contains
    params:
      text: "ready."
      wait_timeout_s: 120
      poll_s: $READYSHELL_C64U_POLL_S
      pre_delay_s: $READYSHELL_C64U_APP_LOAD_QUIET_S
      capture_label: readybasic_from_editor_$i
  - id: readybasic_touch_$i
    type: input.sequence
    params:
      keys: [$(keys $'PRINT "RBOK"\r')]
      inter_key_delay_s: $READYSHELL_C64U_KEY_DELAY_S
      post_delay_s: 0.8
  - id: assert_readybasic_touch_$i
    type: assert.screen
    params:
      contains: "RBOK"
  - id: exit_readybasic_$i
    type: input.sequence
    params:
      keys: [$(keys $'EXIT\r')]
      inter_key_delay_s: $READYSHELL_C64U_KEY_DELAY_S
      post_delay_s: 1.0
  - id: wait_launcher_after_readybasic_$i
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 60
      poll_s: $READYSHELL_C64U_POLL_S
      capture_label: launcher_after_readybasic_$i
  - id: move_launcher_to_readyshell_$i
    type: input.sequence
    params:
      keys: [19,17,17,17]
      inter_key_delay_s: $READYSHELL_C64U_KEY_DELAY_S
      post_delay_s: 0.8
  - id: launch_readyshell_after_readybasic_$i
    type: input.sequence
    params:
      keys: [13]
      inter_key_delay_s: $READYSHELL_C64U_KEY_DELAY_S
      post_delay_s: 2.0
  - id: wait_readyshell_loaded_after_readybasic_$i
    type: screen.wait_contains
    params:
      text: "run: cat"
      wait_timeout_s: 180
      poll_s: $READYSHELL_C64U_POLL_S
      pre_delay_s: $READYSHELL_C64U_APP_LOAD_QUIET_S
      capture_label: readyshell_after_readybasic_$i
YAML
emit_readyshell_ver "after_readybasic_$i"
emit_readyshell_lst_overlay "after_readybasic_$i"
emit_readyshell_cat_overlay "after_readybasic_$i"
done

cat >>"$PLAN" <<'YAML'
  - id: dump_final_state
    type: dump.memory_ranges
    params:
      ranges:
        - { label: launcher_shim_c800, start: 0xC800, end: 0xCA00 }
        - { label: app_work_1000, start: 0x1000, end: 0x1800 }
        - { label: upper_app_a000, start: 0xA000, end: 0xC600 }
YAML

if [ "${READYSHELL_C64U_GENERATE_PLAN_ONLY:-0}" = "1" ]; then
  echo "wrote $PLAN"
  exit 0
fi

dotnet build "$PROJECT"
wait_for_c64u_rest
dotnet run --project "$PROJECT" -- run-ultimate-plan --plan "$PLAN" --host "$C64U_HOST" --no-tui
