#!/usr/bin/env bash
set -euo pipefail

repo="/Users/karlprosserpp/dev/c64projects/readyosprecog"
d81="$repo/Releases/0.2.5/precog-d81/readyos-v0.2.5u-d81.d81"
remote_name="r1k250802a.d81"
root="$repo/agentworking/readyos-1kb-shim-skip-bank/c64u-readybasic-retry3"
status="$root/suite.status"
log="$root/suite.log"
out="$root/readybasic-load-selected"

mkdir -p "$root"
if [[ -e "$status" || -e "$log" || -e "$out" ]]; then
  printf 'refusing to overwrite retained retry3 artifacts: %s\n' "$root" >&2
  exit 1
fi
printf '%s\n' RUNNING > "$status"
: > "$log"
mkdir -p "$out"

on_exit() {
  rc=$?
  if [[ "$rc" -ne 0 ]]; then
    printf 'FAIL rc=%s\n' "$rc" > "$status"
  fi
}
trap on_exit EXIT

C64U_HOST="${C64U_HOST:-10.0.0.79}" \
C64U_REMOTE_DIR=USB1 \
C64U_SKIP_UPLOAD=1 \
C64U_MACHINE_REBOOT=1 \
READYOS_CLEAR_REU=1 \
READYOS_CLEAR_REU_BANKS=256 \
READYOS_EXPECT_DMA=1 \
READYOS_EXPECT_DMA_READY=1 \
READYOS_FAIL_ON_DISK_LOADING_WHEN_DMA=1 \
READYOS_BOOT_INITIAL_WAIT_S=120 \
READYOS_BOOT_ACTION=readybasic-load-selected \
  /bin/bash "$repo/build_support/run_readyos_boot_c64u_rest.sh" \
    "$d81" "$remote_name" "$out" >> "$log" 2>&1

actual="$(tr -d '\r\n' < "$out/status")"
if [[ "$actual" != READYOS_READYBASIC_LOAD_SELECTED_PASS ]]; then
  printf 'STATUS MISMATCH expected=%s actual=%s\n' \
    READYOS_READYBASIC_LOAD_SELECTED_PASS "$actual" >> "$log"
  exit 1
fi

printf '%s\n' PASS > "$status"
printf 'SUITE PASS %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$log"
