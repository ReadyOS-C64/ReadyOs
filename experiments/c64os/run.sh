#!/bin/bash
# Build/boot the regular ReadyOS D81 with the private IDE64 environment.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
export READYOS_BRIDGE_ENV="${READYOS_BRIDGE_ENV:-$ROOT/../c64os-readyos-experiment}"
export READYOS_BRIDGE_VICE="$(command -v x64sc)"
test -f "$READYOS_BRIDGE_ENV/backup-manifest.json"
test -f "$READYOS_BRIDGE_ENV/reu-cap-install.json"
mkdir -p "$READYOS_BRIDGE_ENV/readyos"
cd "$ROOT"
# VICE is wrapped only to add the IDE64 hardware and redirect media/log writes.
# Normal ReadyOS generation, preservation and launcher boot stay in run.sh.
skip=0
for arg in "$@"; do
    if [ "$arg" = --skipbuild ]; then skip=1; fi
done
args=(--profile precog-d81 --reu-size 16384)
if [ "$skip" = 0 ]; then args+=(--config "$HERE/apps.ini"); fi
PATH="$HERE/vice-wrapper:$PATH" /bin/bash ./run.sh "${args[@]}" "$@"
