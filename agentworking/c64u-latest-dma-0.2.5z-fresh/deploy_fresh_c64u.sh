#!/usr/bin/env bash
set -uo pipefail

cd /Users/karlprosserpp/dev/c64projects/readyosprecog
out=agentworking/c64u-latest-dma-0.2.5z-fresh/deploy_b_editor
local_image=Releases/0.2.5/precog-d81/readyos-v0.2.5b-d81.d81
remote_name=readyos-v0.2.5z-20260803a.d81

env \
  C64U_HOST=10.0.0.79 \
  C64U_REMOTE_DIR=USB1 \
  C64U_CONNECT_WAIT_S=300 \
  C64U_MACHINE_REBOOT=1 \
  READYOS_EXPECT_DMA_READY=1 \
  READYOS_BOOT_ACTION=editor-direct \
  READYOS_QUIET_AFTER_APP_ENTER_S=60 \
  READYOS_BOOT_INITIAL_WAIT_S=90 \
  /bin/bash build_support/run_readyos_boot_c64u_rest.sh \
  "$local_image" "$remote_name" "$out"
rc=$?
printf '%s\n' "$rc" > agentworking/c64u-latest-dma-0.2.5z-fresh/deploy_b_editor.terminal.exit
exit "$rc"
