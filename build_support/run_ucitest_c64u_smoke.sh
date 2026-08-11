#!/usr/bin/env bash
set -euo pipefail

# Ultimate REST/FTP access from Codex must launch this script in a
# Terminal-owned/background shell; see AGENTS.md.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
READYOS_ROOT="${READYOS_ROOT:-$(cd "$SCRIPT_DIR/.." && pwd)}"
HARNESS_REPO="${VICE_TASKS_REPO:-$READYOS_ROOT/../agenticdevharness}"
PROJECT="$HARNESS_REPO/tools/vice_tasks_dotnet/src/ViceTasks.Binary/ViceTasks.Binary.csproj"
C64U_HOST="${C64U_HOST:-10.0.0.79}"
TEST_SPEED_MHZ="${UCITEST_C64U_SPEED_MHZ:-16}"
OUT_DIR="${UCITEST_C64U_OUT_DIR:-$READYOS_ROOT/logs/ucitest_c64u}"
PLAN="$OUT_DIR/ucitest_c64u.generated.yaml"

case "$TEST_SPEED_MHZ" in
  1|2|3|4|6|8|10|12|14|16|20|24|32|40|48|64) ;;
  *) echo "UCITEST_C64U_SPEED_MHZ is not supported: $TEST_SPEED_MHZ" >&2; exit 64 ;;
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

if [ "${UCITEST_C64U_SKIP_BUILD:-0}" != "1" ]; then
  LAUNCHER_DMA_LOAD=1 /bin/bash ./run.sh --profile precog-d81 --build-only
fi

if [ -n "${UCITEST_C64U_D81:-}" ]; then
  D81="$UCITEST_C64U_D81"
else
  PUBLIC_VERSION="$(python3 build_support/update_build_version.py --current)"
  PUBLIC_VERSION="${PUBLIC_VERSION%[A-Z]}"
  D81="$(ls -t "Releases/$PUBLIC_VERSION/precog-d81/"*.d81 | head -1)"
fi

cleanup_hardware() {
  set +e
  C64U_HOST="$C64U_HOST" \
  C64U_SKIP_UPLOAD=1 \
  C64U_SKIP_CONFIG=1 \
  READYOS_CLEAR_REU=1 \
  READYOS_CLEAR_REU_ONLY=1 \
    /bin/bash build_support/run_readyos_boot_c64u_rest.sh \
      "$D81" ucitest-clear.d81 "$OUT_DIR/post-suite-reu-clear" \
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

C64U_HOST="$C64U_HOST" \
C64U_SKIP_UPLOAD=1 \
C64U_SKIP_CONFIG=1 \
READYOS_CLEAR_REU=1 \
READYOS_CLEAR_REU_ONLY=1 \
  /bin/bash build_support/run_readyos_boot_c64u_rest.sh \
    "$D81" ucitest-clear.d81 "$OUT_DIR/reu-clear-preflight"

/usr/bin/curl --fail --silent --show-error --request PUT \
  "http://$C64U_HOST/v1/machine:reboot" \
  --output "$OUT_DIR/ultimate-reboot.json"
sleep 8

SELECT_UCITEST="19,$(repeat_key 17 19),13"

cat >"$PLAN" <<YAML
version: 1
kind: ultimate_task_plan
plan_id: ucitest_c64u_smoke
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
    disk9: null
    autostart_prg: null
    drive_a_bus_id: 8
    drive_b_bus_id: 9
    drive_a_type: '1581'
    drive_b_type: '1581'
    drive_a_enabled: true
    drive_b_enabled: false
    mount_mode: readwrite
    boot_drive: 8
    capture_video_stream: false
    stream_port: 11000
    stream_timeout_s: 4.0
    default_speed_mhz: 1
    warp_equivalent_mhz: 8
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
    params: { text: "READY OS", wait_timeout_s: 360, poll_s: 1.0, pre_delay_s: 90, capture_label: ucitest_launcher }
  - id: set_test_speed
    type: ultimate.speed.set
    params: { mhz: $TEST_SPEED_MHZ }
  - id: select_ucitest
    type: input.sequence
    params: { keys: [$SELECT_UCITEST], inter_key_delay_s: 0.18, post_delay_s: 2.0 }
  - id: wait_ucitest
    type: screen.wait_contains
    # The launcher loading overlay also says "UCI TESTER". This app-only help
    # token proves initialization and the first draw have completed.
    params: { text: "f1tgt", wait_timeout_s: 180, poll_s: 1.0, pre_delay_s: 20, capture_label: ucitest_initial }
  - id: assert_example_hotkey
    type: assert.screen
    params: { contains: "f8ex" }
  - id: run_detect
    type: input.sequence
    params: { keys: [135], inter_key_delay_s: 0.1, post_delay_s: 0.5 }
  - id: wait_detect
    type: screen.wait_contains
    params: { text: "uci base $", wait_timeout_s: 30, poll_s: 0.25, capture_label: ucitest_detect }
  - id: run_async_abort_recovery
    type: input.sequence
    params: { keys: [17,17,17,135], inter_key_delay_s: 0.1, post_delay_s: 0.5 }
  - id: wait_async_abort_recovery
    type: screen.wait_contains
    params: { text: "interface serviced", wait_timeout_s: 10, poll_s: 0.25, capture_label: ucitest_abort_recovered }
  - id: run_id_after_abort
    type: input.sequence
    params: { keys: [145,145,135], inter_key_delay_s: 0.1, post_delay_s: 0.5 }
  - id: wait_id_after_abort
    type: screen.wait_contains
    params: { text: "id $", wait_timeout_s: 10, poll_s: 0.25, capture_label: ucitest_after_abort }
  - id: open_examples
    type: input.sequence
    params: { keys: [140], inter_key_delay_s: 0.1, post_delay_s: 0.2 }
  - id: wait_examples
    type: screen.wait_contains
    params: { text: "prefill example", wait_timeout_s: 10, poll_s: 0.25, capture_label: ucitest_examples }
  - id: choose_protocol_norms
    type: input.sequence
    params: { keys: [17,13], inter_key_delay_s: 0.1, post_delay_s: 0.2 }
  - id: run_protocol_norms
    type: input.sequence
    params: { keys: [135], inter_key_delay_s: 0.1, post_delay_s: 0.5 }
  - id: assert_protocol_norms
    type: screen.wait_contains
    params: { text: "push is async; idle is not done", wait_timeout_s: 10, poll_s: 0.25, capture_label: ucitest_protocol_norms }
  - id: scroll_protocol_norms
    type: input.sequence
    params: { keys: [139,$(repeat_key 17 12)], inter_key_delay_s: 0.1, post_delay_s: 0.2 }
  - id: assert_protocol_norms_tail
    type: screen.wait_contains
    params: { text: "verify on real hardware at high speed", wait_timeout_s: 10, poll_s: 0.25, capture_label: ucitest_protocol_norms_tail }
  - id: choose_dos_identify
    type: input.sequence
    params: { keys: [140,17,13,135], inter_key_delay_s: 0.1, post_delay_s: 0.5 }
  - id: wait_dos_identify
    type: screen.wait_contains
    params: { text: "ultimate", wait_timeout_s: 30, poll_s: 0.25, capture_label: ucitest_dos_identify }
  - id: choose_network_address
    type: input.sequence
    params: { keys: [140,$(repeat_key 17 3),13,135], inter_key_delay_s: 0.1, post_delay_s: 0.5 }
  - id: wait_network_address
    type: screen.wait_contains
    params: { text: "ip:", wait_timeout_s: 30, poll_s: 0.25, capture_label: ucitest_network_address }
  - id: choose_http_get_prefill
    type: input.sequence
    params: { keys: [140,17,17,13], inter_key_delay_s: 0.1, post_delay_s: 0.5 }
  - id: assert_http_prefill
    type: assert.screen
    params: { contains: "example.com/" }
  - id: assert_http_verb_label
    type: assert.screen
    params: { contains: "GET" }
  - id: capture_final
    type: screen.capture
    params: { label: ucitest_http_prefill }
YAML

dotnet build "$PROJECT" | tee "$OUT_DIR/dotnet-build.log"
dotnet run --project "$PROJECT" -- run-ultimate-plan \
  --plan "$PLAN" --host "$C64U_HOST" --no-tui | tee "$OUT_DIR/harness.log"

echo "UCI Tester C64 Ultimate smoke passed at ${TEST_SPEED_MHZ} MHz; artifacts: $OUT_DIR"
