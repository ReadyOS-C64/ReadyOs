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
PLAN="${READYBASIC_PLAN:-$SCRIPT_DIR/readybasic_gfx_phase1_probe.generated.yaml}"
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
plan_id: readybasic_gfx_phase1_probe
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
    step_s: 90
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
      capture_label: readybasic_gfx_prompt

  - id: bitmap_program
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 GFXMODE("HIRES"):GFXCLEAR(0):PLOT(40,40,1)\r20 PNT(40,40,A%)\r30 PNT(41,40,B%)\r40 H%=GFXSURF("HIRES"):GFXBLIT(H%)\r50 GFXTEXT()\r60 PRINT "GFXPASS";A%;B%\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 2.0
  - id: capture_bitmap_program
    type: screen.capture
    params:
      label: readybasic_gfx_bitmap_program
  - id: assert_bitmap_program
    type: assert.screen
    params:
      contains: "GFXPASS"

  - id: tile_sprite_input_program
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rNEW\r10 GFXMODE("TILE"):GFXCLEAR(0):PLOT(3,3,5)\r20 PNT(3,3,A%)\r30 SPRSET(0,1,2,0):SPRMOVE(0,80,90):SPRCOL(0,4)\r40 JOY(2,J%):KEYP(K%):KEYSCAN():KEYLAST(L%)\r50 GFXTEXT()\r60 PRINT "GFXIO";A%\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 2.0
  - id: capture_tile_sprite_input_program
    type: screen.capture
    params:
      label: readybasic_gfx_tile_sprite_input_program
  - id: assert_tile_sprite_input_program
    type: assert.screen
    params:
      contains: "READY."
  - id: assert_tile_sprite_input_no_error
    type: assert.screen_not_contains
    params:
      not_contains: "?"

  - id: load_demo
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rLOAD "RBGFX01",8\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.5
  - id: list_demo
    type: input.sequence
    params:
      keys: [76,73,83,84,13]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: assert_demo_lists
    type: assert.screen
    params:
      contains: "RBGFX01 MODES"
YAML

if [ "${READYBASIC_GENERATE_PLAN_ONLY:-0}" = "1" ]; then
  echo "wrote $PLAN"
  exit 0
fi

dotnet run --project "$PROJECT" -- run --plan "$PLAN" $CLI_CLOSE_ARG
