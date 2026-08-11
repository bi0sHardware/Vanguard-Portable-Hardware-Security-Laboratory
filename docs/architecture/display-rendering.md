# Display and Rendering

## Panel and driver

The badge's display is an ST7789 TFT (`firmware/main/display/display.h`/
`.cpp`), driven through the Adafruit `Adafruit_ST7789` library over SPI. The
panel's native resolution is 240x320; the badge runs it at rotation 3, which
puts it into a 320x240 landscape active area (rotation 1 was tried and found
to render upside-down on this hardware). The SPI bus is initialized at
40 MHz — noted in-source as the confirmed-working speed for the shared FSPI
bus this display sits on. `display::init()` fills the screen black rather
than leaving the panel's power-on state visible, specifically because the
display is active on-screen during `setup()` before the first real frame is
drawn, and a white fill was visibly flashing on power-on before this was
added.

## Backlight: hardwired, no PWM

The display's backlight is wired directly to GND/3.3V — there is no GPIO
connection and no PWM channel driving it. `display.h` is explicit that
`setBrightness()`/`brightness()` are **not** hardware brightness control:
they exist purely as in-memory (and NVS-persisted, via Settings) storage of
a "brightness" value, so callers that already plumb a brightness setting
through Settings/NVS don't need to be rewritten if the backlight is ever
made controllable in a future hardware revision. The header notes explicitly
that nothing should rely on these calls actually changing what's on screen
under the current hardware. This also explains why GPIO1 (the battery-sense
ADC input) is not repurposed for this — it is a different pin from wherever
a PWM-controlled backlight would need to attach, and `pins.h` documents that
no `TFT_BL` GPIO constant exists at all.

## Two rendering approaches

Vanguard's screens are drawn using one of two approaches, chosen per screen
based on how its content behaves:

### Region-based rendering (`ui::Renderer`)

`firmware/main/ui/renderer.h`/`.cpp` implements a dirty-region renderer for
screens whose content is naturally structured as a small number of
independent, mostly-static areas — menus, list screens, forms. A screen
declares an array of `ui::Region` entries, each with fixed screen bounds, a
`draw()` callback, a pointer to the screen's own state, and a `changed()`
comparator (or a generic `memcmp` fallback against a stored snapshot no
larger than 16 bytes, enforced at compile time via
`UI_ASSERT_REGION_STATE_FITS`).

`ui::frame()` is called once per tick for the active screen. On the tick
after `ui::invalidate()` was called (always true on screen entry), it does a
full-screen clear, runs the screen's `onEnter()`, and draws its header; every
region is then unconditionally drawn once. On every other tick, each
region's current state is compared against its last-drawn snapshot, and only
regions whose state actually changed are cleared (to that region's own
`clearColor`) and redrawn. This avoids full-screen SPI fills on every
keypress or state change — a big win for screens where most of the screen
(chrome, unrelated list rows) never changes tick to tick.

```mermaid
flowchart TD
    A["ui::frame(screen) called"] --> B{invalidate() since last call?}
    B -- yes --> C["full clearScreen(), onEnter(), draw header"]
    C --> D["draw every region unconditionally"]
    B -- no --> E["for each region: compare state vs snapshot"]
    E --> F{changed or never drawn?}
    F -- yes --> G["clear region bounds, call draw(), update snapshot"]
    F -- no --> H["skip — region untouched"]
```

### Dirty-flag / direct-draw

Some screens — Radio Chat, PeerDrop, and simpler games like Snake — bypass
the `Renderer` entirely and manage their own single `dirty` boolean plus
direct `tft.fillRect()`/`tft.print()` calls. Radio Chat's source comments
explain the rationale directly: this style suits screens "driven by
continuous radio state that fits 'redraw when changed' better than diffing
static row content" — i.e., content whose changes are driven by asynchronous
events (an incoming radio packet, a BLE peer's RSSI updating, a Morse symbol
timing out) rather than by a small number of cleanly-separable regions. These
screens are responsible for their own chrome-once/content-delta discipline
(for example Radio Chat's list screens draw static header/footer chrome
once on entry and then only redraw the specific rows or state that changed
on subsequent ticks), rather than getting that behavior for free from
`ui::Renderer`.

## SPI bus sharing

The display's SPI lines (`TFT_MOSI`/`TFT_SCK`, and `TFT_MISO` though the
display does not use it) are physically the same bus as the LoRa module and
the LED shift registers (see `docs/architecture/lora-hardware.md` for the
full pin map and the boot-ordering consequence of that sharing). The
display's own chip-select (`TFT_CS`) is otherwise independent, so normal
frame rendering does not need to coordinate with LoRa or the shift registers
beyond the one-time deselect-ordering requirement handled in `setup()`.
