#!/usr/bin/env python3
"""Mission Control -- Level 3 authenticated uplink (organiser testing tool).

This is NOT the intended player workflow. A player is expected to recover
the phrase themselves -- decrypt Packets 6-10 (single-byte XOR brute force),
read the directive it reveals, find the badge's own PCB silkscreen
inscription, translate it, and type the *exact* translated text into the
badge by hand over PuTTY, after pressing OK through Searching -> Satellite
Found -> Mission Control / USB Ready -> Connected -> Streaming -> OK again
for "Submit Uplink" (mission3.cpp). The uplink itself carries no encryption
or transformation -- the badge sends exactly what's typed.

This script exists so organisers can verify a badge's Level 3 flow end to
end without doing that recovery by hand every time: run it once the badge
is already sitting at the "Uplink Payload" prompt (i.e. you've pressed OK
through to Submit yourself), and it types the known-correct phrase for you.

    python uplink.py [port]
"""
import sys
import time
import serial

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"

# The exact translated inscription Level 3 expects -- see docs/SOLUTIONS.md
# (organisers only) for the full recovery chain. Must match
# satellite/satellite_sim.cpp's kExpectedUplinkPhrase exactly, case included
# -- the satellite compares case-sensitively.
PHRASE = "kneelorbeerased"

with serial.Serial(PORT, 115200, timeout=1) as s:
    time.sleep(0.3)
    s.reset_input_buffer()
    s.write((PHRASE + "\r").encode())

    deadline = time.time() + 15
    while time.time() < deadline:
        line = s.readline().decode(errors="replace").rstrip()
        if line:
            print(line)
