"""Read the two executable SHARC images from a user's interleaved EAA 1.11 ROM."""
import hashlib

GAME_DIGEST = "7917bd59231acb639f85a1478a6fb8844799db94aad6ab742f2fe7c473b294db"


def unpack(data, offset):
    size = int.from_bytes(data[offset:offset + 4], "big")
    stream = memoryview(data)[offset + 4:offset + 4 + size]
    result = bytearray()
    ring = bytearray(4096)
    cursor = 0xFEE
    index = 0
    while index < len(stream):
        flags = stream[index]
        index += 1
        for bit in range(8):
            if index == len(stream):
                break
            if flags & (1 << bit):
                val = stream[index]
                index += 1
                ring[cursor] = val
                cursor = (cursor + 1) & 0xFFF
                result.append(val)
            else:
                low, high = stream[index:index + 2]
                index += 2
                source = low | ((high & 0xF0) << 4)
                for j in range(3 + (high & 15)):
                    val = ring[(source + j) & 0xFFF]
                    ring[cursor] = val
                    cursor = (cursor + 1) & 0xFFF
                    result.append(val)
    return result


def program_words(rom):
    if hashlib.sha256(rom).hexdigest() != GAME_DIGEST:
        raise ValueError("Expected original, interleaved EAA 1.11 program")
    for address, count, padding in [(0x1F4000, 488, 0), (0x1F4F00, 18723, 2)]:
        image = unpack(rom, address)
        if len(image) != count * 6 + padding or any(image[count * 6:]):
            raise ValueError(f"Unexpected firmware size or padding at {address:#x}")
        for pos in range(0, count * 6, 6):
            yield int.from_bytes(image[pos:pos + 6], "big")
