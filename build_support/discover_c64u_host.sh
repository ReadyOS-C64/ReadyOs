#!/usr/bin/env bash
set -uo pipefail

log="${C64U_DISCOVERY_LOG:-/tmp/c64u_discovery.log}"
password="${C64U_PASSWORD:-}"
scan_subnet="${C64U_SCAN_SUBNET:-}"

rm -f "$log"

add_candidate() {
  local value="$1"
  value="${value#http://}"
  value="${value#https://}"
  value="${value%%/*}"
  [[ -n "$value" ]] || return 0
  printf '%s\n' "$value" >> /tmp/c64u_candidates.$$
}

probe_http() {
  local host="$1"
  local route="$2"
  local output
  if [[ -n "$password" ]]; then
    output="$(curl --connect-timeout 1 --max-time 2 --silent --show-error \
      -H "X-Password: ${password}" "http://${host}/v1/${route}" 2>&1)"
  else
    output="$(curl --connect-timeout 1 --max-time 2 --silent --show-error \
      "http://${host}/v1/${route}" 2>&1)"
  fi
  local rc=$?
  {
    echo "  /v1/${route} rc=${rc}"
    printf '%s\n' "$output" | sed 's/^/    /'
  } >> "$log"
  [[ "$rc" -eq 0 ]] || return 1
  printf '%s' "$output" | grep -Eiq 'ultimate|1541|firmware|version|machine|drives|errors|\{'
}

probe_host() {
  local host="$1"
  echo "== ${host} ==" >> "$log"
  if probe_http "$host" "version"; then
    printf '%s\n' "$host"
    return 0
  fi
  if probe_http "$host" "info"; then
    printf '%s\n' "$host"
    return 0
  fi
  return 1
}

date > "$log"
echo "C64U discovery" >> "$log"

: > /tmp/c64u_candidates.$$
host_list="${C64U_HOSTS//,/ }"
for raw in ${host_list:-}; do
  add_candidate "$raw"
done
if [[ -n "${C64U_HOST:-}" && "${C64U_HOST}" != "auto" ]]; then
  add_candidate "$C64U_HOST"
fi
add_candidate "10.0.0.79"
arp -an 2>/dev/null |
  sed -n 's/.*(\([0-9][0-9.]*\)).*/\1/p' |
  while read -r ip; do
    case "$ip" in
      10.0.0.0|10.0.0.255) ;;
      10.0.0.*) add_candidate "$ip" ;;
    esac
  done

if [[ -n "$scan_subnet" ]]; then
  base="${scan_subnet%.}"
  for last in $(seq 1 254); do
    add_candidate "${base}.${last}"
  done
fi

sort -u /tmp/c64u_candidates.$$ > /tmp/c64u_candidates_sorted.$$
echo "candidates:" >> "$log"
sed 's/^/  /' /tmp/c64u_candidates_sorted.$$ >> "$log"

found=""
while read -r candidate; do
  if found="$(probe_host "$candidate")"; then
    echo "selected: ${found}" >> "$log"
    printf '%s\n' "$found"
    rm -f /tmp/c64u_candidates.$$ /tmp/c64u_candidates_sorted.$$
    exit 0
  fi
done < /tmp/c64u_candidates_sorted.$$

echo "selected: none" >> "$log"
rm -f /tmp/c64u_candidates.$$ /tmp/c64u_candidates_sorted.$$
exit 1
