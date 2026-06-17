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
PLAN="${READYBASIC_PLAN:-$SCRIPT_DIR/readybasic_second_entry_editor_probe.generated.yaml}"
REPEAT_COUNT="${READYBASIC_SECOND_ENTRY_REPEAT:-5}"
VICE_HEADLESS="true"
if [ "${READYBASIC_VISIBLE:-0}" = "1" ]; then
  VICE_HEADLESS="false"
fi
VICE_CLOSE="true"
if [ "${READYBASIC_KEEP_OPEN:-0}" = "1" ]; then
  VICE_CLOSE="false"
fi
VICE_SPEED="${READYBASIC_VICE_SPEED:-100}"

keys() {
  python3 - "$1" <<'PY'
import sys
s = sys.argv[1]
out = []
for ch in s:
    code = ord(ch)
    # input.sequence pokes the C64 KERNAL keyboard buffer, not host text.
    # In ReadyOS' lowercase/uppercase text mode, unshifted PETSCII letters
    # ($41-$5a) display as lowercase. Host ASCII lowercase ($61-$7a) is the
    # shifted/graphics half and can look invisible or wrong in apps.
    if 0x61 <= code <= 0x7a:
        code -= 0x20
    out.append(str(code))
print(",".join(out))
PY
}

emit_type_step() {
  local id="$1"
  local text="$2"
  local post="${3:-${READYBASIC_TYPE_POST_DELAY:-0.6}}"
  cat >>"$PLAN" <<YAML
  - id: $id
    type: input.sequence
    params:
      keys: [$(keys "$text")]
      inter_key_delay_s: 0.035
      post_delay_s: $post
YAML
}

emit_key_step() {
  local id="$1"
  local key_list="$2"
  local delay="${3:-0.10}"
  local post="${4:-1.0}"
  local pre="${5:-0}"
  cat >>"$PLAN" <<YAML
  - id: $id
    type: input.sequence
    params:
      keys: [$key_list]
      pre_delay_s: $pre
      inter_key_delay_s: $delay
      post_delay_s: $post
YAML
}

emit_wait_step() {
  local id="$1"
  local text="$2"
  local timeout="${3:-60}"
  cat >>"$PLAN" <<YAML
  - id: $id
    type: screen.wait_contains
    params:
      text: "$text"
      wait_timeout_s: $timeout
YAML
}

emit_readybasic_other_apps_tour() {
  local label="$1"

  # Launcher selection starts on ReadyBASIC (index 4). Visit Editor,
  # type a few chars, then return via CTRL+B.
  emit_key_step "move_readybasic_to_editor_$label" "145,145,145,145,13"
  emit_wait_step "wait_editor_$label" "editor"
  emit_type_step "type_editor_chars_$label" $'probe line\r'
  emit_key_step "ctrl_b_editor_$label" "2" "0.03" "1.0"
  emit_wait_step "wait_launcher_after_editor_$label" "READY OS" "30"

  # Selection is now Editor (index 0). Return to ReadyBASIC (index 4).
  # Other REU-overlay apps have their own focused probes; this one validates
  # ReadyBASIC state after an editor app round-trip.
  emit_key_step "move_editor_to_readybasic_$label" "17,17,17,17,13"
  cat >>"$PLAN" <<YAML
  - id: wait_readybasic_after_tour_$label
    type: screen.wait_contains
    params:
      text: "ready."
      wait_timeout_s: 60
      capture_label: readybasic_after_tour_$label
YAML
}

cd "$READYOS_ROOT"
PUBLIC_VERSION_TEXT="$(python3 build_support/update_build_version.py --current)"
PUBLIC_VERSION="${PUBLIC_VERSION_TEXT%[A-Z]}"
if [ "${READYBASIC_SKIP_BUILD:-0}" != "1" ]; then
  RUN_VERSION_TEXT="$(python3 build_support/update_build_version.py --next)"
  make -B \
    BUILD_SUPPORT_DIR=build_support \
    PROFILE=precog-d81 \
    READYOS_VERSION_TEXT="$RUN_VERSION_TEXT" \
    READYOS_CONFIG_RUN_FIRST=readybasic \
    profile
fi

D81_REL="$(ls -t Releases/$PUBLIC_VERSION/precog-d81/*.d81 | head -1)"
PREBOOT_REL="$(ls -t Releases/$PUBLIC_VERSION/precog-d81/*-preboot.prg | head -1)"
D81="$D81_REL"
PREBOOT="$PREBOOT_REL"

A_STR='A-012345678901234567890123456789012345678901234567'
B_STR='B-ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXY'
C_STR='C-READYBASIC-STRING-STATE-SHOULD-SURVIVE-EDITOR-001'
D_STR='D-SECOND-ENTRY-EDITOR-ROUNDTRIP-LONG-STRING-STATE'

cat >"$PLAN" <<YAML
version: 1
kind: vice_task_plan
plan_id: readybasic_second_entry_editor_probe
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
    step_s: 240
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
    speed_percent: $VICE_SPEED
steps:
  - id: launch_preboot
    type: vice.launch
    params:
      kill_stale: true
  - id: wait_first_readybasic_prompt
    type: screen.wait_contains
    params:
      text: "FREE:"
      wait_timeout_s: 180
      capture_label: first_readybasic_prompt
YAML

emit_type_step "first_entry_print_hello" $'PRINT "HELLO"\r'
emit_type_step "first_entry_print_dirty" $'PRINT "DIRTY FIRST ENTRY"\r'
emit_type_step "first_entry_assign_x" $'X=77\r'
emit_type_step "first_entry_print_x" $'PRINT X\r'

cat >>"$PLAN" <<YAML
  - id: assert_first_entry_print_hello
    type: assert.screen
    params:
      contains: "HELLO"
  - id: assert_first_entry_print_dirty
    type: assert.screen
    params:
      contains: "DIRTY FIRST ENTRY"
  - id: assert_first_entry_print_x
    type: assert.screen
    params:
      contains: " 77"
YAML

emit_type_step "first_readybasic_exit" $'EXIT\r' "1.2"

cat >>"$PLAN" <<YAML
  - id: wait_launcher_after_first_exit
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 30
YAML

emit_readybasic_other_apps_tour "before_setup"

emit_type_step "enter_line_10" $'10 PRINT "HELLO"\r'
emit_type_step "enter_line_20" $'20 PRINT "B"\r'
emit_type_step "enter_line_30" $'30 PRINT "C"\r'
emit_type_step "enter_line_40" $'40 PRINT "D"\r'
emit_type_step "enter_line_50" $'50 PRINT "E"\r'
emit_type_step "list_before_run" $'LIST\r'

cat >>"$PLAN" <<YAML
  - id: assert_program_before_run
    type: assert.screen
    params:
      contains: "50 PRINT"
YAML

emit_type_step "run_program" $'RUN\r'

cat >>"$PLAN" <<YAML
  - id: assert_run_output
    type: assert.screen
    params:
      contains: "HELLO"
YAML

emit_type_step "assign_a" $'A$="'"$A_STR"$'"\r'
emit_type_step "print_a_before_exit" $'PRINT A$\r'

cat >>"$PLAN" <<YAML
  - id: assert_a_before_exit
    type: assert.screen
    params:
      contains: "A-0123456789"
YAML

emit_type_step "assign_b" $'B$="'"$B_STR"$'"\r'
emit_type_step "print_b_before_exit" $'PRINT B$\r'

cat >>"$PLAN" <<YAML
  - id: assert_b_before_exit
    type: assert.screen
    params:
      contains: "B-ABCDEFGHIJ"
YAML

emit_type_step "assign_c" $'C$="'"$C_STR"$'"\r'
emit_type_step "print_c_before_exit" $'PRINT C$\r'

cat >>"$PLAN" <<YAML
  - id: assert_c_before_exit
    type: assert.screen
    params:
      contains: "C-READYBASIC"
YAML

emit_type_step "assign_d" $'D$="'"$D_STR"$'"\r'
emit_type_step "print_d_before_exit" $'PRINT D$\r'

cat >>"$PLAN" <<YAML
  - id: assert_d_before_exit
    type: assert.screen
    params:
      contains: "D-SECOND"
YAML

emit_type_step "dim_e" $'DIM E(200)\r'
emit_type_step "assign_e199" $'E(199)=5\r'
emit_type_step "print_e_before_exit" $'PRINT E(199)\r' "1.2"

cat >>"$PLAN" <<YAML
  - id: assert_e_before_exit
    type: assert.screen
    params:
      contains: " 5"
  - id: dump_before_editor_roundtrip
    type: dump.memory_ranges
    params:
      ranges: &state_ranges
        - { label: basic_pnters_002b, start: 0x002B, end: 0x003F }
        - { label: basic_text_and_vars_1200, start: 0x1200, end: 0x1800 }
        - { label: string_heap_9300, start: 0x9300, end: 0x9600 }
        - { label: runtime_state_9600, start: 0x9600, end: 0x9A00 }
        - { label: bridge_c000, start: 0xC000, end: 0xC5FF }
YAML

for i in $(seq 1 "$REPEAT_COUNT"); do
  emit_type_step "exit_readybasic_$i" $'EXIT\r' "1.2"
  cat >>"$PLAN" <<YAML
  - id: wait_launcher_after_exit_$i
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 30
YAML
  emit_readybasic_other_apps_tour "verify_$i"
  emit_type_step "list_after_editor_$i" $'LIST\r'
  cat >>"$PLAN" <<YAML
  - id: assert_program_after_editor_$i
    type: assert.screen
    params:
      contains: "50 PRINT"
YAML
  emit_type_step "print_a_after_editor_$i" $'PRINT A$\r'
  cat >>"$PLAN" <<YAML
  - id: assert_a_after_editor_$i
    type: assert.screen
    params:
      contains: "A-0123456789"
YAML
  emit_type_step "print_b_after_editor_$i" $'PRINT B$\r'
  cat >>"$PLAN" <<YAML
  - id: assert_b_after_editor_$i
    type: assert.screen
    params:
      contains: "B-ABCDEFGHIJ"
YAML
  emit_type_step "print_c_after_editor_$i" $'PRINT C$\r'
  cat >>"$PLAN" <<YAML
  - id: assert_c_after_editor_$i
    type: assert.screen
    params:
      contains: "C-READYBASIC"
YAML
  emit_type_step "print_d_after_editor_$i" $'PRINT D$\r'
  cat >>"$PLAN" <<YAML
  - id: assert_d_after_editor_$i
    type: assert.screen
    params:
      contains: "D-SECOND"
YAML
  emit_type_step "print_e_after_editor_$i" $'PRINT E(199)\r'
  cat >>"$PLAN" <<YAML
  - id: assert_e_after_editor_$i
    type: assert.screen
    params:
      contains: " 5"
YAML
done

cat >>"$PLAN" <<YAML
  - id: dump_final_state
    type: dump.memory_ranges
    params:
      ranges: *state_ranges
YAML

if [ "${READYBASIC_GENERATE_PLAN_ONLY:-0}" = "1" ]; then
  echo "wrote $PLAN"
  exit 0
fi

cd "$READYOS_ROOT"
dotnet build "$PROJECT"
RUN_ARGS=(run-plan --plan "$PLAN")
if [ "$VICE_CLOSE" = "true" ]; then
  RUN_ARGS+=(--close-vice)
fi
dotnet run --project "$PROJECT" -- "${RUN_ARGS[@]}"
