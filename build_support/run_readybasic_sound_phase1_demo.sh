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
PLAN="${READYBASIC_PLAN:-$SCRIPT_DIR/readybasic_sound_phase1_demo.generated.yaml}"
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
plan_id: readybasic_sound_phase1_demo
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
      capture_label: readybasic_sound_phase1_prompt
  - id: load_rbsnd01_for_list
    type: input.sequence
    params:
      keys: [$(keys $'LOAD "RBSND01",8\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 2.0
  - id: list_rbsnd01
    type: input.sequence
    params:
      keys: [76,73,83,84,13]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: capture_list_rbsnd01
    type: screen.capture
    params:
      label: readybasic_sound_phase1_list_rbsnd01
      note: "RBSND01 listed inside ReadyBASIC under ReadyOS"
  - id: assert_list_rbsnd01
    type: assert.screen
    params:
      contains: "SID BASICS"
  - id: run_rbsnd01
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 2.0
  - id: capture_rbsnd01
    type: screen.capture
    params:
      label: readybasic_sound_phase1_sid_basics
      note: "Human audio: triangle, saw, pulse, then noise"
  - id: wait_rbsnd01_done
    type: screen.wait_contains
    params:
      text: "RBSND01 DONE"
      wait_timeout_s: 120
      capture_on_success: true
      capture_label: readybasic_sound_phase1_sid_basics_done
  - id: run_rbsnd02
    type: input.sequence
    params:
      keys: [$(keys $'LOAD "RBSND02",8\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 2.0
  - id: capture_rbsnd02
    type: screen.capture
    params:
      label: readybasic_sound_phase1_voice_state
      note: "Human audio: pulse width and ADSR changes"
  - id: wait_rbsnd02_done
    type: screen.wait_contains
    params:
      text: "RBSND02 DONE"
      wait_timeout_s: 120
      capture_on_success: true
      capture_label: readybasic_sound_phase1_voice_state_done
  - id: run_rbsnd03
    type: input.sequence
    params:
      keys: [$(keys $'LOAD "RBSND03",8\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 2.0
  - id: capture_rbsnd03
    type: screen.capture
    params:
      label: readybasic_sound_phase1_notes
      note: "Human audio: chromatic NOTE command"
  - id: wait_rbsnd03_done
    type: screen.wait_contains
    params:
      text: "RBSND03 DONE"
      wait_timeout_s: 120
      capture_on_success: true
      capture_label: readybasic_sound_phase1_notes_done
  - id: run_rbsnd04
    type: input.sequence
    params:
      keys: [$(keys $'LOAD "RBSND04",8\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 2.0
  - id: capture_rbsnd04
    type: screen.capture
    params:
      label: readybasic_sound_phase1_filter
      note: "Human audio: filter mode and cutoff changes"
  - id: wait_rbsnd04_done
    type: screen.wait_contains
    params:
      text: "RBSND04 DONE"
      wait_timeout_s: 120
      capture_on_success: true
      capture_label: readybasic_sound_phase1_filter_done
  - id: run_rbsnd05
    type: input.sequence
    params:
      keys: [$(keys $'LOAD "RBSND05",8\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 2.0
  - id: capture_rbsnd05
    type: screen.capture
    params:
      label: readybasic_sound_phase1_voice_batch
      note: "Human audio: VOICE packed fast path"
  - id: wait_rbsnd05_done
    type: screen.wait_contains
    params:
      text: "RBSND05 DONE"
      wait_timeout_s: 120
      capture_on_success: true
      capture_label: readybasic_sound_phase1_voice_batch_done
  - id: run_rbsnd06
    type: input.sequence
    params:
      keys: [$(keys $'LOAD "RBSND06",8\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 2.0
  - id: capture_rbsnd06
    type: screen.capture
    params:
      label: readybasic_sound_phase1_three_voice
      note: "Human audio: three SID voices and chord changes"
  - id: wait_rbsnd06_done
    type: screen.wait_contains
    params:
      text: "RBSND06 DONE"
      wait_timeout_s: 120
      capture_on_success: true
      capture_label: readybasic_sound_phase1_three_voice_done
  - id: assert_no_basic_error
    type: assert.screen_not_contains
    params:
      not_contains: "?"
YAML

if [ "${READYBASIC_GENERATE_PLAN_ONLY:-0}" = "1" ]; then
  echo "wrote $PLAN"
  exit 0
fi

dotnet run --project "$PROJECT" -- run --plan "$PLAN" $CLI_CLOSE_ARG
