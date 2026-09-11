#!/usr/bin/env bash
set -euo pipefail

host="${C64U_HOST:-10.0.0.79}"
output="${1:?output file required}"
curl --fail --silent --show-error --max-time 20 \
  "ftp://anonymous:anonymous%40@${host}/USB1/" > "$output"
