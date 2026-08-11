#!/usr/bin/env bash
set -euo pipefail

# Launch from a Terminal-owned/background shell; see AGENTS.md.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
READYOS_ROOT="${READYOS_ROOT:-$(cd "$SCRIPT_DIR/.." && pwd)}"
HARNESS_REPO="${VICE_TASKS_REPO:-$READYOS_ROOT/../agenticdevharness}"
PROJECT="$HARNESS_REPO/tools/vice_tasks_dotnet/src/ViceTasks.Binary/ViceTasks.Binary.csproj"
C64U_HOST="${C64U_HOST:-10.0.0.79}"
TEST_SPEED_MHZ="${SYSINFO_C64U_SPEED_MHZ:-16}"
OUT_DIR="${SYSINFO_C64U_OUT_DIR:-$READYOS_ROOT/logs/sysinfo_c64u}"
PLAN="$OUT_DIR/sysinfo_c64u.generated.yaml"

case "$TEST_SPEED_MHZ" in
  1|2|3|4|6|8|10|12|14|16|20|24|32|40|48|64) ;;
  *) echo "SYSINFO_C64U_SPEED_MHZ is not supported: $TEST_SPEED_MHZ" >&2; exit 64 ;;
esac

repeat_key() {
  python3 - "$1" "$2" <<'PY'
import sys
print(",".join([sys.argv[1]] * int(sys.argv[2])))
PY
}

mkdir -p "$OUT_DIR"
cd "$READYOS_ROOT"
python3 build_support/verify_uci_protocol_contract.py

if [ "${SYSINFO_C64U_SKIP_BUILD:-0}" != "1" ]; then
  LAUNCHER_DMA_LOAD=1 /bin/bash ./run.sh --profile precog-d81 --build-only
fi

if [ -n "${SYSINFO_C64U_D81:-}" ]; then
  D81="$SYSINFO_C64U_D81"
else
  PUBLIC_VERSION="$(python3 build_support/update_build_version.py --current)"
  PUBLIC_VERSION="${PUBLIC_VERSION%[A-Z]}"
  D81="$(ls -t "Releases/$PUBLIC_VERSION/precog-d81/"*.d81 | head -1)"
fi

cleanup_hardware() {
  set +e
  C64U_HOST="$C64U_HOST" C64U_SKIP_UPLOAD=1 C64U_SKIP_CONFIG=1 \
  READYOS_CLEAR_REU=1 READYOS_CLEAR_REU_ONLY=1 \
    /bin/bash build_support/run_readyos_boot_c64u_rest.sh \
      "$D81" sysinfo-clear.d81 "$OUT_DIR/post-suite-reu-clear" \
      >>"$OUT_DIR/cleanup.log" 2>&1
  /usr/bin/curl --fail --silent --show-error --request PUT \
    "http://$C64U_HOST/v1/machine:reboot" \
    --output "$OUT_DIR/post-suite-reboot.json" \
    >>"$OUT_DIR/cleanup.log" 2>&1
  sleep 4
  /usr/bin/curl --fail --silent --show-error --request PUT \
    "http://$C64U_HOST/v1/machine:resume" \
    --output "$OUT_DIR/post-suite-resume.json" \
    >>"$OUT_DIR/cleanup.log" 2>&1
}
trap cleanup_hardware EXIT

C64U_HOST="$C64U_HOST" C64U_SKIP_UPLOAD=1 C64U_SKIP_CONFIG=1 \
READYOS_CLEAR_REU=1 READYOS_CLEAR_REU_ONLY=1 \
  /bin/bash build_support/run_readyos_boot_c64u_rest.sh \
    "$D81" sysinfo-clear.d81 "$OUT_DIR/reu-clear-preflight"
/usr/bin/curl --fail --silent --show-error --request PUT \
  "http://$C64U_HOST/v1/machine:reboot" \
  --output "$OUT_DIR/ultimate-reboot.json"
sleep 8

# HOME selects Browse (index 0); System Info is index 10 in the regular D81
# launcher catalog (two launcher actions followed by eight earlier apps).
SELECT_SYSINFO="19,$(repeat_key 17 10),13"
REFRESH_ULTIMATE="$(repeat_key 13 6)"

cat >"$PLAN" <<YAML
version: 1
kind: ultimate_task_plan
plan_id: sysinfo_uci_smoke
run_mode: ultimate64
global_defaults:
  retry_policy: { max_attempts: 2, backoff_ms: 500, jitter: false }
  timeouts: { launch_s: 180, step_s: 360, read_s: 10 }
  artifact_policy: { capture_screen: true, capture_state: true, capture_dump: false }
  ultimate:
    host: $C64U_HOST
    password: null
    ftp_user: anonymous
    ftp_password: anonymous@
    remote_root: USB1
    disk8: "$D81"
    autostart_prg: null
    drive_a_bus_id: 8
    drive_a_type: '1581'
    drive_a_enabled: true
    drive_b_enabled: false
    mount_mode: readwrite
    boot_drive: 8
    capture_video_stream: false
    default_speed_mhz: 1
steps:
  - id: launch_readyos
    type: ultimate.launch
    params:
      boot_mode: disk
      drives:
        - { slot: a, bus_id: 8, drive_type: '1581', enabled: true, disk: "$D81", remote_disk: "USB1/READYOS.D81", image_type: d81, mount_mode: readwrite }
      boot_drive: 8
      boot_command: "LOAD\"BOOT\",8\r"
      reset_before_boot: true
      post_reset_delay_s: 3.0
      boot_inter_chunk_delay_s: 0.25
  - id: wait_basic_loading_boot
    type: screen.wait_contains
    params: { text: "LOADING", wait_timeout_s: 60, poll_s: 0.5 }
  - id: run_readyos_booter
    type: input.sequence
    params: { pre_delay_s: 45, keys: [82,85,78,13], inter_key_delay_s: 0.1, post_delay_s: 0.5 }
  - id: wait_launcher
    type: screen.wait_contains
    params: { text: "READY OS", wait_timeout_s: 360, poll_s: 1.0, pre_delay_s: 90, capture_label: sysinfo_launcher }
  - id: set_test_speed
    type: ultimate.speed.set
    params: { mhz: $TEST_SPEED_MHZ }
  - id: select_sysinfo
    type: input.sequence
    params: { keys: [$SELECT_SYSINFO], inter_key_delay_s: 0.18, post_delay_s: 2.0 }
  - id: wait_sysinfo
    type: screen.wait_contains
    # Wait for the final System row, not merely the static app shell. The REU
    # probe is intentionally slow at 1 MHz and input sent before it completes
    # can be missed, which would make the UCI tab test a false failure.
    params: { text: "faster above 1mhz", wait_timeout_s: 240, poll_s: 1.0, pre_delay_s: 20, capture_label: sysinfo_system }
  - id: open_ultimate_tab
    type: input.sequence
    params: { keys: [17], inter_key_delay_s: 0.1, post_delay_s: 0.5 }
  - id: capture_after_ultimate_key
    type: screen.capture
    params: { label: sysinfo_after_ultimate_key }
  - id: wait_uci_base
    type: screen.wait_contains
    params: { text: "uci:", wait_timeout_s: 30, poll_s: 0.25, capture_label: sysinfo_ultimate }
  - id: assert_control_target
    type: assert.screen
    params: { contains: "ctrl:" }
  - id: assert_network_target
    type: assert.screen
    params: { contains: "net:" }
  - id: assert_uci_detected
    type: assert.screen_not_contains
    params: { not_contains: "uci: not detected" }
  - id: repeated_back_to_back_refresh
    type: input.sequence
    params: { keys: [$REFRESH_ULTIMATE], inter_key_delay_s: 0.15, post_delay_s: 0.5 }
  - id: wait_after_refresh
    type: screen.wait_contains
    params: { text: "uci:", wait_timeout_s: 30, poll_s: 0.25, capture_label: sysinfo_ultimate_refreshed }
YAML

dotnet build "$PROJECT" | tee "$OUT_DIR/dotnet-build.log"
dotnet run --project "$PROJECT" -- run-ultimate-plan \
  --plan "$PLAN" --host "$C64U_HOST" --no-tui | tee "$OUT_DIR/harness.log"

echo "System Info UCI smoke passed at ${TEST_SPEED_MHZ} MHz; artifacts: $OUT_DIR"
