#!/bin/bash
#
# Ready OS Build Script
# Builds all components and creates D71 disk image
#

set -e
cd "$(dirname "$0")"

echo "=== Ready OS Build Script ==="
echo ""

make clean
make

echo ""
echo "To run: ./run.sh"
