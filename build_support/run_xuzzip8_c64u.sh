#!/usr/bin/env bash
set -euo pipefail

export XUZDEFLATE_ARCHIVE_MODE=zip8
exec /bin/bash "$(dirname "$0")/run_xuzdeflate_c64u.sh" "$@"
