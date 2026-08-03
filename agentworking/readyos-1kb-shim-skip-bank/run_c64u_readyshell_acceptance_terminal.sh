#!/usr/bin/env bash
set -euo pipefail

repo="/Users/karlprosserpp/dev/c64projects/readyosprecog"
d81="$repo/Releases/0.2.5/precog-d81/readyos-v0.2.5u-d81.d81"
root="$repo/agentworking/readyos-1kb-shim-skip-bank/c64u-readyshell"
status="$root/suite.status"

mkdir -p "$root"
printf '%s\n' RUNNING > "$status"

on_exit() {
  rc=$?
  if [ "$rc" -ne 0 ]; then
    printf 'FAIL rc=%s\n' "$rc" > "$status"
  fi
}
trap on_exit EXIT

# The prior generic acceptance leaves the uniquely named D81 mounted. Clear all
# physical REU banks without replacing or remounting that image.
C64U_HOST="${C64U_HOST:-10.0.0.79}" \
C64U_SKIP_UPLOAD=1 \
C64U_SKIP_CONFIG=1 \
READYOS_CLEAR_REU=1 \
READYOS_CLEAR_REU_BANKS=256 \
READYOS_CLEAR_REU_ONLY=1 \
  /bin/bash "$repo/build_support/run_readyos_boot_c64u_rest.sh" \
    "$d81" r1k250802a.d81 "$root/reu-clear" > "$root/reu-clear.log" 2>&1

READYSHELL_C64U_SKIP_BUILD=1 \
READYSHELL_C64U_D81="$d81" \
C64U_HOST="${C64U_HOST:-10.0.0.79}" \
C64U_REMOTE_ROOT=USB1 \
C64U_REMOTE_D81=r1k250802a.d81 \
C64U_IMAGE_PATH_CONFIG=/usb1/r1k250802a.d81 \
READYSHELL_C64U_PREMOUNT_REST=0 \
READYSHELL_C64U_ASSUME_MOUNTED=1 \
READYSHELL_C64U_PLAN="$root/readyshell_cross_app_resume_c64u.generated.yaml" \
READYSHELL_C64U_CONFIG_SRC="$root/readyshell_cross_app_resume_c64u.generated.ini" \
  /bin/bash "$repo/build_support/run_readyshell_cross_app_resume_c64u.sh" \
    > "$root/run.log" 2>&1

printf '%s\n' PASS > "$status"
