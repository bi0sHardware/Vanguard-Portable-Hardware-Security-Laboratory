#!/usr/bin/env bash
# provision_new_badge.sh -- one-shot bring-up for BRAND NEW, never-flashed
# Vanguard badge PCBs. Auto-detects and provisions EVERY badge currently
# plugged in -- one, two, or a dozen -- no need to name ports one at a time.
#
# WHY THIS EXISTS (read before running):
# Every Vanguard badge has a hardware quirk: GPIO45 (MTDI), an ESP32-S3
# strapping pin read at boot to choose the internal flash regulator's
# voltage, shares a net with the joystick DOWN button's 10K pull-up to the
# 3.3V rail. That pull-up holds GPIO45 HIGH on every reset, which tells
# the chip to power its flash at 1.8V -- but the actual flash chip on this
# board (Fudan Micro FM25Q128) is a 3.3V part, so it browns out and never
# responds (JEDEC ID reads as all-Fs, flashing fails outright with no
# useful error). See docs/deployment/troubleshooting.md for the full
# root-cause writeup.
#
# The fix is a ONE-TIME, PERMANENT eFuse burn (espefuse set_flash_voltage
# 3.3V) that forces the flash regulator to 3.3V in hardware, ignoring the
# GPIO45 strap entirely from then on. This only ever needs to happen ONCE
# per physical board, for its whole lifetime -- re-running it on an
# already-fixed board is harmless (espefuse just reports the fuses are
# already set and does nothing), but there's no reason to do it twice.
#
# For EVERY badge, this script runs the full new-badge bring-up in order:
#   1. Burn the VDD_SPI 3.3V eFuse fix (pass --skip-efuse to skip this for
#      the whole batch, e.g. when re-flashing boards already fixed before).
#   2. Write the current firmware build (bootloader + partition table +
#      OTA data + app) -- built ONCE up front, then written to each board.
#   3. Erase the NVS partition (identity, contacts, challenge progress,
#      settings -- partitions.csv offset 0x9000, size 0x5000), so every
#      board comes out with a genuinely blank NVS, never carrying over
#      whoever last tested it.
#   4. Reset the board so it boots straight into the new firmware.
#
# ZERO-TOUCH DESIGN: this badge's native USB-Serial/JTAG has no dedicated
# auto-reset transistor circuit the way a board with a separate
# CP2102/CH340 USB-UART bridge would -- esptool's automatic "toggle
# DTR/RTS to enter bootloader" handshake is unreliable on this hardware,
# not just occasionally flaky. Three design choices remove the need to
# touch anything, including re-running the script, per badge:
#   - Steps 2-4 are chained through raw esptool calls (not `idf.py flash`,
#     which hardcodes a hard-reset after every invocation via sdkconfig)
#     using --before no_reset / --after no_reset between every step but
#     the last. The board only needs to successfully ENTER download mode
#     ONCE per boot cycle -- at the eFuse step if running, else at the
#     flash step -- and stays there through erase + the final reset.
#   - Every entry attempt (the eFuse step, and the first flash command)
#     retries automatically a few times with a short pause before giving
#     up on that board -- a transient handshake failure on first try often
#     succeeds on the next one with no physical intervention at all.
#   - `--watch` mode (see below) means you never re-run the command per
#     badge either: plug one in, it gets provisioned automatically within
#     a few seconds, plug in the next, repeat -- the script keeps running
#     and picks up every new arrival on its own until you stop it.
#
# Usage:
#   ./provision_new_badge.sh --watch          # RECOMMENDED for a batch of new badges:
#                                              # runs forever, auto-provisions every badge
#                                              # the moment it's plugged in. Ctrl+C to stop.
#   ./provision_new_badge.sh                  # one-shot: provision every badge plugged in right now, then exit
#   ./provision_new_badge.sh -y               # skip the eFuse confirmation prompt (needed for --watch,
#                                              # implied automatically by --watch itself)
#   ./provision_new_badge.sh --skip-efuse     # skip the eFuse step for the whole batch
#   ./provision_new_badge.sh /dev/ttyACM0 /dev/ttyACM1   # one-shot, only these specific ports
#
# If a board still fails after the built-in retries, it almost certainly
# means its BOOT/RESET buttons genuinely need a manual press this one time
# (a hardware limitation of this design, not something a script can talk
# around) -- hold BOOT, tap RESET once, release RESET (keep holding BOOT),
# then either re-run this script naming just that port, or (in --watch
# mode) just unplug and replug it once you're holding BOOT, and release
# BOOT once it starts writing. A failure on one board never stops or
# pauses the others.

set -uo pipefail
cd "$(dirname "$0")/../firmware"

IDF_IMAGE="espressif/idf:release-v5.3"

# Separate flags from any explicit port arguments, in any order.
SKIP_EFUSE=false
AUTO_YES=false
WATCH=false
PORTS=()
for arg in "$@"; do
    case "$arg" in
        --skip-efuse) SKIP_EFUSE=true ;;
        -y|--yes) AUTO_YES=true ;;
        --watch) WATCH=true; AUTO_YES=true ;;
        *) PORTS+=("$arg") ;;
    esac
done

if $WATCH && [ ${#PORTS[@]} -gt 0 ]; then
    echo "--watch auto-detects badges as they're plugged in -- don't combine it with explicit ports."
    exit 1
fi

if ! $SKIP_EFUSE && ! $AUTO_YES; then
    echo "This will PERMANENTLY burn the VDD_SPI 3.3V eFuse fix on every badge"
    echo "provisioned below. It only needs to happen once per physical board --"
    echo "pass --skip-efuse if these have all been fixed before, or -y/--watch"
    echo "to skip this prompt."
    read -rp "Continue? [y/N] " confirm
    if [[ ! "$confirm" =~ ^[Yy]$ ]]; then
        echo "Aborted -- no changes made."
        exit 1
    fi
fi

TTY_FLAGS=""
[ -t 0 ] && TTY_FLAGS="-it"

# Runs a docker/esptool(or espefuse) command up to $1 times with a short
# pause between attempts, so a one-off auto-reset handshake failure
# resolves itself without anyone touching the board. Prints progress on
# each retry so a genuinely stuck board is still obvious, not silent.
run_with_retries() {
    local attempts="$1"; shift
    local n=1
    while true; do
        if "$@"; then return 0; fi
        if [ "$n" -ge "$attempts" ]; then return 1; fi
        echo "   (attempt $n/$attempts failed, retrying in 2s...)"
        sleep 2
        n=$((n + 1))
    done
}

# Build once up front -- identical firmware goes to every board. In
# --watch mode this means a badge plugged in five minutes from now still
# gets today's build without rebuilding per arrival.
echo "== Building firmware (shared across all boards) =="
docker run --rm $TTY_FLAGS \
    -v "$(pwd)":/project \
    -w /project \
    "$IDF_IMAGE" \
    idf.py build

# Provisions exactly one port through all four steps. Returns 0/1; never
# exits the script itself, so callers (the one-shot loop, or --watch's
# per-arrival call) can keep going after a single board's failure.
provision_port() {
    local PORT="$1"
    echo
    echo "======================================================================"
    echo "Provisioning $PORT"
    echo "======================================================================"

    if ! $SKIP_EFUSE; then
        echo "-- Step 1/4: Burning VDD_SPI 3.3V eFuse fix on $PORT --"
        if ! run_with_retries 4 docker run --rm $TTY_FLAGS \
            -v "$(pwd)":/project \
            -w /project \
            --device="$PORT" \
            "$IDF_IMAGE" \
            espefuse.py --chip esp32s3 --port "$PORT" --do-not-confirm set_flash_voltage 3.3V; then
            echo "!! eFuse step failed on $PORT after retries -- see the note at the top of this script."
            return 1
        fi
    else
        echo "-- Step 1/4: Skipping eFuse burn (--skip-efuse) --"
    fi

    echo "-- Step 2/4: Writing firmware to $PORT --"
    if ! run_with_retries 4 docker run --rm $TTY_FLAGS \
        -v "$(pwd)":/project \
        -w /project \
        --device="$PORT" \
        "$IDF_IMAGE" \
        python -m esptool --chip esp32s3 --port "$PORT" -b 460800 \
            --before default_reset --after no_reset write_flash \
            --flash_mode dio --flash_freq 80m --flash_size 16MB \
            0x0 build/bootloader/bootloader.bin \
            0x8000 build/partition_table/partition-table.bin \
            0xe000 build/ota_data_initial.bin \
            0x10000 build/vanguard_firmware.bin; then
        echo "!! Flash step failed on $PORT after retries -- see the note at the top of this script."
        return 1
    fi

    # Board is confirmed already in download mode from the flash step
    # above (--after no_reset) -- both remaining steps stay there via
    # --before no_reset, no fresh handshake needed, so no retry wrapper.
    echo "-- Step 3/4: Erasing NVS partition on $PORT (identity/contacts/challenge progress) --"
    docker run --rm $TTY_FLAGS \
        -v "$(pwd)":/project \
        -w /project \
        --device="$PORT" \
        "$IDF_IMAGE" \
        python -m esptool --chip esp32s3 --port "$PORT" \
            --before no_reset --after no_reset erase_region 0x9000 0x5000

    echo "-- Step 4/4: Resetting $PORT into the new firmware --"
    docker run --rm \
        -v "$(pwd)":/project \
        -w /project \
        --device="$PORT" \
        "$IDF_IMAGE" \
        python -m esptool --chip esp32s3 --port "$PORT" \
            --before no_reset --after hard_reset run

    echo "-- $PORT provisioned successfully --"
    return 0
}

if $WATCH; then
    echo
    echo "== Watch mode: plug in badges any time -- each is auto-provisioned =="
    echo "== within a couple seconds of appearing. Press Ctrl+C to stop.      =="
    OK_COUNT=0
    FAIL_COUNT=0
    # Tracked by the USB serial-number-derived /dev/serial/by-id name, not
    # the /dev/ttyACMn path -- that path's number depends on plug order
    # and can even change for the SAME physical board across the reset
    # cycles provisioning itself triggers, which would otherwise either
    # reprocess one board repeatedly or silently skip another. The by-id
    # name embeds the chip's own fixed MAC-derived serial, so it's stable
    # for a given physical board all session.
    declare -A HANDLED
    while true; do
        shopt -s nullglob
        for idlink in /dev/serial/by-id/*; do
            id_name=$(basename "$idlink")
            [ -n "${HANDLED[$id_name]:-}" ] && continue
            HANDLED["$id_name"]=1
            real_port=$(readlink -f "$idlink")
            echo
            echo ">> New badge detected ($id_name) on $real_port"
            if provision_port "$real_port"; then
                OK_COUNT=$((OK_COUNT + 1))
            else
                FAIL_COUNT=$((FAIL_COUNT + 1))
            fi
            echo ">> Running total: $OK_COUNT OK, $FAIL_COUNT failed. Waiting for the next badge..."
        done
        shopt -u nullglob
        sleep 2
    done
    # Unreachable (loop only exits via Ctrl+C), kept for clarity if that
    # ever changes.
    exit 0
fi

# One-shot mode: provision whatever's plugged in right now and exit.
if [ ${#PORTS[@]} -eq 0 ]; then
    shopt -s nullglob
    PORTS=(/dev/ttyACM* /dev/ttyUSB*)
    shopt -u nullglob
fi

if [ ${#PORTS[@]} -eq 0 ]; then
    echo "No badges found (no /dev/ttyACM* or /dev/ttyUSB* devices) -- plug one in and retry, or use --watch."
    exit 1
fi

echo "Found ${#PORTS[@]} badge(s): ${PORTS[*]}"

FAILED=()
OK=()
for PORT in "${PORTS[@]}"; do
    if provision_port "$PORT"; then
        OK+=("$PORT")
    else
        FAILED+=("$PORT")
    fi
done

echo
echo "======================================================================"
echo "Done. ${#OK[@]}/${#PORTS[@]} board(s) provisioned successfully."
[ ${#OK[@]} -gt 0 ] && printf '  OK:     %s\n' "${OK[@]}"
[ ${#FAILED[@]} -gt 0 ] && printf '  FAILED: %s\n' "${FAILED[@]}"
[ ${#FAILED[@]} -eq 0 ]
