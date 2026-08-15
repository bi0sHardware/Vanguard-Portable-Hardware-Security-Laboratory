# Satellite Simulator

Levels 2 through 4 of the challenge arc depend on a companion badge
running a satellite-simulator build variant, built from the same source
tree as the player firmware via a CMake build-time flag
(`-DVANGUARD_SATELLITE_SIM=1`) rather than a separate codebase — so both
sides of the RF protocol cannot drift apart from each other.

## Building and flashing the satellite variant

```bash
docker run --rm -v "$PWD/firmware":/project -w /project espressif/idf:release-v5.3 \
    idf.py -DVANGUARD_SATELLITE_SIM=1 -B build_sat build

docker run --rm -v "$PWD/firmware":/project -w /project --device=/dev/ttyACM0 \
    espressif/idf:release-v5.3 idf.py -B build_sat -p /dev/ttyACM0 flash
```

## Deployment

Flash this build to one spare badge (or any board carrying the same LoRa
module) and power it centrally in the play area for the duration of the
event. It broadcasts telemetry, authenticates uplinks, and streams the
Level 4 payload on a continuous loop.

The badge's own TFT shows a live status screen — current mode, loop
position, uplink authentications, and payload streaming state, plus a
short event log — so a laptop tethered to the serial console isn't
required to see that it's alive. Serial output is still available in
parallel for anyone who wants it.

## Operating modes

`JoyLeft` / `JoyRight` on the satellite badge itself cycle through:

- **AUTO** (default) — broadcasts the Packet 1–10 telemetry loop and
  listens for uplinks at the same time, exactly as Levels 2–4 require.
  This is the only mode that's correct for an actual event; it's also
  what the badge boots into, so an operator who never touches the
  joystick gets the right behavior automatically.
- **TX ONLY** — broadcasts the loop (and payload, if already
  authenticated) but never listens. Useful for confirming badges can
  receive the satellite's signal in isolation, separate from uplink
  reception.
- **RX ONLY** — listens for uplinks (and still authenticates them) but
  never transmits. Useful for confirming an uplink is reaching the
  satellite without the telemetry loop or payload stream contending for
  airtime.

Switching modes only changes which of transmit/receive this build calls
each tick — the underlying protocol logic, timing, and authentication are
identical in every mode.

## Radio considerations

The LoRa link is half-duplex, and the simulator is itself cycling through
its own broadcast loop while listening for uplinks, so an uplink can
occasionally need a retry if it lands at the wrong point in that cycle —
the firmware retransmits automatically (see
[Level 3](../challenges/challenge-3-uplink.md)). If this proves unreliable
with many badges active in one room, the simulator's telemetry period is
a build-time constant that can be lengthened to reduce contention.
