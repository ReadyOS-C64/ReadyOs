#!/usr/bin/env bash
set -u

log="${1:-/tmp/c64u_configs_probe.log}"
status="${2:-/tmp/c64u_configs_probe.status}"

rm -f "$log" "$status"
date > "$log"

query() {
  local label="$1"
  local url="$2"
  {
    echo
    echo "### $label"
    curl -sS --max-time 30 "$url"
    echo
  } >> "$log" 2>&1
}

query "iec all" "http://10.0.0.79/v1/configs/iec*/*"
query "drive paths" "http://10.0.0.79/v1/configs/*drive*/*path*"
query "drive bus ids" "http://10.0.0.79/v1/configs/*drive*/*bus*"
query "all configs" "http://10.0.0.79/v1/configs"

echo done > "$status"
