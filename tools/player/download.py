#!/usr/bin/env python3
"""Mission Control -- Level 4 payload download.

Asks the badge for the payload it received from the satellite and saves it
as payload.bin.

    python download.py [port]

Then work out what the file actually is.
"""
import sys
import time
import serial

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"

with serial.Serial(PORT, 115200, timeout=2) as s:
    time.sleep(0.3)
    s.reset_input_buffer()
    s.write(b"DOWNLOAD\n")

    hex_chunks, capturing, expected = [], False, 0
    deadline = time.time() + 30
    while time.time() < deadline:
        line = s.readline().decode(errors="replace").strip()
        if not line:
            continue
        if line.startswith("PAYLOAD_BEGIN"):
            capturing = True
            expected = int(line.split()[1])
            print(f"Receiving {expected} bytes...")
            continue
        if line.startswith("PAYLOAD_END"):
            break
        if capturing:
            hex_chunks.append(line)
        else:
            print(line)

if not hex_chunks:
    sys.exit("No payload received. Is the transfer complete on the badge?")

data = bytes.fromhex("".join(hex_chunks))
if expected and len(data) != expected:
    print(f"WARNING: expected {expected} bytes, got {len(data)}")

with open("payload.bin", "wb") as f:
    f.write(data)
print(f"Saved payload.bin ({len(data)} bytes)")
print(f"First bytes: {data[:4].hex(' ')}")
