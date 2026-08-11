# Morse (CW) Messaging

Radio Chat includes a Morse-code (CW) mode built entirely in
`firmware/main/radiochat/radio_chat.cpp`, alongside the app's plain-text
messaging. It reuses the same `radiolink` transport as text chat — a Morse
message is simply sent as a `radiolink::Type::MorseMsg` frame whose payload
is the dot/dash string itself, so no separate protocol or session exists for
it. Text entry for chat's own free-form text messages is handled by a
generic on-device character picker (`firmware/main/radiochat/text_entry.h`/
`.cpp`); Morse mode does not use it, since it is keyed directly rather than
typed.

## Sending: quick phrases vs. manual keying

Radio Chat's Morse Home screen offers two ways to send a Morse message:

- **Morse Quick Messages** — a fixed table of pre-encoded phrases
  (`kMorsePhrases` in `radio_chat.cpp`), each pairing a plain-text label with
  its exact Morse transcription (for example `"GG"` → `"--. --."`, or
  `"GOOD LUCK"` → `"--. --- --- -.. / .-.. ..- -.-. -.-"`). Selecting one
  sends that literal dot/dash string as the `MorseMsg` payload.
- **Manual keying** — the badge's OK button is bound to a dash and its
  joystick-select button to a dot. Each keypress starts a fixed-duration
  tone/LED symbol (180 ms for a dot, 550 ms for a dash — roughly the
  standard 3:1 Morse ratio) rather than timing the key's hold duration; the
  code notes that duration-based keying proved error-prone on real hardware,
  so a fixed symbol length was chosen instead. While a symbol is sounding,
  further keypresses are ignored.

Manual keying builds a running string of `.`, `-`, and separators based on
the gap since the last symbol ended:

- a gap of at least 600 ms inserts a letter boundary (a space),
- a gap of at least 1400 ms instead inserts a word boundary (`" / "`),
- a gap of at least 2600 ms with a non-empty buffer finalizes the message —
  trailing separators are trimmed and the accumulated string is sent as a
  `MorseMsg`, then the buffer resets.

The letter-gap and word-gap checks and the send check are independent: the
word-gap threshold (1400 ms) is reached well before the send threshold
(2600 ms), so the code deliberately does not gate the send check behind
"a gap marker was already inserted," or a message could never actually
reach the send threshold while sitting idle.

When a Morse Quick Message is sent, the Sending screen also plays back the
message's real dot/dash pattern as tone and LED (reusing the same timing
constants as manual keying) for visual/audio feedback — decoupled from the
actual radio transmission, which completes independently and typically much
faster; if the real send resolves first, the playback is cut short rather
than continuing after the result is already shown.

## Receiving and decoding

An incoming `MorseMsg` is, on the wire, just the sender's literal dot/dash/
separator string — `radiolink` does not treat it specially. Radio Chat's
receive handler stores that raw string unconditionally (`s_rxMorseRaw`) and
additionally attempts to decode it against the same fixed phrase table used
for sending, via `decodeKnownMorse()`. That function does an exact string
comparison against every entry in `kMorsePhrases`; only a byte-for-byte match
(same dots, dashes, and letter/word separators) resolves to a plain-text
label. Anything that doesn't match exactly — including manually-keyed
messages that don't happen to correspond to a table entry — is left
undecoded, and the received-message card shows the raw Morse instead of a
translation.

The received card also plays the message back as tone/LED using the
sender's raw dot/dash string, driven by the same fixed dot/dash/letter-gap/
word-gap timing constants as the send side, so the two directions of the
link look and sound consistent to a user regardless of which end typed a
quick phrase and which end keyed it manually.

## Summary

Morse mode adds no wire-level protocol of its own — it is a UI and encoding
convention layered entirely on top of `radiolink`'s existing `MorseMsg`
message type (see `docs/protocols/radio-chat.md`), with the encoding step
(quick-phrase lookup or manual keying) and the decoding step (exact-match
phrase lookup) both living in the application layer, not the link layer.
