#!/usr/bin/env bash
set -euo pipefail

cd /Users/karlprosserpp/dev/c64projects/readyosprecog
out=agentworking/c64u-latest-dma-0.2.5z-fresh/predeploy_unload
mkdir -p "$out"

put_key() {
  local key="$1"
  curl --fail --silent --show-error --max-time 10 -X PUT \
    "http://10.0.0.79/v1/machine:writemem?address=00C5&data=0000" >/dev/null
  curl --fail --silent --show-error --max-time 10 -X PUT \
    "http://10.0.0.79/v1/machine:writemem?address=0277&data=${key}" >/dev/null
  curl --fail --silent --show-error --max-time 10 -X PUT \
    "http://10.0.0.79/v1/machine:writemem?address=00C6&data=01" >/dev/null
}

put_key 02
sleep 10
put_key 88
sleep 3
C64U_HOST=10.0.0.79 /bin/bash build_support/capture_uci_dma_probe_c64u.sh "$out"
printf '0\n' > agentworking/c64u-latest-dma-0.2.5z-fresh/predeploy_unload.exit
