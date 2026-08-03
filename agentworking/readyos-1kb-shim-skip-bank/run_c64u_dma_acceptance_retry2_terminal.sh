#!/usr/bin/env bash
set -euo pipefail

repo="/Users/karlprosserpp/dev/c64projects/readyosprecog"
d81="$repo/Releases/0.2.5/precog-d81/readyos-v0.2.5u-d81.d81"
remote_name="r1k250802a.d81"
root="$repo/agentworking/readyos-1kb-shim-skip-bank/c64u-dma-retry2"
suite_status="$root/suite.status"
suite_log="$root/suite.log"

mkdir -p "$root"
if [[ -e "$suite_status" || -e "$suite_log" ]]; then
  printf 'refusing to overwrite retained retry2 suite artifacts: %s\n' "$root" >&2
  exit 1
fi
printf '%s\n' RUNNING > "$suite_status"
: > "$suite_log"

on_exit() {
  rc=$?
  if [[ "$rc" -ne 0 ]]; then
    printf 'FAIL rc=%s\n' "$rc" > "$suite_status"
  fi
}
trap on_exit EXIT

run_action() {
  action="$1"
  expected="$2"
  out="$root/$action"
  if [[ -e "$out" ]]; then
    printf 'refusing to overwrite retained action artifacts: %s\n' "$out" >&2
    return 1
  fi
  mkdir -p "$out"
  printf 'START %s %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$action" | tee -a "$suite_log"
  C64U_HOST="${C64U_HOST:-10.0.0.79}" \
  C64U_REMOTE_DIR=USB1 \
  C64U_SKIP_UPLOAD=1 \
  C64U_MACHINE_REBOOT=1 \
  READYOS_CLEAR_REU=1 \
  READYOS_CLEAR_REU_BANKS=256 \
  READYOS_EXPECT_DMA=1 \
  READYOS_EXPECT_DMA_READY=1 \
  READYOS_FAIL_ON_DISK_LOADING_WHEN_DMA=1 \
  READYOS_BOOT_INITIAL_WAIT_S=90 \
  READYOS_QUIET_AFTER_MANIFEST_OPEN_S=120 \
  READYOS_MANIFEST_DIALOG_WAIT_S=240 \
  READYOS_BOOT_ACTION="$action" \
    /bin/bash "$repo/build_support/run_readyos_boot_c64u_rest.sh" \
      "$d81" "$remote_name" "$out" >> "$suite_log" 2>&1
  actual="$(tr -d '\r\n' < "$out/status")"
  if [[ "$actual" != "$expected" ]]; then
    printf 'STATUS MISMATCH %s expected=%s actual=%s\n' \
      "$action" "$expected" "$actual" | tee -a "$suite_log"
    return 1
  fi
  printf 'PASS %s %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$action" | tee -a "$suite_log"
}

run_action manifest-sidetris READYOS_MANIFEST_SIDETRIS_PASS
run_action readybasic-load-selected READYOS_READYBASIC_LOAD_SELECTED_PASS

printf '%s\n' PASS > "$suite_status"
printf 'SUITE PASS %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" | tee -a "$suite_log"
