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
PLAN="${READYBASIC_PLAN:-$SCRIPT_DIR/readybasic_large_vars_probe.generated.yaml}"
READYBASIC_VISIBLE="${READYBASIC_VISIBLE:-0}"
VICE_HEADLESS="true"
if [ "$READYBASIC_VISIBLE" = "1" ]; then
  VICE_HEADLESS="false"
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
plan_id: readybasic_large_vars_probe
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
    close_vice: true
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
  - id: build_program_lines
    type: input.sequence
    params:
      keys: [$(keys $'NEW\r10 PRINT "ONE"\r20 PRINT "TWO"\r30 PRINT "THREE"\r40 PRINT "FOUR"\r50 PRINT "FIVE"\r')]
      inter_key_delay_s: 0.04
      post_delay_s: 0.8
  - id: clear_variables
    type: input.sequence
    params:
      keys: [$(keys $'CLR\r')]
      inter_key_delay_s: 0.04
      post_delay_s: 2.0
  - id: assign_alpha_string
    type: input.sequence
    params:
      keys: [$(keys $'A$="ALPHA"\r')]
      inter_key_delay_s: 0.05
      post_delay_s: 2.0
  - id: assign_bravo_string
    type: input.sequence
    params:
      keys: [$(keys $'B$="BRAVO"\r')]
      inter_key_delay_s: 0.05
      post_delay_s: 2.0
  - id: assign_charlie_string
    type: input.sequence
    params:
      keys: [$(keys $'G$="CHARLIE"\r')]
      inter_key_delay_s: 0.05
      post_delay_s: 2.0
  - id: dump_after_abc_assignment
    type: dump.memory_ranges
    params:
      ranges:
        - { label: basic_pnters_002b, start: 0x002B, end: 0x003F }
        - { label: basic_var_table_2b00, start: 0x2B00, end: 0x2B3F }
        - { label: string_heap_9f80, start: 0x9F80, end: 0x9FFF }
        - { label: bridge_c000, start: 0xC000, end: 0xC5FF }
  - id: print_charlie_after_c_assignment
    type: input.sequence
    params:
      keys: [$(keys $'PRINT G$\r')]
      inter_key_delay_s: 0.04
      post_delay_s: 2.0
  - id: dump_after_first_print_charlie
    type: dump.memory_ranges
    params:
      ranges:
        - { label: basic_pnters_002b, start: 0x002B, end: 0x003F }
        - { label: basic_var_table_2b00, start: 0x2B00, end: 0x2B3F }
        - { label: string_heap_9f80, start: 0x9F80, end: 0x9FFF }
        - { label: bridge_c000, start: 0xC000, end: 0xC5FF }
  - id: assert_charlie_after_c_assignment
    type: assert.screen
    params:
      contains: "CHARLIE"
  - id: assign_delta_string
    type: input.sequence
    params:
      keys: [$(keys $'D$="DELTA"\r')]
      inter_key_delay_s: 0.05
      post_delay_s: 2.0
  - id: dump_after_delta_assignment
    type: dump.memory_ranges
    params:
      ranges:
        - { label: basic_pnters_002b, start: 0x002B, end: 0x003F }
        - { label: basic_var_table_2b00, start: 0x2B00, end: 0x2B3F }
        - { label: string_heap_9f80, start: 0x9F80, end: 0x9FFF }
        - { label: bridge_c000, start: 0xC000, end: 0xC5FF }
  - id: print_charlie_after_delta_assignment
    type: input.sequence
    params:
      keys: [$(keys $'PRINT G$\r')]
      inter_key_delay_s: 0.04
      post_delay_s: 2.0
  - id: dump_after_second_print_charlie
    type: dump.memory_ranges
    params:
      ranges:
        - { label: basic_pnters_002b, start: 0x002B, end: 0x003F }
        - { label: basic_var_table_2b00, start: 0x2B00, end: 0x2B3F }
        - { label: string_heap_9f80, start: 0x9F80, end: 0x9FFF }
        - { label: bridge_c000, start: 0xC000, end: 0xC5FF }
  - id: assert_charlie_after_delta_assignment
    type: assert.screen
    params:
      contains: "CHARLIE"
  - id: dump_after_delta_before_dim
    type: dump.memory_ranges
    params:
      ranges:
        - { label: basic_pnters_002b, start: 0x002B, end: 0x003F }
        - { label: basic_var_table_2b00, start: 0x2B00, end: 0x2B3F }
        - { label: string_heap_9f80, start: 0x9F80, end: 0x9FFF }
  - id: dim_large_array
    type: input.sequence
    params:
      keys: [$(keys $'DIM E(200)\rE(199)=1234\r')]
      inter_key_delay_s: 0.05
      post_delay_s: 0.8
  - id: print_large_direct_vars_before_exit
    type: input.sequence
    params:
      keys: [$(keys $'PRINT A$\rPRINT B$\rPRINT G$\rPRINT D$\rPRINT E(199)\r')]
      inter_key_delay_s: 0.04
      post_delay_s: 1.5
  - id: dump_before_exit
    type: dump.memory_ranges
    params:
      ranges:
        - { label: basic_pnters_002b, start: 0x002B, end: 0x003F }
        - { label: basic_var_table_2b00, start: 0x2B00, end: 0x2B3F }
        - { label: basic_vars_start_1200, start: 0x1200, end: 0x1600 }
        - { label: string_heap_9f80, start: 0x9F80, end: 0x9FFF }
        - { label: runtime_state_9600, start: 0x9600, end: 0x9A00 }
        - { label: bridge_c000, start: 0xC000, end: 0xC5FF }
  - id: assert_before_exit_alpha
    type: assert.screen
    params:
      contains: "ALPHA"
  - id: assert_before_exit_delta
    type: assert.screen
    params:
      contains: "DELTA"
  - id: assert_before_exit_charlie
    type: assert.screen
    params:
      contains: "CHARLIE"
  - id: assert_before_exit_array
    type: assert.screen
    params:
      contains: "1234"
  - id: exit_readybasic
    type: input.sequence
    params:
      keys: [$(keys $'EXIT\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: wait_launcher_after_exit
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 30
  - id: resume_readybasic
    type: monitor.command
    params:
      command: "keybuf \\\\x0d"
  - id: wait_prompt_after_resume
    type: screen.wait_contains
    params:
      text: "READY."
      wait_timeout_s: 30
  - id: print_large_direct_vars_after_resume
    type: input.sequence
    params:
      keys: [$(keys $'PRINT A$+B$+G$+D$\rPRINT E(199)\rLIST\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.5
  - id: dump_after_resume
    type: dump.memory_ranges
    params:
      ranges:
        - { label: basic_pnters_002b, start: 0x002B, end: 0x003F }
        - { label: basic_var_table_2b00, start: 0x2B00, end: 0x2B3F }
        - { label: basic_vars_start_1200, start: 0x1200, end: 0x1600 }
        - { label: string_heap_9f80, start: 0x9F80, end: 0x9FFF }
        - { label: runtime_state_9600, start: 0x9600, end: 0x9A00 }
        - { label: bridge_c000, start: 0xC000, end: 0xC5FF }
  - id: assert_after_resume_alpha
    type: assert.screen
    params:
      contains: "ALPHABRAVOCHARLIEDELTA"
  - id: assert_after_resume_bravo
    type: assert.screen
    params:
      contains: "BRAVO"
  - id: assert_after_resume_charlie
    type: assert.screen
    params:
      contains: "CHARLIE"
  - id: assert_after_resume_delta
    type: assert.screen
    params:
      contains: "DELTA"
  - id: assert_after_resume_array
    type: assert.screen
    params:
      contains: "1234"
  - id: assert_after_resume_program
    type: assert.screen
    params:
      contains: "50 PRINT"
  - id: dump_after_resume_verified
    type: dump.memory_ranges
    params:
      ranges:
        - { label: basic_pnters_002b, start: 0x002B, end: 0x003F }
        - { label: basic_var_table_2b00, start: 0x2B00, end: 0x2B3F }
        - { label: basic_vars_start_1200, start: 0x1200, end: 0x1600 }
        - { label: string_heap_9f80, start: 0x9F80, end: 0x9FFF }
        - { label: runtime_state_9600, start: 0x9600, end: 0x9A00 }
        - { label: bridge_c000, start: 0xC000, end: 0xC5FF }
YAML

if [ "${READYBASIC_GENERATE_PLAN_ONLY:-0}" = "1" ]; then
  echo "wrote $PLAN"
  exit 0
fi

cd "$READYOS_ROOT"
dotnet build "$PROJECT"
dotnet run --project "$PROJECT" -- run-plan --plan "$PLAN" --close-vice
