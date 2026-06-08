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
PLAN="${READYBASIC_PLAN:-$SCRIPT_DIR/readybasic_state_probe.generated.yaml}"
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
plan_id: readybasic_state_probe
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
      capture_label: readybasic_prompt
  - id: dump_initial_state
    type: dump.memory_ranges
    params:
      ranges: &state_ranges
        - { label: basic_zp_002b, start: 0x002B, end: 0x003F }
        - { label: chrget_0073, start: 0x0073, end: 0x008A }
        - { label: stack_0100, start: 0x0100, end: 0x01FF }
        - { label: basic_text_1200, start: 0x1200, end: 0x1300 }
        - { label: runtime_9500, start: 0x9500, end: 0x9A00 }
        - { label: bridge_c000, start: 0xC000, end: 0xC5FF }
        - { label: screen_0400, start: 0x0400, end: 0x07E7 }

  - id: direct_string_assignment
    type: input.sequence
    params:
      keys: [78,69,87,13,65,36,61,34,72,69,76,76,79,34,13,80,82,73,78,84,32,65,36,13]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_direct_string_before_exit
    type: assert.screen
    params:
      contains: "HELLO"
  - id: dump_direct_string_before_exit
    type: dump.memory_ranges
    params:
      ranges: *state_ranges
  - id: exit_direct_string
    type: input.sequence
    params:
      keys: [69,88,73,84,13]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: wait_launcher_after_direct_string_exit
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 30
  - id: resume_direct_string
    type: input.sequence
    params:
      keys: [13]
      inter_key_delay_s: 0.03
      post_delay_s: 2.0
  - id: wait_prompt_after_direct_string_resume
    type: screen.wait_contains
    params:
      text: "READY."
      wait_timeout_s: 30
  - id: print_direct_string_after_resume
    type: input.sequence
    params:
      keys: [80,82,73,78,84,32,65,36,43,34,33,34,13]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_direct_string_after_resume
    type: assert.screen
    params:
      contains: "HELLO!"
  - id: allocate_second_direct_string
    type: input.sequence
    params:
      keys: [66,36,61,34,87,79,82,76,68,34,13,80,82,73,78,84,32,65,36,43,34,50,34,13]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_direct_string_after_second_allocation
    type: assert.screen
    params:
      contains: "HELLO2"
  - id: dump_direct_string_after_resume
    type: dump.memory_ranges
    params:
      ranges: *state_ranges
  - id: reset_after_direct_string_case
    type: input.sequence
    params:
      keys: [78,69,87,13,80,82,73,78,84,32,67,72,82,36,40,49,52,55,41,13]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8

  - id: type_multiline_program
    type: input.sequence
    params:
      keys: [49,48,32,80,82,73,78,84,32,34,72,69,76,76,79,34,13,50,48,32,80,82,73,78,84,32,34,66,34,13,51,48,32,80,82,73,78,84,32,34,67,34,13]
      inter_key_delay_s: 0.03
      post_delay_s: 0.5
  - id: list_multiline_before_exit
    type: input.sequence
    params:
      keys: [76,73,83,84,13]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_multiline_before_exit
    type: assert.screen
    params:
      contains: "30 PRINT"
  - id: exit_multiline
    type: input.sequence
    params:
      keys: [69,88,73,84,13]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: wait_launcher_after_multiline_exit
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 30
  - id: resume_multiline
    type: input.sequence
    params:
      keys: [13]
      inter_key_delay_s: 0.03
      post_delay_s: 2.0
  - id: wait_prompt_after_multiline_resume
    type: screen.wait_contains
    params:
      text: "READY."
      wait_timeout_s: 30
  - id: list_multiline_after_resume
    type: input.sequence
    params:
      keys: [76,73,83,84,13]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: capture_multiline_after_resume
    type: screen.capture
    params:
      label: multiline_after_resume
  - id: assert_multiline_after_resume
    type: assert.screen
    params:
      contains: "30 PRINT"
  - id: dump_multiline_after_resume
    type: dump.memory_ranges
    params:
      ranges: *state_ranges

  - id: new_and_clear_screen
    type: input.sequence
    params:
      keys: [78,69,87,13,80,82,73,78,84,32,67,72,82,36,40,49,52,55,41,13]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: list_after_new
    type: input.sequence
    params:
      keys: [76,73,83,84,13]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_new_empty
    type: assert.screen_not_contains
    params:
      not_contains: "10 PRINT"
  - id: exit_after_new
    type: input.sequence
    params:
      keys: [69,88,73,84,13]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: wait_launcher_after_new_exit
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 30
  - id: resume_after_new
    type: input.sequence
    params:
      keys: [13]
      inter_key_delay_s: 0.03
      post_delay_s: 2.0
  - id: wait_prompt_after_new_resume
    type: screen.wait_contains
    params:
      text: "READY."
      wait_timeout_s: 30
  - id: clear_and_list_after_new_resume
    type: input.sequence
    params:
      keys: [80,82,73,78,84,32,67,72,82,36,40,49,52,55,41,13,76,73,83,84,13]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_new_still_empty
    type: assert.screen_not_contains
    params:
      not_contains: "10 PRINT"

  - id: type_variable_program
    type: input.sequence
    params:
      keys: [49,48,32,65,61,52,50,13,50,48,32,66,36,61,34,89,79,34,13,51,48,32,68,73,77,32,88,40,50,41,13,52,48,32,88,40,49,41,61,55,13,82,85,78,13]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: print_variables_before_exit
    type: input.sequence
    params:
      keys: [80,82,73,78,84,32,65,13,80,82,73,78,84,32,66,36,13,80,82,73,78,84,32,88,40,49,41,13]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_variables_before_exit
    type: assert.screen
    params:
      contains: "YO"
  - id: exit_variables
    type: input.sequence
    params:
      keys: [69,88,73,84,13]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: wait_launcher_after_variables_exit
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 30
  - id: resume_variables
    type: input.sequence
    params:
      keys: [13]
      inter_key_delay_s: 0.03
      post_delay_s: 2.0
  - id: wait_prompt_after_variables_resume
    type: screen.wait_contains
    params:
      text: "READY."
      wait_timeout_s: 30
  - id: print_variables_after_resume
    type: input.sequence
    params:
      keys: [80,82,73,78,84,32,65,13,80,82,73,78,84,32,66,36,13,80,82,73,78,84,32,88,40,49,41,13]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_string_after_resume
    type: assert.screen
    params:
      contains: "YO"
  - id: assert_numeric_after_resume
    type: assert.screen
    params:
      contains: "42"
  - id: clr_then_list
    type: input.sequence
    params:
      keys: [67,76,82,13,80,82,73,78,84,32,67,72,82,36,40,49,52,55,41,13,76,73,83,84,13]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_clr_keeps_program
    type: assert.screen
    params:
      contains: "40 X"

  - id: new_rb_program
    type: input.sequence
    params:
      keys: [$(keys $'NEW\r10 PRINT "A"\r20 A%=ZADD16(5,10)\r30 PRINT "SADD";A%\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: assert_rb_program_output
    type: assert.screen
    params:
      contains: "SADD 15"
  - id: exit_rb_program
    type: input.sequence
    params:
      keys: [69,88,73,84,13]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: wait_launcher_after_rb_exit
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 30
  - id: resume_rb_program
    type: input.sequence
    params:
      keys: [13]
      inter_key_delay_s: 0.03
      post_delay_s: 2.0
  - id: wait_prompt_after_rb_resume
    type: screen.wait_contains
    params:
      text: "READY."
      wait_timeout_s: 30
  - id: list_rb_after_resume
    type: input.sequence
    params:
      keys: [76,73,83,84,13]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_rb_line_after_resume
    type: assert.screen
    params:
      contains: "20 A%=ZADD16"
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
dotnet run --project "$PROJECT" -- run-plan --plan "$PLAN" $CLI_CLOSE_ARG
