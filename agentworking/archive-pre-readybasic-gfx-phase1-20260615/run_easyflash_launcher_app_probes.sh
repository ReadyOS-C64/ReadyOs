#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HARNESS="${VICE_TASKS_REPO:-$ROOT/../agenticdevharness}/tools/vice_tasks_dotnet"
PROJECT="$HARNESS/src/ViceTasks.Binary/ViceTasks.Binary.csproj"
CRT="$ROOT/Releases/0.2.4/precog-easyflash/readyos_easyflash.crt"
D64="$ROOT/Releases/0.2.4/precog-easyflash/readyos_data.d64"
OUT_DIR="$ROOT/agentworking/easyflash_launcher_probes"

keys_raw() {
  python3 - "$1" <<'PY'
import sys
print(",".join(str(ord(ch)) for ch in sys.argv[1]))
PY
}

keys_upper() {
  python3 - "$1" <<'PY'
import sys
out = []
for ch in sys.argv[1]:
    code = ord(ch)
    if 0x61 <= code <= 0x7a:
        code -= 0x20
    out.append(str(code))
print(",".join(out))
PY
}

mkdir -p "$OUT_DIR"

READYBASIC_PLAN="$OUT_DIR/easyflash_readybasic_from_launcher.yaml"
READYSHELL_PLAN="$OUT_DIR/easyflash_readyshell_from_launcher.yaml"

cat >"$READYBASIC_PLAN" <<YAML
version: 1
kind: vice_task_plan
plan_id: easyflash_readybasic_from_launcher
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
    launch_s: 60
    step_s: 90
    read_s: 2
  artifact_policy:
    capture_screen: true
    capture_state: true
    capture_dump: false
  vice:
    cart_crt: "$CRT"
    autostart_enabled: false
    disk8: "$D64"
    drive8_enabled: true
    drive8_type: 1541
    drive9_enabled: false
    true_drive: true
    close_vice: true
    headless: true
    speed_percent: 100
steps:
  - id: launch_easyflash
    type: vice.launch
    params:
      kill_stale: true
  - id: wait_launcher
    type: screen.wait_contains
    params:
      text: "READY OS"
      pre_delay_s: 4
      poll_s: 0.5
      wait_timeout_s: 240
      capture_label: easyflash_launcher_readybasic_start
  - id: navigate_to_readybasic
    type: input.sequence
    params:
      keys: [17,17,17,17,13]
      inter_key_delay_s: 0.08
      post_delay_s: 2.0
  - id: wait_readybasic_prompt
    type: screen.wait_contains
    params:
      text: "READYBASIC"
      wait_timeout_s: 120
      capture_label: easyflash_readybasic_prompt
  - id: dump_readybasic_prompt_core
    type: dump.memory_ranges
    params:
      ranges:
        - { label: entry_1000, start: 0x1000, end: 0x1120 }
        - { label: hidden_shadow_9a00, start: 0x9A00, end: 0x9F40 }
        - { label: hidden_visible_a000, start: 0xA000, end: 0xA540 }
        - { label: vectors_0300, start: 0x0300, end: 0x030B }
        - { label: kernal_vectors_0324, start: 0x0324, end: 0x032B }
        - { label: chrget_0073, start: 0x0073, end: 0x008A }
        - { label: basic_zp_002b, start: 0x002B, end: 0x003F }
        - { label: txtptr_007a, start: 0x007A, end: 0x007B }
        - { label: kernal_mem_bounds_0280, start: 0x0280, end: 0x0287 }
        - { label: input_buffer_0200, start: 0x0200, end: 0x0258 }
        - { label: low_ram_0100_0600, start: 0x0100, end: 0x0600 }
        - { label: screen_0400, start: 0x0400, end: 0x07E7 }
        - { label: basic_text_1200, start: 0x1200, end: 0x1300 }
        - { label: readybasic_bridge_c000, start: 0xC000, end: 0xC5FF }
  - id: readybasic_direct_print
    type: input.sequence
    params:
      keys: [$(keys_upper $'PRINT 6*7\r')]
      inter_key_delay_s: 0.035
      post_delay_s: 3.0
  - id: capture_readybasic_direct_print
    type: screen.capture
    params:
      label: easyflash_readybasic_after_direct_print
  - id: assert_readybasic_direct_print
    type: screen.wait_contains
    params:
      text: " 42"
      wait_timeout_s: 30
      capture_label: easyflash_readybasic_direct_print
  - id: readybasic_zadd16
    type: input.sequence
    params:
      keys: [$(keys_upper $'PRINT "SUM";ZADD16(1,2)\r')]
      inter_key_delay_s: 0.035
      post_delay_s: 3.0
  - id: assert_readybasic_zadd16
    type: screen.wait_contains
    params:
      text: "SUM 3"
      wait_timeout_s: 30
      capture_label: easyflash_readybasic_zadd16
  - id: dump_readybasic_state
    type: dump.memory_ranges
    params:
      ranges:
        - { label: shim_c800, start: 0xC800, end: 0xCA00 }
        - { label: readybasic_window, start: 0x1000, end: 0x1800 }
YAML

cat >"$READYSHELL_PLAN" <<YAML
version: 1
kind: vice_task_plan
plan_id: easyflash_readyshell_from_launcher
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
    launch_s: 60
    step_s: 120
    read_s: 2
  artifact_policy:
    capture_screen: true
    capture_state: true
    capture_dump: false
  vice:
    cart_crt: "$CRT"
    autostart_enabled: false
    disk8: "$D64"
    drive8_enabled: true
    drive8_type: 1541
    drive9_enabled: false
    true_drive: true
    close_vice: true
    headless: true
    speed_percent: 100
steps:
  - id: launch_easyflash
    type: vice.launch
    params:
      kill_stale: true
  - id: wait_launcher
    type: screen.wait_contains
    params:
      text: "READY OS"
      pre_delay_s: 4
      poll_s: 0.5
      wait_timeout_s: 240
      capture_label: easyflash_launcher_readyshell_start
  - id: navigate_to_readyshell
    type: input.sequence
    params:
      keys: [17,13]
      inter_key_delay_s: 0.08
      post_delay_s: 2.0
  - id: wait_readyshell_prompt
    type: screen.wait_contains
    params:
      text: "run: cat"
      wait_timeout_s: 120
      capture_label: easyflash_readyshell_prompt
  - id: readyshell_ver
    type: input.sequence
    params:
      keys: [$(keys_upper $'VER\r')]
      inter_key_delay_s: 0.035
      post_delay_s: 1.0
  - id: assert_readyshell_ver
    type: assert.screen
    params:
      contains: "version 0.2"
  - id: readyshell_lst_rshelp
    type: input.sequence
    params:
      keys: [$(keys_upper $'LST "RSHELP"\r')]
      inter_key_delay_s: 0.035
      post_delay_s: 2.0
  - id: assert_readyshell_lst_rshelp
    type: screen.wait_contains
    params:
      text: "BLOCKS"
      wait_timeout_s: 90
      capture_label: easyflash_readyshell_lst_rshelp
  - id: dump_readyshell_state
    type: dump.memory_ranges
    params:
      ranges:
        - { label: shim_c800, start: 0xC800, end: 0xCA00 }
        - { label: readyshell_window, start: 0x1000, end: 0x1800 }
        - { label: readyshell_overlay_meta, start: 0xC760, end: 0xC784 }
YAML

dotnet build "$PROJECT"
dotnet run --project "$PROJECT" -- run-plan --plan "$READYBASIC_PLAN" --close-vice
dotnet run --project "$PROJECT" -- run-plan --plan "$READYSHELL_PLAN" --close-vice
