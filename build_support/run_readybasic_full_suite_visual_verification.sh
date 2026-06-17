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
PLAN="${READYBASIC_FULL_PLAN:-$SCRIPT_DIR/readybasic_full_suite_visual_verification.generated.yaml}"
READYBASIC_VISIBLE="${READYBASIC_VISIBLE:-0}"
READYBASIC_KEEP_VICE="${READYBASIC_KEEP_VICE:-0}"
VICE_HEADLESS="true"
VICE_CLOSE="true"
CLI_CLOSE_ARG="--close-vice"

if [ ! -f "$PROJECT" ]; then
  echo "ReadyBASIC full VICE suite could not find the VICE task runner at: $PROJECT" >&2
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
plan_id: readybasic_full_suite_visual_verification
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
    step_s: 120
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
      capture_label: readybasic_full_suite_visual_prompt
  - id: dump_initial_plugin_state
    type: dump.memory_ranges
    params:
      ranges: &plugin_ranges
        - { label: entry_1000, start: 0x1000, end: 0x1120 }
        - { label: resident_1200, start: 0x1200, end: 0x1BFF }
        - { label: low_overlay_1c00, start: 0x1C00, end: 0x23FF }
        - { label: shared_frames_2400, start: 0x2400, end: 0x27FF }
        - { label: hidden_shadow_9a00, start: 0x9A00, end: 0x9FFF }
        - { label: hidden_visible_a000, start: 0xA000, end: 0xA5FF }
        - { label: hidden_overlay_a800, start: 0xA800, end: 0xA8FF }
        - { label: bridge_c000, start: 0xC000, end: 0xC5FF }
        - { label: reu_alloc_table_c600, start: 0xC600, end: 0xC6FF }

  - id: direct_ping
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-ZECHO HEALTH LOW OVL STATEMENT STORES P%=1 SCREEN D-ZECHO\rZECHO1(P%)\rPRINT "D-ZECHO";P%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_ping
    type: assert.screen
    params:
      contains: "D-ZECHO 1"
  - id: dump_direct_ping
    type: dump.memory_ranges
    params:
      ranges: *plugin_ranges

  - id: direct_add16
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-ADD NUM PARAMS RETURN A%=15 SCREEN D-ADD\rA%=ZADD16(5,10)\rPRINT "D-ADD";A%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_add16
    type: assert.screen
    params:
      contains: "D-ADD 15"

  - id: direct_module_slots
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-SLOTS BUILTIN MODULE SLOT PROOFS\rPRINT "D-S0";ZSLOT0()\rPRINT "D-S1";ZSLOT1()\rPRINT "D-S2";ZSLOT2()\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_module_slot0
    type: assert.screen
    params:
      contains: "D-S0 30"
  - id: assert_direct_module_slot1
    type: assert.screen
    params:
      contains: "D-S1 31"
  - id: assert_direct_module_slot2
    type: assert.screen
    params:
      contains: "D-S2 32"

  - id: direct_module_span_overlay_copy
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-MOD SPAN OVERLAY COPY PROOFS\rPRINT "D-RST";ZCPYRST()\rPRINT "D-SPAN";ZSPAN()\rPRINT "D-OVL";ZOVL1();"/";ZOVL2()\rPRINT "D-RST";ZCPYRST()\rA=ZSLOT1()\rB=ZSLOT1()\rPRINT "D-COPY";ZCOPY()\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_module_span
    type: assert.screen
    params:
      contains: "D-SPAN 40"
  - id: assert_direct_module_overlay
    type: assert.screen
    params:
      contains: "D-OVL 51 / 52"
  - id: assert_direct_module_copy_skip
    type: assert.screen
    params:
      contains: "D-COPY 2"

  - id: direct_disk_module_load12
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-DISKMODULE LOAD PROOFS\rPRINT "D-LD1";ZMODLD("RBM.SAMPLE1")\rPRINT "D-DM1";ZDM1()\rPRINT "D-LD2";ZMODLD("RBM.SAMPLE2")\rPRINT "D-DM2";ZDM2S()\rPRINT "D-DOV";ZDOV1();"/";ZDOV2()\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_disk_module_load1
    type: assert.screen
    params:
      contains: "D-LD1 1"
  - id: assert_direct_disk_module_cmd1
    type: assert.screen
    params:
      contains: "D-DM1 61"
  - id: assert_direct_disk_module_load2
    type: assert.screen
    params:
      contains: "D-LD2 3"
  - id: assert_direct_disk_module_span
    type: assert.screen
    params:
      contains: "D-DM2 74"
  - id: assert_direct_disk_module_overlay
    type: assert.screen
    params:
      contains: "D-DOV 72 / 73"

  - id: direct_disk_module_load3
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-DISKMODULE RBM3 PROOFS\rPRINT "D-LD3";ZMODLD("RBM.SAMPLE3")\rPRINT "D-M3";ZSAA();"/";ZUEB()\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_disk_module_load3
    type: assert.screen
    params:
      contains: "D-LD3 30"
  - id: assert_direct_disk_module_rbm3
    type: assert.screen
    params:
      contains: "D-M3 4 / 113"

  - id: direct_strup_variable
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-STR VAR STRING RETURN T$=ABC SCREEN D-STR\rS$="abc"\rT$=UPPER(S$)\rPRINT "D-STR";T$\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_strup_variable
    type: assert.screen
    params:
      contains: "D-STRABC"

  - id: direct_strup_literal
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-LIT QUOTED STRING RETURN U$=DEF SCREEN D-LIT\rU$=UPPER("def")\rPRINT "D-LIT";U$\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_strup_literal
    type: assert.screen
    params:
      contains: "D-LITDEF"

  - id: direct_lower_variable
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-LOWER VAR STRING RETURN L$=abc SCREEN D-LOW\rS$="ABC"\rL$=LOWER(S$)\rPRINT "D-LOWASC";ASC(L$);ASC(MID$(L$,2,1));ASC(MID$(L$,3,1))\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_lower_variable
    type: assert.screen
    params:
      contains: "D-LOWASC 97  98  99"

  - id: direct_lower_literal
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-LOWER LITERAL STRING RETURN L$=ghi SCREEN D-LLOW\rL$=LOWER("GHI")\rPRINT "D-LLOWASC";ASC(L$);ASC(MID$(L$,2,1));ASC(MID$(L$,3,1))\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_lower_literal
    type: assert.screen
    params:
      contains: "D-LLOWASC 103  104  105"

  - id: direct_hcrc_hidden
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-ZHIDDENRAM HIDDEN A000 WORKER RETURN H%=131 SCREEN D-ZHIDDENRAM\rH%=ZHIDDENRAM("AB")\rPRINT "D-ZHIDDENRAM";H%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_hcrc_hidden
    type: assert.screen
    params:
      contains: "D-ZHIDDENRAM 131"
  - id: dump_direct_hidden_worker
    type: dump.memory_ranges
    params:
      ranges: *plugin_ranges

  - id: direct_sumai
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-SUM INT ARRAY PTR COUNT RETURN S%=6 SCREEN D-SUM\rDIM A%(3)\rA%(0)=1:A%(1)=2:A%(2)=3\rS%=ZSUMNUMARRAY(A%(0),3)\rPRINT "D-SUM";S%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_sumai
    type: assert.screen
    params:
      contains: "D-SUM 6"

  - id: direct_rangeai
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-RANGE RESULT FRAME TO INT ARRAY SCREEN D-RANGE\rDIM R%(4)\rZRANGENUMARRAY(7,4,R%(0))\rPRINT "D-RANGE";R%(0);R%(3)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_rangeai
    type: assert.screen
    params:
      contains: "D-RANGE 7  10"

  - id: direct_bufmake
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-BUFMAKE REU HANDLE TABLE RETURN H%=1 SCREEN D-BUF\rH%=BUFMAKE(300)\rPRINT "D-BUF";H%;":END"\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_bufmake
    type: assert.screen
    params:
      contains: "D-BUF 1 :END"
  - id: dump_direct_bufmake
    type: dump.memory_ranges
    params:
      ranges: *plugin_ranges

  - id: direct_buffill
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-BUFFILL REU HANDLE H% BYTE 170 SCREEN D-FILL\rBUFFILL(H%,170)\rPRINT "D-FILL OK"\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_buffill
    type: assert.screen
    params:
      contains: "D-FILL OK"
  - id: assert_direct_buffill_no_error
    type: assert.screen_not_contains
    params:
      not_contains: "?RB ERROR"

  - id: direct_bufdrop
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-BUFDROP RELEASE REU HANDLE H% SCREEN D-FREE\rBUFDROP(H%)\rPRINT "D-FREE OK"\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_bufdrop
    type: assert.screen
    params:
      contains: "D-FREE OK"
  - id: assert_direct_bufdrop_no_error
    type: assert.screen_not_contains
    params:
      not_contains: "?RB ERROR"

  - id: direct_scrcap_scrput_slot128
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-SCRCAP TEXT COLOR HANDLE RETURN AND SLOT128 SCRPUT\rPRINT "D-SCRMARK"\rS%=SCRCAP()\rPRINT "D-CHANGED"\rSCRPUT(S%)\rPRINT "D-SCRPUT";S%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_scrcap_restored_text
    type: assert.screen
    params:
      contains: "D-SCRMARK"
  - id: assert_direct_scrput_slot128
    type: assert.screen
    params:
      contains: "D-SCRPUT 1"
  - id: direct_buffill_rejects_screen_handle
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-BUFFILL MUST REJECT SCREEN HANDLE TYPE\rBUFFILL(S%,170)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_buffill_rejects_screen_handle
    type: assert.screen
    params:
      contains: "?RB ERROR 40"
  - id: direct_bufdrop_screen_handle
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-BUFDROP RELEASES SCREEN HANDLE TYPE\rBUFDROP(S%)\rPRINT "D-SCRFREE";S%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_bufdrop_screen_handle
    type: assert.screen
    params:
      contains: "D-SCRFREE 1"
  - id: assert_direct_bufdrop_screen_no_error
    type: assert.screen_not_contains
    params:
      not_contains: "?RB ERROR"
  - id: direct_scrput_rejects_buffer_handle
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-SCRPUT MUST REJECT BUFFER HANDLE TYPE\rB%=BUFMAKE(300)\rSCRPUT(B%)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_scrput_rejects_buffer_handle
    type: assert.screen
    params:
      contains: "?RB ERROR 40"
  - id: direct_free_buffer_after_type_error
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rBUFDROP(B%)\rPRINT "D-BUFREE";B%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_free_buffer_after_type_error
    type: assert.screen
    params:
      contains: "D-BUFREE 1"
  - id: assert_direct_free_buffer_no_error
    type: assert.screen_not_contains
    params:
      not_contains: "?RB ERROR"

  - id: direct_handle_128_edge
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 FOR I=1 TO 128\r20 H%=BUFMAKE(1)\r30 NEXT I\r40 PRINT "D-HMAX";H%\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 5.0
  - id: assert_direct_handle_128_edge
    type: assert.screen
    params:
      contains: "D-HMAX 128"
  - id: assert_direct_handle_128_no_error
    type: assert.screen_not_contains
    params:
      not_contains: "?RB ERROR"
  - id: direct_handle_129_rejected
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rE%=BUFMAKE(1)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_handle_129_rejected
    type: assert.screen
    params:
      contains: "?RB ERROR 33"
  - id: direct_free_128_handles
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 FOR I=1 TO 128\r20 BUFDROP(I)\r30 NEXT I\r40 PRINT "D-HFREED"\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 5.0
  - id: assert_direct_free_128_handles
    type: assert.screen
    params:
      contains: "D-HFREED"
  - id: assert_direct_free_128_no_error
    type: assert.screen_not_contains
    params:
      not_contains: "?RB ERROR"
  - id: direct_48k_heap_edge
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rM%=BUFMAKE(49152)\rPRINT "D-MAXBUF";M%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_48k_heap_edge
    type: assert.screen
    params:
      contains: "D-MAXBUF 1"
  - id: direct_48k_heap_full
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rE%=BUFMAKE(1)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_48k_heap_full
    type: assert.screen
    params:
      contains: "?RB ERROR 34"
  - id: direct_free_48k_heap
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rBUFDROP(M%)\rPRINT "D-MAXFREE";M%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_free_48k_heap
    type: assert.screen
    params:
      contains: "D-MAXFREE 1"
  - id: assert_direct_free_48k_no_error
    type: assert.screen_not_contains
    params:
      not_contains: "?RB ERROR"
  - id: direct_screen_heap_edge
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 FOR I=1 TO 24\r20 S%=SCRCAP()\r30 NEXT I\r40 PRINT "D-SMAX";S%\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 5.0
  - id: assert_direct_screen_heap_edge
    type: assert.screen
    params:
      contains: "D-SMAX 24"
  - id: direct_screen_heap_full
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rS%=SCRCAP()\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_screen_heap_full
    type: assert.screen
    params:
      contains: "?RB ERROR 34"
  - id: direct_free_screen_heap
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 FOR I=1 TO 24\r20 BUFDROP(I)\r30 NEXT I\r40 PRINT "D-SFREED"\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 5.0
  - id: assert_direct_free_screen_heap
    type: assert.screen
    params:
      contains: "D-SFREED"
  - id: assert_direct_free_screen_no_error
    type: assert.screen_not_contains
    params:
      not_contains: "?RB ERROR"

  - id: direct_tempscratch
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-TEMP REU TEMP PAGES RETURN T%=3 SCREEN D-TEMP\rT%=ZTEMPSCRATCH(513)\rPRINT "D-TEMP";T%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_tempscratch
    type: assert.screen
    params:
      contains: "D-TEMP 3"

  - id: direct_fail_clears_output
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-ZFAIL ERROR PATH CLEARS X% THEN RB ERROR 7\rX%=99\rZFAIL(7,X%)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_fail_error
    type: assert.screen
    params:
      contains: "?RB ERROR 7"
  - id: direct_fail_print_cleared
    type: input.sequence
    params:
      keys: [$(keys $'PRINT "D-FCLR";X%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_fail_cleared
    type: assert.screen
    params:
      contains: "D-FCLR 0"

  - id: direct_unknown_command
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-NOPE REGISTRY MISS SHOULD RB ERROR 1\rNOPE()\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_unknown_command
    type: assert.screen
    params:
      contains: "SYNTAX"

  - id: direct_old_ping_rejected
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-OLD SCALAR HAS NO ALIAS\rSCALAR(P%)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_old_ping_rejected
    type: assert.screen
    params:
      contains: "ERROR"

  - id: direct_old_add16_rejected
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-OLD ADD16 HAS NO ALIAS\rADD16(1,2,A%)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_old_add16_rejected
    type: assert.screen
    params:
      contains: "ERROR"

  - id: direct_old_strup_rejected
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-OLD STRUP HAS NO ALIAS\rSTRUP("abc",T$)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_old_strup_rejected
    type: assert.screen
    params:
      contains: "ERROR"

  - id: direct_old_hcrc_rejected
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-OLD HCRC HAS NO ALIAS\rHCRC("AB",H%)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_old_hcrc_rejected
    type: assert.screen
    params:
      contains: "ERROR"

  - id: direct_old_sumai_rejected
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-OLD SUMAI HAS NO ALIAS\rSUMAI(A%(0),1,S%)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_old_sumai_rejected
    type: assert.screen
    params:
      contains: "ERROR"

  - id: direct_old_rangeai_rejected
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-OLD RANGEAI HAS NO ALIAS\rRANGEAI(1,1,A%(0))\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_old_rangeai_rejected
    type: assert.screen
    params:
      contains: "ERROR"

  - id: direct_command_name_in_string
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-COMMAND NAME IN STRING SHOULD NOT DISPATCH\rPRINT "NOPE()"\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_command_name_in_string
    type: assert.screen
    params:
      contains: "NOPE()"
  - id: assert_direct_command_name_in_string_no_error
    type: assert.screen_not_contains
    params:
      not_contains: "?RB ERROR"

  - id: direct_command_name_in_rem
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM NOPE()\rPRINT "D-REMOK"\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_command_name_in_rem
    type: assert.screen
    params:
      contains: "D-REMOK"
  - id: assert_direct_command_name_in_rem_no_error
    type: assert.screen_not_contains
    params:
      not_contains: "?RB ERROR"

  - id: direct_first_comma_rejected
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-FIRST ARG COMMA IS NOT CANONICAL\rZADD16(,1,2,A%)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_first_comma_rejected
    type: assert.screen
    params:
      contains: "SYNTAX"

  - id: direct_missing_out_marker_rejected
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-LEGACY SPACE FORM REJECTED\rZADD16 1,2,A%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_missing_out_marker_rejected
    type: assert.screen
    params:
      contains: "SYNTAX"

  - id: direct_seed_resume_vars
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-RESUME SAVE BASIC VARS THROUGH READYOS EXIT\rV%=321:VS$="OK"\rPRINT "D-SEED";V%;":";VS$\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_seed_resume_vars
    type: assert.screen
    params:
      contains: "D-SEED 321 :OK"
  - id: direct_exit_to_launcher
    type: input.sequence
    params:
      keys: [$(keys $'EXIT\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: wait_launcher_after_exit
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 30
  - id: resume_after_exit
    type: input.sequence
    params:
      keys: [145,17,13]
      inter_key_delay_s: 0.10
      post_delay_s: 3.0
  - id: wait_readybasic_after_resume
    type: screen.wait_contains
    params:
      text: "READY."
      wait_timeout_s: 30
      capture_label: readybasic_full_suite_visual_after_resume
  - id: direct_registry_after_resume
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM D-POSTRES BASIC VARS AND REGISTRY STILL ACTIVE\rPRINT "D-STATE";V%;":";VS$\rP%=ZADD16(0,1)\rPRINT "D-RESUME";P%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_direct_state_after_resume
    type: assert.screen
    params:
      contains: "D-STATE 321 :OK"
  - id: assert_direct_registry_after_resume
    type: assert.screen
    params:
      contains: "D-RESUME 1"

  - id: program_ping_enter_and_list
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 REM ZECHO1 HEALTH LOW OVL STATEMENT STORES P%=1 SCREEN P-ZECHO\r20 ZECHO1(P%)\r30 PRINT "P-ZECHO";P%\rLIST\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_program_ping_rem
    type: assert.screen
    params:
      contains: "REM ZECHO1 HEALTH"
  - id: program_ping_run
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_program_ping_run
    type: assert.screen
    params:
      contains: "P-ZECHO 1"

  - id: program_chain_enter_and_list
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 REM ADD NUM PARAMS SAME LINE RETURN A%=3 SCREEN P-CHAIN\r20 A%=ZADD16(1,2):PRINT "P-CHAIN";A%\rLIST\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_program_chain_rem
    type: assert.screen
    params:
      contains: "REM ADD NUM PARAMS"
  - id: program_chain_run
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_program_chain_run
    type: assert.screen
    params:
      contains: "P-CHAIN 3"

  - id: program_if_true_run
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 REM IF TRUE THEN RETURN COMMAND SCREEN P-IFTRUE\r20 P%=0\r30 IF 1 THEN P%=ZADD16(0,1)\r40 PRINT "P-IFTRUE";P%\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_program_if_true_run
    type: assert.screen
    params:
      contains: "P-IFTRUE 1"

  - id: program_if_false_run
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 REM IF FALSE SKIPS RETURN COMMAND SCREEN P-IFFALSE\r20 P%=0\r30 IF 0 THEN P%=ZADD16(0,1)\r40 PRINT "P-IFFALSE";P%\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_program_if_false_run
    type: assert.screen
    params:
      contains: "P-IFFALSE 0"

  - id: program_for_next_run
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 REM FOR NEXT COMMAND RETURN SCREEN P-FOR\r20 FOR I=1 TO 3\r30 A%=ZADD16(I,10)\r40 NEXT\r50 PRINT "P-FOR";A%\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_program_for_next_run
    type: assert.screen
    params:
      contains: "P-FOR 13"

  - id: program_strup_enter_and_list
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 REM UPPER VAR STRING RETURN T$=ABC SCREEN P-STR\r20 S$="abc"\r30 T$=UPPER(S$)\r40 PRINT "P-STR";T$\rLIST\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_program_strup_rem
    type: assert.screen
    params:
      contains: "REM UPPER VAR STRING"
  - id: program_strup_run
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_program_strup_run
    type: assert.screen
    params:
      contains: "P-STRABC"

  - id: program_lower_enter_and_list
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 REM LOWER VAR STRING RETURN T$=abc SCREEN P-LOW\r20 S$="ABC"\r30 T$=LOWER(S$)\r40 PRINT "P-LOWASC";ASC(T$);ASC(MID$(T$,2,1));ASC(MID$(T$,3,1))\rLIST\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_program_lower_rem
    type: assert.screen
    params:
      contains: "REM LOWER VAR STRING"
  - id: program_lower_run
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_program_lower_run
    type: assert.screen
    params:
      contains: "P-LOWASC 97  98  99"

  - id: program_hcrc_enter_and_list
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 REM ZHIDDENRAM HIDDEN A000 WORKER RETURN H%=131 SCREEN P-ZHIDDENRAM\r20 H%=ZHIDDENRAM("AB")\r30 PRINT "P-ZHIDDENRAM";H%\rLIST\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_program_hcrc_rem
    type: assert.screen
    params:
      contains: "REM ZHIDDENRAM HIDDEN"
  - id: program_hcrc_run
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_program_hcrc_run
    type: assert.screen
    params:
      contains: "P-ZHIDDENRAM 131"
  - id: dump_program_hidden_worker
    type: dump.memory_ranges
    params:
      ranges: *plugin_ranges

  - id: program_sumai_enter_and_list
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 REM ZSUMNUMARRAY INT ARRAY PTR COUNT RETURN S%=6 SCREEN P-SUM\r20 DIM A%(3)\r30 A%(0)=1:A%(1)=2:A%(2)=3\r40 S%=ZSUMNUMARRAY(A%(0),3)\r50 PRINT "P-SUM";S%\rLIST\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_program_sumai_rem
    type: assert.screen
    params:
      contains: "REM ZSUMNUMARRAY INT ARRAY"
  - id: program_sumai_run
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_program_sumai_run
    type: assert.screen
    params:
      contains: "P-SUM 6"

  - id: program_range_enter_and_list
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 REM RANGE RESULT FRAME TO INT ARRAY SCREEN P-RANGE\r20 DIM R%(4)\r30 ZRANGENUMARRAY(7,4,R%(0))\r40 PRINT "P-RANGE";R%(0);R%(3)\rLIST\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_program_range_rem
    type: assert.screen
    params:
      contains: "REM RANGE RESULT FRAME"
  - id: program_range_run
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_program_range_run
    type: assert.screen
    params:
      contains: "P-RANGE 7  10"

  - id: program_handle_enter_and_list
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 REM BUF REU HANDLE ALLOC RETURN FILL FREE H%=1 SCREEN P-BUF\r20 H%=BUFMAKE(300)\r30 PRINT "P-BUF";H%\r40 BUFFILL(H%,170)\r50 BUFDROP(H%)\r60 PRINT "P-FREE";H%\rLIST\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_program_handle_rem
    type: assert.screen
    params:
      contains: "REM BUF REU HANDLE"
  - id: program_handle_run
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_program_handle_new
    type: assert.screen
    params:
      contains: "P-BUF 1"
  - id: assert_program_handle_free
    type: assert.screen
    params:
      contains: "P-FREE 1"
  - id: dump_program_handle
    type: dump.memory_ranges
    params:
      ranges: *plugin_ranges

  - id: program_fail_enter_and_list
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 REM ZFAIL ERROR CLEARS X% BEFORE RB ERROR 7 SCREEN P-FCLR\r20 X%=99\r30 ZFAIL(7,X%)\r40 PRINT "P-AFTER";X%\rLIST\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_program_fail_rem
    type: assert.screen
    params:
      contains: "REM ZFAIL ERROR"
  - id: program_fail_run
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_program_fail_error
    type: assert.screen
    params:
      contains: "?RB ERROR 7"
  - id: program_fail_print_cleared
    type: input.sequence
    params:
      keys: [$(keys $'PRINT "P-FCLR";X%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: assert_program_fail_cleared
    type: assert.screen
    params:
      contains: "P-FCLR 0"

  - id: procfunc_load_rbproc1
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rLOAD "RBPROC1",8\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.5
  - id: procfunc_list_rbproc1
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rLIST 1000-1020\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 2.0
  - id: assert_procfunc_list_proc
    type: assert.screen
    params:
      contains: "PROC SHOW0"
  - id: assert_procfunc_list_endp
    type: assert.screen
    params:
      contains: "ENDP"
  - id: procfunc_list_func_addi
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rLIST 1300-1320\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 2.0
  - id: assert_procfunc_list_func
    type: assert.screen
    params:
      contains: "FUNC ADDI"
  - id: procfunc_list_nested_func
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rLIST 1600-1630\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 2.0
  - id: assert_procfunc_list_exec_out
    type: assert.screen
    params:
      contains: "R%=INNER(X%)"
  - id: procfunc_run_rbproc1
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 4.0
  - id: assert_procfunc_run_float_nested
    type: assert.screen
    params:
      contains: "NFADD"
  - id: assert_procfunc_run_string_nested
    type: assert.screen
    params:
      contains: "NGS MIX"
  - id: assert_procfunc_run_float_statement
    type: assert.screen
    params:
      contains: "SFADD"

  - id: dump_final_plugin_state
    type: dump.memory_ranges
    params:
      ranges: *plugin_ranges
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
