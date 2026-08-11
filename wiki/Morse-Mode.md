# Morse Mode

**Module:** `firmware/main/radiochat/radio_chat.cpp` (Morse screens),
`firmware/main/radiochat/text_entry.*`

## Purpose

Manual CW (continuous-wave-style) send/receive layered on top of the
Radio Chat link, alongside a small pre-encoded phrase table for known
Morse phrases.

## User flow

From Radio Chat, entering Morse mode offers a home/manual screen for
composing and sending Morse-timed input, and a receive path that
attempts to decode known phrases against the pre-encoded table.

## Technical design

Decoding is an exact-match lookup against a table of known phrases rather
than a general Morse-timing decoder — see
[`docs/protocols/morse.md`](../docs/protocols/morse.md) for the phrase
table and matching approach.

## Dependencies

Radio Chat's link-layer send/receive path, `text_entry::` for manual
character entry.

## Storage usage

None beyond what Radio Chat itself persists.

## Known limitations

Decode is limited to the known-phrase table; free-form Morse timing
decode is not implemented. See
[`docs/protocols/morse.md`](../docs/protocols/morse.md).
