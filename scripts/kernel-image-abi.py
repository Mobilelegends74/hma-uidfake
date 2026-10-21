#!/usr/bin/env python3
"""Read ARM64 Image export CRCs; never rewrite a kernel or module.

Supports the absolute 32-bit CRC / PREL32 kernel_symbol layout used by the
provided 5.10 image. Other layouts are rejected, not guessed or rewritten.
"""
import array
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
import zipfile


def exports(image):
    if image[0x38:0x3c] != b'ARM\x64':
        raise ValueError('Not a raw ARM64 Image')
    # The ARM64 header's image_size includes BSS. Exported BSS variables may
    # lie past the raw file, but symbol names/namespaces and CRCs may not.
    memory_size = struct.unpack_from('<Q', image, 0x10)[0] or len(image)
    if memory_size < len(image) or memory_size > len(image) + 67108864:
        raise ValueError('Unsupported ARM64 memory image size')
    strings = {m.start() for m in re.finditer(b'module_layout\x00', image)}
    words = array.array('i')
    words.frombytes(image[:len(image) // 4 * 4])
    if sys.byteorder != 'little':
        words.byteswap()

    def entry(pos):
        if pos < 0 or pos + 12 > len(image):
            return None
        value, name, namespace = struct.unpack_from('<iii', image, pos)
        target = pos + 4 + name
        ns = pos + 8 + namespace
        if not (0 <= pos + value < memory_size and 0 <= target < len(image)
                and 0 <= ns < len(image)):
            return None
        end = image.find(b'\0', target, target + 4096)
        name = image[target:end] if end >= 0 else b''
        if not re.fullmatch(rb'[a-zA-Z_][a-zA-Z_0-9.]*', name):
            return None
        return name.decode()

    tables = []
    for index, value in enumerate(words):
        if index * 4 + value not in strings:
            continue
        start = end = index * 4 - 4
        while entry(start - 12):
            start -= 12
        while entry(end):
            end += 12
        if (end - start) // 12 >= 1000:
            tables.append((start, end))
    tables = sorted(set(tables))
    if len(tables) != 1:
        raise ValueError('Cannot identify a unique export table')
    start, end = tables[0]
    names = [entry(pos) for pos in range(start, end, 12)]
    if len(set(names)) != len(names):
        raise ValueError('Duplicate export names')
    if end + len(names) * 4 > len(image):
        raise ValueError('Truncated CRC table')
    crcs = {name: struct.unpack_from('<I', image, end + i * 4)[0]
            for i, name in enumerate(names)}
    if any(not crc for crc in crcs.values()):
        raise ValueError('Unsupported CRC table layout / zero CRC')
    return crcs


if __name__ == '__main__':
    file = Path(sys.argv[1])
    if file.suffix == '.zip':
        with zipfile.ZipFile(file) as archive:
            image = archive.read('Image')
    else:
        image = file.read_bytes()
    crcs = exports(image)
    print(json.dumps({'sha256': hashlib.sha256(image).hexdigest(),
                      'exports': {name: f'{value:08x}' for name, value in crcs.items()}}, sort_keys=True))
