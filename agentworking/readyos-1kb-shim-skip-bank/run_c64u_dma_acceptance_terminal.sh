#!/usr/bin/env bash
set -euo pipefail

repo="/Users/karlprosserpp/dev/c64projects/readyosprecog"
d81="$repo/Releases/0.2.5/precog-d81/readyos-v0.2.5u-d81.d81"
remote_name="r1k250802a.d81"
root="$repo/agentworking/readyos-1kb-shim-skip-bank/c64u-dma"
suite_status="$root/suite.status"
suite_log="$root/suite.log"

mkdir -p "$root"
: > "$suite_log"
printf '%s\n' RUNNING > "$suite_status"

on_exit() {
  rc=$?
  if [ "$rc" -ne 0 ]; then
    printf 'FAIL rc=%s\n' "$rc" > "$suite_status"
  fi
}
trap on_exit EXIT

run_action() {
  action="$1"
  expected="$2"
  skip_upload="$3"
  out="$root/$action"
  rm -rf "$out"
  mkdir -p "$out"
  printf 'START %s %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$action" | tee -a "$suite_log"
  C64U_HOST="${C64U_HOST:-10.0.0.79}" \
  C64U_REMOTE_DIR=USB1 \
  C64U_SKIP_UPLOAD="$skip_upload" \
  C64U_MACHINE_REBOOT=1 \
  READYOS_CLEAR_REU=1 \
  READYOS_CLEAR_REU_BANKS=256 \
  READYOS_EXPECT_DMA=1 \
  READYOS_EXPECT_DMA_READY=1 \
  READYOS_FAIL_ON_DISK_LOADING_WHEN_DMA=1 \
  READYOS_BOOT_INITIAL_WAIT_S=90 \
  READYOS_BOOT_ACTION="$action" \
    /bin/bash "$repo/build_support/run_readyos_boot_c64u_rest.sh" \
      "$d81" "$remote_name" "$out" >> "$suite_log" 2>&1
  actual="$(tr -d '\r\n' < "$out/status")"
  if [ "$actual" != "$expected" ]; then
    printf 'STATUS MISMATCH %s expected=%s actual=%s\n' \
      "$action" "$expected" "$actual" | tee -a "$suite_log"
    return 1
  fi
  printf 'PASS %s %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$action" | tee -a "$suite_log"
}

run_action editor-direct-dma-return READYOS_EDITOR_DIRECT_DMA_RETURN_PASS 0
run_action editor-load-selected READYOS_EDITOR_LOAD_SELECTED_PASS 1
run_action loadall-readyshell-overlay-smoke READYOS_LOADALL_READYSHELL_OVERLAY_PASS 1
run_action manifest-sidetris READYOS_MANIFEST_SIDETRIS_PASS 1
run_action readybasic-load-selected READYOS_READYBASIC_LOAD_SELECTED_PASS 1

printf '%s\n' PASS > "$suite_status"
printf 'SUITE PASS %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" | tee -a "$suite_log"
