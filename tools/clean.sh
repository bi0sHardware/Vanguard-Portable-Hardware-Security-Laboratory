#!/usr/bin/env bash
# Cleans the build directory via Docker.
set -euo pipefail
cd "$(dirname "$0")/../firmware"

IDF_IMAGE="espressif/idf:release-v5.3"

TTY_FLAGS=""
[ -t 0 ] && TTY_FLAGS="-it"

docker run --rm $TTY_FLAGS \
    -v "$(pwd)":/project \
    -w /project \
    "$IDF_IMAGE" \
    idf.py fullclean
