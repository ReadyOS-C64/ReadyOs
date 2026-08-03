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
PLAN="${QUICKNOTES_OWNED_REU_PLAN:-$SCRIPT_DIR/quicknotes_owned_reu_probe.generated.yaml}"
PLAN_AFTER="${QUICKNOTES_OWNED_REU_AFTER_PLAN:-$SCRIPT_DIR/quicknotes_owned_reu_unload_probe.generated.yaml}"
QUICKNOTES_OWNED_REU_VISIBLE="${QUICKNOTES_OWNED_REU_VISIBLE:-0}"
QUICKNOTES_OWNED_REU_KEEP_VICE="${QUICKNOTES_OWNED_REU_KEEP_VICE:-0}"
VICE_HEADLESS="true"
VICE_CLOSE="true"
CLI_CLOSE_ARG="--close-vice"
if [ "$QUICKNOTES_OWNED_REU_VISIBLE" = "1" ]; then
  VICE_HEADLESS="false"
fi
if [ "$QUICKNOTES_OWNED_REU_KEEP_VICE" = "1" ]; then
  VICE_CLOSE="false"
  CLI_CLOSE_ARG=""
fi

cd "$READYOS_ROOT"
PUBLIC_VERSION_TEXT="$(python3 build_support/update_build_version.py --current)"
PUBLIC_VERSION="${PUBLIC_VERSION_TEXT%[A-Z]}"
if [ "${QUICKNOTES_OWNED_REU_SKIP_BUILD:-0}" != "1" ]; then
  RUN_VERSION_TEXT="$(python3 build_support/update_build_version.py --next)"
  make -B \
    BUILD_SUPPORT_DIR=build_support \
    PROFILE=precog-d81 \
    READYOS_VERSION_TEXT="$RUN_VERSION_TEXT" \
    profile
fi

D81_REL="$(ls -t Releases/$PUBLIC_VERSION/precog-d81/*.d81 | head -1)"
PREBOOT_REL="$(ls -t Releases/$PUBLIC_VERSION/precog-d81/*-preboot.prg | head -1)"
D81="$D81_REL"
PREBOOT="$PREBOOT_REL"

cat >"$PLAN" <<YAML
version: 1
kind: vice_task_plan
plan_id: quicknotes_owned_reu_probe_before
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
  - id: wait_launcher_initial
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 180
      capture_on_success: true
      capture_label: qn_owned_launcher_initial
  - id: launch_quicknotes
    type: input.sequence
    params:
      keys: [17,17,17,17,17,17,17,17,17,17,17,13]
      inter_key_delay_s: 0.08
      post_delay_s: 2.0
  - id: wait_quicknotes
    type: screen.wait_contains
    params:
      text: "QUICKNOTES"
      wait_timeout_s: 90
      capture_on_success: true
      capture_label: qn_owned_quicknotes_loaded
  - id: return_quicknotes_to_launcher
    type: input.sequence
    params:
      keys: [2]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: wait_launcher_after_quicknotes
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 45
      capture_on_success: true
      capture_label: qn_owned_launcher_after_quicknotes
  - id: dump_before_viewer_ram
    type: dump.memory_ranges
    params:
      ranges: &owned_reu_ranges
        - { label: app_snapshot_private_c600, start: 0xC600, end: 0xC6FF }
        - { label: app_snapshot_private_c700, start: 0xC700, end: 0xC7FF }
  - id: open_reuviewer_before_unload
    type: input.sequence
    params:
      keys: [145,145,13]
      inter_key_delay_s: 0.08
      post_delay_s: 2.0
  - id: wait_reuviewer_before_unload
    type: screen.wait_contains
    params:
      text: "REU MEMORY MAP"
      wait_timeout_s: 90
      capture_on_success: true
      capture_label: qn_owned_reuviewer_entry_before
  - id: move_reuviewer_to_quicknotes_owned_bank
    type: input.sequence
    params:
      keys: [17,17,29,29,29]
      inter_key_delay_s: 0.08
      post_delay_s: 0.5
  - id: capture_owned_bank_before_unload
    type: screen.capture
    params:
      label: qn_owned_reuviewer_owned_bank_before_unload
      note: Cursor on QuickNotes app-owned note bank before launcher unload.
  - id: assert_reuviewer_owner_before
    type: assert.screen
    params:
      contains: "OWNER:"
  - id: assert_reuviewer_tag_before
    type: assert.screen
    params:
      contains: "TAG"
  - id: dump_before_unload_reu
    type: dump.reu
    params:
      stage_tag: quicknotes_owned_before_unload
      mode: sampled
  - id: regs_final
    type: monitor.command
    params:
      command: "r"
YAML

cat >"$PLAN_AFTER" <<YAML
version: 1
kind: vice_task_plan
plan_id: quicknotes_owned_reu_probe_after
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
  - id: wait_launcher_initial
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 180
      capture_on_success: true
      capture_label: qn_owned_after_launcher_initial
  - id: launch_quicknotes
    type: input.sequence
    params:
      keys: [17,17,17,17,17,17,17,17,17,17,17,13]
      inter_key_delay_s: 0.08
      post_delay_s: 2.0
  - id: wait_quicknotes
    type: screen.wait_contains
    params:
      text: "QUICKNOTES"
      wait_timeout_s: 90
      capture_on_success: true
      capture_label: qn_owned_after_quicknotes_loaded
  - id: return_quicknotes_to_launcher
    type: input.sequence
    params:
      keys: [2]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: wait_launcher_after_quicknotes
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 45
      capture_on_success: true
      capture_label: qn_owned_after_launcher_after_quicknotes
  - id: dump_before_unload_ram
    type: dump.memory_ranges
    params:
      ranges: &owned_reu_ranges
        - { label: app_snapshot_private_c600, start: 0xC600, end: 0xC6FF }
        - { label: app_snapshot_private_c700, start: 0xC700, end: 0xC7FF }
  - id: unload_quicknotes
    type: input.sequence
    params:
      keys: [136]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: wait_launcher_after_unload
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 45
      capture_on_success: true
      capture_label: qn_owned_launcher_after_unload
  - id: dump_after_unload_ram
    type: dump.memory_ranges
    params:
      ranges: *owned_reu_ranges
  - id: open_reuviewer_after_unload
    type: input.sequence
    params:
      keys: [145,145,13]
      inter_key_delay_s: 0.08
      post_delay_s: 2.0
  - id: wait_reuviewer_after_unload
    type: screen.wait_contains
    params:
      text: "REU MEMORY MAP"
      wait_timeout_s: 90
      capture_on_success: true
      capture_label: qn_owned_reuviewer_entry_after
  - id: move_reuviewer_to_freed_bank
    type: input.sequence
    params:
      keys: [17,17,29,29,29]
      inter_key_delay_s: 0.08
      post_delay_s: 0.5
  - id: capture_owned_bank_after_unload
    type: screen.capture
    params:
      label: qn_owned_reuviewer_owned_bank_after_unload
      note: Cursor on the former QuickNotes app-owned bank after launcher unload.
  - id: assert_reuviewer_free_after
    type: assert.screen
    params:
      contains: "TYPE: FREE"
  - id: dump_after_unload_reu
    type: dump.reu
    params:
      stage_tag: quicknotes_owned_after_unload
      mode: sampled
  - id: regs_final
    type: monitor.command
    params:
      command: "r"
YAML

if [ "${QUICKNOTES_OWNED_REU_GENERATE_PLAN_ONLY:-0}" = "1" ]; then
  echo "wrote $PLAN"
  echo "wrote $PLAN_AFTER"
  exit 0
fi

dotnet build "$PROJECT"
dotnet run --project "$PROJECT" -- run-plan --plan "$PLAN" $CLI_CLOSE_ARG
dotnet run --project "$PROJECT" -- run-plan --plan "$PLAN_AFTER" $CLI_CLOSE_ARG
