#!/usr/bin/env bash
set +e

cd /Users/karlprosserpp/dev/c64projects/readyosprecog || exit 90

C64U_HOST=10.0.0.79 \
C64U_REMOTE_DIR=USB1 \
READYOS_CLEAR_REU=1 \
READYOS_EXPECT_DMA_READY=1 \
READYOS_BOOT_INITIAL_WAIT_S=90 \
/bin/bash build_support/run_readyos_boot_c64u_rest.sh \
  Releases/0.2.5/precog-d81/readyos-v0.2.5e-d81.d81 \
  readyos-dma-20260808-1.d81 \
  agentworking/c64u-manual-dma-20260808/deploy \
  > agentworking/c64u-manual-dma-20260808/terminal.log 2>&1

exit_code=$?
printf '%s\n' "$exit_code" > agentworking/c64u-manual-dma-20260808/terminal-exit.txt
exit "$exit_code"
