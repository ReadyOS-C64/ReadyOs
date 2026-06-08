#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [ -n "${VICE_TASKS_ROOT:-}" ]; then
  VICE_TOOL_ROOT="$(cd "$VICE_TASKS_ROOT" && pwd)"
  HARNESS_REPO="$(cd "$VICE_TOOL_ROOT/../.." && pwd)"
else
  HARNESS_REPO="${VICE_TASKS_REPO:-$ROOT/../agenticdevharness}"
  HARNESS_REPO="$(cd "$HARNESS_REPO" && pwd)"
  VICE_TOOL_ROOT="$HARNESS_REPO/tools/vice_tasks_dotnet"
fi
PROJECT="$VICE_TOOL_ROOT/src/ViceTasks.Binary/ViceTasks.Binary.csproj"
PLAN="${READYBASIC_REPEAT_PLAN:-$ROOT/build_support/readybasic_repeat_label_probe.generated.yaml}"
READYBASIC_VISIBLE="${READYBASIC_VISIBLE:-0}"
READYBASIC_KEEP_VICE="${READYBASIC_KEEP_VICE:-0}"
VICE_HEADLESS="true"
VICE_CLOSE="true"
CLI_CLOSE_ARG="--close-vice"

if [ ! -f "$PROJECT" ]; then
  echo "ReadyBASIC repeat-label VICE probe could not find the VICE task runner at: $PROJECT" >&2
  echo "Set VICE_TASKS_REPO or VICE_TASKS_ROOT to the external agenticdevharness checkout." >&2
  exit 2
fi
mkdir -p "$(dirname "$PLAN")"

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
print(",".join(str(ord(ch)) for ch in sys.argv[1]))
PY
}

cd "$ROOT"
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
plan_id: readybasic_repeat_label_probe
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
    step_s: 60
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
      text: "FREE:"
      wait_timeout_s: 180
      capture_label: readybasic_repeat_label_prompt

  - id: repeat_multiline
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 I%=0\r20 REPEAT\r30 I%=I%+1\r40 UNTIL I%=3\r50 PRINT "RPTA";I%\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.2
  - id: assert_repeat_multiline
    type: assert.screen
    params:
      contains: "RPTA 3"

  - id: repeat_colon
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 I%=3\r20 REPEAT:I%=I%-1:UNTIL I%=0\r30 PRINT "RPTB";I%\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.2
  - id: assert_repeat_colon
    type: assert.screen
    params:
      contains: "RPTB 0"

  - id: repeat_nested
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 A%=0:B%=0\r20 REPEAT\r30 A%=A%+1:B%=0\r40 REPEAT:B%=B%+1:UNTIL B%=2\r50 UNTIL A%=3\r60 PRINT "RPTN";A%;B%\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.4
  - id: assert_repeat_nested
    type: assert.screen
    params:
      contains: "RPTN 3  2"

  - id: until_without_repeat
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 UNTIL 1\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: assert_until_without_repeat
    type: assert.screen
    params:
      contains: "?RB ERROR 36"

  - id: repeat_overflow
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 REPEAT:REPEAT:REPEAT:REPEAT:REPEAT\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: assert_repeat_overflow
    type: assert.screen
    params:
      contains: "?RB ERROR 35"

  - id: label_backward_if_then
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 LABEL LP\r20 I%=I%+1\r30 IF I%<3 THEN JUMP LP\r40 PRINT "LBLB";I%\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.4
  - id: assert_label_backward_if_then
    type: assert.screen
    params:
      contains: "LBLB 3"

  - id: label_forward
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 PRINT "A"\r20 JUMP SKIP\r30 PRINT "BAD"\r40 LABEL SKIP:PRINT "LBLF"\rPRINT CHR$(147)\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.2
  - id: assert_label_forward
    type: assert.screen
    params:
      contains: "LBLF"
  - id: assert_label_forward_no_bad
    type: assert.screen_not_contains
    params:
      not_contains: "BAD"

  - id: label_noop
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 LABEL HERE:PRINT "LBLN"\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: assert_label_noop
    type: assert.screen
    params:
      contains: "LBLN"

  - id: missing_label
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 JUMP MISSING\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: assert_missing_label
    type: assert.screen
    params:
      contains: "?RB ERROR 39"

  - id: numeric_goto_still_rom
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 GOTO 30\r20 PRINT "BAD"\r30 PRINT "GTON"\rPRINT CHR$(147)\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: assert_numeric_goto
    type: assert.screen
    params:
      contains: "GTON"
  - id: assert_numeric_goto_no_bad
    type: assert.screen_not_contains
    params:
      not_contains: "BAD"

  - id: err_status_after_error
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 ZFAIL(6,X%)\rRUN\rPRINT "ERX";ERRCODE();ERRLINE()\rERRCODE(E%):ERRLINE(L%):PRINT "ERS";E%;L%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.4
  - id: assert_err_error
    type: assert.screen
    params:
      contains: "?RB ERROR 6"
  - id: assert_err_expr
    type: assert.screen
    params:
      contains: "ERX 6  10"
  - id: assert_err_stmt
    type: assert.screen
    params:
      contains: "ERS 6  10"

  - id: regs_final
    type: monitor.command
    params:
      command: "r"
YAML

if [ "${READYBASIC_GENERATE_PLAN_ONLY:-0}" = "1" ]; then
  echo "wrote $PLAN"
  exit 0
fi

cd "$ROOT"
dotnet build "$PROJECT"
dotnet run --project "$PROJECT" -- run-plan --plan "$PLAN" $CLI_CLOSE_ARG
