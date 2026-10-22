#!/usr/bin/env python3
"""Extract the reference kernel's BTF for an external-module SDK, not an Image build."""
import hashlib
import json
import lzma
from pathlib import Path
import struct
import sys


def extract(image):
    candidates = []
    pos = 0
    while True:
        pos = image.find(b'\x9f\xeb\x01\x00', pos)
        if pos < 0:
            break
        if pos + 24 <= len(image):
            _, _, _, header, offset, types, strings, length = struct.unpack_from('<HBBIIIII', image, pos)
            end = pos + header + strings + length
            if (header == 24 and offset == 0 and strings == types and types > 100000
                    and length > 100000 and types % 4 == 0 and end <= len(image)
                    and image[pos + header + strings] == 0 and image[end - 1] == 0):
                candidates.append(image[pos:end])
        pos += 4
    if len(candidates) != 1:
        raise ValueError('Cannot identify one complete kernel BTF table')
    return candidates[0]


if __name__ == '__main__':
    reference = json.loads(Path('compat/oneplus-6.12.38/abi.json').read_text())
    action, source, destination = sys.argv[1:]
    data = Path(source).read_bytes()
    if action == '--extract':
        if hashlib.sha256(data).hexdigest() != reference['image_sha256']:
            raise ValueError('Not the authenticated public reference Image')
        btf = extract(data)
    elif action == '--unpack':
        btf = lzma.decompress(data, memlimit=67108864)
    else:
        raise ValueError('Use --extract Image output.xz or --unpack input.xz output')
    if hashlib.sha256(btf).hexdigest() != reference['btf_sha256']:
        raise ValueError('Reference BTF checksum mismatch')
    Path(destination).write_bytes(lzma.compress(btf) if action == '--extract' else btf)
    print('Verified reference BTF:', len(btf), 'bytes')
