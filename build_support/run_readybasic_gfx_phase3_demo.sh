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
PLAN="${READYBASIC_PLAN:-$SCRIPT_DIR/readybasic_gfx_phase3_demo.generated.yaml}"
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
plan_id: readybasic_gfx_phase3_demo
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
  - id: launch_readyos_preboot
    type: vice.launch
    params:
      kill_stale: true
  - id: wait_readybasic_loaded_by_readyos
    type: screen.wait_contains
    params:
      text: "FREE:"
      wait_timeout_s: 180
      capture_on_success: true
      capture_label: readybasic_gfx_phase3_prompt
  - id: clear_for_phase3
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.4
  - id: probe_pbuf_free
    type: input.sequence
    params:
      keys: [$(keys $'PBMAKE(3,H%):PBDROP(H%):PRINT "OK 73"\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: assert_pbuf_free
    type: assert.screen
    params:
      contains: "OK 73"
  - id: load_rbgfx17
    type: input.sequence
    params:
      keys: [$(keys $'LOAD "RBGFX17",8\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 2.2
  - id: list_rbgfx17
    type: input.sequence
    params:
      keys: [76,73,83,84,13]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: capture_list_rbgfx17
    type: screen.capture
    params:
      label: readybasic_gfx_phase3_list_rbgfx17
      pitch: "RBGFX17 listed inside ReadyBASIC under ReadyOS"
  - id: assert_list_rbgfx17
    type: assert.screen
    params:
      contains: "DEMO 17"
  - id: run_rbgfx17
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 5.0
  - id: capture_poly_array
    type: screen.capture
    params:
      label: readybasic_gfx_phase3_poly_array
      pitch: "POLY(A%(0),count,color) outline polygons"
  - id: finish_rbgfx17
    type: input.key
    params:
      key: SPACE
      post_delay_s: 1.0
  - id: wait_rbgfx17_done
    type: screen.wait_contains
    params:
      text: "PHASE3 DONE 17"
      wait_timeout_s: 60
      capture_on_success: true
      capture_label: readybasic_gfx_phase3_poly_array_done
  - id: load_rbgfx18
    type: input.sequence
    params:
      keys: [$(keys $'LOAD "RBGFX18",8\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 5.0
  - id: capture_fpoly_array
    type: screen.capture
    params:
      label: readybasic_gfx_phase3_fpoly_array
      pitch: "FPOLY(A%(0),count,color) filled polygon demo"
  - id: finish_rbgfx18
    type: input.key
    params:
      key: SPACE
      post_delay_s: 1.0
  - id: wait_rbgfx18_done
    type: screen.wait_contains
    params:
      text: "PHASE3 DONE 18"
      wait_timeout_s: 60
      capture_on_success: true
      capture_label: readybasic_gfx_phase3_fpoly_array_done
  - id: load_rbgfx19
    type: input.sequence
    params:
      keys: [$(keys $'LOAD "RBGFX19",8\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 5.0
  - id: capture_poly_reu
    type: screen.capture
    params:
      label: readybasic_gfx_phase3_poly_reu
      pitch: "PBMAKE/PBUFSET plus POLYH handle polygon demo"
  - id: finish_rbgfx19
    type: input.key
    params:
      key: SPACE
      post_delay_s: 1.0
  - id: wait_rbgfx19_done
    type: screen.wait_contains
    params:
      text: "PHASE3 DONE 19"
      wait_timeout_s: 60
      capture_on_success: true
      capture_label: readybasic_gfx_phase3_poly_reu_done
  - id: load_rbgfx20
    type: input.sequence
    params:
      keys: [$(keys $'LOAD "RBGFX20",8\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 5.0
  - id: capture_fpoly_reu
    type: screen.capture
    params:
      label: readybasic_gfx_phase3_fpoly_reu_showcase
      pitch: "PBMAKE/PBUFSET plus FPOLYH filled polygon showcase"
  - id: finish_rbgfx20
    type: input.key
    params:
      key: SPACE
      post_delay_s: 1.0
  - id: wait_rbgfx20_done
    type: screen.wait_contains
    params:
      text: "PHASE3 DONE 20"
      wait_timeout_s: 60
      capture_on_success: true
      capture_label: readybasic_gfx_phase3_fpoly_reu_done
  - id: assert_no_error
    type: assert.screen_not_contains
    params:
      not_contains: "?"
YAML

if [ "${READYBASIC_GENERATE_PLAN_ONLY:-0}" = "1" ]; then
  echo "wrote $PLAN"
  exit 0
fi

dotnet run --project "$PROJECT" -- run --plan "$PLAN" $CLI_CLOSE_ARG
