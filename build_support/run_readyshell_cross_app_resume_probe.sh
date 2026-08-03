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
PLAN="${READYSHELL_PLAN:-$SCRIPT_DIR/readyshell_cross_app_resume_probe.generated.yaml}"
REPEAT_COUNT="${READYSHELL_CROSS_APP_REPEAT:-1}"
READYSHELL_VISIBLE="${READYSHELL_VISIBLE:-0}"
VICE_HEADLESS="true"
if [ "$READYSHELL_VISIBLE" = "1" ]; then
  VICE_HEADLESS="false"
fi

keys() {
  python3 - "$1" <<'PY'
import sys
s = sys.argv[1]
out = []
for ch in s:
    code = ord(ch)
    if 0x61 <= code <= 0x7a:
        code -= 0x20
    out.append(str(code))
print(",".join(out))
PY
}

emit_type_step() {
  local id="$1"
  local text="$2"
  local post="${3:-0.8}"
  cat >>"$PLAN" <<YAML
  - id: $id
    type: input.sequence
    params:
      keys: [$(keys "$text")]
      inter_key_delay_s: 0.035
      post_delay_s: $post
YAML
}

emit_key_step() {
  local id="$1"
  local key_list="$2"
  local delay="${3:-0.08}"
  local post="${4:-1.0}"
  cat >>"$PLAN" <<YAML
  - id: $id
    type: input.sequence
    params:
      keys: [$key_list]
      inter_key_delay_s: $delay
      post_delay_s: $post
YAML
}

emit_wait_step() {
  local id="$1"
  local text="$2"
  local timeout="${3:-60}"
  cat >>"$PLAN" <<YAML
  - id: $id
    type: screen.wait_contains
    params:
      text: "$text"
      wait_timeout_s: $timeout
YAML
}

emit_assert_step() {
  local id="$1"
  local text="$2"
  cat >>"$PLAN" <<YAML
  - id: $id
    type: assert.screen
    params:
      contains: "$text"
YAML
}

emit_capture_step() {
  local id="$1"
  local label="$2"
  cat >>"$PLAN" <<YAML
  - id: $id
    type: screen.capture
    params:
      label: $label
YAML
}

emit_readyshell_ver() {
  local label="$1"
  emit_type_step "readyshell_ver_$label" $'VER\r' "1.0"
  emit_assert_step "assert_readyshell_ver_$label" "version 0.2"
}

emit_readyshell_lst_overlay() {
  local label="$1"
  emit_type_step "readyshell_lst_$label" $'LST "RSHELP"\r' "2.0"
  emit_wait_step "wait_readyshell_lst_result_$label" "BLOCKS" "60"
  emit_type_step "readyshell_ver_after_lst_$label" $'VER\r' "1.0"
  emit_assert_step "assert_readyshell_ver_after_lst_$label" "version 0.2"
}

emit_readyshell_cat_overlay() {
  local label="$1"
  emit_type_step "readyshell_cat_$label" $'CAT "RSHELP" ! TOP 1\r' "2.0"
  emit_capture_step "capture_readyshell_cat_$label" "readyshell_cat_$label"
  emit_wait_step "wait_readyshell_cat_result_$label" "readyshell quick ref" "60"
  emit_type_step "readyshell_ver_after_cat_$label" $'VER\r' "1.0"
  emit_assert_step "assert_readyshell_ver_after_cat_$label" "version 0.2"
}

cd "$READYOS_ROOT"
PUBLIC_VERSION_TEXT="$(python3 build_support/update_build_version.py --current)"
PUBLIC_VERSION="${PUBLIC_VERSION_TEXT%[A-Z]}"
if [ "${READYSHELL_SKIP_BUILD:-0}" != "1" ]; then
  RUN_VERSION_TEXT="$(python3 build_support/update_build_version.py --next)"
  make -B \
    BUILD_SUPPORT_DIR=build_support \
    PROFILE=precog-d81 \
    READYOS_VERSION_TEXT="$RUN_VERSION_TEXT" \
    READYOS_CONFIG_RUN_FIRST=editor \
    profile
fi

D81_REL="$(ls -t Releases/$PUBLIC_VERSION/precog-d81/*.d81 | head -1)"
PREBOOT_REL="$(ls -t Releases/$PUBLIC_VERSION/precog-d81/*-preboot.prg | head -1)"
D81="$D81_REL"
PREBOOT="$PREBOOT_REL"

cat >"$PLAN" <<YAML
version: 1
kind: vice_task_plan
plan_id: readyshell_cross_app_resume_probe
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
    close_vice: true
    headless: $VICE_HEADLESS
    speed_percent: 100
steps:
  - id: launch_preboot
    type: vice.launch
    params:
      kill_stale: true
  - id: wait_editor_initial
    type: screen.wait_contains
    params:
      text: "EDITOR:"
      wait_timeout_s: 180
      capture_label: editor_initial
  - id: clear_keyboard_buffer_before_editor_entry
    type: memory.write
    params:
      start: 198
      bytes_hex: "00"
YAML

for i in $(seq 1 "$REPEAT_COUNT"); do
cat >>"$PLAN" <<YAML
  - id: editor_touch_$i
    type: input.sequence
    params:
      keys: [$(keys $'readyshell probe editor leg\r')]
      inter_key_delay_s: 0.035
      post_delay_s: 0.8
  - id: ctrl_b_editor_to_launcher_$i
    type: input.sequence
    params:
      keys: [2]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: wait_launcher_after_editor_$i
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 30
  - id: move_editor_to_readybasic_$i
    type: input.sequence
    params:
      keys: [17,17,17,17,13]
      inter_key_delay_s: 0.08
      post_delay_s: 2.0
  - id: wait_readybasic_from_editor_$i
    type: screen.wait_contains
    params:
      text: "ready."
      wait_timeout_s: 60
      capture_label: readybasic_from_editor_$i
  - id: readybasic_touch_$i
    type: input.sequence
    params:
      keys: [$(keys $'PRINT "RBOK"\r')]
      inter_key_delay_s: 0.035
      post_delay_s: 0.8
  - id: assert_readybasic_touch_$i
    type: assert.screen
    params:
      contains: "RBOK"
  - id: exit_readybasic_$i
    type: input.sequence
    params:
      keys: [$(keys $'EXIT\r')]
      inter_key_delay_s: 0.035
      post_delay_s: 1.0
  - id: wait_launcher_after_readybasic_$i
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 30
  - id: move_readybasic_to_editor_$i
    type: input.sequence
    params:
      keys: [145,145,145,145]
      inter_key_delay_s: 0.08
      post_delay_s: 0.5
  - id: clear_keyboard_buffer_before_editor_relaunch_$i
    type: memory.write
    params:
      start: 0x00C6
      bytes_hex: "00"
  # The binary-monitor key helper can drop a lone RETURN immediately after
  # ReadyBASIC's EXIT return. VICE's text-monitor keybuf path is deterministic.
  - id: launch_editor_after_readybasic_$i
    type: monitor.command
    params:
      command: "keybuf \\\\x0d"
  - id: capture_after_move_readybasic_to_editor_$i
    type: screen.capture
    params:
      label: after_move_readybasic_to_editor_$i
  - id: wait_editor_after_readybasic_$i
    type: screen.wait_contains
    params:
      text: "EDITOR:"
      wait_timeout_s: 60
      capture_label: editor_after_readybasic_$i
  - id: ctrl_b_editor_to_launcher_before_readyshell_$i
    type: input.sequence
    params:
      keys: [2]
      inter_key_delay_s: 0.03
      post_delay_s: 1.0
  - id: wait_launcher_after_editor_before_readyshell_$i
    type: screen.wait_contains
    params:
      text: "READY OS"
      wait_timeout_s: 30
  - id: move_editor_to_readyshell_$i
    type: input.sequence
    params:
      keys: [17,13]
      inter_key_delay_s: 0.08
      post_delay_s: 2.0
  - id: wait_readyshell_loaded_from_editor_$i
    type: screen.wait_contains
    params:
      text: "run: cat"
      wait_timeout_s: 90
      capture_label: readyshell_from_editor_$i
YAML
emit_readyshell_ver "from_editor_$i"
emit_readyshell_lst_overlay "from_editor_$i"
emit_readyshell_cat_overlay "from_editor_$i"
done

cat >>"$PLAN" <<'YAML'
  - id: dump_final_state
    type: dump.memory_ranges
    params:
      ranges:
        - { label: launcher_shim_c600, start: 0xC600, end: 0xCA00 }
        - { label: app_work_1000, start: 0x1000, end: 0x1800 }
        - { label: upper_app_a000, start: 0xA000, end: 0xC600 }
YAML

if [ "${READYSHELL_GENERATE_PLAN_ONLY:-0}" = "1" ]; then
  echo "wrote $PLAN"
  exit 0
fi

cd "$READYOS_ROOT"
dotnet build "$PROJECT"
RESULT_LOG="$(mktemp)"
set +e
dotnet run --project "$PROJECT" -- run-plan --plan "$PLAN" --close-vice | tee "$RESULT_LOG"
RUN_RC=${PIPESTATUS[0]}
set -e
if [ "$RUN_RC" -eq 2 ]; then
  python3 - "$RESULT_LOG" <<'PY'
import json
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
start = text.rfind("{")
while start != -1:
    try:
        payload = json.loads(text[start:])
        break
    except json.JSONDecodeError:
        start = text.rfind("{", 0, start)
else:
    sys.exit(2)

if (
    payload.get("Status") == "partial"
    and payload.get("FailedStep") is None
    and not payload.get("DegradedSteps")
):
    print("treating final-dump-only partial as pass; all ReadyShell cross-app probe steps passed")
    sys.exit(0)
sys.exit(2)
PY
  RUN_RC=$?
fi
rm -f "$RESULT_LOG"
exit "$RUN_RC"
