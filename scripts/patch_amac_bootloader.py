"""Guarded aMac/P4 rev3 DCDC settling delay for the pinned IDF 5.5.4 bootloader.

Only the 1000 us wait in rtc_clk_init changes to 2000 us. Power-control
register operations and feedback switching are unchanged. Both versions
are matched by complete SHA-256, and ESP checksum/digest are regenerated.
"""
import hashlib
import struct

ORIGINAL_SHA256 = '2ca6f9322d367ff74d672d7036faf1b6b397466e73971b46cb8920a6c47c42b1'
PATCHED_SHA256 = '583308c793c279381c9802c94ec45ec002548701d83c1bc136c002ae68379833'
WAIT_INSTRUCTION_ADDRESS = 0x4FFACE0E


def image_layout(image):
    """Validate one unencrypted ESP32-P4 image; return segments and checksum index."""
    if len(image) < 24 or image[0] != 0xe9 or not 1 <= image[1] <= 16:
        raise ValueError('Invalid ESP image header')
    if int.from_bytes(image[12:14], 'little') != 18 or image[23] != 1:
        raise ValueError('Expected an ESP32-P4 image with SHA-256 digest')
    position = 24
    segments = []
    checksum = 0xef
    for _ in range(image[1]):
        if position + 8 > len(image):
            raise ValueError('Truncated ESP segment header')
        address, size = struct.unpack_from('<II', image, position)
        position += 8
        if position + size > len(image):
            raise ValueError('Truncated ESP segment')
        segments.append((address, position, size))
        for value in image[position:position + size]:
            checksum ^= value
        position += size
    checksum_index = (position + 16) // 16 * 16 - 1
    if checksum_index + 33 != len(image) or image[checksum_index] != checksum:
        raise ValueError('Invalid ESP checksum or length')
    if image[checksum_index + 1:] != hashlib.sha256(image[:checksum_index + 1]).digest():
        raise ValueError('Invalid ESP image SHA-256')
    return segments, checksum_index


def _rewrite_delay(image, microseconds):
    segments, checksum_index = image_layout(image)
    locations = [start + WAIT_INSTRUCTION_ADDRESS - address
                 for address, start, size in segments
                 if address <= WAIT_INSTRUCTION_ADDRESS and WAIT_INSTRUCTION_ADDRESS + 4 <= address + size]
    if len(locations) != 1 or microseconds not in (1000, 2000):
        raise ValueError('Unsupported bootloader instruction layout')
    offset = locations[0]
    instruction = int.from_bytes(image[offset:offset + 4], 'little')
    if instruction not in (0x3e800513, 0x7d000513):
        raise ValueError('Unexpected rtc_clk_init wait instruction')
    result = bytearray(image)
    result[offset:offset + 4] = ((microseconds << 20) | 0x513).to_bytes(4, 'little')
    checksum = 0xef
    for _, start, size in segments:
        for value in result[start:start + size]:
            checksum ^= value
    result[checksum_index] = checksum
    result[checksum_index + 1:] = hashlib.sha256(result[:checksum_index + 1]).digest()
    image_layout(result)
    return bytes(result)


def patch_bootloader(image):
    digest = hashlib.sha256(image).hexdigest()
    if digest == PATCHED_SHA256:
        image_layout(image)
        return image
    if digest != ORIGINAL_SHA256:
        raise ValueError('Unsupported aMac bootloader: review the SDK before applying the clock wait fix')
    result = _rewrite_delay(image, 2000)
    if hashlib.sha256(result).hexdigest() != PATCHED_SHA256:
        raise ValueError('Unexpected patched bootloader digest')
    return result


def paired_bootloader(image):
    """Exact, verified counterpart, for a target-matched upgrade/rollback backup."""
    digest = hashlib.sha256(image).hexdigest()
    if digest == ORIGINAL_SHA256:
        return patch_bootloader(image)
    if digest == PATCHED_SHA256:
        result = _rewrite_delay(image, 1000)
        if hashlib.sha256(result).hexdigest() == ORIGINAL_SHA256:
            return result
    raise ValueError('No verified bootloader migration for this image')
