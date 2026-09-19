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
PLAN="${READYBASIC_PLAN:-$SCRIPT_DIR/readybasic_module_overlay_probe.generated.yaml}"
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

if [ "${READYBASIC_GENERATE_PLAN_ONLY:-0}" != "1" ] && command -v c1541 >/dev/null 2>&1; then
  LISTING="$(mktemp)"
  c1541 "$D81" -list >"$LISTING"
  grep -Eiq '"rbm\.sample1"[[:space:]]+seq' "$LISTING"
  grep -Eiq '"rbm\.sample2"[[:space:]]+seq' "$LISTING"
  grep -Eiq '"rbm\.sample3"[[:space:]]+seq' "$LISTING"
  ! grep -Eiq '"rbm1"|\"rbm2\"' "$LISTING"
  rm -f "$LISTING"
fi

cat >"$PLAN" <<YAML
version: 1
kind: vice_task_plan
plan_id: readybasic_module_overlay_probe
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
      capture_label: readybasic_module_overlay_prompt
  - id: load_disk_demo_commands
    type: input.sequence
    params:
      keys: [$(keys $'PRINT LDMOD("RBM.SAMPLE1")\rPRINT LDMOD("RBM.SAMPLE2")\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 2.0
  - id: builtin_overlay_bank_probe
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rMEMAVL()\rPRINT "SLOTS";SLOT0();"/";SLOT1();"/";SLOT2()\rPRINT "SPAN";SPAN()\rPRINT "OVL";OVL1();"/";OVL2()\rPRINT "CPY";CPYRST();"/";COPY()\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.5
  - id: capture_builtin_overlay_bank_probe
    type: screen.capture
    params:
      label: after_builtin_overlay_bank_probe
  - id: assert_builtin_slots
    type: assert.screen
    params:
      contains: "SLOTS 30 / 31 / 32"
  - id: assert_builtin_span
    type: assert.screen
    params:
      contains: "SPAN 40"
  - id: assert_builtin_overlays
    type: assert.screen
    params:
      contains: "OVL 51 / 52"
  - id: rbm_sample1_2_probe
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rPRINT "LD1";LDMOD("RBM.SAMPLE1")\rPRINT "DM1";DM1()\rPRINT "LD2";LDMOD("RBM.SAMPLE2")\rPRINT "DM2";DM2S()\rPRINT "DOV";DOV1();"/";DOV2()\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.5
  - id: assert_ld1
    type: assert.screen
    params:
      contains: "LD1 12"
  - id: assert_dm1
    type: assert.screen
    params:
      contains: "DM1 61"
  - id: assert_ld2
    type: assert.screen
    params:
      contains: "LD2 7"
  - id: assert_dm2
    type: assert.screen
    params:
      contains: "DM2 74"
  - id: assert_dov
    type: assert.screen
    params:
      contains: "DOV 72 / 73"
  - id: rbm_sample3_load
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rPRINT "LD3";LDMOD("RBM.SAMPLE3")\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.5
  - id: capture_rbm_sample3_load
    type: screen.capture
    params:
      label: after_rbm_sample3_load
  - id: assert_ld3
    type: assert.screen
    params:
      contains: "LD3 32"
  - id: rbm_sample3_probe
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM RBM3 FIRST CALLS: EACH OVERLAY STATE STARTS AT ZERO AFTER LOAD\rPRINT "M3A";S6AA();"/";S6EB()\rPRINT "M3B";S7AA();"/";S7EB()\rPRINT "M3C";S8AA();"/";S8EB()\rMEMAVL()\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 2.0
  - id: capture_rbm_sample3
    type: screen.capture
    params:
      label: after_rbm_sample3
  - id: assert_m3a
    type: assert.screen
    params:
      contains: "M3A 4 / 13"
  - id: assert_m3b
    type: assert.screen
    params:
      contains: "M3B 54 / 63"
  - id: assert_m3c
    type: assert.screen
    params:
      contains: "M3C 104 / 113"
  - id: assert_free_same
    type: assert.screen
    params:
      contains: "30013"
  - id: rbm_sample3_copy_same_overlay_probe
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM SAME OVERLAY: S6AA LOADS, S6AB REUSES RESIDENT IMAGE\rPRINT "R1";CPYRST()\rA=S6AA()\rB=S6AB()\rPRINT "S1";A;"/";B\rPRINT "C1";COPY()\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.5
  - id: capture_rbm_sample3_copy_same_overlay
    type: screen.capture
    params:
      label: after_rbm_sample3_copy_same_overlay
  - id: assert_copy_same_overlay_no_reload
    type: assert.screen
    params:
      contains: "C1 2"
  - id: assert_state_same_overlay_kept
    type: assert.screen
    params:
      contains: "S1 4 / 6"
  - id: rbm_sample3_copy_different_overlay_probe
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM DIFFERENT OVERLAY: S6BA REPLACES S6AA SLOT IMAGE\rPRINT "R2";CPYRST()\rA=S6AA()\rB=S6BA()\rC=S6AA()\rPRINT "S2";A;"/";B;"/";C\rPRINT "C2";COPY()\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.5
  - id: capture_rbm_sample3_copy_different_overlay
    type: screen.capture
    params:
      label: after_rbm_sample3_copy_different_overlay
  - id: assert_copy_different_overlay_reload
    type: assert.screen
    params:
      contains: "C2 4"
  - id: assert_state_different_overlay_reloaded
    type: assert.screen
    params:
      contains: "S2 4 / 6 / 4"
  - id: rbm_sample3_copy_different_submodule_probe
    type: input.sequence
    params:
      keys: [$(keys $'PRINT CHR$(147)\rREM DIFFERENT SUBMODULE: S7AA REPLACES S6AA SLOT IMAGE\rPRINT "R3";CPYRST()\rA=S6AA()\rB=S7AA()\rC=S6AA()\rPRINT "S3";A;"/";B;"/";C\rPRINT "C3";COPY()\r')]
      inter_key_delay_s: 0.03
      post_delay_s: 1.5
  - id: capture_rbm_sample3_copy_different_submodule
    type: screen.capture
    params:
      label: after_rbm_sample3_copy_different_submodule
  - id: assert_copy_different_submodule_reload
    type: assert.screen
    params:
      contains: "C3 4"
  - id: assert_state_different_submodule_reloaded
    type: assert.screen
    params:
      contains: "S3 4 / 54 / 4"
YAML

if [ "${READYBASIC_GENERATE_PLAN_ONLY:-0}" = "1" ]; then
  echo "wrote $PLAN"
  exit 0
fi

cd "$READYOS_ROOT"
dotnet build "$PROJECT"
dotnet run --project "$PROJECT" -- run-plan --plan "$PLAN" $CLI_CLOSE_ARG
