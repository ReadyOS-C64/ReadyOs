#!/usr/bin/env bash
set -euo pipefail

repo="/Users/karlprosserpp/dev/c64projects/readyosprecog"
d81="$repo/Releases/0.2.5/precog-d81/readyos-v0.2.5w-d81.d81"
root="$repo/agentworking/readyos-1kb-shim-skip-bank/c64u-readyirc-w"
status="$root/suite.status"
log="$root/suite.log"

if [[ ! -f "$d81" ]]; then
  printf 'missing DMA-enabled ReadyIRC test image: %s\n' "$d81" >&2
  exit 1
fi
if [[ -e "$root" ]]; then
  printf 'refusing to overwrite retained ReadyIRC artifacts: %s\n' "$root" >&2
  exit 1
fi
mkdir -p "$root"
printf '%s\n' RUNNING > "$status"
: > "$log"

on_exit() {
  rc=$?
  if [[ "$rc" -ne 0 ]]; then
    printf 'FAIL rc=%s\n' "$rc" > "$status"
  fi
}
trap on_exit EXIT

READYIRC_C64U_SKIP_BUILD=1 \
READYIRC_C64U_D81="$d81" \
READYIRC_C64U_OUT_DIR="$root" \
  /bin/bash "$repo/build_support/run_readyirc_c64u_suite.sh" \
    >> "$log" 2>&1

printf '%s\n' PASS > "$status"
printf 'SUITE PASS %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$log"
