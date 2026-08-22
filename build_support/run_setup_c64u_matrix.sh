#!/usr/bin/env bash
set -euo pipefail

READYOS_ROOT="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
HARNESS="${VICE_TASKS_ROOT:-$READYOS_ROOT/../agenticdevharness/tools/vice_tasks_dotnet}"
HOST="${C64U_HOST:-10.0.0.79}"
SPEEDS="${SETUP_C64U_SPEEDS:-1 16 64}"
STREAM_PORT="${SETUP_C64U_STREAM_PORT:-12000}"
SOURCE="${SETUP_C64U_D81:-$READYOS_ROOT/Releases/0.5/precog-ultimate/readyos-v0.5-ultimate.d81}"
OUT="${SETUP_C64U_OUT_DIR:-$READYOS_ROOT/logs/setup_c64u_matrix}"
RUN_TAG="$(date +%Y%m%d-%H%M%S)-$$"

case "$SOURCE" in
  "$READYOS_ROOT"/*) ;;
  *) echo "SETUP_C64U_D81 must be inside the ReadyOS workspace" >&2; exit 64 ;;
esac
test -f "$SOURCE"
mkdir -p "$OUT" "$READYOS_ROOT/build/setup_ultimate"

for speed in $SPEEDS; do
  case "$speed" in 1|16|64) ;; *) echo "SETUP speeds must be 1, 16, or 64" >&2; exit 64 ;; esac
  leaf="TARGET-${RUN_TAG}-${speed}MHZ.D81"
  remote_dir="USB1/READYOS_SETUP_TEST/${RUN_TAG}/${speed}MHZ"
  remote_image="${remote_dir}/${leaf}"
  saved_path="/${remote_image}"
  path_keys="$(python3 -c 'import sys; print(",".join(str(value) for value in sys.argv[1].encode("ascii")))' "$saved_path")"
  invalid_path="/${remote_dir}/MISSING.D81"
  invalid_path_keys="$(python3 -c 'import sys; print(",".join(str(value) for value in sys.argv[1].encode("ascii")))' "$invalid_path")"
  fixture="$READYOS_ROOT/build/setup_ultimate/${leaf}"
  plan="$OUT/${speed}mhz.yaml"
  log="$OUT/${speed}mhz.log"

  python3 "$READYOS_ROOT/build_support/prepare_setup_fixture.py" \
    --source "$SOURCE" --output "$fixture" --saved-path ""
  sed \
    -e "s#host: 10.0.0.79#host: ${HOST}#g" \
    -e "s#stream_port: 11000#stream_port: ${STREAM_PORT}#g" \
    -e "s#remote_root: USB1/READYOS_SETUP_TEST/RUN#remote_root: ${remote_dir}#g" \
    -e "s#prg: bin/setup.prg#prg: ${READYOS_ROOT}/bin/setup.prg#g" \
    -e "s#build/setup_ultimate/fixture.d81#${fixture}#g" \
    -e "s#USB1/READYOS_SETUP_TEST/RUN/fixture.d81#${remote_image}#g" \
    -e "s#mhz: 16#mhz: ${speed}#g" \
    -e "s#SETUP_INVALID_PATH_KEYS#${invalid_path_keys}#g" \
    -e "s#SETUP_PATH_KEYS#${path_keys}#g" \
    "$READYOS_ROOT/build_support/setup_ultimate.generated.yaml" > "$plan"

  (cd "$HARNESS" && ULTIMATE_ASSUME_MOUNTED=1 /usr/local/share/dotnet/dotnet run \
    --project src/ViceTasks.Binary/ViceTasks.Binary.csproj \
    -- run-ultimate-plan --plan "$plan" --no-tui) >"$log" 2>&1

  downloaded="$OUT/${speed}mhz-result.d81"
  curl --fail --silent --show-error --max-time 120 \
    "ftp://anonymous:anonymous%40@${HOST}/${remote_image}" -o "$downloaded"
  config="$OUT/${speed}mhz-apps.cfg"
  c1541 "$downloaded" -read apps.cfg,s "$config" >/dev/null
  python3 - "$config" "$saved_path" <<'PY'
import pathlib, sys
data = pathlib.Path(sys.argv[1]).read_bytes().upper()
path = sys.argv[2].encode("ascii").upper()
assert b"DMA_LOADING=1" in data
assert b"C64U_IMAGE_PATH=" + path in data
PY
  listing="$(c1541 "$downloaded" -list | tr '[:upper:]' '[:lower:]')"
  if ! grep -Eq '"apps\.cfg"[[:space:]]+seq' <<<"$listing"; then
    echo "apps.cfg did not remain a SEQ file in $downloaded" >&2
    exit 5
  fi
  if grep -Eq 'rdyset\.(seq|bak)' <<<"$listing"; then
    echo "staging file remained in $downloaded" >&2
    exit 5
  fi
done

echo "SETUP C64U matrix passed at: $SPEEDS" | tee "$OUT/result.txt"
