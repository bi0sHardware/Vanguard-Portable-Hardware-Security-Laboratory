# Battery Monitoring

**Module:** `firmware/main/power/battery.*`

## Purpose

Reports battery voltage/percentage from a dedicated ADC sense line.

## User flow

Surfaced as a status indicator elsewhere in the UI (not a dedicated
screen of its own).

## Technical design

GPIO1/`VBATT_SENSE` is a dedicated ADC input via a resistor divider, not
shared with any other function. Sampling is rate-limited
(`cfg::BATT_SAMPLE_INTERVAL_MS`) rather than read every tick.
Charging-state ("likely charging") is inferred heuristically from
voltage behavior, since no TP4056 CHRG/STAT line is wired to the MCU —
it is not a direct hardware signal.

## Dependencies

None beyond the ADC peripheral.

## Storage usage

None — battery state is not persisted, only sampled live.

## Known limitations

Charging detection is heuristic, not a direct hardware read, since the
charger IC's status line isn't connected to the MCU on this board
revision.
