#!/usr/bin/env python3
"""Synthetic ARM64 export tables; no kernel/module code is executed."""
import runpy
import struct

exports = runpy.run_path('scripts/kernel-image-abi.py')['exports']
names = sorted(['module_layout'] + [f'test_symbol_{i:04}' for i in range(1100)])
start = 4096
crc_start = start + 12 * len(names)
string_start = crc_start + 4 * len(names) + 64
image = bytearray(string_start)
image[0x38:0x3c] = b'ARM\x64'
expected = {}
for i, name in enumerate(names):
    name_offset = len(image)
    image.extend(name.encode() + b'\0')
    pos = start + i * 12
    struct.pack_into('<iii', image, pos, 1024 - pos, name_offset - pos - 4, 256 - pos - 8)
    crc = 0x12340000 + i + 1
    struct.pack_into('<I', image, crc_start + i * 4, crc)
    expected[name] = crc
assert exports(bytes(image)) == expected
for bad in [b'not a kernel', bytes(128), b'\x7fELF' + bytes(256)]:
    try:
        exports(bad)
    except ValueError:
        pass
    else:
        raise AssertionError('Invalid format accepted')
print('OK: kernel-image export CRC extraction and unsupported-image rejection')
