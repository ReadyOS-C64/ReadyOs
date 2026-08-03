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
PLAN="${READYBASIC_SCRREU_PLAN:-$SCRIPT_DIR/readybasic_screen_reu_temp_probe.generated.yaml}"

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
mkdir -p obj

python3 - <<'PY'
from pathlib import Path
Path("obj/rbscrreu.bas").write_text(
"""10 print chr$(147);"RBSCRREU TEMP"
20 print "T1 SCALAR START"
30 print "M1"
40 scrcap(s%)
50 print "C1"
60 scrput(s%)
70 print "T1 DONE";s%:zpause(1)
80 print "T2 ARRAY CAP START"
90 dim h%(4)
100 print chr$(147);"A1"
110 poke1024,65:poke55296,2
120 scrcap(h%(1))
130 print "CAP1";h%(1)
140 print chr$(147);"A2"
150 poke1025,66:poke55297,3
160 scrcap(h%(2))
170 print "CAP2";h%(2)
180 print chr$(147);"A3"
190 poke1026,67:poke55298,4
200 scrcap(h%(3))
210 print "CAP3";h%(3)
220 print chr$(147);"A4"
230 poke1027,68:poke55299,5
240 scrcap(h%(4))
250 print "CAP4";h%(4):zpause(1)
260 print chr$(147);"T3 DIRECT ARRAY PUT"
270 scrput(h%(1))
280 print chr$(19);"PUT1";h%(1)
290 zpause(4)
300 scrput(h%(2))
310 print chr$(19);"PUT2";h%(2)
320 zpause(4)
330 scrput(h%(3))
340 print chr$(19);"PUT3";h%(3)
350 zpause(4)
360 scrput(h%(4))
370 print chr$(19);"PUT4";h%(4)
380 zpause(1)
390 print chr$(147);"T4 LOOP ARRAY PUT"
400 for s=1 to 4
410 print "B4";s;h%(s)
420 scrput(h%(s))
430 print chr$(19);"LPUT";s;h%(s)
440 zpause(4)
450 next s
455 print "LPUT DONE":zpause(1)
460 print chr$(147);"RBSCRREU DONE"
470 zpause(1)
480 end
""",
encoding="ascii",
)
PY

petcat -w2 -l 2ac1 -o obj/rbscrreu.prg -- obj/rbscrreu.bas

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

c1541 "$D81" -delete "rbscrreu" >/dev/null 2>&1 || true
c1541 "$D81" -write obj/rbscrreu.prg "rbscrreu" >/dev/null

cat >"$PLAN" <<YAML
version: 1
kind: vice_task_plan
plan_id: readybasic_screen_reu_temp_probe
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
  - id: launch_preboot
    type: vice.launch
    params:
      kill_stale: true
  - id: wait_readybasic_prompt
    type: screen.wait_contains
    params:
      text: "readybasic"
      wait_timeout_s: 180
      capture_label: scrreu_prompt
  - id: dump_initial_ranges
    type: dump.memory_ranges
    params:
      ranges: &scrreu_ranges
        - { label: screen_0400, start: 0x0400, end: 0x07E7 }
        - { label: color_d800, start: 0xD800, end: 0xDBE7 }
        - { label: descriptors_1000, start: 0x1000, end: 0x11FF }
        - { label: resident_1200, start: 0x1200, end: 0x1BFF }
        - { label: command_overlay_a800, start: 0xA800, end: 0xAFFF }
        - { label: shim_resident_c600, start: 0xC600, end: 0xC9FF }
  - id: load_temp_program
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rLOAD "RBSCRREU",8\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: wait_load_ready
    type: screen.wait_contains
    params:
      text: "READY."
      wait_timeout_s: 30
      capture_label: scrreu_loaded
  - id: dump_after_load
    type: dump.memory_ranges
    params:
      ranges: *scrreu_ranges
  - id: run_temp_program
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rRUN\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 30.0
  - id: capture_temp_program_progress
    type: screen.capture
    params:
      label: scrreu_progress
      pitch: screen-backed REU program progress after 30 seconds
  - id: assert_done
    type: assert.memory
    params:
      start: 1024
      end: 1036
      equals_hex: "52 42 53 43 52 52 45 55 20 44 4F 4E 45"
  - id: dump_after_done
    type: dump.memory_ranges
    params:
      ranges: *scrreu_ranges
  - id: regs_final
    type: monitor.command
    params:
      command: "r"
YAML

if [ "${READYBASIC_GENERATE_PLAN_ONLY:-0}" = "1" ]; then
  echo "wrote $PLAN"
  exit 0
fi

dotnet run --project "$PROJECT" -- run \
  --plan "$PLAN" \
  --vice-bin "${VICE_BIN:-x64sc}" \
  $CLI_CLOSE_ARG
