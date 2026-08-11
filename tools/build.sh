#!/usr/bin/env bash
# Builds the Vanguard firmware entirely inside the official espressif/idf
# Docker image -- no local ESP-IDF install required.
#
# Usage: ./build.sh [extra idf.py build args]
set -euo pipefail
cd "$(dirname "$0")/../firmware"

IDF_IMAGE="espressif/idf:release-v5.3"

TTY_FLAGS=""
[ -t 0 ] && TTY_FLAGS="-it"

docker run --rm $TTY_FLAGS \
    -v "$(pwd)":/project \
    -w /project \
    "$IDF_IMAGE" \
    idf.py build "$@"
