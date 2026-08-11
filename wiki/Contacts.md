# Contacts

**Module:** `firmware/main/contacts/contacts.*`

## Purpose

Lists contacts exchanged via PeerDrop (name/organization), with a preview
panel for email/phone.

## User flow

Reached from the Main Menu. Browse the contact list; Ok shows a preview
panel with full details; Pause deletes the selected contact.

## Technical design

A straightforward list-driven screen over the `storage::Contact` records
persisted by PeerDrop. Deleting a contact re-packs the underlying NVS
storage rather than leaving a gap.

## Dependencies

`storage::`, `ui::` widgets/renderer.

## Storage usage

Reads/writes the `contacts` NVS namespace, capped at a fixed maximum
contact count (see `firmware/include/config.h`, `NVS_MAX_CONTACTS`).

## Known limitations

Contacts are added exclusively through PeerDrop exchange — there is no
manual on-device contact entry screen.
