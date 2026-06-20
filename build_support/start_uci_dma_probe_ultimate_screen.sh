#!/usr/bin/env bash
set -euo pipefail

repo="/Users/karlprosserpp/dev/c64projects/readyosprecog"
stamp="$(date +%Y%m%d_%H%M%S)"
session="${UCI_DMA_SCREEN_SESSION:-uci_dma_probe_${stamp}}"
log="${UCI_DMA_LOG:-/tmp/uci_dma_probe_ultimate_${stamp}.log}"
status="${UCI_DMA_STATUS:-/tmp/uci_dma_probe_ultimate_${stamp}.status}"
host="${C64U_HOST:-10.0.0.79}"
remote_dir="${C64U_REMOTE_DIR:-}"

if [[ -n "$remote_dir" ]]; then
  case "$remote_dir" in
    USB0|USB1) ;;
    *)
      echo "C64U_REMOTE_DIR must be USB0 or USB1" >&2
      exit 64
      ;;
  esac
fi

rm -f "$log" "$status"

screen -dmS "$session" /bin/bash -lc \
  "cd '$repo' && C64U_HOST='$host' C64U_REMOTE_DIR='$remote_dir' /bin/bash build_support/run_uci_dma_probe_ultimate.sh '$log' '$status'"

echo "session=$session"
echo "log=$log"
echo "status=$status"
echo "host=$host"
echo "discover=${C64U_DISCOVER:-0}"
if [[ -n "$remote_dir" ]]; then
  echo "remote_dir=$remote_dir"
else
  echo "remote_dir=auto"
fi
