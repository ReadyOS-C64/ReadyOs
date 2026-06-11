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
PLAN="${READYBASIC_REUVIEWER_CHAIN_PLAN:-$SCRIPT_DIR/readybasic_reuviewer_f2_chain_probe.generated.yaml}"
READYBASIC_VISIBLE="${READYBASIC_VISIBLE:-0}"
READYBASIC_KEEP_VICE="${READYBASIC_KEEP_VICE:-0}"
HOTKEY_INPUT_MODE="${READYBASIC_HOTKEY_INPUT_MODE:-pending}"
HOTKEY_DEBUG="${READYBASIC_HOTKEY_DEBUG:-0}"
BOOT_MODE="${READYBASIC_CHAIN_BOOT_MODE:-runfirst}"
READYBASIC_BANK_RAW="${READYBASIC_CHAIN_READYBASIC_BANK:-1}"
REUVIEWER_BANK_RAW="${READYBASIC_CHAIN_REUVIEWER_BANK:-4}"
RESOURCE_BANKS_RAW="${READYBASIC_CHAIN_RESOURCE_BANKS:-}"
CONSTRAIN_BITMAP="${READYBASIC_CHAIN_CONSTRAIN_BITMAP:-0}"
VICE_HEADLESS="true"
VICE_CLOSE="true"
CLI_CLOSE_ARG="--close-vice"

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

keys() {
  python3 - "$1" <<'PY'
import sys
s = sys.argv[1]
print(",".join(str(ord(ch)) for ch in s))
PY
}

READYBASIC_BANK="$((READYBASIC_BANK_RAW))"
REUVIEWER_BANK="$((REUVIEWER_BANK_RAW))"
if [ "$READYBASIC_BANK" -lt 1 ] || [ "$READYBASIC_BANK" -gt 23 ]; then
  echo "READYBASIC_CHAIN_READYBASIC_BANK must be in 1..23, got '$READYBASIC_BANK_RAW'" >&2
  exit 1
fi
if [ "$REUVIEWER_BANK" -lt 1 ] || [ "$REUVIEWER_BANK" -gt 23 ]; then
  echo "READYBASIC_CHAIN_REUVIEWER_BANK must be in 1..23, got '$REUVIEWER_BANK_RAW'" >&2
  exit 1
fi
if [ "$READYBASIC_BANK" -eq "$REUVIEWER_BANK" ]; then
  echo "ReadyBASIC and REUViewer chain banks must differ" >&2
  exit 1
fi
case "$BOOT_MODE" in
  runfirst|launcher) ;;
  *)
    echo "READYBASIC_CHAIN_BOOT_MODE must be runfirst or launcher, got '$BOOT_MODE'" >&2
    exit 1
    ;;
esac

READYBASIC_BANK_HEX="$(printf '%02X' "$READYBASIC_BANK")"
REUVIEWER_BANK_HEX="$(printf '%02X' "$REUVIEWER_BANK")"

if [ -z "$RESOURCE_BANKS_RAW" ] && [ "$READYBASIC_BANK" -eq 1 ] && [ "$REUVIEWER_BANK" -eq 4 ]; then
  RESOURCE_BANKS_RAW="2,3"
fi

bitmap_add_bank() {
  local bank="$1"
  if [ "$bank" -lt 1 ] || [ "$bank" -gt 23 ]; then
    echo "chain bitmap bank must be in 1..23, got '$bank'" >&2
    exit 1
  fi
  if [ "$bank" -lt 8 ]; then
    BITMAP_LO=$((BITMAP_LO | (1 << bank)))
  elif [ "$bank" -lt 16 ]; then
    BITMAP_HI=$((BITMAP_HI | (1 << (bank - 8))))
  else
    BITMAP_XHI=$((BITMAP_XHI | (1 << (bank - 16))))
  fi
}

BITMAP_LO=0
BITMAP_HI=0
BITMAP_XHI=0
bitmap_add_bank "$READYBASIC_BANK"
bitmap_add_bank "$REUVIEWER_BANK"
if [ -n "$RESOURCE_BANKS_RAW" ]; then
  IFS=',' read -r -a RESOURCE_BANKS <<<"$RESOURCE_BANKS_RAW"
  for raw_bank in "${RESOURCE_BANKS[@]}"; do
    bank="${raw_bank//[[:space:]]/}"
    [ -z "$bank" ] && continue
    bitmap_add_bank "$((bank))"
  done
fi
CHAIN_BITMAP_HEX="$(printf '%02X %02X %02X' "$BITMAP_LO" "$BITMAP_HI" "$BITMAP_XHI")"

emit_bitmap_control_steps() {
  if [ "$CONSTRAIN_BITMAP" = "1" ] || [ -n "$RESOURCE_BANKS_RAW" ]; then
    cat <<YAML
  - id: constrain_loaded_bitmap_for_readybasic_reuviewer_chain
    type: memory.write
    params:
      start: 51254
      bytes_hex: "$CHAIN_BITMAP_HEX"
YAML
  else
    cat <<YAML
  - id: capture_loaded_bitmap_before_reuviewer_f2
    type: monitor.command
    params:
      command: "raw:m c836 c838"
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
  local key_hex=""
  local key_code=""
  local modifiers=""
  local matrix_hex=""
  local shift_hex=""

  case "$hotkey" in
    ctrl_b)
      key_byte="2"
      key_hex="02"
      key_code="11"
      modifiers="      modifiers: [control]"
      matrix_hex="1C"
      shift_hex="04"
      ;;
    f2)
      key_byte="137"
      key_hex="89"
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
      effective_mode="keyd"
    fi
  fi
  if [ "$effective_mode" = "pending" ] && [ "$target_context" != "readybasic" ]; then
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
    keylog_lo="$(printf '%02X' $((KEYLOG_ADDR_DEC & 255)))"
    keylog_hi="$(printf '%02X' $(((KEYLOG_ADDR_DEC >> 8) & 255)))"
    cat <<YAML
  - id: ${id}_keylog_stub
    type: memory.write
    params:
      start: 828
      bytes_hex: "A9 $shift_hex 8D 8D 02 A9 $matrix_hex 85 CB 20 $keylog_lo $keylog_hi 4C CF E5"
  - id: ${id}_clear_keylog_breakpoints
    type: monitor.command
    params:
      command: "raw:delete"
  - id: ${id}_break_after_keylog
    type: monitor.command
    params:
      command: "raw:break 0348"
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
  elif [ "$effective_mode" = "pending" ]; then
    cat <<YAML
  - id: ${id}_set_readybasic_pending_hotkey
    type: memory.write
    params:
      start: $PENDING_DEC
      bytes_hex: "$key_hex"
  - id: ${id}_assert_readybasic_pending_hotkey
    type: assert.memory
    params:
      start: $PENDING_DEC
      end: $PENDING_DEC
      equals_hex: "$key_hex"
  - id: ${id}_queue_readybasic_hotkey_line
    type: memory.write
    params:
      start: 631
      bytes_hex: "52 45 4D 0D"
  - id: $id
    type: memory.write
    params:
      start: 198
      bytes_hex: "04"
YAML
    if [ "$HOTKEY_DEBUG" = "1" ]; then
      cat <<YAML
  - id: ${id}_capture_after_readybasic_hotkey_line
    type: screen.capture
    params:
      label: ${id}_after_readybasic_hotkey_line
  - id: ${id}_assert_pending_cleared_after_hotkey_line
    type: assert.memory
    params:
      start: $PENDING_DEC
      end: $PENDING_DEC
      equals_hex: "00"
  - id: ${id}_registers_after_hotkey_line
    type: monitor.command
    params:
      command: "raw:r"
YAML
    fi
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

emit_initial_entry_steps() {
  if [ "$BOOT_MODE" = "launcher" ]; then
    cat <<YAML
  - id: wait_launcher_initial
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 180
      capture_label: chain_launcher_initial
  - id: launch_readybasic_from_launcher
    type: input.sequence
    params:
      keys: [17,17,17,17,17,17,13]
      inter_key_delay_s: 0.08
      post_delay_s: 2.5
  - id: wait_readybasic_initial
    type: screen.wait_contains
    params:
      text: "READYBASIC"
      wait_timeout_s: 120
      capture_label: chain_readybasic_initial
YAML
  else
    cat <<YAML
  - id: wait_readybasic_initial
    type: screen.wait_contains
    params:
      text: "READYBASIC"
      wait_timeout_s: 180
      capture_label: chain_readybasic_initial
YAML
  fi
}

emit_wait_readybasic_warm_steps() {
  local id="$1"
  local label="$2"
  cat <<YAML
  - id: capture_${id}
    type: screen.capture
    params:
      label: $label
      note: warm ReadyBASIC entry evidence; bank/vector assertions and typed sentinel prove keyboard liveness
YAML
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
  make -B \
    "${make_args[@]}" \
    profile
  PUBLIC_VERSION="${RUN_VERSION_TEXT%[A-Z]}"
fi

D81_REL="$(ls -t Releases/$PUBLIC_VERSION/precog-d81/*.d81 | head -1)"
PREBOOT_REL="$(ls -t Releases/$PUBLIC_VERSION/precog-d81/*-preboot.prg | head -1)"
D81="$D81_REL"
PREBOOT="$PREBOOT_REL"

ca65 -l obj/readybasic_reuviewer_f2_chain_probe.lst -o obj/readybasic_reuviewer_f2_chain_probe_list.o src/apps/readybasic/readybasic.s
IRQ_ADDR_HEX="$(awk '/rb_irq:/ { sub(/r.*/, "", $1); sub(/.*:/, "", $1); print $1; exit }' obj/readybasic_reuviewer_f2_chain_probe.lst)"
KEYLOG_ADDR_HEX="$(awk '/rb_keylog:/ { sub(/r.*/, "", $1); sub(/.*:/, "", $1); print $1; exit }' obj/readybasic_reuviewer_f2_chain_probe.lst)"
PENDING_OFF_HEX="$(awk '/rb_hotkey_pending:/ { sub(/r.*/, "", $1); sub(/.*:/, "", $1); print $1; exit }' obj/readybasic_reuviewer_f2_chain_probe.lst)"
if [ -z "$IRQ_ADDR_HEX" ] || [ -z "$KEYLOG_ADDR_HEX" ] || [ -z "$PENDING_OFF_HEX" ]; then
  echo "Could not find ReadyBASIC hotkey symbols in list file" >&2
  exit 1
fi
IRQ_ADDR_DEC="$((0x1200 + 16#$IRQ_ADDR_HEX))"
IRQ_VEC_HEX="$(printf '%02X %02X' $((IRQ_ADDR_DEC & 255)) $(((IRQ_ADDR_DEC >> 8) & 255)))"
KEYLOG_ADDR_DEC="$((0xC000 + 16#$KEYLOG_ADDR_HEX))"
KEYLOG_VEC_HEX="$(printf '%02X %02X' $((KEYLOG_ADDR_DEC & 255)) $(((KEYLOG_ADDR_DEC >> 8) & 255)))"
PENDING_DEC="$((0x1200 + 16#$PENDING_OFF_HEX))"
KERNAL_KEYLOG_VEC_HEX="48 EB"
KERNAL_IRQ_VEC_HEX="31 EA"

cat >"$PLAN" <<YAML
version: 1
kind: vice_task_plan
plan_id: readybasic_reuviewer_f2_chain_probe
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
  - id: assert_irq_vector_initial
    type: assert.memory
    params:
      start: 788
      end: 789
      equals_hex: "$IRQ_VEC_HEX"
  - id: assert_keylog_vector_initial
    type: assert.memory
    params:
      start: 655
      end: 656
      equals_hex: "$KEYLOG_VEC_HEX"
  - id: readybasic_first_type
    type: input.sequence
    params:
      keys: [$(keys $'Q%=101:PRINT "CHAIN1";Q%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_readybasic_first_type
    type: assert.screen
    params:
      contains: "CHAIN1 101"
$(emit_hotkey_step readybasic_ctrl_b_1 ctrl_b 1.0)
  - id: wait_launcher_after_readybasic_ctrl_b_1
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 40
      pre_delay_s: 1.0
      capture_label: chain_launcher_after_rb_ctrl_b_1
  - id: assert_launcher_bank_after_ctrl_b_1
    type: assert.memory
    params:
      start: 51252
      end: 51252
      equals_hex: "00"
  - id: reenter_readybasic_1
    type: input.sequence
    params:
      keys: [13]
      inter_key_delay_s: 0.08
      post_delay_s: 2.0
$(emit_wait_readybasic_warm_steps readybasic_reentry_1 chain_readybasic_reentry_1)
  - id: assert_readybasic_bank_reentry_1
    type: assert.memory
    params:
      start: 51252
      end: 51252
      equals_hex: "$READYBASIC_BANK_HEX"
  - id: readybasic_second_type
    type: input.sequence
    params:
      keys: [$(keys $'PRINT "CHAIN2";Q%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_readybasic_second_type
    type: assert.screen
    params:
      contains: "CHAIN2 101"
$(emit_hotkey_step readybasic_ctrl_b_2 ctrl_b 1.0)
  - id: wait_launcher_after_readybasic_ctrl_b_2
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 40
      pre_delay_s: 1.0
      capture_label: chain_launcher_after_rb_ctrl_b_2
  - id: launch_reuviewer_from_readybasic_position
    type: input.sequence
    params:
      keys: [17,17,17,13]
      inter_key_delay_s: 0.08
      post_delay_s: 2.5
  - id: wait_reuviewer
    type: screen.wait_contains
    params:
      text: "REU MEMORY"
      wait_timeout_s: 90
      capture_label: chain_reuviewer_loaded
  - id: assert_reuviewer_bank
    type: assert.memory
    params:
      start: 51252
      end: 51252
      equals_hex: "$REUVIEWER_BANK_HEX"
$(emit_bitmap_control_steps)
$(emit_hotkey_step reuviewer_f2_to_readybasic f2 1.2 app)
$(emit_wait_readybasic_warm_steps readybasic_after_reuviewer_f2 chain_readybasic_after_reuviewer_f2)
  - id: assert_readybasic_bank_after_reuviewer_f2
    type: assert.memory
    params:
      start: 51252
      end: 51252
      equals_hex: "$READYBASIC_BANK_HEX"
  - id: assert_irq_vector_after_reuviewer_f2
    type: assert.memory
    params:
      start: 788
      end: 789
      equals_hex: "$IRQ_VEC_HEX"
  - id: assert_keylog_vector_after_reuviewer_f2
    type: assert.memory
    params:
      start: 655
      end: 656
      equals_hex: "$KEYLOG_VEC_HEX"
  - id: assert_keyboard_buffer_empty_after_reuviewer_f2
    type: assert.memory
    params:
      start: 198
      end: 198
      equals_hex: "00"
  - id: readybasic_type_after_reuviewer_f2
    type: input.sequence
    params:
      keys: [$(keys $'PRINT "CHAIN3";Q%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_readybasic_type_after_reuviewer_f2
    type: assert.screen
    params:
      contains: "CHAIN3 101"
$(emit_hotkey_step readybasic_f2_back_to_reuviewer f2 1.2)
  - id: wait_reuviewer_after_readybasic_f2
    type: screen.wait_contains
    params:
      text: "REU MEMORY"
      wait_timeout_s: 80
      pre_delay_s: 1.0
      capture_label: chain_reuviewer_after_readybasic_f2
  - id: assert_app_keylog_vector_restored_after_readybasic_f2
    type: assert.memory
    params:
      start: 655
      end: 656
      equals_hex: "$KERNAL_KEYLOG_VEC_HEX"
  - id: assert_app_irq_vector_restored_after_readybasic_f2
    type: assert.memory
    params:
      start: 788
      end: 789
      equals_hex: "$KERNAL_IRQ_VEC_HEX"
  - id: assert_default_input_device_after_readybasic_f2
    type: assert.memory
    params:
      start: 153
      end: 153
      equals_hex: "00"
  - id: assert_reuviewer_bank_after_readybasic_f2
    type: assert.memory
    params:
      start: 51252
      end: 51252
      equals_hex: "$REUVIEWER_BANK_HEX"
  - id: assert_keyboard_buffer_empty_after_readybasic_f2
    type: assert.memory
    params:
      start: 198
      end: 198
      equals_hex: "00"
$(emit_hotkey_step reuviewer_ctrl_b_after_readybasic_f2 ctrl_b 1.0 app)
  - id: capture_after_reuviewer_ctrl_b_input
    type: screen.capture
    params:
      label: chain_after_reuviewer_ctrl_b_hotkey
      note: capture after resumed REU Viewer Ctrl+B hotkey
  - id: wait_launcher_after_reuviewer_ctrl_b
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 40
      capture_label: chain_launcher_after_reuviewer_ctrl_b
  - id: assert_launcher_bank_after_reuviewer_ctrl_b
    type: assert.memory
    params:
      start: 51252
      end: 51252
      equals_hex: "00"
  - id: reenter_readybasic_after_reuviewer_ctrl_b
    type: input.sequence
    params:
      keys: [145,145,145,13]
      inter_key_delay_s: 0.08
      post_delay_s: 2.0
$(emit_wait_readybasic_warm_steps readybasic_after_reuviewer_ctrl_b_reentry chain_readybasic_after_reuviewer_ctrl_b_reentry)
  - id: readybasic_type_after_reuviewer_ctrl_b_reentry
    type: input.sequence
    params:
      keys: [$(keys $'PRINT "CHAIN4";Q%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_readybasic_type_after_reuviewer_ctrl_b_reentry
    type: assert.screen
    params:
      contains: "CHAIN4 101"
  - id: readybasic_exit_after_multi_hop_reentry
    type: input.sequence
    params:
      keys: [$(keys $'EXIT\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.4
  - id: wait_launcher_stable_after_readybasic_exit
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 30
      pre_delay_s: 2.0
      capture_label: chain_launcher_stable_after_readybasic_exit
  - id: assert_launcher_bank_after_readybasic_exit
    type: assert.memory
    params:
      start: 51252
      end: 51252
      equals_hex: "00"
  - id: assert_readybasic_not_auto_reentered_after_exit
    type: assert.screen_not_contains
    params:
      not_contains: "ready."
  - id: reenter_readybasic_after_exit_guard
    type: input.sequence
    params:
      keys: [13]
      inter_key_delay_s: 0.08
      post_delay_s: 2.0
$(emit_wait_readybasic_warm_steps readybasic_after_exit_guard_reentry chain_readybasic_after_exit_guard_reentry)
  - id: readybasic_type_after_exit_guard_reentry
    type: input.sequence
    params:
      keys: [$(keys $'PRINT "CHAIN5";Q%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_readybasic_type_after_exit_guard_reentry
    type: assert.screen
    params:
      contains: "CHAIN5 101"
YAML

if [ "${READYBASIC_GENERATE_PLAN_ONLY:-0}" = "1" ]; then
  echo "wrote $PLAN"
  exit 0
fi

cd "$READYOS_ROOT"
dotnet build "$PROJECT"
dotnet run --project "$PROJECT" -- run-plan --plan "$PLAN" $CLI_CLOSE_ARG
