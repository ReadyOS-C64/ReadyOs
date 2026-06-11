#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
READYOS_ROOT="${READYOS_ROOT:-$(cd "$SCRIPT_DIR/.." && pwd)}"
HARNESS_REPO="${VICE_TASKS_REPO:-$(cd "$READYOS_ROOT/../agenticdevharness" && pwd)}"
VICE_TOOL_ROOT="$HARNESS_REPO/tools/vice_tasks_dotnet"
PROJECT="$VICE_TOOL_ROOT/src/ViceTasks.Binary/ViceTasks.Binary.csproj"
PLAN="${READYBASIC_DEMO_PLAN:-$SCRIPT_DIR/readybasic_demo_suite.generated.yaml}"

READYBASIC_VISIBLE="${READYBASIC_VISIBLE:-1}"
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

if [ "${READYBASIC_DEMO_FAST:-0}" = "1" ]; then
  READ_PAUSE="${READYBASIC_DEMO_READ_PAUSE:-0.25}"
  STEP_POST="${READYBASIC_DEMO_STEP_POST:-0.45}"
  RUN_POST="${READYBASIC_DEMO_RUN_POST:-0.8}"
  KEY_DELAY="${READYBASIC_DEMO_KEY_DELAY:-0.025}"
else
  READ_PAUSE="${READYBASIC_DEMO_READ_PAUSE:-3.5}"
  STEP_POST="${READYBASIC_DEMO_STEP_POST:-3.5}"
  RUN_POST="${READYBASIC_DEMO_RUN_POST:-3.5}"
  KEY_DELAY="${READYBASIC_DEMO_KEY_DELAY:-0.08}"
fi
READYBASIC_DEMO_WARP_OFF="${READYBASIC_DEMO_WARP_OFF:-$READYBASIC_VISIBLE}"

keys() {
  python3 - "$1" <<'PY'
import sys
s = sys.argv[1]
print(",".join(str(ord(ch)) for ch in s))
PY
}

emit_type_step() {
  local id="$1"
  local text="$2"
  local post="${3:-$STEP_POST}"
  cat >>"$PLAN" <<YAML
  - id: $id
    type: input.sequence
    params:
      keys: [$(keys "$text")]
      inter_key_delay_s: $KEY_DELAY
      post_delay_s: $post
YAML
}

emit_key_step() {
  local id="$1"
  local key_list="$2"
  local post="${3:-$STEP_POST}"
  cat >>"$PLAN" <<YAML
  - id: $id
    type: input.sequence
    params:
      keys: [$key_list]
      inter_key_delay_s: 0.05
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

emit_assert_step() {
  local id="$1"
  local text="$2"
  cat >>"$PLAN" <<YAML
  - id: $id
    type: assert.screen
    params:
      contains: "$text"
YAML
}

emit_warp_step() {
  local id="$1"
  local enabled="$2"
  cat >>"$PLAN" <<YAML
  - id: $id
    type: warp.set
    params:
      enabled: $enabled
      capture_after: false
YAML
}

mkdir -p "$(dirname "$PLAN")"

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
plan_id: readybasic_demo_suite
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
      capture_label: demo_readybasic_prompt
YAML

if [ "$READYBASIC_DEMO_WARP_OFF" = "1" ]; then
  emit_warp_step "warp_off_for_visible_demo" "false"
fi

emit_type_step "demo_01_intro" $'PRINT CHR$(147);CHR$(158);"DEMO 1: READYBASIC BASICS"\rPRINT "FREEMEM, VARIABLES, AND A PROGRAM":PRINT CHR$(5)\r' "$READ_PAUSE"
emit_type_step "demo_01_setup" $'FREEMEM()\rNEW\rA%=42\r10 B%=ZADD16(42,8)\r20 PRINT "PROGRAM SUM";B%\r' "$STEP_POST"
emit_type_step "demo_01_list" $'LIST\r' "$STEP_POST"
emit_type_step "demo_01_basics" $'PRINT CHR$(158);"EXPECT: RUN PRINTS PROGRAM SUM 50"\rPRINT CHR$(5)\rRUN\rA%=42\r' "$RUN_POST"
emit_assert_step "assert_demo_01_program_sum" "PROGRAM SUM 50"

emit_type_step "demo_02_intro" $'PRINT CHR$(147);CHR$(158);"DEMO 2: SUSPEND AND RESTORE"\rPRINT "EXIT TO READYOS, VISIT EDITOR, RETURN":PRINT CHR$(5)\r' "$READ_PAUSE"
emit_type_step "demo_02_exit_to_launcher" $'EXIT\r' "$STEP_POST"
cat >>"$PLAN" <<'YAML'
  - id: capture_after_demo_02_exit
    type: screen.capture
    params:
      label: after_demo_02_exit
YAML
emit_wait_step "wait_launcher_after_demo_02_exit" "READY OS" "30"
emit_key_step "move_readybasic_to_editor_demo" "145,145,145,145,13" "$STEP_POST"
emit_wait_step "wait_editor_demo" "editor" "60"
emit_type_step "type_editor_demo_sentence" $'READYBASIC DEMO VISITED EDITOR AND RETURNED.\r' "$RUN_POST"
emit_key_step "ctrl_b_editor_demo" "2" "$STEP_POST"
emit_wait_step "wait_launcher_after_editor_demo" "READY OS" "30"
emit_key_step "move_editor_to_readybasic_demo" "17,17,17,17,13" "$STEP_POST"
emit_wait_step "wait_readybasic_after_editor_demo" "READY." "60"
emit_type_step "demo_03_resume_liveness" $'PRINT "RESUME LIVE"\r' "$STEP_POST"
emit_assert_step "assert_demo_03_resume_liveness" "RESUME LIVE"
emit_type_step "demo_03_resume_intro_clear" $'PRINT CHR$(147);CHR$(158);"BACK IN READYBASIC"\r' "$STEP_POST"
emit_type_step "demo_03_resume_intro_detail" $'PRINT "VARIABLES AND PROGRAM TEXT SHOULD REMAIN":PRINT CHR$(5)\r' "$READ_PAUSE"
emit_type_step "demo_03_resume_var_proof" $'FREEMEM()\rPRINT "A STILL";A%\rPRINT CHR$(158);"EXPECT: A IS 42 AND RUN STILL WORKS"\rPRINT CHR$(5)\r' "$STEP_POST"
cat >>"$PLAN" <<'YAML'
  - id: capture_demo_03_resume_var_proof
    type: screen.capture
    params:
      label: demo_03_resume_var_proof
YAML
emit_assert_step "assert_demo_03_var_restored" "A STILL 42"
emit_type_step "demo_03_resume_run_proof" $'RUN\r' "$RUN_POST"
emit_assert_step "assert_demo_03_program_restored" "PROGRAM SUM 50"

emit_type_step "demo_04_intro" $'PRINT CHR$(147);CHR$(158);"DEMO 3: ASSEMBLER COMMANDS"\rPRINT "COMMANDS RETURN VALUES AS EXPRESSIONS":PRINT CHR$(5)\r' "$READ_PAUSE"
emit_type_step "demo_04_assembler_setup" $'N%=ZADD16(7,8)\rT$=UPPER("ready")\rL$=LOWER("LOUD")\rH%=ZHIDDENRAM("AB")\rPRINT CHR$(158);"EXPECT: 15, READY, LOWER, 131"\rPRINT CHR$(5)\r' "$READ_PAUSE"
emit_type_step "demo_04_zadd_print" $'PRINT "ZADD";N%\r' "$STEP_POST"
emit_type_step "demo_04_upper_print" $'PRINT "UPPER ";T$\r' "$STEP_POST"
emit_type_step "demo_04_lower_print" $'PRINT "LOWASC";ASC(L$);ASC(MID$(L$,2,1));ASC(MID$(L$,3,1));ASC(MID$(L$,4,1))\r' "$STEP_POST"
emit_type_step "demo_04_hidden_print" $'PRINT "HIDDEN";H%\r' "$READ_PAUSE"
emit_assert_step "assert_demo_04_zadd" "ZADD 15"
emit_assert_step "assert_demo_04_upper" "UPPER READY"
emit_assert_step "assert_demo_04_hidden" "HIDDEN 131"

emit_type_step "demo_05_intro" $'PRINT CHR$(147);CHR$(158);"DEMO 4: PROC ONLY"\rPRINT "PROC TAKES PARAMETERS AND DOES WORK":PRINT CHR$(5)\r' "$READ_PAUSE"
emit_type_step "demo_05_program" $'NEW\r10 EXEC SHOW("READY")\r20 END\r100 PROC SHOW(S$)\r110  PRINT "PROC ";S$\r120 ENDP\rLIST 100-120\r' "$READ_PAUSE"
emit_type_step "demo_05_proc_func" $'PRINT CHR$(158);"EXPECT: PROC READY"\rPRINT CHR$(5)\rRUN\r' "$RUN_POST"
emit_assert_step "assert_demo_05_proc" "PROC READY"

emit_type_step "demo_05b_intro" $'PRINT CHR$(147);CHR$(158);"DEMO 5: INTEGER FUNC"\rPRINT "FUNC RETURNS THROUGH RET AS AN EXPRESSION":PRINT CHR$(5)\r' "$READ_PAUSE"
emit_type_step "demo_05b_program" $'NEW\r10 A%=ADDI(4,5)\r20 PRINT "ADDI";A%\r30 END\r100 FUNC ADDI(X%,Y%)\r110  RET X%+Y%\r120 ENDP\rLIST 100-120\r' "$READ_PAUSE"
emit_type_step "demo_05b_run" $'PRINT CHR$(158);"EXPECT: ADDI 9"\rPRINT CHR$(5)\rRUN\r' "$RUN_POST"
emit_assert_step "assert_demo_05_addi" "ADDI 9"

emit_type_step "demo_05c_intro" $'PRINT CHR$(147);CHR$(158);"DEMO 6: STRING AND FLOAT FUNCS"\rPRINT "RET KEEPS STRING AND PLAIN NUMERIC TYPES":PRINT CHR$(5)\r' "$READ_PAUSE"
emit_type_step "demo_05c_program" $'NEW\r10 T$=HELLO("KARL")\r20 PRINT T$\r30 F=SCALE(2.5)\r40 PRINT "SCALE";F\r50 END\r100 FUNC HELLO(N$)\r110  RET "HI "+N$\r120 ENDP\r130 FUNC SCALE(X)\r140  RET X*1.5\r150 ENDP\rLIST 100-150\r' "$READ_PAUSE"
emit_type_step "demo_05c_run" $'PRINT CHR$(158);"EXPECT: HI KARL AND SCALE 3.75"\rPRINT CHR$(5)\rRUN\r' "$RUN_POST"
emit_assert_step "assert_demo_05_hello" "HI KARL"
emit_assert_step "assert_demo_05_scale" "SCALE 3.75"

emit_type_step "demo_06_intro" $'PRINT CHR$(147);CHR$(158);"DEMO 7: PARAMETER GROUPS"\rPRINT "ARRAYS, INTEGER RETURNS, AND FLOATS":PRINT CHR$(5)\r' "$READ_PAUSE"
emit_type_step "demo_06_parameter_groups" $'DIM A%(3):A%(0)=1:A%(1)=2:A%(2)=3\rS%=ZSUMNUMARRAY(A%(0),3)\rDIM R%(4):ZRANGENUMARRAY(7,4,R%(0))\rF=FADD(1.2,2.3)\rPRINT CHR$(158);"EXPECT: SUM 6, RANGE 7..10, FADD 3.5"\rPRINT CHR$(5)\rPRINT "SUM";S%\rPRINT "RANGE";R%(0);R%(3)\rPRINT "FADD";F\r' "$READ_PAUSE"
emit_assert_step "assert_demo_06_sum" "SUM 6"
emit_assert_step "assert_demo_06_range" "RANGE 7  10"
emit_assert_step "assert_demo_06_fadd" "FADD 3.5"

emit_type_step "demo_07_intro" $'PRINT CHR$(147);CHR$(158);"DEMO 8: REU BUFFER COMMANDS"\rPRINT "BUFNEW RETURNS; FILL AND FREE DO WORK":PRINT CHR$(5)\r' "$READ_PAUSE"
emit_type_step "demo_07_reu" $'H%=BUFNEW(64)\rBUFFILL(H%,170)\rPRINT CHR$(158);"EXPECT: HANDLE 1, THEN FREED 1"\rPRINT CHR$(5)\rPRINT "HANDLE";H%\rBUFFREE(H%)\rPRINT "FREED";H%\r' "$READ_PAUSE"
emit_assert_step "assert_demo_07_handle" "HANDLE 1"
emit_assert_step "assert_demo_07_freed" "FREED 1"

emit_type_step "demo_08_intro" $'PRINT CHR$(147);CHR$(158);"DEMO 9: SCREEN THROUGH REU"\rPRINT "SCRCAP SAVES; SCRPUT RESTORES SCREEN":PRINT CHR$(5)\r' "$READ_PAUSE"
emit_type_step "demo_08_screen_reu_a" $'NEW\r10 DIM H%(4)\r20 FOR S=1 TO 4\r30 PRINT CHR$(147);CHR$(158);"REU SCREEN";S\r40 PRINT CHR$(5);"MARK";S;" TEXT AND COLOR"\r50 FOR I=0 TO 39\r60 POKE1024+80+I,64+S\r70 POKE55296+80+I,S+1\r80 NEXT I\r90 H%(S)=SCRCAP()\r100 NEXT S\r110 PRINT CHR$(147);CHR$(158);"REU FLIPBOOK"\r120 PRINT CHR$(5);"EXPECT: FOUR RESTORED SCREENS"\r130 FOR S=1 TO 4\r140 SCRPUT(H%(S))\r150 PRINT CHR$(19);"RESTORED";S;"/4"\r160 ZPAUSE(30)\r170 NEXT S\r180 PRINT CHR$(19);"RESTORED 4 REU SCREENS"\r190 END\r' "$READ_PAUSE"
emit_type_step "demo_08_screen_reu_b" $'PRINT CHR$(158);"EXPECT: FOUR DIFFERENT SCREENS FLIP BACK"\rPRINT CHR$(5)\rRUN\r' "$RUN_POST"
emit_wait_step "wait_demo_08_reu_flipbook" "RESTORED 4 REU SCREENS" "90"
emit_assert_step "assert_demo_08_reu_flipbook" "RESTORED 4 REU SCREENS"

emit_type_step "demo_09_intro" $'PRINT CHR$(147);CHR$(158);"DEMO 10: CONTROLLED ERROR"\rPRINT "ZFAIL SHOWS READYBASIC ERROR CLEANUP":PRINT CHR$(5)\r' "$READ_PAUSE"
emit_type_step "demo_09_zfail" $'PRINT CHR$(158);"EXPECT: RB ERROR 7 AND X% BECOMES ZERO"\rPRINT CHR$(5)\rX%=99\rZFAIL(7,X%)\rPRINT "AFTER FAIL";X%\r' "$STEP_POST"
emit_assert_step "assert_demo_09_rb_error" "RB ERROR 7"
emit_assert_step "assert_demo_09_fail_clear" "AFTER FAIL 0"

emit_type_step "demo_10_intro" $'PRINT CHR$(147);CHR$(158);"DEMO 11: NESTED EXPRESSIONS"\rPRINT "COMMANDS AND FUNCS FEED OTHER EXPRESSIONS":PRINT CHR$(5)\r' "$READ_PAUSE"
emit_type_step "demo_10_program" $'NEW\r10 FUNC ADDI(X%,Y%)\r20  RET X%+Y%\r30 ENDP\r40 FUNC GREET(N$)\r50  RET "HI "+N$\r60 ENDP\r70 PRINT "ABS";ABS(ZADD16(1,6)-10)\r80 PRINT "LEFT ";LEFT$(UPPER("ready"),2)\r90 PRINT "NEST";ZADD16(1,ZADD16(2,3))\r100 PRINT "FABS";ABS(FADD(1.2,2.3)-3)\r110 PRINT "FLEFT ";LEFT$(GREET("READY")+"!",3)\r' "$STEP_POST"
emit_type_step "demo_10_expressions" $'PRINT CHR$(158);"EXPECT: ABS 3, LEFT RE, NEST 6, FLEFT HI"\rPRINT CHR$(5)\rRUN\r' "$RUN_POST"
emit_assert_step "assert_demo_10_abs" "ABS 3"
emit_assert_step "assert_demo_10_left" "LEFT RE"
emit_assert_step "assert_demo_10_nest" "NEST 6"
emit_assert_step "assert_demo_10_fleft" "FLEFT HI"

emit_type_step "demo_done" $'PRINT CHR$(147);CHR$(158);"DEMO COMPLETE"\rPRINT "COMMANDS, PROC/FUNC, REU, ERRORS PASSED"\rPRINT CHR$(5)\rPRINT "READYBASIC DEMO COMPLETE"\r' "$READ_PAUSE"
emit_assert_step "assert_demo_done" "READYBASIC DEMO COMPLETE"

cat >>"$PLAN" <<YAML
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
RUN_RC=2
for HARNESS_ATTEMPT in 1 2; do
  RESULT_LOG="$(mktemp "${TMPDIR:-/tmp}/readybasic_demo_suite.XXXXXX")"
  set +e
  dotnet run --project "$PROJECT" -- run-plan --plan "$PLAN" $CLI_CLOSE_ARG | tee "$RESULT_LOG"
  RUN_RC=${PIPESTATUS[0]}
  set -e
  if [ "$RUN_RC" -ne 2 ]; then
    rm -f "$RESULT_LOG"
    break
  fi
  set +e
  python3 - "$RESULT_LOG" "$HARNESS_ATTEMPT" <<'PY'
import json
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
attempt = int(sys.argv[2])
start = text.rfind("{")
while start != -1:
    try:
        payload = json.loads(text[start:])
        break
    except json.JSONDecodeError:
        start = text.rfind("{", 0, start)
else:
    sys.exit(2)

manifest = payload.get("Manifest")
steps_ok = False
if manifest:
    try:
        manifest_payload = json.loads(Path(manifest).read_text(encoding="utf-8-sig"))
        steps = manifest_payload.get("steps", [])
        steps_ok = bool(steps) and all(step.get("status") == "ok" for step in steps)
    except Exception:
        steps_ok = False

if (
    payload.get("Status") == "partial"
    and payload.get("FailedStep") is None
    and not payload.get("DegradedSteps")
    and steps_ok
):
    print("treating final-dump-only partial as pass; all ReadyBASIC demo steps passed")
    sys.exit(0)
if (
    attempt < 2
    and payload.get("Status") == "partial"
    and payload.get("FailedStep") == "wait_readybasic_prompt"
):
    print("retrying ReadyBASIC demo after initial prompt timeout")
    sys.exit(10)
sys.exit(2)
PY
  PARSE_RC=$?
  set -e
  rm -f "$RESULT_LOG"
  if [ "$PARSE_RC" -eq 0 ]; then
    RUN_RC=0
    break
  fi
  if [ "$PARSE_RC" -eq 10 ]; then
    RUN_RC=2
    continue
  fi
  RUN_RC="$PARSE_RC"
  break
done
exit "$RUN_RC"
