# Competition Deployment Checklist

1. **Provision every player badge** — see [Badge Provisioning](badge-provisioning.md).
   New hardware needs the eFuse fix; previously-provisioned badges can
   skip it with `--skip-efuse`.
2. **Flash and deploy one satellite simulator** — see
   [Satellite Simulator](satellite-simulator.md). Power it centrally in
   the play area for the duration of the event.
3. **Verify Level 1 on real hardware** with a logic analyzer before doors
   open — it gates every subsequent level (see
   [Level 1](../challenges/challenge-1-uart-recon.md)).
4. **Distribute any player-side tooling** from `tools/player/` if the
   event provides companion scripts, or confirm players are expected to
   write their own (Level 3 explicitly involves scripting a protocol
   response).
5. **Confirm the flag submission path** — players submit flags through
   the on-device Submit Flag utility over serial/USB (see
   [Challenge Framework](../challenges/challenge-framework.md)).
6. **Have a re-provisioning path ready** for badges that need to be reset
   between sessions (`tools/provision_new_badge.sh` or `tools/flash.sh`,
   both of which erase NVS/progress).

## Recovery during an event

See [`TROUBLESHOOTING.md`](../../TROUBLESHOOTING.md) for flashing and
hardware failure modes, in particular the LoRa radio's lack of a
firmware-accessible reset line — a power cycle, not a reflash, is the
correct recovery for a stuck radio.
