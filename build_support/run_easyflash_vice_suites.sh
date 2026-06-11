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
PROJECT="$VICE_TOOL_ROOT/src/ViceTasks.Binary/ViceTasks.Binary.csproj"
OUT_DIR="${EASYFLASH_VICE_PLAN_DIR:-$READYOS_ROOT/agentworking/easyflash_full_vice_plans}"
SCOPE="${1:-all}"

case "$SCOPE" in
  all|readybasic|readyshell) ;;
  --readybasic-only) SCOPE="readybasic" ;;
  --readyshell-only) SCOPE="readyshell" ;;
  *)
    echo "usage: $0 [all|readybasic|readyshell|--readybasic-only|--readyshell-only]" >&2
    exit 2
    ;;
esac

cd "$READYOS_ROOT"
PUBLIC_VERSION_TEXT="$(python3 build_support/update_build_version.py --current)"
PUBLIC_VERSION="${PUBLIC_VERSION_TEXT%[A-Z]}"
mkdir -p "$OUT_DIR"

RUN_VERSION_TEXT="$(python3 build_support/update_build_version.py --next)"
make -B \
  BUILD_SUPPORT_DIR=build_support \
  PROFILE=precog-d81 \
  READYOS_VERSION_TEXT="$RUN_VERSION_TEXT" \
  READYOS_CONFIG_RUN_FIRST=readybasic \
  profile
make READYOS_VERSION_TEXT="$RUN_VERSION_TEXT" easyflash

CRT_REL="$(ls -t Releases/$PUBLIC_VERSION/precog-easyflash/readyos_easyflash.crt | head -1)"
D64_REL="$(ls -t Releases/$PUBLIC_VERSION/precog-easyflash/readyos_data.d64 | head -1)"
CRT="$(cd "$(dirname "$CRT_REL")" && pwd)/$(basename "$CRT_REL")"
TEST_D64="$OUT_DIR/readyos_data.easyflash-vice.d64"
cp -f "$D64_REL" "$TEST_D64"
D64="$(cd "$(dirname "$TEST_D64")" && pwd)/$(basename "$TEST_D64")"

declare -a RUN_PLANS=()

emit_readybasic_plan() {
  local name="$1"
  local script="$2"
  local plan_var="$3"
  local regular_plan="$OUT_DIR/$name.regular.yaml"
  local easyflash_plan="$OUT_DIR/$name.easyflash.yaml"

  env \
    READYBASIC_SKIP_BUILD=1 \
    READYBASIC_GENERATE_PLAN_ONLY=1 \
    READYBASIC_HOTKEY_EXPECT_F4="${READYBASIC_HOTKEY_EXPECT_F4:-}" \
    READYBASIC_HOTKEY_EXPECT_F2="${READYBASIC_HOTKEY_EXPECT_F2:-}" \
    READYBASIC_HOTKEY_F4_RETURN_MODE="${READYBASIC_HOTKEY_F4_RETURN_MODE:-}" \
    READYBASIC_HOTKEY_SCENARIO="${READYBASIC_HOTKEY_SCENARIO:-}" \
    READYBASIC_CHAIN_READYBASIC_BANK="${READYBASIC_CHAIN_READYBASIC_BANK:-}" \
    READYBASIC_CHAIN_REUVIEWER_BANK="${READYBASIC_CHAIN_REUVIEWER_BANK:-}" \
    READYBASIC_CHAIN_RESOURCE_BANKS="${READYBASIC_CHAIN_RESOURCE_BANKS:-}" \
    READYBASIC_CHAIN_CONSTRAIN_BITMAP="${READYBASIC_CHAIN_CONSTRAIN_BITMAP:-}" \
    "$plan_var=$regular_plan" \
    "$script"

  python3 build_support/easyflash_plan_from_regular.py \
    --input "$regular_plan" \
    --output "$easyflash_plan" \
    --crt "$CRT" \
    --disk8 "$D64" \
    --start-app readybasic

  RUN_PLANS+=("$easyflash_plan")
}

emit_readyshell_plan() {
  local name="$1"
  local script="$2"
  local regular_plan="$OUT_DIR/$name.regular.yaml"
  local easyflash_plan="$OUT_DIR/$name.easyflash.yaml"

  env \
    READYSHELL_SKIP_BUILD=1 \
    READYSHELL_GENERATE_PLAN_ONLY=1 \
    "READYSHELL_PLAN=$regular_plan" \
    "$script"

  python3 build_support/easyflash_plan_from_regular.py \
    --input "$regular_plan" \
    --output "$easyflash_plan" \
    --crt "$CRT" \
    --disk8 "$D64" \
    --start-app editor

  RUN_PLANS+=("$easyflash_plan")
}

if [ "$SCOPE" = "all" ] || [ "$SCOPE" = "readybasic" ]; then
  emit_readybasic_plan readybasic_demo_suite "$SCRIPT_DIR/run_readybasic_demo_suite.sh" READYBASIC_DEMO_PLAN
  emit_readybasic_plan readybasic_repeat_label_probe "$SCRIPT_DIR/vice_readybasic_repeat_label_probe.sh" READYBASIC_REPEAT_PLAN
  emit_readybasic_plan readybasic_lifecycle_probe "$SCRIPT_DIR/run_readybasic_lifecycle_probe.sh" READYBASIC_PLAN
  emit_readybasic_plan readybasic_module_overlay_probe "$SCRIPT_DIR/run_readybasic_module_overlay_probe.sh" READYBASIC_PLAN
  emit_readybasic_plan readybasic_plugin_command_probe "$SCRIPT_DIR/run_readybasic_plugin_command_probe.sh" READYBASIC_PLAN
  emit_readybasic_plan readybasic_program_probe "$SCRIPT_DIR/run_readybasic_program_probe.sh" READYBASIC_PLAN
  emit_readybasic_plan readybasic_rbtest1_probe "$SCRIPT_DIR/run_readybasic_rbtest1_probe.sh" READYBASIC_PLAN
  emit_readybasic_plan readybasic_resume_min_probe "$SCRIPT_DIR/run_readybasic_resume_min_probe.sh" READYBASIC_PLAN
  emit_readybasic_plan readybasic_screen_reu_temp_probe "$SCRIPT_DIR/run_readybasic_screen_reu_temp_probe.sh" READYBASIC_SCRREU_PLAN
  emit_readybasic_plan readybasic_state_probe "$SCRIPT_DIR/run_readybasic_state_probe.sh" READYBASIC_PLAN
  emit_readybasic_plan readybasic_large_vars_probe "$SCRIPT_DIR/run_readybasic_large_vars_probe.sh" READYBASIC_PLAN
  READYBASIC_HOTKEY_EXPECT_F4="CLIPBOARD" READYBASIC_HOTKEY_F4_RETURN_MODE="stop_after_f4" \
    emit_readybasic_plan readybasic_hotkey_f4_probe "$SCRIPT_DIR/run_readybasic_hotkey_probe.sh" READYBASIC_HOTKEY_PLAN
  READYBASIC_HOTKEY_EXPECT_F2="CALENDAR 26" READYBASIC_HOTKEY_SCENARIO="f2_only" \
    emit_readybasic_plan readybasic_hotkey_f2_probe "$SCRIPT_DIR/run_readybasic_hotkey_probe.sh" READYBASIC_HOTKEY_PLAN
  READYBASIC_CHAIN_READYBASIC_BANK=5 READYBASIC_CHAIN_REUVIEWER_BANK=8 READYBASIC_CHAIN_CONSTRAIN_BITMAP=1 \
    emit_readybasic_plan readybasic_reuviewer_f2_chain_probe "$SCRIPT_DIR/run_readybasic_reuviewer_f2_chain_probe.sh" READYBASIC_REUVIEWER_CHAIN_PLAN
  emit_readybasic_plan readybasic_cross_app_resume_probe "$SCRIPT_DIR/run_readybasic_cross_app_resume_probe.sh" READYBASIC_PLAN
  emit_readybasic_plan readybasic_second_entry_editor_probe "$SCRIPT_DIR/run_readybasic_second_entry_editor_probe.sh" READYBASIC_PLAN
  emit_readybasic_plan readybasic_full_suite_visual_verification "$SCRIPT_DIR/run_readybasic_full_suite_visual_verification.sh" READYBASIC_FULL_PLAN
fi

if [ "$SCOPE" = "all" ] || [ "$SCOPE" = "readyshell" ]; then
  emit_readyshell_plan readyshell_cross_app_resume_probe "$SCRIPT_DIR/run_readyshell_cross_app_resume_probe.sh"
fi

write_prg_fixture() {
  local path="$1"
  local name="$2"
  if [ -f "$path" ]; then
    c1541 "$D64" -delete "$name" >/dev/null 2>&1 || true
    c1541 "$D64" -write "$path" "$name" >/dev/null
  fi
}

write_seq_fixture() {
  local path="$1"
  local name="$2"
  if [ -f "$path" ]; then
    c1541 "$D64" -delete "$name" >/dev/null 2>&1 || true
    c1541 "$D64" -write "$path" "$name,s" >/dev/null
  fi
}

write_prg_fixture obj/rbtest1.prg rbtest1
write_prg_fixture obj/rbproc1.prg rbproc1
write_prg_fixture obj/rbprocerr.prg rbprocerr
write_prg_fixture obj/rbscrreu.prg rbscrreu
write_seq_fixture obj/readybasic_modules/rbm.sample1.seq rbm.sample1
write_seq_fixture obj/readybasic_modules/rbm.sample2.seq rbm.sample2
write_seq_fixture obj/readybasic_modules/rbm.sample3.seq rbm.sample3

dotnet build "$PROJECT"

for plan in "${RUN_PLANS[@]}"; do
  echo "==> EasyFlash VICE plan: $(basename "$plan")"
  plan_rc=1
  for attempt in 1 2; do
    if [ "$attempt" -gt 1 ]; then
      echo "==> Retrying EasyFlash VICE plan after cold-start pause: $(basename "$plan")"
      sleep 15
    else
      sleep 5
    fi
    set +e
    dotnet run --project "$PROJECT" -- run-plan --plan "$plan" --close-vice
    plan_rc=$?
    set -e
    if [ "$plan_rc" -eq 0 ]; then
      break
    fi
  done
  if [ "$plan_rc" -ne 0 ]; then
    exit "$plan_rc"
  fi
done
