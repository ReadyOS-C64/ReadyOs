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
PLAN="${READYBASIC_KEYBOARD_REGRESSION_PLAN:-$SCRIPT_DIR/readybasic_keyboard_regression_probe.generated.yaml}"
HOTKEY_INPUT_MODE="${READYBASIC_HOTKEY_INPUT_MODE:-keylog}"
BOOT_MODE="${READYBASIC_KEYBOARD_BOOT_MODE:-runfirst}"
READYBASIC_BANK_RAW="${READYBASIC_KEYBOARD_READYBASIC_BANK:-1}"
READYBASIC_LAUNCH_KEYS="${READYBASIC_KEYBOARD_LAUNCH_KEYS:-17,17,17,17,17,17,13}"
READYBASIC_VISIBLE="${READYBASIC_VISIBLE:-0}"
READYBASIC_KEEP_VICE="${READYBASIC_KEEP_VICE:-0}"
VICE_HEADLESS="true"
VICE_CLOSE="true"
CLI_CLOSE_ARG="--close-vice"

if [ "$HOTKEY_INPUT_MODE" = "host" ] || [ "$READYBASIC_VISIBLE" = "1" ]; then
  VICE_HEADLESS="false"
fi
if [ "$READYBASIC_KEEP_VICE" = "1" ]; then
  VICE_CLOSE="false"
  CLI_CLOSE_ARG=""
fi
case "$BOOT_MODE" in
  runfirst|launcher) ;;
  *)
    echo "READYBASIC_KEYBOARD_BOOT_MODE must be runfirst or launcher, got '$BOOT_MODE'" >&2
    exit 1
    ;;
esac

keys() {
  python3 - "$1" <<'PY'
import sys
s = sys.argv[1]
print(",".join(str(ord(ch)) for ch in s))
PY
}

emit_initial_entry_steps() {
  if [ "$BOOT_MODE" = "launcher" ]; then
    cat <<YAML
  - id: wait_launcher_initial
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 180
      capture_label: keyboard_regression_launcher_initial
  - id: launch_readybasic_from_launcher
    type: input.sequence
    params:
      keys: [$READYBASIC_LAUNCH_KEYS]
      inter_key_delay_s: 0.08
      post_delay_s: 2.5
  - id: wait_readybasic_initial
    type: screen.wait_contains
    params:
      text: "readybasic"
      wait_timeout_s: 180
      capture_label: keyboard_regression_readybasic_initial
YAML
  else
    cat <<YAML
  - id: wait_readybasic_initial
    type: screen.wait_contains
    params:
      text: "readybasic"
      wait_timeout_s: 180
      capture_label: keyboard_regression_readybasic_initial
YAML
  fi
}

emit_hotkey_step() {
  local id="$1"
  local hotkey="$2"
  local post_delay="${3:-1.0}"
  local target_context="${4:-readybasic}"
  local effective_mode="$HOTKEY_INPUT_MODE"
  local key_byte=""
  local key_code=""
  local modifiers=""
  local matrix_hex=""
  local shift_hex=""

  case "$hotkey" in
    ctrl_b)
      key_byte="2"
      key_code="11"
      modifiers="      modifiers: [control]"
      matrix_hex="1C"
      shift_hex="04"
      ;;
    f2)
      key_byte="137"
      key_code="120"
      matrix_hex="04"
      shift_hex="01"
      ;;
    *)
      echo "unknown hotkey '$hotkey'" >&2
      exit 1
      ;;
  esac

  if [ "$effective_mode" = "keylog" ] && [ "$target_context" != "readybasic" ]; then
    if [ "$hotkey" = "ctrl_b" ]; then
      effective_mode="keybuf"
    else
      effective_mode="input"
    fi
  fi

  if [ "$effective_mode" = "host" ]; then
    if [ -n "$modifiers" ]; then
      cat <<YAML
  - id: $id
    type: host.key
    params:
      key_code: $key_code
$modifiers
      post_delay_s: $post_delay
YAML
    else
      cat <<YAML
  - id: $id
    type: host.key
    params:
      key_code: $key_code
      post_delay_s: $post_delay
YAML
    fi
  elif [ "$effective_mode" = "keylog" ]; then
    local keylog_lo
    local keylog_hi
    local active_lo
    local active_hi
    keylog_lo="$(printf '%02X' $((KEYLOG_ADDR_DEC & 255)))"
    keylog_hi="$(printf '%02X' $(((KEYLOG_ADDR_DEC >> 8) & 255)))"
    active_lo="$(printf '%02X' $((CHRIN_ACTIVE_DEC & 255)))"
    active_hi="$(printf '%02X' $(((CHRIN_ACTIVE_DEC >> 8) & 255)))"
    cat <<YAML
  - id: ${id}_keylog_stub
    type: memory.write
    params:
      start: 828
      bytes_hex: "A9 01 8D $active_lo $active_hi A9 $shift_hex 8D 8D 02 A9 $matrix_hex 85 CB 20 $keylog_lo $keylog_hi A9 00 8D $active_lo $active_hi 4C 74 A4"
  - id: ${id}_clear_keylog_breakpoints
    type: monitor.command
    params:
      command: "raw:delete"
  - id: $id
    type: monitor.command
    params:
      command: "raw:g 033c"
YAML
  elif [ "$effective_mode" = "keybuf" ]; then
    cat <<YAML
  - id: $id
    type: monitor.command
    params:
      command: "keybuf \\\\x$(printf '%02x' "$key_byte")"
YAML
  else
    cat <<YAML
  - id: $id
    type: input.sequence
    params:
      keys: [$key_byte]
      inter_key_delay_s: 0.03
      post_delay_s: $post_delay
YAML
  fi
}

cd "$READYOS_ROOT"
PUBLIC_VERSION_TEXT="$(python3 build_support/update_build_version.py --current)"
PUBLIC_VERSION="${PUBLIC_VERSION_TEXT%[A-Z]}"
if [ "${READYBASIC_SKIP_BUILD:-0}" != "1" ]; then
  RUN_VERSION_TEXT="$(python3 build_support/update_build_version.py --next)"
  make_args=(
    BUILD_SUPPORT_DIR=build_support
    PROFILE=precog-d81
    READYOS_VERSION_TEXT="$RUN_VERSION_TEXT"
  )
  if [ "$BOOT_MODE" = "runfirst" ]; then
    make_args+=(READYOS_CONFIG_RUN_FIRST=readybasic)
  fi
  make -B "${make_args[@]}" profile
  PUBLIC_VERSION="${RUN_VERSION_TEXT%[A-Z]}"
fi

D81_REL="$(ls -t Releases/$PUBLIC_VERSION/precog-d81/*.d81 | head -1)"
PREBOOT_REL="$(ls -t Releases/$PUBLIC_VERSION/precog-d81/*-preboot.prg | head -1)"
D81="$D81_REL"
PREBOOT="$PREBOOT_REL"

ca65 -l obj/readybasic_keyboard_regression_probe.lst -o obj/readybasic_keyboard_regression_probe_list.o src/apps/readybasic/readybasic.s
CHRIN_ADDR_HEX="$(awk '/rb_chrin:/ { sub(/r.*/, "", $1); sub(/.*:/, "", $1); print $1; exit }' obj/readybasic_keyboard_regression_probe.lst)"
KEYLOG_ADDR_HEX="$(awk '/rb_keylog:/ { sub(/r.*/, "", $1); sub(/.*:/, "", $1); print $1; exit }' obj/readybasic_keyboard_regression_probe.lst)"
CHRIN_ACTIVE_HEX="$(awk '/rb_chrin_active:/ { sub(/r.*/, "", $1); sub(/.*:/, "", $1); print $1; exit }' obj/readybasic_keyboard_regression_probe.lst)"
if [ -z "$CHRIN_ADDR_HEX" ] || [ -z "$KEYLOG_ADDR_HEX" ] || [ -z "$CHRIN_ACTIVE_HEX" ]; then
  echo "Could not find ReadyBASIC keyboard regression symbols in list file" >&2
  exit 1
fi
CHRIN_ADDR_DEC="$((0x1200 + 16#$CHRIN_ADDR_HEX))"
CHRIN_VEC_HEX="$(printf '%02X %02X' $((CHRIN_ADDR_DEC & 255)) $(((CHRIN_ADDR_DEC >> 8) & 255)))"
KEYLOG_ADDR_DEC="$((0xC000 + 16#$KEYLOG_ADDR_HEX))"
KEYLOG_VEC_HEX="$(printf '%02X %02X' $((KEYLOG_ADDR_DEC & 255)) $(((KEYLOG_ADDR_DEC >> 8) & 255)))"
CHRIN_ACTIVE_DEC="$((0x1200 + 16#$CHRIN_ACTIVE_HEX))"
READYBASIC_BANK_HEX="$(printf '%02X' "$((READYBASIC_BANK_RAW))")"
KERNAL_KEYLOG_VEC_HEX="48 EB"
KERNAL_CHRIN_VEC_HEX="57 F1"

cat >"$PLAN" <<YAML
version: 1
kind: vice_task_plan
plan_id: readybasic_keyboard_regression_probe
run_mode: gui_vice
global_defaults:
  monitor_host: 127.0.0.1
  monitor_port_start: 6502
  monitor_port_span: 40
  retry_policy:
    max_attempts: 2
    backoff_ms: 250
    jitter: false
  timeouts:
    launch_s: 45
    step_s: 180
    read_s: 2
  artifact_policy:
    capture_screen: true
    capture_state: true
    capture_dump: false
  vice:
    disk8: "$D81"
    disk9: "$D81"
    autostart_prg: "$PREBOOT"
    drive8_type: 1581
    drive9_type: 1581
    true_drive: false
    close_vice: $VICE_CLOSE
    headless: $VICE_HEADLESS
    speed_percent: 100
steps:
  - id: launch_preboot
    type: vice.launch
    params:
      kill_stale: true
$(emit_initial_entry_steps)
  - id: assert_readybasic_bank_initial
    type: assert.memory
    params:
      start: 51252
      end: 51252
      equals_hex: "$READYBASIC_BANK_HEX"
  - id: assert_chrin_vector_initial
    type: assert.memory
    params:
      start: 804
      end: 805
      equals_hex: "$CHRIN_VEC_HEX"
  - id: assert_keylog_vector_initial
    type: assert.memory
    params:
      start: 655
      end: 656
      equals_hex: "$KEYLOG_VEC_HEX"
  - id: assert_warp_enabled
    type: warp.set
    params:
      enabled: true
      capture_after: false
  - id: type_space_string_once
    type: input.sequence
    params:
      keys: [$(keys $'PRINT "A B C"\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_single_space_string_output
    type: assert.screen
    params:
      contains: "A B C"
  - id: assert_keyboard_buffer_empty_after_space_string
    type: assert.memory
    params:
      start: 198
      end: 198
      equals_hex: "00"
  - id: type_partial_line_10
    type: input.sequence
    params:
      keys: [49,48]
      inter_key_delay_s: 0.03
      post_delay_s: 0.1
$(emit_hotkey_step partial_line_ctrl_b ctrl_b 1.0)
  - id: wait_launcher_after_partial_ctrl_b
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 40
      capture_label: keyboard_regression_launcher_after_partial_ctrl_b
  - id: assert_no_rem_after_partial_ctrl_b
    type: assert.screen_not_contains
    params:
      not_contains: "REM"
  - id: assert_launcher_bank_after_partial_ctrl_b
    type: assert.memory
    params:
      start: 51252
      end: 51252
      equals_hex: "00"
  - id: reenter_readybasic_after_partial_ctrl_b
    type: input.sequence
    params:
      keys: [13]
      inter_key_delay_s: 0.08
      post_delay_s: 2.0
  - id: wait_readybasic_after_partial_ctrl_b
    type: screen.wait_contains
    params:
      text: "ready."
      wait_timeout_s: 60
      capture_label: keyboard_regression_readybasic_after_partial_ctrl_b
  - id: list_after_partial_ctrl_b
    type: input.sequence
    params:
      keys: [$(keys $'LIST\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_no_rem_listed_after_partial_ctrl_b
    type: assert.screen_not_contains
    params:
      not_contains: "REM"
  - id: type_after_partial_ctrl_b
    type: input.sequence
    params:
      keys: [$(keys $'PRINT "AFTERCTRL"\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_type_after_partial_ctrl_b
    type: assert.screen
    params:
      contains: "AFTERCTRL"
$(emit_hotkey_step clean_ctrl_b_to_launcher ctrl_b 1.0)
  - id: wait_launcher_to_load_editor
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 40
  - id: move_readybasic_to_editor
    type: input.sequence
    params:
      keys: [145,145,145,145,13]
      inter_key_delay_s: 0.08
      post_delay_s: 2.0
  - id: wait_editor_loaded
    type: screen.wait_contains
    params:
      text: "editor"
      wait_timeout_s: 60
      capture_label: keyboard_regression_editor_loaded
$(emit_hotkey_step editor_ctrl_b_to_launcher ctrl_b 1.0 app)
  - id: wait_launcher_after_editor_ctrl_b
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 40
  - id: move_editor_to_readybasic
    type: input.sequence
    params:
      keys: [17,17,17,17,13]
      inter_key_delay_s: 0.08
      post_delay_s: 2.0
  - id: wait_readybasic_before_partial_f2
    type: screen.wait_contains
    params:
      text: "ready."
      wait_timeout_s: 60
      capture_label: keyboard_regression_readybasic_before_partial_f2
  - id: type_partial_line_20
    type: input.sequence
    params:
      keys: [50,48]
      inter_key_delay_s: 0.03
      post_delay_s: 0.1
$(emit_hotkey_step partial_line_f2 f2 1.0)
  - id: wait_editor_after_partial_f2
    type: screen.wait_contains
    params:
      text: "editor"
      wait_timeout_s: 60
      capture_label: keyboard_regression_editor_after_partial_f2
  - id: assert_no_rem_after_partial_f2
    type: assert.screen_not_contains
    params:
      not_contains: "REM"
  - id: assert_keylog_restored_after_partial_f2
    type: assert.memory
    params:
      start: 655
      end: 656
      equals_hex: "$KERNAL_KEYLOG_VEC_HEX"
  - id: assert_chrin_restored_after_partial_f2
    type: assert.memory
    params:
      start: 804
      end: 805
      equals_hex: "$KERNAL_CHRIN_VEC_HEX"
  - id: assert_keyboard_buffer_empty_after_partial_f2
    type: assert.memory
    params:
      start: 198
      end: 198
      equals_hex: "00"
YAML

if [ "${READYBASIC_GENERATE_PLAN_ONLY:-0}" = "1" ]; then
  echo "wrote $PLAN"
  exit 0
fi

dotnet build "$PROJECT"
dotnet run --project "$PROJECT" -- run-plan --plan "$PLAN" $CLI_CLOSE_ARG
