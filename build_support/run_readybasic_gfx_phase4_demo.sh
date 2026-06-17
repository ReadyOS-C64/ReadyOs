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
PLAN="${READYBASIC_PLAN:-$SCRIPT_DIR/readybasic_gfx_phase4_demo.generated.yaml}"
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
plan_id: readybasic_gfx_phase4_demo
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
    step_s: 220
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
      capture_label: readybasic_gfx_phase4_prompt
  - id: probe_dlnew
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147):DLMAKE(12,H%):PRINT "DLPROBE";H%;ERRCODE()\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: capture_dlnew_probe
    type: screen.capture
    params:
      label: readybasic_gfx_phase4_dlnew_probe
  - id: assert_dlnew_probe
    type: assert.screen
    params:
      contains: "DLPROBE"
  - id: load_rbgfx23
    type: input.sequence
    params:
      keys: [$(keys $'LOAD "RBGFX23",8\rLIST\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.5
  - id: capture_rbgfx23_list
    type: screen.capture
    params:
      label: readybasic_gfx_phase4_dlist_list
  - id: run_rbgfx23
    type: input.sequence
    params:
      keys: [$(keys $'RUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: capture_dlist
    type: screen.capture
    params:
      label: readybasic_gfx_phase4_dlist
      note: "DLMAKE, DLPLOT, DLLINE, DLRECT, DLFRECT, DLDRAW"
  - id: finish_rbgfx23
    type: input.key
    params:
      key: SPACE
      post_delay_s: 1.0
  - id: wait_rbgfx23_done
    type: screen.wait_contains
    params:
      text: "PHASE4 DLIST DONE"
      wait_timeout_s: 60
      capture_on_success: true
      capture_label: readybasic_gfx_phase4_dlist_done
  - id: load_rbgfx24
    type: input.sequence
    params:
      keys: [$(keys $'LOAD "RBGFX24",8\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 18.0
  - id: capture_tilemap
    type: screen.capture
    params:
      label: readybasic_gfx_phase4_tilemap
      note: "CHRMAKE, CHRROW, CHRUSE, TSMAKE, TSSET, TMMAKE, TMSET, TMDRAW"
  - id: finish_rbgfx24
    type: input.key
    params:
      key: SPACE
      post_delay_s: 1.0
  - id: wait_rbgfx24_done
    type: screen.wait_contains
    params:
      text: "PHASE4 TILEMAP DONE"
      wait_timeout_s: 60
      capture_on_success: true
      capture_label: readybasic_gfx_phase4_tilemap_done
  - id: load_rbgfx25
    type: input.sequence
    params:
      keys: [$(keys $'LOAD "RBGFX25",8\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: capture_mbitmap_cells
    type: screen.capture
    params:
      label: readybasic_gfx_phase4_mbitmap_cells
      note: "MCELL and MCBG explicit multicolor bitmap cell controls"
  - id: finish_rbgfx25
    type: input.key
    params:
      key: SPACE
      post_delay_s: 1.0
  - id: wait_rbgfx25_done
    type: screen.wait_contains
    params:
      text: "PHASE4 MBITMAP CELLS DONE"
      wait_timeout_s: 60
      capture_on_success: true
      capture_label: readybasic_gfx_phase4_mbitmap_cells_done
  - id: load_rbgfx26
    type: input.sequence
    params:
      keys: [$(keys $'LOAD "RBGFX26",8\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: capture_mode_hires
    type: screen.capture
    params:
      label: readybasic_gfx_phase4_mode_matrix_hires
      note: "Immediate PLOT/LINE/RECT/FRECT/CIRCLE/PNT in HIRES"
  - id: next_mode_mbitmap
    type: input.key
    params:
      key: SPACE
      post_delay_s: 3.0
  - id: capture_mode_mbitmap
    type: screen.capture
    params:
      label: readybasic_gfx_phase4_mode_matrix_mbitmap
      note: "Immediate PLOT/LINE/RECT/FRECT/CIRCLE/PNT in MBITMAP"
  - id: next_mode_tile
    type: input.key
    params:
      key: SPACE
      post_delay_s: 3.0
  - id: capture_mode_tile
    type: screen.capture
    params:
      label: readybasic_gfx_phase4_mode_matrix_tile
      note: "Immediate PLOT/LINE/RECT/FRECT/PNT in TILE"
  - id: next_mode_mtile
    type: input.key
    params:
      key: SPACE
      post_delay_s: 3.0
  - id: capture_mode_mtile
    type: screen.capture
    params:
      label: readybasic_gfx_phase4_mode_matrix_mtile
      note: "Immediate PLOT/LINE/RECT/FRECT/PNT in MTILE"
  - id: finish_rbgfx26
    type: input.key
    params:
      key: SPACE
      post_delay_s: 1.0
  - id: wait_rbgfx26_done
    type: screen.wait_contains
    params:
      text: "MODE MATRIX DONE"
      wait_timeout_s: 60
      capture_on_success: true
      capture_label: readybasic_gfx_phase4_mode_matrix_done
  - id: load_rbgfx27
    type: input.sequence
    params:
      keys: [$(keys $'LOAD "RBGFX27",8\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 4.0
  - id: capture_target_blit_text
    type: screen.capture
    params:
      label: readybasic_gfx_phase4_target_blit_text
      note: "GFXSURF/GFXTARGET/GFXSYNC/GFXBLIT status output"
  - id: finish_rbgfx27
    type: input.key
    params:
      key: SPACE
      post_delay_s: 1.0
  - id: wait_rbgfx27_done
    type: screen.wait_contains
    params:
      text: "TARGET BLIT DONE"
      wait_timeout_s: 60
      capture_on_success: true
      capture_label: readybasic_gfx_phase4_target_blit_done
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
