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
PLAN="${READYBASIC_PLAN:-$SCRIPT_DIR/readybasic_cross_app_resume_probe.generated.yaml}"
REPEAT_COUNT="${READYBASIC_CROSS_APP_REPEAT:-10}"
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
plan_id: readybasic_cross_app_resume_probe
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
      text: "ready."
      wait_timeout_s: 180
      capture_label: readybasic_prompt
  - id: clear_keyboard_buffer_before_program_entry
    type: memory.write
    params:
      start: 198
      bytes_hex: "00"
  - id: build_program_lines
    type: input.sequence
    params:
      keys: [$(keys $'NEW\r10 PRINT "ONE"\r20 PRINT "TWO"\r30 PRINT "THREE"\r40 PRINT "FOUR"\r50 PRINT "FIVE"\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: clear_variables_after_program_entry
    type: input.sequence
    params:
      keys: [$(keys $'CLR\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.5
  - id: seed_large_direct_vars
    type: input.sequence
    params:
      keys: [$(keys $'A$="ALPHA"\rB$="BRAVO"\rG$="CHARLIE"\rD$="DELTA"\rDIM E(200)\rE(199)=1234\rPRINT A$\rPRINT B$\rPRINT G$\rPRINT D$\rPRINT E(199)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.5
  - id: assert_setup_alpha
    type: assert.screen
    params:
      contains: "ALPHA"
  - id: assert_setup_bravo
    type: assert.screen
    params:
      contains: "BRAVO"
  - id: assert_setup_charlie
    type: assert.screen
    params:
      contains: "CHARLIE"
  - id: assert_setup_delta
    type: assert.screen
    params:
      contains: "DELTA"
  - id: assert_setup_array
    type: assert.screen
    params:
      contains: "1234"
YAML

for i in $(seq 1 "$REPEAT_COUNT"); do
cat >>"$PLAN" <<YAML
  - id: exit_readybasic_$i
    type: input.sequence
    params:
      keys: [$(keys $'EXIT\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: wait_launcher_after_exit_$i
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 30
  # The cross-app leg deliberately uses Editor as the deterministic
  # "other app" so this probe tests ReadyBASIC suspend/resume only.
  - id: move_from_readybasic_to_editor_$i
    type: input.sequence
    params:
      keys: [145,145,145,145,13]
      inter_key_delay_s: 0.08
      post_delay_s: 2.0
  - id: wait_editor_$i
    type: screen.wait_contains
    params:
      text: "editor"
      wait_timeout_s: 60
  - id: ctrl_b_to_launcher_from_editor_$i
    type: input.sequence
    params:
      keys: [2]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: wait_launcher_after_editor_$i
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 30
  - id: move_from_editor_to_readybasic_$i
    type: input.sequence
    params:
      keys: [17,17,17,17,13]
      inter_key_delay_s: 0.08
      post_delay_s: 2.0
  - id: wait_readybasic_resume_$i
    type: screen.wait_contains
    params:
      text: "ready."
      wait_timeout_s: 60
      capture_label: readybasic_resume_$i
  - id: list_after_cross_app_$i
    type: input.sequence
    params:
      keys: [$(keys $'LIST\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: assert_program_after_cross_app_$i
    type: assert.screen
    params:
      contains: "50 PRINT"
  - id: print_a_after_cross_app_$i
    type: input.sequence
    params:
      keys: [$(keys $'PRINT A$\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.4
  - id: assert_a_after_cross_app_$i
    type: assert.screen
    params:
      contains: "ALPHA"
  - id: print_b_after_cross_app_$i
    type: input.sequence
    params:
      keys: [$(keys $'PRINT B$\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.4
  - id: assert_b_after_cross_app_$i
    type: assert.screen
    params:
      contains: "BRAVO"
  - id: print_g_after_cross_app_$i
    type: input.sequence
    params:
      keys: [$(keys $'PRINT G$\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.4
  - id: assert_g_after_cross_app_$i
    type: assert.screen
    params:
      contains: "CHARLIE"
  - id: print_d_after_cross_app_$i
    type: input.sequence
    params:
      keys: [$(keys $'PRINT D$\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.4
  - id: assert_d_after_cross_app_$i
    type: assert.screen
    params:
      contains: "DELTA"
  - id: print_e_after_cross_app_$i
    type: input.sequence
    params:
      keys: [$(keys $'PRINT E(199)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.4
  - id: assert_e_after_cross_app_$i
    type: assert.screen
    params:
      contains: "1234"
YAML
done

cat >>"$PLAN" <<'YAML'
  - id: dump_final_state
    type: dump.memory_ranges
    params:
      ranges:
        - { label: basic_pnters_002b, start: 0x002B, end: 0x003F }
        - { label: basic_text_and_vars_1200, start: 0x1200, end: 0x1700 }
        - { label: string_heap_9400, start: 0x9400, end: 0x9600 }
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
