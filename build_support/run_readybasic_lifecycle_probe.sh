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
PLAN="${READYBASIC_PLAN:-$SCRIPT_DIR/readybasic_lifecycle_probe.generated.yaml}"
READYBASIC_VISIBLE="${READYBASIC_VISIBLE:-0}"
READYBASIC_KEEP_VICE="${READYBASIC_KEEP_VICE:-0}"
VICE_HEADLESS="true"
VICE_CLOSE="true"
CLI_CLOSE_ARG="--close-vice"
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

cat >"$PLAN" <<YAML
version: 1
kind: vice_task_plan
plan_id: readybasic_lifecycle_probe
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
  - id: wait_readybasic_loading
    type: screen.wait_contains
    params:
      text: "readybasic"
      wait_timeout_s: 180
  - id: capture_after_readybasic_loading
    type: screen.capture
    params:
      label: after_readybasic_loading
      note: after READYBASIC first appears, before READY prompt wait
  - id: wait_readybasic_prompt
    type: screen.wait_contains
    params:
      text: "readybasic"
      wait_timeout_s: 180
      capture_label: readybasic_prompt
  - id: dump_prompt_core
    type: dump.memory_ranges
    params:
      ranges: &core_ranges
        - { label: entry_1000, start: 0x1000, end: 0x1120 }
        - { label: hidden_shadow_9a00, start: 0x9A00, end: 0x9F40 }
        - { label: hidden_visible_a000, start: 0xA000, end: 0xA540 }
        - { label: vectors_0300, start: 0x0300, end: 0x030B }
        - { label: kernal_vectors_0324, start: 0x0324, end: 0x032B }
        - { label: chrget_0073, start: 0x0073, end: 0x008A }
        - { label: basic_zp_002b, start: 0x002B, end: 0x003F }
        - { label: txtptr_007a, start: 0x007A, end: 0x007B }
        - { label: kernal_mem_bounds_0280, start: 0x0280, end: 0x0287 }
        - { label: input_buffer_0200, start: 0x0200, end: 0x0258 }
        - { label: low_ram_0100_0600, start: 0x0100, end: 0x0600 }
        - { label: screen_0400, start: 0x0400, end: 0x07E7 }
        - { label: basic_text_1200, start: 0x1200, end: 0x1300 }
        - { label: readybasic_bridge_c000, start: 0xC000, end: 0xC5FF }
  - id: type_print_1
    type: input.sequence
    params:
      keys: [80, 82, 73, 78, 84, 32, 49, 13]
      inter_key_delay_s: 0.03
      post_delay_s: 0.5
  - id: capture_print_1
    type: screen.capture
    params:
      label: after_print_1
      note: after direct print 1
  - id: assert_print_1_no_error
    type: assert.screen_not_contains
    params:
      not_contains: "?"
  - id: dump_print_1_core
    type: dump.memory_ranges
    params:
      ranges: *core_ranges
  - id: type_print_hello
    type: input.sequence
    params:
      keys: [80, 82, 73, 78, 84, 32, 34, 72, 69, 76, 76, 79, 34, 13]
      inter_key_delay_s: 0.03
      post_delay_s: 0.5
  - id: capture_print_hello
    type: screen.capture
    params:
      label: after_print_hello
      note: after direct print hello
  - id: assert_print_hello_no_error
    type: assert.screen_not_contains
    params:
      not_contains: "?"
  - id: dump_print_hello_core
    type: dump.memory_ranges
    params:
      ranges: *core_ranges
  - id: type_line_10_print_1
    type: input.sequence
    params:
      keys: [49, 48, 32, 80, 82, 73, 78, 84, 32, 49, 13]
      inter_key_delay_s: 0.03
      post_delay_s: 0.5
  - id: capture_line_entry
    type: screen.capture
    params:
      label: after_10_print_1
      note: after numbered line entry
  - id: dump_line_entry_core
    type: dump.memory_ranges
    params:
      ranges: *core_ranges
  - id: assert_line_entry_no_error
    type: assert.screen_not_contains
    params:
      not_contains: "?"
  - id: assert_line_entry_no_at_artifacts
    type: assert.screen_not_contains
    params:
      not_contains: "@"
  - id: type_list
    type: input.sequence
    params:
      keys: [76, 73, 83, 84, 13]
      inter_key_delay_s: 0.03
      post_delay_s: 0.5
  - id: capture_list
    type: screen.capture
    params:
      label: after_list
      note: after LIST
  - id: assert_list_has_line
    type: assert.screen
    params:
      contains: "10 PRINT 1"
  - id: type_run
    type: input.sequence
    params:
      keys: [82, 85, 78, 13]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: capture_run
    type: screen.capture
    params:
      label: after_run
      note: after RUN
  - id: assert_run_no_error
    type: assert.screen_not_contains
    params:
      not_contains: "?"
  - id: assert_run_no_at_artifacts
    type: assert.screen_not_contains
    params:
      not_contains: "@"
  - id: dump_run_core
    type: dump.memory_ranges
    params:
      ranges: *core_ranges
  - id: type_rb_direct_text
    type: input.sequence
    params:
      keys: [$(keys $'P%=ZADD16(0,1)\rPRINT "LSCALAR";P%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.5
  - id: capture_rb_direct_text
    type: screen.capture
    params:
      label: after_rb_direct_text
      note: after direct scalar command statement
  - id: dump_rb_direct_text_core
    type: dump.memory_ranges
    params:
      ranges: *core_ranges
  - id: assert_rb_direct_text
    type: assert.screen
    params:
      contains: "LSCALAR 1"
  - id: type_rb_direct_add
    type: input.sequence
    params:
      keys: [$(keys $'A%=ZADD16(5,10)\rPRINT "LADD";A%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.5
  - id: capture_rb_direct_add
    type: screen.capture
    params:
      label: after_rb_direct_add
      note: after direct ZADD16 statement
  - id: dump_rb_direct_add_core
    type: dump.memory_ranges
    params:
      ranges: *core_ranges
  - id: assert_rb_direct_add_screen
    type: assert.screen
    params:
      contains: "LADD 15"
  - id: type_line_20_rb_text
    type: input.sequence
    params:
      keys: [$(keys $'20 L$=UPPER("line")\r30 PRINT L$\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.5
  - id: type_run_with_rb_line
    type: input.sequence
    params:
      keys: [82, 85, 78, 13]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: capture_run_with_rb_line
    type: screen.capture
    params:
      label: after_run_with_rb_line
      note: after RUN with stored ReadyBASIC command
  - id: assert_run_with_rb_line
    type: assert.screen
    params:
      contains: "LINE"
  - id: type_exit_upper
    type: input.sequence
    params:
      keys: [69, 88, 73, 84, 13]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: capture_after_exit
    type: screen.capture
    params:
      label: after_exit
      note: after uppercase exit command
  - id: wait_launcher_after_exit
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 30
  - id: type_resume_readybasic
    type: input.sequence
    params:
      keys: [145,17,13]
      inter_key_delay_s: 0.10
      post_delay_s: 3.0
  - id: wait_readybasic_prompt_after_resume
    type: screen.wait_contains
    params:
      text: "READY."
      wait_timeout_s: 30
      capture_on_success: true
      capture_label: readybasic_prompt_after_resume
  - id: type_list_after_resume
    type: input.sequence
    params:
      keys: [76, 73, 83, 84, 13]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: capture_list_after_resume
    type: screen.capture
    params:
      label: after_list_after_resume
      note: after relaunching ReadyBASIC from REU and LISTing
  - id: assert_resume_preserved_line
    type: assert.screen
    params:
      contains: "10 PRINT 1"
  - id: regs_final
    type: monitor.command
    params:
      command: "r"
YAML

if [ "${READYBASIC_GENERATE_PLAN_ONLY:-0}" = "1" ]; then
  echo "wrote $PLAN"
  exit 0
fi

cd "$READYOS_ROOT"
dotnet build "$PROJECT"
dotnet run --project "$PROJECT" -- run-plan --plan "$PLAN" $CLI_CLOSE_ARG
