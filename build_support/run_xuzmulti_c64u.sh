#!/usr/bin/env bash
set -euo pipefail

export XUZDEFLATE_ARCHIVE_MODE=zipmulti
exec /bin/bash "$(dirname "$0")/run_xuzdeflate_c64u.sh" "$@"
