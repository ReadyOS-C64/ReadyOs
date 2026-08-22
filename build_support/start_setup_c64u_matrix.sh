#!/usr/bin/env bash
set -euo pipefail

READYOS_ROOT="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
LOG="${SETUP_C64U_START_LOG:-/tmp/setup_c64u_matrix_terminal.log}"
STATUS="${SETUP_C64U_START_STATUS:-/tmp/setup_c64u_matrix_terminal.status}"
HOST="${C64U_HOST:-10.0.0.79}"
SPEEDS="${SETUP_C64U_SPEEDS:-1 16 64}"
STREAM_PORT="${SETUP_C64U_STREAM_PORT:-12000}"
OUT_DIR="${SETUP_C64U_OUT_DIR:-$READYOS_ROOT/logs/setup_c64u_matrix}"
rm -f "$LOG" "$STATUS"

command_text="cd '$READYOS_ROOT' && C64U_HOST='$HOST' SETUP_C64U_SPEEDS='$SPEEDS' SETUP_C64U_STREAM_PORT='$STREAM_PORT' SETUP_C64U_OUT_DIR='$OUT_DIR' /bin/bash build_support/run_setup_c64u_matrix.sh >'$LOG' 2>&1; rc=\$?; echo \$rc >'$STATUS'"
osascript -e 'tell application "Terminal" to do script "'"${command_text//\"/\\\"}"'"' >/dev/null
echo "Started Terminal-owned SETUP C64U matrix; status: $STATUS; log: $LOG"
