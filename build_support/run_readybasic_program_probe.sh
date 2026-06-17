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
PLAN="${READYBASIC_PLAN:-$SCRIPT_DIR/readybasic_program_probe.generated.yaml}"
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
plan_id: readybasic_program_probe
run_mode: gui_vice
global_defaults:
  monitor_host: 127.0.0.1
  monitor_port_start: 6502
  monitor_port_span: 40
  retry_policy:
    max_attempts: 1
    backoff_ms: 250
    jitter: false
  timeouts:
    launch_s: 45
    step_s: 45
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
      capture_label: readybasic_program_prompt

  - id: program_ping_enter_and_list
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 ZECHO1(P%)\r20 PRINT "PRSCALAR";P%\rLIST\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: assert_program_ping_lists
    type: assert.screen
    params:
      contains: "10 ZECHO1"
  - id: program_ping_run
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: assert_program_ping_run
    type: assert.screen
    params:
      contains: "PRSCALAR 1"

  - id: program_chain_run
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 A%=ZADD16(1,2):PRINT "PRCHAIN";A%\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: assert_program_chain_run
    type: assert.screen
    params:
      contains: "PRCHAIN 3"

  - id: program_if_true_run
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 P%=0\r20 IF 1 THEN P%=ZADD16(0,1)\r30 PRINT "PRIFTRUE";P%\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: assert_program_if_true_run
    type: assert.screen
    params:
      contains: "PRIFTRUE 1"

  - id: program_if_false_run
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 P%=0\r20 IF 0 THEN P%=ZADD16(0,1)\r30 PRINT "PRIFFALSE";P%\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: assert_program_if_false_run
    type: assert.screen
    params:
      contains: "PRIFFALSE 0"

  - id: program_text_then_command_name_safe
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 PRINT "THEN NOPE()"\r20 REM THEN NOPE()\r30 DATA THEN NOPE()\r40 READ A$\r50 PRINT "PRSAFE";A$\rPRINT CHR$(147)\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.2
  - id: assert_program_text_then_command_name_safe
    type: assert.screen
    params:
      contains: "PRSAFETHEN NOPE()"
  - id: assert_program_text_then_command_name_no_colon
    type: assert.screen_not_contains
    params:
      not_contains: "PRSAFETHEN :NOPE()"
  - id: assert_program_text_then_command_name_no_error
    type: assert.screen_not_contains
    params:
      not_contains: "?RB ERROR"

  - id: program_for_next_run
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 FOR I=1 TO 3\r20 A%=ZADD16(I,10)\r30 NEXT\r40 PRINT "PRFOR";A%\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.2
  - id: assert_program_for_next_run
    type: assert.screen
    params:
      contains: "PRFOR 13"

  - id: program_strup_run
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 S$="abc"\r20 T$=UPPER(S$)\r30 PRINT "PRSTR";T$\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.2
  - id: assert_program_strup_run
    type: assert.screen
    params:
      contains: "PRSTRABC"

  - id: program_lower_run
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 S$="ABC"\r20 T$=LOWER(S$)\r30 PRINT "PRLOWASC";ASC(T$);ASC(MID$(T$,2,1));ASC(MID$(T$,3,1))\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.2
  - id: assert_program_lower_run
    type: assert.screen
    params:
      contains: "PRLOWASC 97  98  99"

  - id: program_hidden_hcrc_run
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 H%=ZHIDDENRAM("AB")\r20 PRINT "PRHCRC";H%\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: assert_program_hidden_hcrc_run
    type: assert.screen
    params:
      contains: "PRHCRC 131"

  - id: program_array_sum_run
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 DIM A%(3)\r20 A%(0)=1:A%(1)=2:A%(2)=3\r30 S%=ZSUMNUMARRAY(A%(0),3)\r40 PRINT "PRSUM";S%\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.3
  - id: assert_program_array_sum_run
    type: assert.screen
    params:
      contains: "PRSUM 6"

  - id: program_array_range_run
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 DIM R%(4)\r20 ZRANGENUMARRAY(7,4,R%(0))\r30 PRINT "PRRANGE";R%(0);R%(3)\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.3
  - id: assert_program_array_range_run
    type: assert.screen
    params:
      contains: "PRRANGE 7  10"

  - id: program_handle_lifecycle_run
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 H%=BUFMAKE(300)\r20 PRINT "PRBUF";H%\r30 BUFFILL(H%,170)\r40 BUFDROP(H%)\r50 PRINT "PRFREE";H%\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.5
  - id: assert_program_handle_new
    type: assert.screen
    params:
      contains: "PRBUF 1"
  - id: assert_program_handle_free
    type: assert.screen
    params:
      contains: "PRFREE 1"

  - id: program_fail_run
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 X%=99\r20 ZFAIL(7,X%)\r30 PRINT "PRAFTER";X%\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: assert_program_fail_error
    type: assert.screen
    params:
      contains: "?RB ERROR 7"
  - id: program_fail_cleared_check
    type: input.sequence
    params:
      keys: [$(keys $'PRINT "PRFAILCLR";X%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_program_fail_cleared
    type: assert.screen
    params:
      contains: "PRFAILCLR 0"

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
