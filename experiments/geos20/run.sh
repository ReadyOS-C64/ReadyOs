#!/bin/bash
# Regular ReadyOS build and launcher boot; private GEOS media on drive 9.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
export GEOS_BRIDGE_PROJECT="$ROOT/../GEOS20"
export GEOS_BRIDGE_VICE="$(command -v x64sc)"
cd "$ROOT"
PATH="$HERE/vice-wrapper:$PATH" /bin/bash ./run.sh --profile precog-geos20 --reu-size 16384 --skipbuild "$@"
