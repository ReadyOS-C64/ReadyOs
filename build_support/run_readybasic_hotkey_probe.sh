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
PLAN="${READYBASIC_HOTKEY_PLAN:-$SCRIPT_DIR/readybasic_hotkey_probe.generated.yaml}"
READYBASIC_PRG="${READYBASIC_PRG:-bin/readybasic.prg}"
READYBASIC_VISIBLE="${READYBASIC_VISIBLE:-0}"
READYBASIC_KEEP_VICE="${READYBASIC_KEEP_VICE:-0}"
VICE_HEADLESS="true"
VICE_CLOSE="true"
CLI_CLOSE_ARG="--close-vice"
HOTKEY_INPUT_MODE="${READYBASIC_HOTKEY_INPUT_MODE:-}"
DUPLICATE_KEYLOG="${READYBASIC_HOTKEY_DUPLICATE_KEYLOG:-1}"
STABILITY_WAIT="${READYBASIC_HOTKEY_STABILITY_WAIT:-2.5}"
BOOT_MODE="${READYBASIC_HOTKEY_BOOT_MODE:-runfirst}"
READYBASIC_BANK_RAW="${READYBASIC_HOTKEY_READYBASIC_BANK:-3}"
EDITOR_BANK_RAW="${READYBASIC_HOTKEY_EDITOR_BANK:-1}"
REUVIEWER_BANK_RAW="${READYBASIC_HOTKEY_REUVIEWER_BANK:-2}"
READYBASIC_BANK_HEX="$(printf '%02X' "$((READYBASIC_BANK_RAW))")"
EDITOR_BANK_HEX="$(printf '%02X' "$((EDITOR_BANK_RAW))")"
REUVIEWER_BANK_HEX="$(printf '%02X' "$((REUVIEWER_BANK_RAW))")"
if [ -n "${READYBASIC_HOTKEY_HOST_KEYS+x}" ]; then
  HOST_HOTKEYS="$READYBASIC_HOTKEY_HOST_KEYS"
else
  HOST_HOTKEYS="0"
fi
if [ -z "$HOTKEY_INPUT_MODE" ]; then
  if [ "$HOST_HOTKEYS" = "1" ]; then
    HOTKEY_INPUT_MODE="host"
  else
    HOTKEY_INPUT_MODE="keylog"
  fi
fi
if [ "$HOTKEY_INPUT_MODE" = "host" ]; then
  VICE_HEADLESS="false"
fi
if [ "$READYBASIC_VISIBLE" = "1" ]; then
  VICE_HEADLESS="false"
fi
if [ "$READYBASIC_KEEP_VICE" = "1" ]; then
  VICE_CLOSE="false"
  CLI_CLOSE_ARG=""
fi
case "$BOOT_MODE" in
  runfirst|launcher) ;;
  *)
    echo "READYBASIC_HOTKEY_BOOT_MODE must be runfirst or launcher, got '$BOOT_MODE'" >&2
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

emit_hotkey_step() {
  local id="$1"
  local hotkey="$2"
  local post_delay="${3:-1.0}"
  local target_context="${4:-readybasic}"
  local pre_delay="${5:-1.0}"
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
    f4)
      key_byte="138"
      key_code="118"
      matrix_hex="05"
      shift_hex="01"
      ;;
    *)
      echo "unknown hotkey '$hotkey'" >&2
      exit 1
      ;;
  esac

  if [ "$effective_mode" = "keylog" ] && [ "$target_context" != "readybasic" ]; then
    effective_mode="keybuf"
  fi

  if [ "$effective_mode" = "host" ]; then
    if [ -n "$modifiers" ]; then
      cat <<YAML
  - id: $id
    type: host.key
    params:
      key_code: $key_code
$modifiers
      pre_delay_s: $pre_delay
      post_delay_s: $post_delay
YAML
    else
      cat <<YAML
  - id: $id
    type: host.key
    params:
      key_code: $key_code
      pre_delay_s: $pre_delay
      post_delay_s: $post_delay
YAML
    fi
  elif [ "$effective_mode" = "keybuf" ]; then
    cat <<YAML
  - id: $id
    type: monitor.command
    params:
      command: "keybuf \\\\x$(printf '%02x' "$key_byte")"
YAML
  elif [ "$effective_mode" = "keylog" ]; then
    local keylog_lo
    local keylog_hi
    local stub_hex
    keylog_lo="$(printf '%02X' $((KEYLOG_ADDR_DEC & 255)))"
    keylog_hi="$(printf '%02X' $(((KEYLOG_ADDR_DEC >> 8) & 255)))"
    stub_hex="A9 $shift_hex 8D 8D 02 A9 $matrix_hex 85 CB 20 $keylog_lo $keylog_hi 4C 74 A4"
    if [ "$DUPLICATE_KEYLOG" = "1" ] && [ "$target_context" = "readybasic" ]; then
      stub_hex="A9 $shift_hex 8D 8D 02 A9 $matrix_hex 85 CB 20 $keylog_lo $keylog_hi A9 $shift_hex 8D 8D 02 A9 $matrix_hex 85 CB 20 $keylog_lo $keylog_hi 4C 74 A4"
    fi
    cat <<YAML
  - id: ${id}_keylog_stub
    type: memory.write
    params:
      start: 828
      bytes_hex: "$stub_hex"
  - id: ${id}_clear_keylog_breakpnts
    type: monitor.command
    params:
      command: "raw:delete"
  - id: $id
    type: monitor.command
    params:
      command: "raw:g 033c"
YAML
  else
    cat <<YAML
  - id: $id
    type: input.sequence
    params:
      keys: [$key_byte]
      inter_key_delay_s: 0.03
      pre_delay_s: $pre_delay
      post_delay_s: $post_delay
YAML
  fi
}

emit_hotkey_and_wait() {
  local id="$1"
  local hotkey="$2"
  local expected_text="$3"
  local post_delay="${4:-1.2}"
  local capture_label="${5:-${id}_screen}"
  local pre_delay="${6:-1.0}"
  local target_context="${7:-readybasic}"
  local expected_bank_hex="${8:-}"
  emit_hotkey_step "$id" "$hotkey" "$post_delay" "$target_context" "$pre_delay"
  cat <<YAML
  - id: ${id}_capture_after_input
    type: screen.capture
    params:
      label: "${capture_label}_after_input"
      pitch: "after ${hotkey} input before waiting for ${expected_text}"
  - id: wait_${id}
    type: screen.wait_contains
    params:
      text: "$expected_text"
      wait_timeout_s: 60
      capture_label: "$capture_label"
  - id: assert_keyboard_buffer_empty_${id}
    type: assert.memory
    params:
      start: 198
      end: 198
      equals_hex: "00"
YAML
  if [ -n "$expected_bank_hex" ]; then
    cat <<YAML
  - id: assert_active_bank_${id}
    type: assert.memory
    params:
      start: 51252
      end: 51252
      equals_hex: "$expected_bank_hex"
YAML
  fi
  cat <<YAML
  - id: wait_${id}_stable
    type: screen.wait_contains
    params:
      text: "$expected_text"
      wait_timeout_s: 30
      pre_delay_s: $STABILITY_WAIT
      capture_label: "${capture_label}_stable"
  - id: assert_keyboard_buffer_empty_stable_${id}
    type: assert.memory
    params:
      start: 198
      end: 198
      equals_hex: "00"
YAML
  if [ -n "$expected_bank_hex" ]; then
    cat <<YAML
  - id: assert_active_bank_stable_${id}
    type: assert.memory
    params:
      start: 51252
      end: 51252
      equals_hex: "$expected_bank_hex"
YAML
  fi
}

EXPECT_F4="${READYBASIC_HOTKEY_EXPECT_F4:-editor}"
EXPECT_F2="${READYBASIC_HOTKEY_EXPECT_F2:-editor}"
F4_RETURN_MODE="${READYBASIC_HOTKEY_F4_RETURN_MODE:-launcher}"
SCENARIO="${READYBASIC_HOTKEY_SCENARIO:-full}"

cd "$READYOS_ROOT"
PUBLIC_VERSION_TEXT="$(python3 build_support/update_build_version.py --current)"
PUBLIC_VERSION="${PUBLIC_VERSION_TEXT%[A-Z]}"
if [ "${READYBASIC_SKIP_BUILD:-0}" != "1" ]; then
  RUN_VERSION_TEXT="$(python3 build_support/update_build_version.py --next)"
  RUN_FIRST_BUILD="${READYOS_CONFIG_RUN_FIRST:-readybasic}"
  if [ "$BOOT_MODE" = "launcher" ] || [ "$SCENARIO" = "launcher_cycle" ]; then
    RUN_FIRST_BUILD="launcher"
  fi
  make -B \
    BUILD_SUPPORT_DIR=build_support \
    PROFILE=precog-d81 \
    READYOS_VERSION_TEXT="$RUN_VERSION_TEXT" \
    READYOS_CONFIG_RUN_FIRST="$RUN_FIRST_BUILD" \
    profile
  PUBLIC_VERSION="${RUN_VERSION_TEXT%[A-Z]}"
fi

D81_REL="$(ls -t Releases/$PUBLIC_VERSION/precog-d81/*.d81 | head -1)"
PREBOOT_REL="$(ls -t Releases/$PUBLIC_VERSION/precog-d81/*-preboot.prg | head -1)"
D81="$D81_REL"
PREBOOT="$PREBOOT_REL"
ca65 -l obj/readybasic_hotkey_probe.lst -o obj/readybasic_hotkey_probe_list.o src/apps/readybasic/readybasic.s
PENDING_OFF_HEX="$(awk '/rb_hotkey_pending:/ { sub(/r.*/, "", $1); sub(/.*:/, "", $1); print $1; exit }' obj/readybasic_hotkey_probe.lst)"
if [ -z "$PENDING_OFF_HEX" ]; then
  echo "Could not find rb_hotkey_pending in obj/readybasic_hotkey_probe.lst" >&2
  exit 1
fi
PENDING_DEC="$((0x1200 + 16#$PENDING_OFF_HEX))"
SUPPRESS_OFF_HEX="$(awk '/rb_hotkey_suppress:/ { sub(/r.*/, "", $1); sub(/.*:/, "", $1); print $1; exit }' obj/readybasic_hotkey_probe.lst)"
if [ -z "$SUPPRESS_OFF_HEX" ]; then
  echo "Could not find rb_hotkey_suppress in obj/readybasic_hotkey_probe.lst" >&2
  exit 1
fi
SUPPRESS_DEC="$((0x1200 + 16#$SUPPRESS_OFF_HEX))"
CHRIN_ADDR_HEX="$(awk '/rb_chrin:/ { sub(/r.*/, "", $1); sub(/.*:/, "", $1); print $1; exit }' obj/readybasic_hotkey_probe.lst)"
if [ -z "$CHRIN_ADDR_HEX" ]; then
  echo "Could not find rb_chrin in obj/readybasic_hotkey_probe.lst" >&2
  exit 1
fi
CHRIN_ADDR_DEC="$((0x1200 + 16#$CHRIN_ADDR_HEX))"
CHRIN_VEC_HEX="$(printf '%02X %02X' $((CHRIN_ADDR_DEC & 255)) $(((CHRIN_ADDR_DEC >> 8) & 255)))"
KEYLOG_ADDR_HEX="$(awk '/rb_keylog:/ { sub(/r.*/, "", $1); sub(/.*:/, "", $1); print $1; exit }' obj/readybasic_hotkey_probe.lst)"
if [ -z "$KEYLOG_ADDR_HEX" ]; then
  echo "Could not find rb_keylog in obj/readybasic_hotkey_probe.lst" >&2
  exit 1
fi
KEYLOG_ADDR_DEC="$((0xC000 + 16#$KEYLOG_ADDR_HEX))"
KEYLOG_VEC_HEX="$(printf '%02X %02X' $((KEYLOG_ADDR_DEC & 255)) $(((KEYLOG_ADDR_DEC >> 8) & 255)))"
CHRIN_DISPATCH_HEX="$(awk '/rb_hotkey_chrin_dispatch:/ { sub(/r.*/, "", $1); sub(/.*:/, "", $1); print $1; exit }' obj/readybasic_hotkey_probe.lst)"
if [ -z "$CHRIN_DISPATCH_HEX" ]; then
  echo "Could not find rb_hotkey_chrin_dispatch in obj/readybasic_hotkey_probe.lst" >&2
  exit 1
fi
CHRIN_DISPATCH_DEC="$((0x1200 + 16#$CHRIN_DISPATCH_HEX))"
if [ "$SCENARIO" = "f2_only" ]; then
cat >"$PLAN" <<YAML
version: 1
kind: vice_task_plan
plan_id: readybasic_hotkey_f2_probe
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
  - id: wait_readybasic_prompt
    type: screen.wait_contains
    params:
      text: "readybasic"
      wait_timeout_s: 180
      capture_label: readybasic_hotkey_f2_prompt
  - id: assert_chrin_vector_installed
    type: assert.memory
    params:
      start: 804
      end: 805
      equals_hex: "$CHRIN_VEC_HEX"
  - id: assert_keylog_vector_installed
    type: assert.memory
    params:
      start: 655
      end: 656
      equals_hex: "$KEYLOG_VEC_HEX"
$(emit_hotkey_step f2_to_next_loaded_app f2 1.0)
  - id: wait_target_after_f2
    type: screen.wait_contains
    params:
      text: "$EXPECT_F2"
      wait_timeout_s: 60
      capture_label: target_after_readybasic_f2
  - id: assert_keyboard_buffer_empty_after_f2
    type: assert.memory
    params:
      start: 198
      end: 198
      equals_hex: "00"
  - id: wait_target_stable_after_f2
    type: screen.wait_contains
    params:
      text: "$EXPECT_F2"
      wait_timeout_s: 30
      pre_delay_s: $STABILITY_WAIT
      capture_label: target_stable_after_readybasic_f2
  - id: assert_keyboard_buffer_empty_stable_after_f2
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

  cd "$READYOS_ROOT"
  dotnet build "$PROJECT"
  dotnet run --project "$PROJECT" -- run-plan --plan "$PLAN" $CLI_CLOSE_ARG
  exit $?
fi

if [ "$SCENARIO" = "launcher_cycle" ]; then
  if [ "$BOOT_MODE" != "launcher" ]; then
    echo "launcher_cycle scenario requires READYBASIC_HOTKEY_BOOT_MODE=launcher" >&2
    exit 1
  fi
cat >"$PLAN" <<YAML
version: 1
kind: vice_task_plan
plan_id: readybasic_hotkey_launcher_cycle
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
  - id: wait_launcher_initial
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 300
      pre_delay_s: 8.0
      capture_on_success: true
      capture_label: readybasic_hotkey_launcher_initial
  - id: move_to_editor_for_preload
    type: input.sequence
    params:
      keys: [17,17]
      inter_key_delay_s: 0.10
      post_delay_s: 0.8
  - id: move_to_editor_for_preload_capture
    type: screen.capture
    params:
      label: readybasic_hotkey_launcher_editor_selected
      pitch: after selecting Editor in launcher before F3 preload
  - id: load_editor_to_reu
    type: input.sequence
    params:
      keys: [134]
      inter_key_delay_s: 0.10
      post_delay_s: 8.0
  - id: load_editor_to_reu_capture
    type: screen.capture
    params:
      label: readybasic_hotkey_launcher_editor_preloaded
      pitch: after F3 loading Editor to REU
  - id: assert_editor_preload_label
    type: assert.screen
    params:
      contains: "EDITOR"
  - id: wait_editor_preloaded
    type: screen.wait_contains
    params:
      text: "PRESS ANY KEY"
      wait_timeout_s: 60
      capture_on_success: true
      capture_label: readybasic_hotkey_editor_preload_done
  - id: dismiss_editor_preload_done
    type: input.sequence
    params:
      keys: [32]
      inter_key_delay_s: 0.10
      post_delay_s: 1.0
  - id: wait_launcher_after_editor_preload
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 40
      capture_on_success: true
      capture_label: readybasic_hotkey_launcher_after_editor_preload
  - id: move_editor_to_reuviewer_for_preload
    type: input.sequence
    params:
      keys: [17,17,17,17,17,17,17]
      inter_key_delay_s: 0.10
      post_delay_s: 0.8
  - id: move_editor_to_reuviewer_for_preload_capture
    type: screen.capture
    params:
      label: readybasic_hotkey_launcher_reuviewer_selected
      pitch: after selecting REU Viewer in launcher before F3 preload
  - id: load_reuviewer_to_reu
    type: input.sequence
    params:
      keys: [134]
      inter_key_delay_s: 0.10
      post_delay_s: 8.0
  - id: load_reuviewer_to_reu_capture
    type: screen.capture
    params:
      label: readybasic_hotkey_launcher_reuviewer_preloaded
      pitch: after F3 loading REU Viewer to REU
  - id: assert_reuviewer_preload_label
    type: assert.screen
    params:
      contains: "REU VIEWER"
  - id: wait_reuviewer_preloaded
    type: screen.wait_contains
    params:
      text: "PRESS ANY KEY"
      wait_timeout_s: 60
      capture_on_success: true
      capture_label: readybasic_hotkey_reuviewer_preload_done
  - id: dismiss_reuviewer_preload_done
    type: input.sequence
    params:
      keys: [32]
      inter_key_delay_s: 0.10
      post_delay_s: 1.0
  - id: wait_launcher_after_reuviewer_preload
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 40
      capture_on_success: true
      capture_label: readybasic_hotkey_launcher_after_reuviewer_preload
  - id: launch_readybasic_from_reuviewer_position
    type: input.sequence
    params:
      keys: [145,145,145,13]
      inter_key_delay_s: 0.10
      post_delay_s: 2.5
  - id: launch_readybasic_from_reuviewer_position_capture
    type: screen.capture
    params:
      label: readybasic_hotkey_launcher_after_readybasic_nav
      pitch: after navigating launcher from REU Viewer to ReadyBASIC
  - id: wait_readybasic_prompt
    type: screen.wait_contains
    params:
      text: "readybasic"
      wait_timeout_s: 180
      capture_on_success: true
      capture_label: readybasic_hotkey_prompt
  - id: assert_readybasic_initial_bank
    type: assert.memory
    params:
      start: 51252
      end: 51252
      equals_hex: "$READYBASIC_BANK_HEX"
  - id: assert_chrin_vector_installed
    type: assert.memory
    params:
      start: 804
      end: 805
      equals_hex: "$CHRIN_VEC_HEX"
  - id: assert_keylog_vector_installed
    type: assert.memory
    params:
      start: 655
      end: 656
      equals_hex: "$KEYLOG_VEC_HEX"
  - id: seed_readybasic_list_program
    type: input.sequence
    params:
      keys: [$(keys $'10 PRINT "HOT CYCLE"\rLIST\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: seed_readybasic_list_program_capture
    type: screen.capture
    params:
      label: readybasic_hotkey_seed_list_program
      pitch: after listing the seed program in ReadyBASIC
  - id: assert_readybasic_list_program
    type: assert.screen
    params:
      contains: "10 PRINT \"HOT CYCLE\""
$(emit_hotkey_and_wait f2_round1_to_editor f2 "EDITOR:" 1.2 readybasic_hotkey_f2_round1_editor 1.0 readybasic "$EDITOR_BANK_HEX")
$(emit_hotkey_and_wait f2_round1_to_reuviewer f2 "REU MEMORY" 1.2 readybasic_hotkey_f2_round1_reuviewer 1.0 app "$REUVIEWER_BANK_HEX")
$(emit_hotkey_and_wait f2_round1_back_to_readybasic f2 "ready." 1.2 readybasic_hotkey_f2_round1_readybasic 1.0 app "$READYBASIC_BANK_HEX")
$(emit_hotkey_and_wait f2_round2_to_editor f2 "EDITOR:" 1.2 readybasic_hotkey_f2_round2_editor 1.0 readybasic "$EDITOR_BANK_HEX")
$(emit_hotkey_and_wait f2_round2_to_reuviewer f2 "REU MEMORY" 1.2 readybasic_hotkey_f2_round2_reuviewer 1.0 app "$REUVIEWER_BANK_HEX")
$(emit_hotkey_and_wait f2_round2_back_to_readybasic f2 "ready." 1.2 readybasic_hotkey_f2_round2_readybasic 1.0 app "$READYBASIC_BANK_HEX")
$(emit_hotkey_and_wait f4_round1_to_reuviewer f4 "REU MEMORY" 1.2 readybasic_hotkey_f4_round1_reuviewer 1.0 readybasic "$REUVIEWER_BANK_HEX")
$(emit_hotkey_and_wait f4_round1_to_editor f4 "EDITOR:" 1.2 readybasic_hotkey_f4_round1_editor 1.0 app "$EDITOR_BANK_HEX")
$(emit_hotkey_and_wait f4_round1_back_to_readybasic f4 "ready." 1.2 readybasic_hotkey_f4_round1_readybasic 1.0 app "$READYBASIC_BANK_HEX")
$(emit_hotkey_and_wait f4_round2_to_reuviewer f4 "REU MEMORY" 1.2 readybasic_hotkey_f4_round2_reuviewer 1.0 readybasic "$REUVIEWER_BANK_HEX")
$(emit_hotkey_and_wait f4_round2_to_editor f4 "EDITOR:" 1.2 readybasic_hotkey_f4_round2_editor 1.0 app "$EDITOR_BANK_HEX")
$(emit_hotkey_and_wait f4_round2_back_to_readybasic f4 "ready." 1.2 readybasic_hotkey_f4_round2_readybasic 1.0 app "$READYBASIC_BANK_HEX")
  - id: list_after_cycles_before_ctrl_b
    type: input.sequence
    params:
      keys: [76,73,83,84,13]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: list_after_cycles_before_ctrl_b_capture
    type: screen.capture
    params:
      label: readybasic_hotkey_list_after_cycles_before_ctrl_b
      pitch: after LIST following two F2 and two F4 loaded-app cycles
  - id: assert_list_after_cycles_before_ctrl_b
    type: assert.screen
    params:
      contains: "HOT CYCLE"
  - id: run_after_cycles_before_ctrl_b
    type: input.sequence
    params:
      keys: [82,85,78,13]
      inter_key_delay_s: 0.03
      post_delay_s: 1.5
  - id: run_after_cycles_before_ctrl_b_capture
    type: screen.capture
    params:
      label: readybasic_hotkey_run_after_cycles_before_ctrl_b
      pitch: after RUN following two F2 and two F4 loaded-app cycles
  - id: assert_run_after_cycles_before_ctrl_b
    type: assert.screen
    params:
      contains: "HOT CYCLE"
  - id: debug_hotkey_state_before_ctrl_b_after_cycles
    type: monitor.command
    params:
      command: "raw:m $(printf '%04x' "$PENDING_DEC") $(printf '%04x' "$CHRIN_DISPATCH_DEC")"
  - id: debug_basic_state_before_ctrl_b_after_cycles
    type: monitor.command
    params:
      command: "raw:m 0039 003a; m 007a 007b; m 0099 0099; m 00c5 00cb; m 028d 028d"
$(emit_hotkey_step ctrl_b_from_readybasic_after_cycles ctrl_b 1.0 readybasic 3.0)
  - id: debug_hotkey_state_after_ctrl_b_after_cycles
    type: monitor.command
    params:
      command: "raw:m $(printf '%04x' "$PENDING_DEC") $(printf '%04x' "$CHRIN_DISPATCH_DEC")"
  - id: debug_basic_state_after_ctrl_b_after_cycles
    type: monitor.command
    params:
      command: "raw:m 0039 003a; m 007a 007b; m 0099 0099; m 00c5 00cb; m 028d 028d"
  - id: ctrl_b_from_readybasic_after_cycles_capture
    type: screen.capture
    params:
      label: readybasic_hotkey_ctrl_b_after_cycles
      pitch: after Ctrl+B at end of cycling before launcher wait
  - id: wait_launcher_after_readybasic_ctrl_b
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 40
      capture_on_success: true
      capture_label: readybasic_hotkey_launcher_after_readybasic_ctrl_b
  - id: assert_launcher_bank_after_readybasic_ctrl_b
    type: assert.memory
    params:
      start: 51252
      end: 51252
      equals_hex: "00"
  - id: reenter_readybasic_after_ctrl_b
    type: input.sequence
    params:
      keys: [13]
      inter_key_delay_s: 0.08
      post_delay_s: 2.0
  - id: reenter_readybasic_after_ctrl_b_capture
    type: screen.capture
    params:
      label: readybasic_hotkey_reenter_after_ctrl_b
      pitch: after returning from launcher before ReadyBASIC reentry wait
  - id: wait_readybasic_after_ctrl_b
    type: screen.wait_contains
    params:
      text: "ready."
      wait_timeout_s: 60
      capture_label: readybasic_hotkey_readybasic_after_ctrl_b
  - id: assert_readybasic_bank_after_ctrl_b
    type: assert.memory
    params:
      start: 51252
      end: 51252
      equals_hex: "$READYBASIC_BANK_HEX"
  - id: final_list_after_reentry
    type: input.sequence
    params:
      keys: [76,73,83,84,13]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: final_list_after_reentry_capture
    type: screen.capture
    params:
      label: readybasic_hotkey_final_list_after_reentry
      pitch: after LIST on final ReadyBASIC reentry
  - id: assert_final_list_after_reentry
    type: assert.screen
    params:
      contains: "HOT CYCLE"
  - id: final_run_after_reentry
    type: input.sequence
    params:
      keys: [82,85,78,13]
      inter_key_delay_s: 0.03
      post_delay_s: 1.5
  - id: final_run_after_reentry_capture
    type: screen.capture
    params:
      label: readybasic_hotkey_final_run_after_reentry
      pitch: after RUN on final ReadyBASIC reentry
  - id: assert_final_run_output
    type: assert.screen
    params:
      contains: "HOT CYCLE"
  - id: exit_after_final_run
    type: input.sequence
    params:
      keys: [69,88,73,84,13]
      inter_key_delay_s: 0.03
      post_delay_s: 0.4
  - id: exit_after_final_run_capture
    type: screen.capture
    params:
      label: readybasic_hotkey_exit_after_final_run
      pitch: after EXIT before launcher wait
  - id: wait_launcher_after_exit
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 30
      pre_delay_s: 2.0
      capture_on_success: true
      capture_label: readybasic_hotkey_launcher_after_exit
  - id: assert_launcher_stable_after_exit
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 20
      pre_delay_s: 2.5
      capture_on_success: true
      capture_label: readybasic_hotkey_launcher_stable_after_exit
YAML

  if [ "${READYBASIC_GENERATE_PLAN_ONLY:-0}" = "1" ]; then
    echo "wrote $PLAN"
    exit 0
  fi

  cd "$READYOS_ROOT"
  dotnet build "$PROJECT"
  dotnet run --project "$PROJECT" -- run-plan --plan "$PLAN" $CLI_CLOSE_ARG
  exit $?
fi

cat >"$PLAN" <<YAML
version: 1
kind: vice_task_plan
plan_id: readybasic_hotkey_probe
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
  - id: wait_readybasic_prompt
    type: screen.wait_contains
    params:
      text: "readybasic"
      wait_timeout_s: 180
      capture_label: readybasic_hotkey_prompt
  - id: assert_chrin_vector_installed
    type: assert.memory
    params:
      start: 804
      end: 805
      equals_hex: "$CHRIN_VEC_HEX"
  - id: assert_keylog_vector_installed
    type: assert.memory
    params:
      start: 655
      end: 656
      equals_hex: "$KEYLOG_VEC_HEX"
  - id: seed_readybasic_state
    type: input.sequence
    params:
      keys: [$(keys $'V%=321:VS$="HOT"\rPRINT "HOTSEED";V%;":";VS$\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: seed_readybasic_state_capture
    type: screen.capture
    params:
      label: seed_readybasic_state
      pitch: after seeding ReadyBASIC state before hotkey tests
  - id: assert_seed_state
    type: assert.screen
    params:
      contains: "HOTSEED 321 :HOT"
$(emit_hotkey_step ctrl_b_from_readybasic ctrl_b 0.05)
  - id: ctrl_b_from_readybasic_capture
    type: screen.capture
    params:
      label: readybasic_hotkey_ctrl_b_from_readybasic_after_input
      pitch: after Ctrl+B input before launcher wait
  - id: wait_launcher_after_ctrl_b
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 40
      capture_label: launcher_after_readybasic_ctrl_b
  - id: assert_keyboard_buffer_empty_after_ctrl_b
    type: assert.memory
    params:
      start: 198
      end: 198
      equals_hex: "00"
  - id: move_readybasic_to_editor
    type: input.sequence
    params:
      keys: [145,145,145,145,13]
      inter_key_delay_s: 0.08
      post_delay_s: 2.0
  - id: move_readybasic_to_editor_capture
    type: screen.capture
    params:
      label: readybasic_hotkey_readybasic_to_editor_menu
      pitch: after navigating to editor from launcher
  - id: wait_editor_loaded
    type: screen.wait_contains
    params:
      text: "editor"
      wait_timeout_s: 60
      capture_label: editor_loaded_for_hotkey_probe
$(emit_hotkey_step ctrl_b_from_editor ctrl_b 1.0 app)
  - id: ctrl_b_from_editor_capture
    type: screen.capture
    params:
      label: readybasic_hotkey_ctrl_b_from_editor_after_input
      pitch: after Ctrl+B input from editor before launcher wait
  - id: wait_launcher_after_editor
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 40
  - id: move_editor_to_readybasic_for_f4
    type: input.sequence
    params:
      keys: [17,17,17,17,13]
      inter_key_delay_s: 0.08
      post_delay_s: 2.0
  - id: move_editor_to_readybasic_for_f4_capture
    type: screen.capture
    params:
      label: readybasic_hotkey_editor_to_readybasic_menu
      pitch: after navigating back to ReadyBASIC before F4 test
  - id: wait_readybasic_resume_for_f4
    type: screen.wait_contains
    params:
      text: "ready."
      wait_timeout_s: 60
      capture_label: readybasic_resume_before_f4
  - id: assert_readybasic_state_after_ctrl_b
    type: input.sequence
    params:
      keys: [$(keys $'PRINT "HOTSTATE";V%;":";VS$\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_readybasic_state_after_ctrl_b_capture
    type: screen.capture
    params:
      label: readybasic_hotkey_state_after_ctrl_b
      pitch: after typing HOTSTATE verification command
  - id: assert_hot_state_after_ctrl_b
    type: assert.screen
    params:
      contains: "HOTSTATE 321 :HOT"
  - id: build_list_timing_program
    type: input.sequence
    params:
      keys: [$(keys $'10 PRINT "HOTLIST1"\r20 PRINT "HOTLIST2"\r30 PRINT "HOTLIST3"\rLIST\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.2
  - id: build_list_timing_program_capture
    type: screen.capture
    params:
      label: readybasic_hotkey_list_before_f4
      pitch: after building LIST program before F4
  - id: wait_list_visible_before_f4
    type: screen.wait_contains
    params:
      text: "HOTLIST3"
      wait_timeout_s: 20
      capture_label: readybasic_list_before_f4
$(emit_hotkey_step f4_to_prev_loaded_app f4 1.0)
  - id: wait_target_after_f4
    type: screen.wait_contains
    params:
      text: "$EXPECT_F4"
      wait_timeout_s: 60
      capture_label: target_after_readybasic_f4
  - id: assert_keyboard_buffer_empty_after_f4
    type: assert.memory
    params:
      start: 198
      end: 198
      equals_hex: "00"
  - id: wait_target_stable_after_f4
    type: screen.wait_contains
    params:
      text: "$EXPECT_F4"
      wait_timeout_s: 30
      pre_delay_s: $STABILITY_WAIT
      capture_label: target_stable_after_readybasic_f4
  - id: assert_keyboard_buffer_empty_stable_after_f4
    type: assert.memory
    params:
      start: 198
      end: 198
      equals_hex: "00"
YAML

if [ "$F4_RETURN_MODE" = "stop_after_f4" ]; then
  :
elif [ "$F4_RETURN_MODE" = "split_easyflash" ]; then
cat >>"$PLAN" <<YAML
  - id: relaunch_easyflash_for_f2
    type: vice.launch
    params:
      kill_stale: true
  - id: wait_easyflash_launcher_for_f2
    type: screen.wait_contains
    params:
      text: "READY OS"
      pre_delay_s: 4
      poll_s: 0.5
      wait_timeout_s: 240
      capture_label: easyflash_readybasic_launcher_for_f2
  - id: start_readybasic_for_f2
    type: input.sequence
    params:
      keys: [17,17,17,17,13]
      inter_key_delay_s: 0.08
      post_delay_s: 2.5
  - id: wait_readybasic_resume_for_f2
    type: screen.wait_contains
    params:
      text: "readybasic"
      wait_timeout_s: 180
      capture_label: readybasic_resume_before_f2
YAML
elif [ "$F4_RETURN_MODE" = "next_to_readybasic" ]; then
cat >>"$PLAN" <<YAML
  - id: f2_from_f4_target_to_readybasic
    type: input.sequence
    params:
      keys: [137]
      inter_key_delay_s: 0.03
      post_delay_s: 1.5
  - id: wait_readybasic_resume_for_f2
    type: screen.wait_contains
    params:
      text: "ready."
      wait_timeout_s: 60
      capture_label: readybasic_resume_before_f2
YAML
else
cat >>"$PLAN" <<YAML
$(emit_hotkey_step ctrl_b_from_f4_target ctrl_b 1.0 app)
  - id: wait_launcher_after_f4_target
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 40
  - id: move_editor_to_readybasic_for_f2
    type: input.sequence
    params:
      # F4 switched ReadyBASIC directly to another app, so the launcher's saved
      # selection remains ReadyBASIC when that app returns via CTRL+B.
      keys: [13]
      inter_key_delay_s: 0.08
      post_delay_s: 2.0
  - id: wait_readybasic_resume_for_f2
    type: screen.wait_contains
    params:
      text: "ready."
      wait_timeout_s: 60
      capture_label: readybasic_resume_before_f2
YAML
fi

if [ "$F4_RETURN_MODE" != "stop_after_f4" ]; then
cat >>"$PLAN" <<YAML
$(emit_hotkey_step f2_to_next_loaded_app f2 1.0)
  - id: wait_target_after_f2
    type: screen.wait_contains
    params:
      text: "$EXPECT_F2"
      wait_timeout_s: 60
      capture_label: target_after_readybasic_f2
  - id: assert_keyboard_buffer_empty_after_f2
    type: assert.memory
    params:
      start: 198
      end: 198
      equals_hex: "00"
  - id: wait_target_stable_after_f2
    type: screen.wait_contains
    params:
      text: "$EXPECT_F2"
      wait_timeout_s: 30
      pre_delay_s: $STABILITY_WAIT
      capture_label: target_stable_after_readybasic_f2
  - id: assert_keyboard_buffer_empty_stable_after_f2
    type: assert.memory
    params:
      start: 198
      end: 198
      equals_hex: "00"
YAML
fi

if [ "${READYBASIC_GENERATE_PLAN_ONLY:-0}" = "1" ]; then
  echo "wrote $PLAN"
  exit 0
fi

cd "$READYOS_ROOT"
dotnet build "$PROJECT"
dotnet run --project "$PROJECT" -- run-plan --plan "$PLAN" $CLI_CLOSE_ARG
