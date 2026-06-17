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
PLAN="${READYBASIC_PLAN:-$SCRIPT_DIR/readybasic_gfx_phase5_demo.generated.yaml}"
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
plan_id: readybasic_gfx_phase5_demo
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
      capture_label: readybasic_gfx_phase5_prompt
  - id: load_rbgfx28
    type: input.sequence
    params:
      keys: [$(keys $'LOAD "RBGFX28",8\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: capture_tile_visible
    type: screen.capture
    params:
      label: readybasic_gfx_phase5_tile_visible
      note: "TILE immediate PLOT/LINE/RECT/FRECT through default Bank D glyphs"
  - id: finish_rbgfx28
    type: input.key
    params:
      key: SPACE
      post_delay_s: 1.0
  - id: wait_rbgfx28_done
    type: screen.wait_contains
    params:
      text: "TILE VISIBLE"
      wait_timeout_s: 60
      capture_on_success: true
      capture_label: readybasic_gfx_phase5_tile_visible_done
  - id: load_rbgfx29
    type: input.sequence
    params:
      keys: [$(keys $'LOAD "RBGFX29",8\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: capture_mtile_visible
    type: screen.capture
    params:
      label: readybasic_gfx_phase5_mtile_visible
      note: "MTILE immediate PLOT/LINE/RECT/FRECT through default Bank D glyphs"
  - id: finish_rbgfx29
    type: input.key
    params:
      key: SPACE
      post_delay_s: 1.0
  - id: wait_rbgfx29_done
    type: screen.wait_contains
    params:
      text: "MTILE VISIBLE"
      wait_timeout_s: 60
      capture_on_success: true
      capture_label: readybasic_gfx_phase5_mtile_visible_done
  - id: load_rbgfx30
    type: input.sequence
    params:
      keys: [$(keys $'LOAD "RBGFX30",8\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: capture_mbitmap_dlist
    type: screen.capture
    params:
      label: readybasic_gfx_phase5_mbitmap_dlist
      note: "MBITMAP retained DLPLOT/DLLINE/DLRECT/DLFRECT/DLDRAW replay"
  - id: finish_rbgfx30
    type: input.key
    params:
      key: SPACE
      post_delay_s: 1.0
  - id: wait_rbgfx30_done
    type: screen.wait_contains
    params:
      text: "MBITMAP DLIST DONE"
      wait_timeout_s: 60
      capture_on_success: true
      capture_label: readybasic_gfx_phase5_mbitmap_dlist_done
  - id: load_rbgfx31
    type: input.sequence
    params:
      keys: [$(keys $'LOAD "RBGFX31",8\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: capture_sync_source
    type: screen.capture
    params:
      label: readybasic_gfx_phase5_sync_source
      note: "Visible Bank D surface before GFXSYNC snapshot"
  - id: sync_next_clear
    type: input.key
    params:
      key: SPACE
      post_delay_s: 1.0
  - id: capture_sync_cleared
    type: screen.capture
    params:
      label: readybasic_gfx_phase5_sync_cleared
      note: "Visible Bank D surface after GFXCLEAR"
  - id: sync_next_blit
    type: input.key
    params:
      key: SPACE
      post_delay_s: 1.0
  - id: capture_sync_restored
    type: screen.capture
    params:
      label: readybasic_gfx_phase5_sync_restored
      note: "Visible Bank D surface restored by GFXBLIT"
  - id: finish_rbgfx31
    type: input.key
    params:
      key: SPACE
      post_delay_s: 1.0
  - id: wait_rbgfx31_done
    type: screen.wait_contains
    params:
      text: "SYNC BLIT DONE"
      wait_timeout_s: 60
      capture_on_success: true
      capture_label: readybasic_gfx_phase5_sync_blit_done
  - id: load_rbgfx32
    type: input.sequence
    params:
      keys: [$(keys $'LOAD "RBGFX32",8\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: capture_convex_poly
    type: screen.capture
    params:
      label: readybasic_gfx_phase5_convex_poly
      note: "POLY and FPOLY convex polygon showcase"
  - id: finish_rbgfx32
    type: input.key
    params:
      key: SPACE
      post_delay_s: 1.0
  - id: wait_rbgfx32_done
    type: screen.wait_contains
    params:
      text: "CONVEX POLY DONE"
      wait_timeout_s: 60
      capture_on_success: true
      capture_label: readybasic_gfx_phase5_convex_poly_done
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
