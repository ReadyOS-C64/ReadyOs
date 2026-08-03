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
PLAN="${LAUNCHER_REU_STATE_PLAN:-$SCRIPT_DIR/launcher_reu_state_probe.generated.yaml}"
LAUNCHER_REU_VISIBLE="${LAUNCHER_REU_VISIBLE:-0}"
LAUNCHER_REU_KEEP_VICE="${LAUNCHER_REU_KEEP_VICE:-0}"
VICE_HEADLESS="true"
VICE_CLOSE="true"
CLI_CLOSE_ARG="--close-vice"
if [ "$LAUNCHER_REU_VISIBLE" = "1" ]; then
  VICE_HEADLESS="false"
fi
if [ "$LAUNCHER_REU_KEEP_VICE" = "1" ]; then
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
if [ "${LAUNCHER_REU_STATE_SKIP_BUILD:-0}" != "1" ]; then
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
plan_id: launcher_reu_state_probe
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
      capture_on_success: true
      capture_label: readybasic_initial_prompt
  - id: type_baseline_print
    type: input.sequence
    params:
      keys: [$(keys $'PRINT "RBBASE"\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.6
  - id: assert_baseline_print
    type: assert.screen
    params:
      contains: "RBBASE"
  - id: type_exit_readybasic
    type: input.sequence
    params:
      keys: [$(keys $'EXIT\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: wait_launcher_after_exit
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 45
      capture_on_success: true
      capture_label: launcher_after_readybasic_exit
  - id: dump_after_exit_ram
    type: dump.memory_ranges
    params:
      ranges: &launcher_reu_ranges
        - { label: shim_hot_state_c834, start: 0xC834, end: 0xC83F }
        - { label: shim_resident_c600, start: 0xC600, end: 0xC9FF }
  - id: dump_after_exit_reu
    type: dump.reu
    params:
      stage_tag: after_readybasic_exit
      mode: sampled
  - id: normalize_readybasic_selection
    type: input.sequence
    params:
      keys: [145,17]
      inter_key_delay_s: 0.08
      post_delay_s: 0.2
  - id: unload_selected_readybasic
    type: input.sequence
    params:
      keys: [136]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: wait_launcher_after_unload
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 30
      capture_on_success: true
      capture_label: launcher_after_unload
  - id: assert_last_saved_invalidated
    type: assert.memory
    params:
      start: 0xC835
      end: 0xC835
      equals_hex: "FF"
  - id: dump_after_unload_ram
    type: dump.memory_ranges
    params:
      ranges: *launcher_reu_ranges
  - id: dump_after_unload_reu
    type: dump.reu
    params:
      stage_tag: after_readybasic_unload
      mode: sampled
  - id: reload_selected_readybasic
    type: input.sequence
    params:
      keys: [134]
      inter_key_delay_s: 0.03
      post_delay_s: 2.0
  - id: wait_reload_result
    type: screen.wait_contains
    params:
      text: "PRESS ANY KEY"
      wait_timeout_s: 90
      capture_on_success: true
      capture_label: readybasic_reload_result
  - id: dump_after_reload_ram
    type: dump.memory_ranges
    params:
      ranges: *launcher_reu_ranges
  - id: acknowledge_reload_result
    type: input.sequence
    params:
      keys: [13]
      inter_key_delay_s: 0.03
      post_delay_s: 0.6
  - id: wait_launcher_after_reload
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 30
  - id: launch_reloaded_readybasic
    type: input.sequence
    params:
      keys: [145,17]
      inter_key_delay_s: 0.08
      post_delay_s: 0.3
  - id: clear_keyboard_buffer_before_relaunch
    type: memory.write
    params:
      start: 0x00C6
      bytes_hex: "00"
  # A lone RETURN immediately after a dump/reload sequence is occasionally
  # dropped by the binary-monitor key helper.  VICE's text-monitor keybuf path
  # both resumes the CPU after monitor inspection and injects it reliably.
  - id: enter_reloaded_readybasic
    type: monitor.command
    params:
      command: "keybuf \\\\x0d"
  - id: wait_readybasic_after_reload
    type: screen.wait_contains
    params:
      text: "READY."
      wait_timeout_s: 90
      capture_on_success: true
      capture_label: readybasic_after_reload_prompt
  - id: type_stability_print
    type: input.sequence
    params:
      keys: [$(keys $'PRINT "RBSTABLE"\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.6
  - id: assert_stability_print
    type: assert.screen
    params:
      contains: "RBSTABLE"
  - id: dump_final_ram
    type: dump.memory_ranges
    params:
      ranges:
        - { label: shim_hot_state_c834, start: 0xC834, end: 0xC83F }
        - { label: shim_resident_c600, start: 0xC600, end: 0xC9FF }
        - { label: readybasic_bridge_c000, start: 0xC000, end: 0xC5FF }
        - { label: readybasic_text_1200, start: 0x1200, end: 0x1400 }
  - id: dump_final_reu
    type: dump.reu
    params:
      stage_tag: final_readybasic_reloaded
      mode: sampled
  - id: regs_final
    type: monitor.command
    params:
      command: "r"
YAML

if [ "${LAUNCHER_REU_STATE_GENERATE_PLAN_ONLY:-0}" = "1" ]; then
  echo "wrote $PLAN"
  exit 0
fi

cd "$READYOS_ROOT"
dotnet build "$PROJECT"
dotnet run --project "$PROJECT" -- run-plan --plan "$PLAN" $CLI_CLOSE_ARG
