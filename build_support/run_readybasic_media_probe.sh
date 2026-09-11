#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
if [ "${READYBASIC_SKIP_BUILD:-0}" != 1 ] && [ "${READYBASIC_GENERATE_PLAN_ONLY:-0}" != 1 ]; then
    /bin/bash ./run.sh --profile precog-d81 --run-first readybasic --build-only
fi
exec python3 build_support/run_readybasic_media_probe.py
