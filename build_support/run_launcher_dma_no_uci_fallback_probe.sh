#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
READYOS_ROOT="${READYOS_ROOT:-$(cd "$SCRIPT_DIR/.." && pwd)}"
if [ -n "${VICE_TASKS_ROOT:-}" ]; then
  VICE_TOOL_ROOT="$(cd "$VICE_TASKS_ROOT" && pwd)"
else
  HARNESS_REPO="${VICE_TASKS_REPO:-$READYOS_ROOT/../agenticdevharness}"
  VICE_TOOL_ROOT="$(cd "$HARNESS_REPO/tools/vice_tasks_dotnet" && pwd)"
fi

PROJECT="${VICE_TASKS_DOTNET_PROJECT:-$VICE_TOOL_ROOT/src/ViceTasks.Binary/ViceTasks.Binary.csproj}"
PROFILE="${READYOS_PROFILE:-precog-d81}"
D81="${1:-$(python3 "$READYOS_ROOT/build_support/readyos_profiles.py" latest-disk --profile "$PROFILE" --drive 8)}"
PREBOOT="${2:-${D81%.d81}-preboot.prg}"
PLAN="${LAUNCHER_DMA_FALLBACK_PLAN:-$SCRIPT_DIR/launcher_dma_no_uci_fallback.generated.yaml}"
VICE_HEADLESS="${VICE_HEADLESS:-true}"

if [ ! -f "$D81" ]; then
  echo "D81 not found: $D81" >&2
  exit 1
fi
if [ ! -f "$PREBOOT" ]; then
  echo "preboot PRG not found: $PREBOOT" >&2
  exit 1
fi

cat >"$PLAN" <<YAML
version: 1
kind: vice_task_plan
plan_id: launcher_dma_no_uci_disk_fallback
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
    autostart_prg: "$PREBOOT"
    drive8_type: 1581
    true_drive: false
    close_vice: true
    headless: $VICE_HEADLESS
    speed_percent: 100
steps:
  - id: launch_preboot
    type: vice.launch
    params:
      kill_stale: true
  - id: wait_launcher
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 220
      capture_on_success: true
      capture_label: launcher_initial
  - id: assert_dma_no
    type: assert.screen
    params:
      contains: "DMA:NO"
  - id: assert_kernal_cursor_hidden
    type: assert.memory
    params:
      start: 0x00CC
      end: 0x00CC
      equals_hex: "01"
  - id: select_editor
    type: input.sequence
    params:
      keys: [19, 17, 17, 13]
      inter_key_delay_s: 0.05
      post_delay_s: 0.5
  - id: wait_editor
    type: screen.wait_contains
    params:
      text: "EDITOR:"
      wait_timeout_s: 120
      capture_on_success: true
      capture_label: editor_loaded_disk
  - id: return_to_launcher_from_editor
    type: input.sequence
    params:
      keys: [2]
      inter_key_delay_s: 0.05
      post_delay_s: 0.8
  - id: wait_launcher_after_editor
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 60
      capture_on_success: true
      capture_label: launcher_after_editor
  - id: select_readyshell
    type: input.sequence
    params:
      keys: [17, 13]
      inter_key_delay_s: 0.05
      post_delay_s: 0.5
  - id: wait_readyshell
    type: screen.wait_contains
    params:
      text: "READYOS READYSHELL"
      wait_timeout_s: 180
      capture_on_success: true
      capture_label: readyshell_loaded_disk
  - id: wait_readyshell_prompt
    type: screen.wait_contains
    params:
      text: "RUN: CAT"
      wait_timeout_s: 60
      capture_on_success: true
      capture_label: readyshell_prompt_disk
YAML

dotnet build "$PROJECT"
dotnet run --project "$PROJECT" -- run-plan --plan "$PLAN" --close-vice
