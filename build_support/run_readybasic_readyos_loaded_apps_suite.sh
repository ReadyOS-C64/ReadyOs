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
PLAN="${READYBASIC_PLAN:-$SCRIPT_DIR/readybasic_readyos_loaded_apps_suite.generated.yaml}"
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

append_load_list_run_text_app() {
  local disk_name="$1"
  local id_name="$2"
  local title="$3"
  local run_assert="$4"
  local run_delay="${5:-2.0}"
  cat >>"$PLAN" <<YAML
  - id: clear_before_${id_name}
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.4
  - id: load_${id_name}
    type: input.sequence
    params:
      keys: [$(keys "LOAD \"${disk_name}\",8"$'\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 2.2
  - id: capture_loaded_${id_name}
    type: screen.capture
    params:
      label: readybasic_loaded_${id_name}
      note: "ReadyOS-loaded ReadyBASIC after LOAD ${disk_name}"
  - id: list_${id_name}
    type: input.sequence
    params:
      keys: [76,73,83,84,13]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: capture_list_${id_name}
    type: screen.capture
    params:
      label: readybasic_list_${id_name}
  - id: assert_list_${id_name}
    type: assert.screen
    params:
      contains: "$title"
  - id: run_${id_name}
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: $run_delay
  - id: capture_run_${id_name}
    type: screen.capture
    params:
      label: readybasic_run_${id_name}
      note: "RUN ${disk_name} from inside ReadyBASIC under ReadyOS"
  - id: assert_run_${id_name}
    type: assert.screen
    params:
      contains: "$run_assert"
  - id: assert_no_error_${id_name}
    type: assert.screen_not_contains
    params:
      not_contains: "?"
YAML
}

append_load_list_run_gfx_app() {
  local disk_name="$1"
  local id_name="$2"
  local title="$3"
  local run_delay="${4:-3.0}"
  cat >>"$PLAN" <<YAML
  - id: clear_before_${id_name}
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.4
  - id: load_${id_name}
    type: input.sequence
    params:
      keys: [$(keys "LOAD \"${disk_name}\",8"$'\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 2.2
  - id: capture_loaded_${id_name}
    type: screen.capture
    params:
      label: readybasic_loaded_${id_name}
      note: "ReadyOS-loaded ReadyBASIC after LOAD ${disk_name}"
  - id: list_${id_name}
    type: input.sequence
    params:
      keys: [76,73,83,84,13]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: capture_list_${id_name}
    type: screen.capture
    params:
      label: readybasic_list_${id_name}
  - id: assert_list_${id_name}
    type: assert.screen
    params:
      contains: "$title"
  - id: run_${id_name}
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: $run_delay
  - id: capture_graphics_${id_name}
    type: screen.capture
    params:
      label: readybasic_graphics_${id_name}
      note: "Graphics screenshot after RUN ${disk_name} from inside ReadyBASIC under ReadyOS"
  - id: restore_text_${id_name}
    type: input.sequence
    params:
      keys: [$(keys "GFXTEXT():PRINT \"${disk_name} COMPLETE\""$'\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: capture_restored_${id_name}
    type: screen.capture
    params:
      label: readybasic_restored_${id_name}
  - id: assert_complete_${id_name}
    type: assert.screen
    params:
      contains: "${disk_name} COMPLETE"
  - id: assert_no_error_${id_name}
    type: assert.screen_not_contains
    params:
      not_contains: "?"
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
plan_id: readybasic_readyos_loaded_apps_suite
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
      capture_label: readyos_loaded_readybasic_prompt
  - id: assert_readybasic_banner
    type: assert.screen
    params:
      contains: "READYBASIC"
YAML

append_load_list_run_text_app "RBTEST1" "rbtest1" "10 ZECHO1" "EXPRSTRREADY" "2.0"
append_load_list_run_text_app "RBPROC1" "rbproc1" "2220 ENDP" "NGS MIX" "4.0"

append_load_list_run_gfx_app "RBGFX01" "rbgfx01" "RBGFX01 MODES" "2.0"
append_load_list_run_gfx_app "RBGFX02" "rbgfx02" "RBGFX02 HIRES PLOT" "3.0"
append_load_list_run_gfx_app "RBGFX03" "rbgfx03" "RBGFX03 HIRES LINES" "3.0"
append_load_list_run_gfx_app "RBGFX04" "rbgfx04" "RBGFX04 RECTS" "3.0"
append_load_list_run_gfx_app "RBGFX05" "rbgfx05" "RBGFX05 POINT READ" "2.0"
append_load_list_run_gfx_app "RBGFX06" "rbgfx06" "RBGFX06 REU SURFACE" "2.0"
append_load_list_run_gfx_app "RBGFX07" "rbgfx07" "RBGFX07 MBITMAP" "3.0"
append_load_list_run_gfx_app "RBGFX08" "rbgfx08" "RBGFX08 TILE" "3.0"
append_load_list_run_gfx_app "RBGFX09" "rbgfx09" "RBGFX09 SPRITES" "4.0"
append_load_list_run_gfx_app "RBGFX10" "rbgfx10" "RBGFX10 COLLISION" "2.5"
append_load_list_run_gfx_app "RBGFX11" "rbgfx11" "RBGFX11 INPUT" "4.0"
append_load_list_run_gfx_app "RBGFX12" "rbgfx12" "RBGFX12 SHOWCASE" "3.0"
append_load_list_run_gfx_app "RBGFX17" "rbgfx17" "DEMO 17" "3.0"
append_load_list_run_gfx_app "RBGFX18" "rbgfx18" "DEMO 18" "3.0"
append_load_list_run_gfx_app "RBGFX19" "rbgfx19" "DEMO 19" "3.0"
append_load_list_run_gfx_app "RBGFX20" "rbgfx20" "DEMO 20" "3.0"

if [ "${READYBASIC_GENERATE_PLAN_ONLY:-0}" = "1" ]; then
  echo "wrote $PLAN"
  exit 0
fi

dotnet run --project "$PROJECT" -- run --plan "$PLAN" $CLI_CLOSE_ARG
