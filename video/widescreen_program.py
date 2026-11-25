"""Prepare the European PPC image for the wide ZR107 renderer before AOT.

Input and output are interleaved, big-endian program bytes. The original is
never modified. A register write patch only affects the full-width cameras:
the wheel, tile layers and the separately configured rear mirror keep their
original scale. The graphics backend must also have a wide render surface.
"""

import argparse
import hashlib
from pathlib import Path
import struct


EAA_SHA256 = "7917bd59231acb639f85a1478a6fb8844799db94aad6ab742f2fe7c473b294db"

# addic r4,rN,-21846 computes aspect = 4/3 in 1.15 fixed point when rN=65536.
# An even 684 pixel viewport keeps integer half-extents and covers the full
# 682 2/3 visible source pixels at 16:9. The unused fraction is scissored.
ASPECT_CALLS = {
    0x1B414: 31, 0x1D1A8: 4, 0x1EC14: 7, 0x1EE78: 30, 0x1EF3C: 6,
    0x1EFA4: 6, 0x1F074: 6, 0x1F0E0: 6, 0x207DC: 4, 0x52B74: 29,
}
VIEWPORT_CALLS = (0x1B430, 0x1D1D4, 0x52B90)


def widescreen_program(source: bytes) -> bytes:
    if hashlib.sha256(source).hexdigest() != EAA_SHA256:
        raise ValueError("Expected the unmodified interleaved midnrun EAA/1.11 program")
    result = bytearray(source)

    def replace(offset, expect, value):
        if struct.unpack_from(">I", source, offset)[0] != expect:
            raise ValueError(f"Unexpected instruction at {offset:#x}")
        struct.pack_into(">I", result, offset, value)

    for offset, reg in ASPECT_CALLS.items():
        op = 0x30800000 | (reg << 16)
        replace(offset, op | 0xAAAA, op | 0xE400)
    for call in VIEWPORT_CALLS:
        # glViewport(-86, 0, 684, 384) keeps the origin at (256,192).
        replace(call - 16, 0x38600000, 0x3860FFAA)
        replace(call - 8, 0x38A00200, 0x38A002AC)
    return bytes(result)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    if args.source.resolve() == args.output.resolve():
        parser.error("Output must use a separate path")
    args.output.write_bytes(widescreen_program(args.source.read_bytes()))
