#!/usr/bin/env bash
set -euo pipefail

READYOS_ROOT="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
LOG="${LAUNCHER_SETUP_START_LOG:-/tmp/launcher_setup_states_terminal.log}"
STATUS="${LAUNCHER_SETUP_START_STATUS:-/tmp/launcher_setup_states_terminal.status}"
HOST="${C64U_HOST:-10.0.0.79}"
STREAM_PORT="${LAUNCHER_SETUP_STREAM_PORT:-12000}"
OUT_DIR="${LAUNCHER_SETUP_OUT_DIR:-$READYOS_ROOT/logs/launcher_setup_states_c64u}"
rm -f "$LOG" "$STATUS"

command_text="cd '$READYOS_ROOT' && C64U_HOST='$HOST' LAUNCHER_SETUP_STREAM_PORT='$STREAM_PORT' LAUNCHER_SETUP_OUT_DIR='$OUT_DIR' /bin/bash build_support/run_launcher_setup_states_c64u.sh >'$LOG' 2>&1; rc=\$?; echo \$rc >'$STATUS'"
osascript -e 'tell application "Terminal" to do script "'"${command_text//\"/\\\"}"'"' >/dev/null
echo "Started Terminal-owned launcher SETUP-state test; status: $STATUS; log: $LOG"
