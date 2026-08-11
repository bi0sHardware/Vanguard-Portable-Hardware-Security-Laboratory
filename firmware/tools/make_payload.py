#!/usr/bin/env python3
"""Regenerate the Level 4 (Operation Vanguard) payload asset from the real
mission photo.

Reads tools/mission.png (the organiser's actual recon photo, already
128x128 8-bit grayscale), gzip-compresses its raw pixel bytes, and writes
that compressed blob straight into main/challenges/payload_data.h -- no
separate embed step, so `python3 tools/make_payload.py` alone is the full
"changed mission.png, now rebuild the satellite" workflow.

The player's intended path, once they've downloaded payload.bin off the
badge:
    xxd payload.bin | head     -> sees 1f 8b  (gzip magic)
    gunzip payload.bin         -> image.raw (16384 bytes)
    Pillow Image.frombytes("L", (128,128), data) -> mission.png -> flag

16384 = 128*128*1 is a clean power of two specifically so a player can
derive the geometry from the raw file's size alone once they've
gunzipped it, rather than being told the dimensions outright.

Replaces an earlier version of this script that procedurally drew a
synthetic scene instead of using the real photo -- that version is why
payload_data.h's own header comment said "regenerate from the source PNG
instead" without one existing to actually do that. This is that script.
Verified 2026-08-02: running it against the mission.png already in this
directory reproduces main/challenges/payload_data.h's then-committed
bytes exactly (including the gzip header's OS byte, forced to 0x03/Unix
below to match -- Python's gzip module's default there is
platform-dependent and would otherwise differ for no functional reason).

Requires: Pillow  (pip install pillow)
"""
import gzip
import io
import pathlib
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("This script needs Pillow:  pip install pillow")

W = H = 128
HERE = pathlib.Path(__file__).resolve().parent
SOURCE_PNG = HERE / "mission.png"
PAYLOAD_HEADER = HERE.parent / "main" / "challenges" / "payload_data.h"
CHUNK_BYTES = 64


def load_source_image() -> "Image.Image":
    if not SOURCE_PNG.exists():
        sys.exit(f"Missing source image: {SOURCE_PNG}\n"
                  "Put the organiser's mission photo there first, already "
                  f"cropped/converted to {W}x{H} 8-bit grayscale.")
    img = Image.open(SOURCE_PNG)
    if img.size != (W, H) or img.mode != "L":
        sys.exit(f"{SOURCE_PNG} is {img.size} mode={img.mode}, expected "
                  f"({W}, {H}) mode=L. Resize/convert it first -- this "
                  "script does not do that for you, since cropping a real "
                  "photo well needs a human's judgement, not an automatic "
                  "resize.")
    return img


def gzip_bytes(raw: bytes) -> bytes:
    buf = io.BytesIO()
    with gzip.GzipFile(fileobj=buf, mode="wb", compresslevel=9, mtime=0) as f:
        f.write(raw)
    blob = bytearray(buf.getvalue())
    blob[9] = 0x03  # OS byte: Unix -- see module docstring for why this is forced
    return bytes(blob)


def write_payload_header(blob: bytes) -> None:
    n = len(blob)
    chunks = (n + CHUNK_BYTES - 1) // CHUNK_BYTES
    lines = [
        "#pragma once",
        "// AUTO-GENERATED from the mission recon-photo source image. Do not edit",
        "// by hand; regenerate with tools/make_payload.py instead.",
        "//",
        "// Level 4 mission payload: a 128x128 8-bit grayscale raw image with the",
        "// final flag, gzip-compressed. The satellite simulator transmits this",
        f"// in {chunks} chunks of up to {CHUNK_BYTES} bytes.",
        "#include <cstdint>",
        "#include <cstddef>",
        "",
        "namespace satpayload {",
        f"constexpr size_t kChunkBytes = {CHUNK_BYTES};",
        f"constexpr size_t kLength = {n};",
        f"constexpr int    kChunks = {chunks};",
        f"static const uint8_t kData[{n}] = {{",
    ]
    for i in range(0, n, 16):
        row = ", ".join("0x%02X" % b for b in blob[i:i + 16])
        lines.append("    " + row + ",")
    lines.append("};")
    lines.append("} // namespace satpayload")
    PAYLOAD_HEADER.write_text("\n".join(lines) + "\n")


def main() -> None:
    img = load_source_image()
    raw = img.tobytes()
    assert len(raw) == W * H, f"expected {W*H} raw bytes, got {len(raw)}"

    blob = gzip_bytes(raw)
    write_payload_header(blob)

    (HERE / "image.raw").write_bytes(raw)
    (HERE / "payload.bin").write_bytes(blob)

    print(f"source       : {SOURCE_PNG.name} ({W}x{H} grayscale)")
    print(f"image.raw    : {len(raw)} bytes")
    print(f"payload.bin  : {len(blob)} bytes (gzip, magic {blob[0]:02x} {blob[1]:02x})")
    print(f"chunks @{CHUNK_BYTES}B  : {(len(blob) + CHUNK_BYTES - 1) // CHUNK_BYTES}")
    print(f"wrote        : {PAYLOAD_HEADER.relative_to(HERE.parent)}")


if __name__ == "__main__":
    main()
