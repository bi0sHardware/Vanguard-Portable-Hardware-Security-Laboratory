#!/usr/bin/env python3
"""Mission Control Terminal -- Level 2 frame receiver.

Prints every raw hex frame the badge forwards over USB Serial. The badge
never decodes or labels anything -- what you see here is exactly what came
off the air.

    python receiver.py [port]

Default port is /dev/ttyACM0 (override with the argument, e.g. COM5).
"""
import sys
import serial

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"

print("=====================================")
print("MISSION CONTROL TERMINAL")
print("=====================================")
print(f"Listening on {PORT}...\n")

with serial.Serial(PORT, 115200, timeout=1) as s:
    while True:
        line = s.readline().decode(errors="replace").strip()
        if line:
            print(line)
