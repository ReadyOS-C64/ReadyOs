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
PLAN="${READYBASIC_PLAN:-$SCRIPT_DIR/readybasic_resume_min_probe.generated.yaml}"
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
D81="${READYBASIC_D81:-__READYBASIC_D81__}"
PREBOOT="${READYBASIC_PREBOOT:-__READYBASIC_PREBOOT__}"

cat >"$PLAN" <<YAML
version: 1
kind: vice_task_plan
plan_id: readybasic_resume_min_probe
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
      capture_label: readybasic_resume_min_prompt
  - id: initial_builtin_probe
    type: input.sequence
    params:
      keys: [$(keys $'A%=ZADD16(5,10)\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: capture_initial_add16_probe
    type: screen.capture
    params:
      label: initial_add16_probe
  - id: initial_ping_probe
    type: input.sequence
    params:
      keys: [$(keys $'ZECHO1(P%)\rPRINT "PRE";A%;":";P%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: capture_initial_builtin_probe
    type: screen.capture
    params:
      label: initial_builtin_probe
  - id: assert_initial_builtin_probe
    type: assert.screen
    params:
      contains: "PRE 15 : 1"
  - id: initial_state_seed
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rV%=321:VS$="OK"\rPRINT "STATE";V%;":";VS$\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: assert_initial_state_seed
    type: assert.screen
    params:
      contains: "STATE 321 :OK"
  - id: dump_before_first_exit
    type: dump.memory_ranges
    params:
      ranges:
        - { label: page3_vectors, start: 0x0304, end: 0x030B }
        - { label: bridge_c000, start: 0xC000, end: 0xC5FF }
        - { label: regseed_live_5000, start: 0x5000, end: 0x503F }
  - id: exit_to_launcher_1
    type: input.sequence
    params:
      keys: [$(keys $'EXIT\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: wait_launcher_1
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 30
  - id: resume_1
    type: input.sequence
    params:
      keys: [13]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: wait_readybasic_after_resume_1
    type: screen.wait_contains
    params:
      text: "READY."
      wait_timeout_s: 30
  - id: builtin_after_resume_1
    type: input.sequence
    params:
      keys: [$(keys $'\rPRINT "STATE";V%;":";VS$\rA%=ZADD16(5,10)\rZECHO1(P%)\rPRINT "R1";A%;":";P%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: capture_after_resume_1
    type: screen.capture
    params:
      label: after_resume_1
  - id: assert_state_after_resume_1
    type: assert.screen
    params:
      contains: "STATE 321 :OK"
  - id: assert_builtin_after_resume_1
    type: assert.screen
    params:
      contains: "R1 15 : 1"
  - id: exit_to_launcher_2
    type: input.sequence
    params:
      keys: [$(keys $'EXIT\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: wait_launcher_2
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 30
  - id: resume_2
    type: input.sequence
    params:
      keys: [13]
      inter_key_delay_s: 0.03
      post_delay_s: 3.0
  - id: wait_readybasic_after_resume_2
    type: screen.wait_contains
    params:
      text: "READY."
      wait_timeout_s: 30
  - id: builtin_after_resume_2
    type: input.sequence
    params:
      keys: [$(keys $'A%=ZADD16(7,8)\rZECHO1(P%)\rPRINT "R2";A%;":";P%\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 0.8
  - id: capture_after_resume_2
    type: screen.capture
    params:
      label: after_resume_2
  - id: assert_builtin_after_resume_2
    type: assert.screen
    params:
      contains: "R2 15 : 1"
YAML

if [ "${READYBASIC_GENERATE_PLAN_ONLY:-0}" = "1" ]; then
  echo "$PLAN"
  exit 0
fi

if [ "${READYBASIC_SKIP_BUILD:-0}" != "1" ]; then
  RUN_VERSION_TEXT="$(python3 build_support/update_build_version.py --next)"
  make -B \
    BUILD_SUPPORT_DIR=build_support \
    PROFILE=precog-d81 \
    READYOS_VERSION_TEXT="$RUN_VERSION_TEXT" \
    READYOS_CONFIG_RUN_FIRST=readybasic \
    profile
fi

if [ "$D81" = "__READYBASIC_D81__" ]; then
  D81="$(ls -t Releases/$PUBLIC_VERSION/precog-d81/*.d81 | head -1)"
fi
if [ "$PREBOOT" = "__READYBASIC_PREBOOT__" ]; then
  PREBOOT="$(ls -t Releases/$PUBLIC_VERSION/precog-d81/*-preboot.prg | head -1)"
fi
python3 - "$PLAN" "$D81" "$PREBOOT" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()
text = text.replace("__READYBASIC_D81__", sys.argv[2])
text = text.replace("__READYBASIC_PREBOOT__", sys.argv[3])
path.write_text(text)
PY

dotnet build "$PROJECT"
dotnet run --project "$PROJECT" -- run --plan "$PLAN" $CLI_CLOSE_ARG
