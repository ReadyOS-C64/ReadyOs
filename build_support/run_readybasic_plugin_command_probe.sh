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
PLAN="${READYBASIC_PLAN:-$SCRIPT_DIR/readybasic_plugin_command_probe.generated.yaml}"
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
plan_id: readybasic_plugin_command_probe
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
      capture_label: readybasic_plugin_prompt
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

  - id: probe_01_ping
    type: input.sequence
    params:
      keys: [$(keys $'ZECHO1(P%)\rPRINT "ZECHO";P%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: capture_01_ping
    type: screen.capture
    params:
      label: after_zecho1
  - id: assert_01_ping
    type: assert.screen
    params:
      contains: "ZECHO 1"
  - id: dump_01_ping
    type: dump.memory_ranges
    params:
      ranges: *plugin_ranges

  - id: probe_02_add16
    type: input.sequence
    params:
      keys: [$(keys $'A%=ZADD16(5,10)\rPRINT "ADD";A%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_02_add16
    type: assert.screen
    params:
      contains: "ADD 15"

  - id: probe_02a_if_then_direct
    type: input.sequence
    params:
      keys: [$(keys $'P%=0\rIF 1 THEN P%=ZADD16(0,1)\rPRINT "IFDIR";P%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: capture_02a_if_then_direct
    type: screen.capture
    params:
      label: after_if_then_direct
  - id: assert_02a_if_then_direct
    type: assert.screen
    params:
      contains: "IFDIR 1"

  - id: probe_03_strup_variable
    type: input.sequence
    params:
      keys: [$(keys $'S$="abc"\rT$=UPPER(S$)\rPRINT "STR";T$\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_03_strup_variable
    type: assert.screen
    params:
      contains: "STRABC"

  - id: probe_04_strup_literal
    type: input.sequence
    params:
      keys: [$(keys $'U$=UPPER("def")\rPRINT "LIT";U$\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_04_strup_literal
    type: assert.screen
    params:
      contains: "LITDEF"

  - id: probe_04a_lower_variable
    type: input.sequence
    params:
      keys: [$(keys $'S$="ABC"\rL$=LOWER(S$)\rPRINT "LOWASC";ASC(L$);ASC(MID$(L$,2,1));ASC(MID$(L$,3,1))\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_04a_lower_variable
    type: assert.screen
    params:
      contains: "LOWASC 97  98  99"

  - id: probe_04b_lower_literal
    type: input.sequence
    params:
      keys: [$(keys $'L$=LOWER("GHI")\rPRINT "LITLOWASC";ASC(L$);ASC(MID$(L$,2,1));ASC(MID$(L$,3,1))\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_04b_lower_literal
    type: assert.screen
    params:
      contains: "LITLOWASC 103  104  105"

  - id: probe_05_hcrc_hidden
    type: input.sequence
    params:
      keys: [$(keys $'H%=ZHIDDENRAM("AB")\rPRINT "ZHIDDENRAM";H%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_05_hcrc_hidden
    type: assert.screen
    params:
      contains: "ZHIDDENRAM 131"
  - id: dump_05_hidden_worker
    type: dump.memory_ranges
    params:
      ranges: *plugin_ranges

  - id: probe_06_sumai
    type: input.sequence
    params:
      keys: [$(keys $'DIM A%(3)\rA%(0)=1:A%(1)=2:A%(2)=3\rS%=ZSUMNUMARRAY(A%(0),3)\rPRINT "SUM";S%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: assert_06_sumai
    type: assert.screen
    params:
      contains: "SUM 6"

  - id: probe_07_rangeai
    type: input.sequence
    params:
      keys: [$(keys $'DIM R%(4)\rZRANGENUMARRAY(7,4,R%(0))\rPRINT "RANGE";R%(0);R%(3)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: assert_07_rangeai
    type: assert.screen
    params:
      contains: "RANGE 7  10"

  - id: probe_08_bufmake
    type: input.sequence
    params:
      keys: [$(keys $'H%=BUFMAKE(300)\rPRINT "BUFMAKE";H%;":END"\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_08_bufmake
    type: assert.screen
    params:
      contains: "BUFMAKE 1 :END"
  - id: dump_08_bufmake
    type: dump.memory_ranges
    params:
      ranges: *plugin_ranges

  - id: probe_09_buffill
    type: input.sequence
    params:
      keys: [$(keys $'BUFFILL(H%,170)\rPRINT "FILL OK"\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_09_buffill
    type: assert.screen
    params:
      contains: "FILL OK"
  - id: assert_09_buffill_no_error
    type: assert.screen_not_contains
    params:
      not_contains: "?RB ERROR"

  - id: probe_10_bufdrop
    type: input.sequence
    params:
      keys: [$(keys $'BUFDROP(H%)\rPRINT "FREE OK"\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_10_bufdrop
    type: assert.screen
    params:
      contains: "FREE OK"
  - id: assert_10_bufdrop_no_error
    type: assert.screen_not_contains
    params:
      not_contains: "?RB ERROR"

  - id: probe_10a_scrcap_scrput_slot128
    type: input.sequence
    params:
      keys: [$(keys $'PRINT "SCRMARK"\rS%=SCRCAP()\rPRINT "CHANGED"\rSCRPUT(S%)\rPRINT "SCRPUT";S%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: assert_10a_scrcap_restored_text
    type: assert.screen
    params:
      contains: "SCRMARK"
  - id: assert_10a_scrput_slot128
    type: assert.screen
    params:
      contains: "SCRPUT 1"
  - id: probe_10b_buffill_rejects_screen_handle
    type: input.sequence
    params:
      keys: [$(keys $'BUFFILL(S%,170)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_10b_buffill_rejects_screen_handle
    type: assert.screen
    params:
      contains: "?RB ERROR 40"
  - id: probe_10c_bufdrop_screen_handle
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rBUFDROP(S%)\rPRINT "SCRFREE";S%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_10c_bufdrop_screen_handle
    type: assert.screen
    params:
      contains: "SCRFREE 1"
  - id: assert_10c_bufdrop_screen_no_error
    type: assert.screen_not_contains
    params:
      not_contains: "?RB ERROR"
  - id: probe_10d_scrput_rejects_buffer_handle
    type: input.sequence
    params:
      keys: [$(keys $'B%=BUFMAKE(300)\rSCRPUT(B%)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_10d_scrput_rejects_buffer_handle
    type: assert.screen
    params:
      contains: "?RB ERROR 40"
  - id: probe_10e_free_buffer_after_type_error
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rBUFDROP(B%)\rPRINT "BUFREE";B%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_10e_free_buffer_after_type_error
    type: assert.screen
    params:
      contains: "BUFREE 1"
  - id: assert_10e_free_buffer_no_error
    type: assert.screen_not_contains
    params:
      not_contains: "?RB ERROR"

  - id: probe_10f_allocate_128_handles
    type: input.sequence
    params:
      keys: [$(keys $'NEW\r10 FOR I=1 TO 128\r20 H%=BUFMAKE(1)\r30 NEXT I\r40 PRINT "HMAX";H%\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 4.0
  - id: assert_10f_allocate_128_handles
    type: assert.screen
    params:
      contains: "HMAX 128"
  - id: assert_10f_allocate_128_no_error
    type: assert.screen_not_contains
    params:
      not_contains: "?RB ERROR"

  - id: probe_10g_handle_129_rejected
    type: input.sequence
    params:
      keys: [$(keys $'E%=BUFMAKE(1)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_10g_handle_129_rejected
    type: assert.screen
    params:
      contains: "?RB ERROR 33"

  - id: probe_10h_free_128_handles
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 FOR I=1 TO 128\r20 BUFDROP(I)\r30 NEXT I\r40 PRINT "HFREED"\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 4.0
  - id: assert_10h_free_128_handles
    type: assert.screen
    params:
      contains: "HFREED"
  - id: assert_10h_free_128_no_error
    type: assert.screen_not_contains
    params:
      not_contains: "?RB ERROR"

  - id: probe_10i_reuse_low_handle
    type: input.sequence
    params:
      keys: [$(keys $'R%=BUFMAKE(1)\rPRINT "HREUSE";R%\rBUFDROP(R%)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_10i_reuse_low_handle
    type: assert.screen
    params:
      contains: "HREUSE 1"
  - id: assert_10i_reuse_low_no_error
    type: assert.screen_not_contains
    params:
      not_contains: "?RB ERROR"

  - id: probe_10j_allocate_48k_heap
    type: input.sequence
    params:
      keys: [$(keys $'M%=BUFMAKE(49152)\rPRINT "HMAXBUF";M%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: assert_10j_allocate_48k_heap
    type: assert.screen
    params:
      contains: "HMAXBUF 1"
  - id: assert_10j_allocate_48k_no_error
    type: assert.screen_not_contains
    params:
      not_contains: "?RB ERROR"

  - id: probe_10k_heap_full_rejected
    type: input.sequence
    params:
      keys: [$(keys $'E%=BUFMAKE(1)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_10k_heap_full_rejected
    type: assert.screen
    params:
      contains: "?RB ERROR 34"

  - id: probe_10l_free_48k_heap
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rBUFDROP(M%)\rPRINT "HMAXFREE";M%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_10l_free_48k_heap
    type: assert.screen
    params:
      contains: "HMAXFREE 1"
  - id: assert_10l_free_48k_no_error
    type: assert.screen_not_contains
    params:
      not_contains: "?RB ERROR"

  - id: probe_10m_screen_handles_fill_heap
    type: input.sequence
    params:
      keys: [$(keys $'NEW\r10 FOR I=1 TO 24\r20 S%=SCRCAP()\r30 NEXT I\r40 PRINT "SMAX";S%\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 4.0
  - id: assert_10m_screen_handles_fill_heap
    type: assert.screen
    params:
      contains: "SMAX 24"
  - id: assert_10m_screen_handles_no_error
    type: assert.screen_not_contains
    params:
      not_contains: "?RB ERROR"

  - id: probe_10n_screen_heap_full_rejected
    type: input.sequence
    params:
      keys: [$(keys $'S%=SCRCAP()\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_10n_screen_heap_full_rejected
    type: assert.screen
    params:
      contains: "?RB ERROR 34"

  - id: probe_10o_free_screen_handles
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 FOR I=1 TO 24\r20 BUFDROP(I)\r30 NEXT I\r40 PRINT "SFREED"\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 4.0
  - id: assert_10o_free_screen_handles
    type: assert.screen
    params:
      contains: "SFREED"
  - id: assert_10o_free_screen_no_error
    type: assert.screen_not_contains
    params:
      not_contains: "?RB ERROR"

  - id: probe_11_tempscratch
    type: input.sequence
    params:
      keys: [$(keys $'T%=ZTEMPSCRATCH(513)\rPRINT "TEMP";T%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_11_tempscratch
    type: assert.screen
    params:
      contains: "TEMP 3"

  - id: probe_12_fail_clears_output
    type: input.sequence
    params:
      keys: [$(keys $'X%=99\rZFAIL(7,X%)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_12_fail_error
    type: assert.screen
    params:
      contains: "?RB ERROR 7"
  - id: probe_12_print_cleared_output
    type: input.sequence
    params:
      keys: [$(keys $'PRINT "FAILCLR";X%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_12_fail_cleared_output
    type: assert.screen
    params:
      contains: "FAILCLR 0"

  - id: probe_13_unknown_command
    type: input.sequence
    params:
      keys: [$(keys $'NOPE()\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_13_unknown_command
    type: assert.screen
    params:
      contains: "SYNTAX"

  - id: probe_13_old_ping_rejected
    type: input.sequence
    params:
      keys: [$(keys $'SCALAR(P%)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_13_old_ping_rejected
    type: assert.screen
    params:
      contains: "ERROR"

  - id: probe_13_old_add16_rejected
    type: input.sequence
    params:
      keys: [$(keys $'ADD16(1,2,A%)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_13_old_add16_rejected
    type: assert.screen
    params:
      contains: "ERROR"

  - id: probe_13_old_strup_rejected
    type: input.sequence
    params:
      keys: [$(keys $'STRUP("abc",T$)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_13_old_strup_rejected
    type: assert.screen
    params:
      contains: "ERROR"

  - id: probe_13_old_hcrc_rejected
    type: input.sequence
    params:
      keys: [$(keys $'HCRC("AB",H%)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_13_old_hcrc_rejected
    type: assert.screen
    params:
      contains: "ERROR"

  - id: probe_13_old_sumai_rejected
    type: input.sequence
    params:
      keys: [$(keys $'SUMAI(A%(0),1,S%)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_13_old_sumai_rejected
    type: assert.screen
    params:
      contains: "ERROR"

  - id: probe_13_old_rangeai_rejected
    type: input.sequence
    params:
      keys: [$(keys $'RANGEAI(1,1,A%(0))\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_13_old_rangeai_rejected
    type: assert.screen
    params:
      contains: "ERROR"

  - id: probe_13a_command_name_in_string
    type: input.sequence
    params:
      keys: [$(keys $'PRINT "NOPE()"\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_13a_command_name_in_string
    type: assert.screen
    params:
      contains: "NOPE()"
  - id: assert_13a_no_rb_error
    type: assert.screen_not_contains
    params:
      not_contains: "?RB ERROR"

  - id: probe_13b_command_name_in_rem
    type: input.sequence
    params:
      keys: [$(keys $'REM NOPE()\rPRINT "REMOK"\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_13b_command_name_in_rem
    type: assert.screen
    params:
      contains: "REMOK"
  - id: assert_13b_no_rb_error
    type: assert.screen_not_contains
    params:
      not_contains: "?RB ERROR"

  - id: probe_13c_first_comma_rejected
    type: input.sequence
    params:
      keys: [$(keys $'ZADD16(,1,2,A%)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_13c_first_comma_rejected
    type: assert.screen
    params:
      contains: "SYNTAX"

  - id: probe_13d_missing_out_marker_rejected
    type: input.sequence
    params:
      keys: [$(keys $'ZADD16 1,2,A%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_13d_missing_out_marker_rejected
    type: assert.screen
    params:
      contains: "SYNTAX"

  - id: probe_14_seed_resume_vars
    type: input.sequence
    params:
      keys: [$(keys $'V%=321:VS$="OK"\rPRINT "SEEDED";V%;":";VS$\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_14_seed_resume_vars
    type: assert.screen
    params:
      contains: "SEEDED 321 :OK"

  - id: probe_15_exit_to_launcher
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
      capture_label: readybasic_after_plugin_resume
  - id: probe_16_registry_after_resume
    type: input.sequence
    params:
      keys: [$(keys $'PRINT "STATE";V%;":";VS$\rP%=ZADD16(0,1)\rPRINT "RESUME";P%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: capture_16_registry_after_resume
    type: screen.capture
    params:
      label: after_registry_resume
  - id: dump_16_vectors_after_resume
    type: dump.memory_ranges
    params:
      ranges:
        - { label: page3_vectors, start: 0x0304, end: 0x030B }
        - { label: desc_buf, start: 0xC480, end: 0xC49F }
        - { label: bridge_tail, start: 0xC1D0, end: 0xC20F }
        - { label: low_slot_head, start: 0xA800, end: 0xA83F }
  - id: assert_16_basic_state_after_resume
    type: assert.screen
    params:
      contains: "STATE 321 :OK"
  - id: assert_16_registry_after_resume
    type: assert.screen
    params:
      contains: "RESUME 1"
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
