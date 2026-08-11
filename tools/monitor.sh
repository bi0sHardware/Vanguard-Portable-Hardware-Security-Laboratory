#!/usr/bin/env bash
# Opens idf.py monitor via Docker. Usage: ./monitor.sh /dev/ttyACM0
set -euo pipefail
cd "$(dirname "$0")/../firmware"

IDF_IMAGE="espressif/idf:release-v5.3"
PORT="${1:?Usage: ./monitor.sh /dev/ttyACMx}"

TTY_FLAGS=""
[ -t 0 ] && TTY_FLAGS="-it"

docker run --rm $TTY_FLAGS \
    -v "$(pwd)":/project \
    -w /project \
    --device="$PORT" \
    "$IDF_IMAGE" \
    idf.py -p "$PORT" monitor
