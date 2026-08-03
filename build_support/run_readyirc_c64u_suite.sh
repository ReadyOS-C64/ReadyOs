#!/usr/bin/env bash
set -euo pipefail

# C64 Ultimate access must be launched from a Terminal-owned/background shell.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
READYOS_ROOT="${READYOS_ROOT:-$(cd "$SCRIPT_DIR/.." && pwd)}"
HARNESS_REPO="${VICE_TASKS_REPO:-$READYOS_ROOT/../agenticdevharness}"
PROJECT="$HARNESS_REPO/tools/vice_tasks_dotnet/src/ViceTasks.Binary/ViceTasks.Binary.csproj"
C64U_HOST="${C64U_HOST:-10.0.0.79}"
FIXTURE_PORT="${READYIRC_FIXTURE_PORT:-16667}"
FIXTURE_CHANNEL="${READYIRC_FIXTURE_CHANNEL:-#readyostest}"
SWITCH_CHANNEL="${READYIRC_SWITCH_CHANNEL:-#secondtest}"
FIXTURE_NICK="${READYIRC_FIXTURE_NICK:-autonick}"
OUT_DIR="${READYIRC_C64U_OUT_DIR:-$READYOS_ROOT/logs/readyirc_c64u}"
PLAN="$OUT_DIR/readyirc_c64u.generated.yaml"
FIXTURE_LOG="$OUT_DIR/fixture.log"
FIXTURE_STATUS="$OUT_DIR/fixture-status.json"
HARNESS_LOG="$OUT_DIR/harness.log"
FIXTURE_PID=""

cleanup() {
  if [ -n "$FIXTURE_PID" ] && kill -0 "$FIXTURE_PID" 2>/dev/null; then
    kill "$FIXTURE_PID" 2>/dev/null || true
    wait "$FIXTURE_PID" 2>/dev/null || true
  fi
}
trap cleanup EXIT

keys() {
  python3 - "$1" <<'PY'
import sys
values = []
for char in sys.argv[1]:
    code = ord(char)
    if 0x61 <= code <= 0x7a:
        code -= 0x20
    values.append(str(code))
print(",".join(values))
PY
}

repeat_key() {
  python3 - "$1" "$2" <<'PY'
import sys
print(",".join([sys.argv[1]] * int(sys.argv[2])))
PY
}

discover_fixture_host() {
  if [ -n "${READYIRC_FIXTURE_HOST:-}" ]; then
    printf '%s\n' "$READYIRC_FIXTURE_HOST"
    return
  fi
  local iface=""
  local address=""
  if command -v route >/dev/null 2>&1; then
    iface="$(route -n get "$C64U_HOST" 2>/dev/null | awk '/interface:/{print $2; exit}')"
  fi
  if [ -n "$iface" ] && command -v ipconfig >/dev/null 2>&1; then
    address="$(ipconfig getifaddr "$iface" 2>/dev/null || true)"
  fi
  if [ -z "$address" ] && command -v hostname >/dev/null 2>&1; then
    address="$(hostname -I 2>/dev/null | awk '{print $1}')"
  fi
  if [ -z "$address" ] || [ "$address" = "127.0.0.1" ]; then
    echo "Unable to discover a LAN address; set READYIRC_FIXTURE_HOST" >&2
    return 1
  fi
  printf '%s\n' "$address"
}

mkdir -p "$OUT_DIR"
cd "$READYOS_ROOT"

FIXTURE_HOST="$(discover_fixture_host)"

if [ "${READYIRC_C64U_GENERATE_PLAN_ONLY:-0}" != "1" ] &&
   [ "${READYIRC_C64U_SKIP_BUILD:-0}" != "1" ]; then
  VERSION_TEXT="$(python3 build_support/update_build_version.py --current)"
  make -B \
    BUILD_SUPPORT_DIR=build_support \
    PROFILE=precog-d81 \
    LAUNCHER_DMA_LOAD=1 \
    READYOS_VERSION_TEXT="$VERSION_TEXT" \
    profile
fi

if [ -n "${READYIRC_C64U_D81:-}" ]; then
  D81="$READYIRC_C64U_D81"
else
  PUBLIC_VERSION="$(python3 build_support/update_build_version.py --current)"
  PUBLIC_VERSION="${PUBLIC_VERSION%[A-Z]}"
  D81="$(ls -t "Releases/$PUBLIC_VERSION/precog-d81/"*.d81 | head -1)"
fi

if [ "${READYIRC_C64U_GENERATE_PLAN_ONLY:-0}" != "1" ]; then
  # The suite's first launch must not resume a stale launcher or ReadyIRC image.
  # Reuse the established ReadyOS Ultimate preflight to clear REU before boot.
  if [ "${READYIRC_C64U_SKIP_REU_CLEAR:-0}" != "1" ]; then
    C64U_HOST="$C64U_HOST" \
    C64U_SKIP_UPLOAD=1 \
    C64U_SKIP_CONFIG=1 \
    READYOS_CLEAR_REU=1 \
    READYOS_CLEAR_REU_ONLY=1 \
      /bin/bash build_support/run_readyos_boot_c64u_rest.sh \
        "$D81" readyirc-clear.d81 "$OUT_DIR/reu-clear-preflight"
  fi

  # A soft C64 reset can preserve a wedged cartridge/UCI state. Reboot the
  # Ultimate core before the harness mounts and boots the test disk.
  if [ "${READYIRC_C64U_SKIP_REBOOT:-0}" != "1" ]; then
    /usr/bin/curl --fail --silent --show-error --request PUT \
      "http://$C64U_HOST/v1/machine:reboot" \
      --output "$OUT_DIR/ultimate-reboot.json"
    sleep 8
  fi

  python3 build_support/readyirc_fixture_server.py \
    --port "$FIXTURE_PORT" \
    --channel "$FIXTURE_CHANNEL" \
    --ultimate-host "$C64U_HOST" \
    --log "$FIXTURE_LOG" \
    --status "$FIXTURE_STATUS" &
  FIXTURE_PID=$!

  for _ in $(seq 1 50); do
    [ -s "$FIXTURE_STATUS" ] && break
    sleep 0.1
  done
  if [ ! -s "$FIXTURE_STATUS" ]; then
    echo "ReadyIRC fixture did not start" >&2
    exit 1
  fi
fi

DEL_SERVER="$(repeat_key 20 26)"
DEL_PORT="$(repeat_key 20 5)"
DEL_NICK="$(repeat_key 20 30)"
DEL_CHANNEL="$(repeat_key 20 30)"
SELECT_READYIRC="19,$(repeat_key 17 18),13"

cat >"$PLAN" <<YAML
version: 1
kind: ultimate_task_plan
plan_id: readyirc_c64u_lifecycle
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
    params: { text: "LOADING", wait_timeout_s: 60, poll_s: 0.5, capture_label: readyirc_basic_loading }
  - id: run_readyos_booter
    type: input.sequence
    params: { pre_delay_s: 45, keys: [$(keys $'RUN\r')], inter_key_delay_s: 0.1, post_delay_s: 0.5 }
  - id: wait_launcher
    type: screen.wait_contains
    params: { text: "READY OS", wait_timeout_s: 360, poll_s: 1.0, pre_delay_s: 90, capture_label: readyirc_launcher }
  - id: select_readyirc
    type: input.sequence
    params: { keys: [$SELECT_READYIRC], inter_key_delay_s: 0.18, post_delay_s: 2.0 }
  - id: wait_setup
    type: screen.wait_contains
    params: { text: "readyirc setup", wait_timeout_s: 180, poll_s: 1.0, pre_delay_s: 20, capture_label: readyirc_setup_defaults }
  - id: assert_default_nick
    type: assert.screen
    params: { contains: "enteryournick" }
  - id: assert_lowercase_charset
    type: assert.memory
    params: { start: 0xD018, end: 0xD018, equals_hex: "17" }
  - id: focus_port
    type: input.sequence
    params: { keys: [17], inter_key_delay_s: 0.1, post_delay_s: 0.2 }
  - id: invalid_port
    type: input.sequence
    params: { keys: [$DEL_PORT,48,13], inter_key_delay_s: 0.05, post_delay_s: 0.5 }
  - id: assert_invalid_port
    type: assert.screen
    params: { contains: "port must be 1-65535" }
  - id: set_valid_port
    type: input.sequence
    params: { keys: [20,$(keys "$FIXTURE_PORT")], inter_key_delay_s: 0.05, post_delay_s: 0.2 }
  - id: focus_server
    type: input.sequence
    params: { keys: [145], inter_key_delay_s: 0.1, post_delay_s: 0.2 }
  - id: set_server
    type: input.sequence
    params: { keys: [$DEL_SERVER,$(keys "$FIXTURE_HOST")], inter_key_delay_s: 0.04, post_delay_s: 0.2 }
  - id: focus_nick
    type: input.sequence
    params: { keys: [17,17], inter_key_delay_s: 0.1, post_delay_s: 0.2 }
  - id: set_nick
    type: input.sequence
    params: { keys: [$DEL_NICK,$(keys "$FIXTURE_NICK")], inter_key_delay_s: 0.04, post_delay_s: 0.2 }
  - id: focus_channel
    type: input.sequence
    params: { keys: [17], inter_key_delay_s: 0.1, post_delay_s: 0.2 }
  - id: set_channel_and_connect
    type: input.sequence
    params: { keys: [$DEL_CHANNEL,$(keys "$FIXTURE_CHANNEL"),13], inter_key_delay_s: 0.04, post_delay_s: 1.0 }
  - id: wait_mixed_case_welcome
    type: screen.wait_contains
    params: { text: "connection 1 mixed case", wait_timeout_s: 60, poll_s: 0.5, capture_label: readyirc_connected_1 }
  - id: echo_message
    type: input.sequence
    params: { keys: [$(keys $'hello\r')], inter_key_delay_s: 0.06, post_delay_s: 0.5 }
  - id: wait_echo
    type: screen.wait_contains
    params: { text: "echo mixed hello", wait_timeout_s: 30, poll_s: 0.5, capture_label: readyirc_echo }
  - id: fill_scrollback
    type: input.sequence
    params: { keys: [$(keys $'fillscroll\r')], inter_key_delay_s: 0.04, post_delay_s: 0.5 }
  - id: wait_filled_scrollback
    type: screen.wait_contains
    params: { text: "fill line 25", wait_timeout_s: 30, poll_s: 0.25, capture_label: readyirc_scroll_filled }
  - id: plant_append_sentinel
    type: input.sequence
    params: { keys: [$(keys $'plantsentinelappend\r')], inter_key_delay_s: 0.04, post_delay_s: 1.0 }
  - id: assert_append_screen_sentinel_planted
    type: assert.memory
    params: { start: 0x0607, end: 0x0607, equals_hex: "7F" }
  - id: assert_append_color_sentinel_planted
    type: assert.memory
    params: { start: 0xDA07, end: 0xDA07, equals_hex: "0E", mask_hex: "0F" }
  - id: append_single_local_line
    type: input.sequence
    params: { keys: [$(keys $'renderprobe\r')], inter_key_delay_s: 0.04, post_delay_s: 1.0 }
  - id: assert_append_shifted_screen_once
    type: assert.memory
    params: { start: 0x05DF, end: 0x05DF, equals_hex: "7F" }
  - id: assert_append_shifted_color_once
    type: assert.memory
    params: { start: 0xD9DF, end: 0xD9DF, equals_hex: "0E", mask_hex: "0F" }
  - id: plant_scroll_sentinel
    type: input.sequence
    params: { keys: [$(keys $'plantsentinelscroll\r')], inter_key_delay_s: 0.04, post_delay_s: 1.0 }
  - id: assert_scroll_screen_sentinel_planted
    type: assert.memory
    params: { start: 0x0607, end: 0x0607, equals_hex: "7E" }
  - id: assert_scroll_color_sentinel_planted
    type: assert.memory
    params: { start: 0xDA07, end: 0xDA07, equals_hex: "07", mask_hex: "0F" }
  - id: scroll_up_one
    type: input.sequence
    params: { keys: [145], inter_key_delay_s: 0.1, post_delay_s: 0.3 }
  - id: assert_scroll_up_shifted_screen_down
    type: assert.memory
    params: { start: 0x062F, end: 0x062F, equals_hex: "7E" }
  - id: assert_scroll_up_shifted_color_down
    type: assert.memory
    params: { start: 0xDA2F, end: 0xDA2F, equals_hex: "07", mask_hex: "0F" }
  - id: scroll_down_one
    type: input.sequence
    params: { keys: [17], inter_key_delay_s: 0.1, post_delay_s: 0.3 }
  - id: assert_scroll_down_restored_screen
    type: assert.memory
    params: { start: 0x0607, end: 0x0607, equals_hex: "7E" }
  - id: assert_scroll_down_restored_color
    type: assert.memory
    params: { start: 0xDA07, end: 0xDA07, equals_hex: "07", mask_hex: "0F" }
  - id: enter_history_view
    type: input.sequence
    params: { keys: [145], inter_key_delay_s: 0.1, post_delay_s: 0.3 }
  - id: plant_history_sentinel
    type: input.sequence
    params: { keys: [$(keys $'plantsentinelhistory\r')], inter_key_delay_s: 0.04, post_delay_s: 1.0 }
  - id: assert_history_screen_sentinel_planted
    type: assert.memory
    params: { start: 0x0607, end: 0x0607, equals_hex: "7D" }
  - id: assert_history_color_sentinel_planted
    type: assert.memory
    params: { start: 0xDA07, end: 0xDA07, equals_hex: "06", mask_hex: "0F" }
  - id: append_while_viewing_history
    type: input.sequence
    params: { keys: [$(keys $'historyprobe\r')], inter_key_delay_s: 0.04, post_delay_s: 1.0 }
  - id: assert_history_screen_unchanged
    type: assert.memory
    params: { start: 0x0607, end: 0x0607, equals_hex: "7D" }
  - id: assert_history_color_unchanged
    type: assert.memory
    params: { start: 0xDA07, end: 0xDA07, equals_hex: "06", mask_hex: "0F" }
  - id: return_to_live_tail
    type: input.sequence
    params: { keys: [17,17,17], inter_key_delay_s: 0.1, post_delay_s: 0.5 }
  - id: wait_history_probe_at_tail
    type: screen.wait_contains
    params: { text: "historyprobe", wait_timeout_s: 10, poll_s: 0.25, capture_label: readyirc_incremental_render }
  - id: names_current_channel
    type: input.sequence
    params: { keys: [$(keys $'/names\r')], inter_key_delay_s: 0.05, post_delay_s: 0.5 }
  - id: wait_names_current
    type: screen.wait_contains
    params: { text: "names $FIXTURE_CHANNEL: @alpha +beta gamma", wait_timeout_s: 20, poll_s: 0.25, capture_label: readyirc_names_current }
  - id: wait_names_current_end
    type: screen.wait_contains
    params: { text: "end names $FIXTURE_CHANNEL", wait_timeout_s: 10, poll_s: 0.25 }
  - id: names_long_explicit_channel
    type: input.sequence
    params: { keys: [$(keys $'/names #longnames\r')], inter_key_delay_s: 0.05, post_delay_s: 0.5 }
  - id: wait_long_names_tail
    type: screen.wait_contains
    params: { text: "longnick30", wait_timeout_s: 20, poll_s: 0.25, capture_label: readyirc_names_long }
  - id: wait_long_names_end
    type: screen.wait_contains
    params: { text: "end names #longnames", wait_timeout_s: 10, poll_s: 0.25 }
  - id: invalid_join
    type: input.sequence
    params: { keys: [$(keys $'/join invalid\r')], inter_key_delay_s: 0.05, post_delay_s: 0.5 }
  - id: assert_invalid_join
    type: assert.screen
    params: { contains: "usage /join #channel" }
  - id: switch_channel
    type: input.sequence
    params: { keys: [$(keys "/join $SWITCH_CHANNEL"),13], inter_key_delay_s: 0.05, post_delay_s: 0.5 }
  - id: wait_switched_channel_header
    type: screen.wait_contains
    params: { text: "readyirc online $SWITCH_CHANNEL", wait_timeout_s: 20, poll_s: 0.25, capture_label: readyirc_join_switched }
  - id: wait_switched_channel_message
    type: screen.wait_contains
    params: { text: "joined new channel", wait_timeout_s: 20, poll_s: 0.25 }
  - id: message_after_join
    type: input.sequence
    params: { keys: [$(keys $'afterjoin\r')], inter_key_delay_s: 0.05, post_delay_s: 0.5 }
  - id: wait_echo_after_join
    type: screen.wait_contains
    params: { text: "echo mixed afterjoin", wait_timeout_s: 20, poll_s: 0.25, capture_label: readyirc_after_join }
  - id: queue_and_suspend
    type: input.sequence
    params: { keys: [$(keys $'queuewhileaway\r'),2], inter_key_delay_s: 0.06, post_delay_s: 0.5 }
  - id: wait_launcher_alive_suspend
    type: screen.wait_contains
    params: { text: "READY OS", wait_timeout_s: 60, poll_s: 0.5, pre_delay_s: 4.0, capture_label: readyirc_alive_launcher }
  - id: resume_readyirc_alive
    type: input.sequence
    params: { keys: [$SELECT_READYIRC], inter_key_delay_s: 0.18, post_delay_s: 2.0 }
  - id: wait_queued_message
    type: screen.wait_contains
    params: { text: "queued while suspended", wait_timeout_s: 60, poll_s: 0.5, capture_label: readyirc_alive_resume }
  - id: assert_no_reconnect_alive
    type: assert.screen_not_contains
    params: { not_contains: "connection 2" }
  - id: drop_and_suspend
    type: input.sequence
    params: { keys: [$(keys $'dropwhileaway\r'),2], inter_key_delay_s: 0.06, post_delay_s: 0.5 }
  - id: wait_launcher_stale_suspend
    type: screen.wait_contains
    params: { text: "READY OS", wait_timeout_s: 60, poll_s: 0.5, pre_delay_s: 5.0, capture_label: readyirc_stale_launcher }
  - id: resume_readyirc_stale
    type: input.sequence
    params: { keys: [$SELECT_READYIRC], inter_key_delay_s: 0.18, post_delay_s: 2.0 }
  - id: wait_reconnected
    type: screen.wait_contains
    params: { text: "connection 2 mixed case", wait_timeout_s: 60, poll_s: 0.5, capture_label: readyirc_reconnected }
  - id: f1_disconnect
    type: input.sequence
    params: { keys: [133], inter_key_delay_s: 0.1, post_delay_s: 1.0 }
  - id: wait_setup_after_f1
    type: screen.wait_contains
    params: { text: "disconnected", wait_timeout_s: 20, poll_s: 0.5, capture_label: readyirc_f1_disconnected }
  - id: reconnect_for_command_test
    type: input.sequence
    params: { keys: [13], inter_key_delay_s: 0.1, post_delay_s: 1.0 }
  - id: wait_connection_3
    type: screen.wait_contains
    params: { text: "connection 3 mixed case", wait_timeout_s: 60, poll_s: 0.5, capture_label: readyirc_connected_3 }
  - id: slash_disconnect
    type: input.sequence
    params: { keys: [$(keys $'/disconnect\r')], inter_key_delay_s: 0.06, post_delay_s: 1.0 }
  - id: suspend_intentionally_offline
    type: input.sequence
    params: { keys: [2], inter_key_delay_s: 0.1, post_delay_s: 0.5 }
  - id: wait_launcher_offline
    type: screen.wait_contains
    params: { text: "READY OS", wait_timeout_s: 60, poll_s: 0.5, pre_delay_s: 3.0, capture_label: readyirc_offline_launcher }
  - id: resume_readyirc_offline
    type: input.sequence
    params: { keys: [$SELECT_READYIRC], inter_key_delay_s: 0.18, post_delay_s: 2.0 }
  - id: wait_offline_setup_resume
    type: screen.wait_contains
    params: { text: "readyirc setup", wait_timeout_s: 60, poll_s: 0.5, pre_delay_s: 3.0, capture_label: readyirc_offline_resume }
  - id: assert_settings_retained
    type: assert.screen
    params: { contains: "$FIXTURE_NICK" }
  - id: assert_switched_channel_retained
    type: assert.screen
    params: { contains: "$SWITCH_CHANNEL" }
  - id: capture_final
    type: screen.capture
    params: { label: readyirc_final_setup }
  - id: dump_final_state
    type: dump.memory_ranges
    criticality: observe
    params:
      ranges:
        - { label: screen_0400, start: 0x0400, end: 0x07E7 }
        - { label: shim_c600, start: 0xC600, end: 0xC9FF }
YAML

if [ "${READYIRC_C64U_GENERATE_PLAN_ONLY:-0}" = "1" ]; then
  echo "wrote $PLAN"
  exit 0
fi

dotnet build "$PROJECT" | tee "$OUT_DIR/dotnet-build.log"
dotnet run --project "$PROJECT" -- run-ultimate-plan \
  --plan "$PLAN" --host "$C64U_HOST" --no-tui | tee "$HARNESS_LOG"

python3 - "$FIXTURE_STATUS" "$FIXTURE_NICK" "$FIXTURE_CHANNEL" "$SWITCH_CHANNEL" <<'PY'
import json
import pathlib
import sys

status = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
expected_nick = sys.argv[2]
expected_channel = sys.argv[3]
switch_channel = sys.argv[4]
errors = []
if status.get("connections") != 3:
    errors.append(f"expected 3 fixture connections, got {status.get('connections')}")
if status.get("registered") != 3:
    errors.append(f"expected 3 registrations, got {status.get('registered')}")
if status.get("quit_count", 0) < 2:
    errors.append(f"expected both intentional QUITs, got {status.get('quit_count')}")
if not status.get("exact_pong"):
    errors.append("mixed-case PING token was not echoed exactly")
registrations = status.get("registrations", [])
for connection in range(1, 4):
    matches = [item for item in registrations if item.get("connection") == connection]
    if len(matches) != 1:
        errors.append(f"connection {connection} registration details missing")
        continue
    item = matches[0]
    if item.get("nick") != expected_nick:
        errors.append(f"connection {connection} NICK was {item.get('nick')!r}")
    if item.get("user") != expected_nick:
        errors.append(f"connection {connection} USER was {item.get('user')!r}")
    wanted_channel = expected_channel if connection == 1 else switch_channel
    if item.get("channel") != wanted_channel:
        errors.append(
            f"connection {connection} JOIN was {item.get('channel')!r}, "
            f"expected {wanted_channel!r}"
        )
if status.get("channel_commands") != [
    f"part {expected_channel}", f"join {switch_channel}"
]:
    errors.append(f"unexpected channel command order: {status.get('channel_commands')!r}")
if status.get("names_requests") != [expected_channel, "#longnames"]:
    errors.append(f"unexpected NAMES requests: {status.get('names_requests')!r}")
if switch_channel not in status.get("privmsg_targets", []):
    errors.append("no PRIVMSG targeted the switched channel")
if errors:
    raise SystemExit("ReadyIRC fixture assertions failed:\n- " + "\n- ".join(errors))
print("ReadyIRC fixture assertions passed")
PY

if [ "${READYIRC_C64U_PRESERVE_TEST_STATE:-0}" != "1" ]; then
  cleanup
  FIXTURE_PID=""
  C64U_HOST="$C64U_HOST" \
  C64U_SKIP_UPLOAD=1 \
  C64U_SKIP_CONFIG=1 \
  READYOS_CLEAR_REU=1 \
  READYOS_CLEAR_REU_ONLY=1 \
    /bin/bash build_support/run_readyos_boot_c64u_rest.sh \
      "$D81" readyirc-clear.d81 "$OUT_DIR/post-suite-reu-clear"
  /usr/bin/curl --fail --silent --show-error --request PUT \
    "http://$C64U_HOST/v1/machine:reboot" \
    --output "$OUT_DIR/post-suite-reboot.json"
  sleep 8
  C64U_HOST="$C64U_HOST" \
  C64U_SKIP_UPLOAD=1 \
  C64U_SKIP_CONFIG=1 \
  READYOS_BOOT_INITIAL_WAIT_S=90 \
    /bin/bash build_support/run_readyos_boot_c64u_rest.sh \
      "$D81" READYOS.D81 "$OUT_DIR/post-suite-clean-boot"
fi

echo "ReadyIRC C64 Ultimate suite passed; artifacts: $OUT_DIR"
