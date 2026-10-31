#!/bin/bash
# Regular ReadyOS build and launcher boot; private GDOS media on drive 9.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
export GDOS_BRIDGE_PROJECT="$ROOT/../gdos64-bridge"
export GDOS_BRIDGE_VICE="$(command -v x64sc)"
cd "$ROOT"
PATH="$HERE/vice-wrapper:$PATH" /bin/bash ./run.sh --profile precog-d81 --reu-size 16384 --skipbuild "$@"
