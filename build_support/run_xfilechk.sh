#!/bin/bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

configure_vice_env() {
    if [ -z "${GSETTINGS_SCHEMA_DIR:-}" ] && [ -f "/opt/homebrew/share/glib-2.0/schemas/gschemas.compiled" ]; then
        export GSETTINGS_SCHEMA_DIR="/opt/homebrew/share/glib-2.0/schemas"
    fi

    if [ -d "/opt/homebrew/share" ]; then
        if [ -n "${XDG_DATA_DIRS:-}" ]; then
            case ":$XDG_DATA_DIRS:" in
                *":/opt/homebrew/share:"*) ;;
                *) export XDG_DATA_DIRS="/opt/homebrew/share:$XDG_DATA_DIRS" ;;
            esac
        else
            export XDG_DATA_DIRS="/opt/homebrew/share"
        fi
    fi
}

configure_vice_env

HARNESS_DIR_REL="artifacts/dev_harness/xfilechk"
HARNESS_BOOT_PRG="${HARNESS_DIR_REL}/xfilechk_boot.prg"
HARNESS_PRG="${HARNESS_DIR_REL}/xfilechk.prg"
HARNESS_DISK_8="${HARNESS_DIR_REL}/xfilechk.d71"
HARNESS_DISK_9="${HARNESS_DIR_REL}/xfilechk_2.d71"
VICE_LOG_FILE="logs/xfilechk_vice.log"

SKIP_BUILD=0
CASE_ID="${XFILECHK_CASE:-0}"
WARP=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --skipbuild)
            SKIP_BUILD=1
            ;;
        --case)
            shift
            CASE_ID="${1:-0}"
            ;;
        --warp)
            WARP=1
            ;;
        -h|--help)
            echo "Usage: build_support/run_xfilechk.sh [--skipbuild] [--case N] [--warp]"
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
    shift
done

if command -v x64sc >/dev/null 2>&1; then
    VICE="x64sc"
elif command -v x64 >/dev/null 2>&1; then
    VICE="x64"
else
    echo "Error: VICE emulator not found (tried x64sc, x64)" >&2
    exit 1
fi

mkdir -p logs

if [ "$SKIP_BUILD" -eq 0 ]; then
    echo "Building xfilechk artifacts (case=$CASE_ID)..."
    make -B \
        "$HARNESS_BOOT_PRG" \
        "$HARNESS_PRG" \
        "$HARNESS_DISK_8" \
        "$HARNESS_DISK_9" \
        XFILECHK_CASE="$CASE_ID"
else
    echo "Skipping build (--skipbuild)"
fi

for path in "$HARNESS_BOOT_PRG" "$HARNESS_PRG" "$HARNESS_DISK_8" "$HARNESS_DISK_9"; do
    if [ ! -f "$path" ]; then
        echo "Missing required harness artifact: $path" >&2
        exit 1
    fi
done

VICE_OPTS=(
    -logfile "$VICE_LOG_FILE"
    -drive8type 1571
    -drive8truedrive
    -devicebackend8 0
    +busdevice8
    -drive9type 1571
    -drive9truedrive
    -devicebackend9 0
    +busdevice9
)

if [ "$WARP" -eq 1 ]; then
    VICE_OPTS+=(-warp)
fi

echo ""
echo "=== XFILECHK ==="
echo "VICE: $VICE"
echo "Target: $HARNESS_BOOT_PRG"
echo "Drive 8: $HARNESS_DISK_8"
echo "Drive 9: $HARNESS_DISK_9"
echo "Case: $CASE_ID"
echo ""

exec "$VICE" "${VICE_OPTS[@]}" \
    -8 "$HARNESS_DISK_8" \
    -9 "$HARNESS_DISK_9" \
    -autostartprgmode 1 \
    "$HARNESS_BOOT_PRG"
