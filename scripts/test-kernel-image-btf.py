#!/usr/bin/env python3
"""Reference SDK BTF verification; no kernel code is loaded."""
import hashlib
import json
import lzma
from pathlib import Path
import runpy
import struct
import subprocess
import tempfile

extract = runpy.run_path('scripts/kernel-image-btf.py')['extract']
types = bytes(100004)
strings = bytes(100001)
btf = struct.pack('<HBBIIIII', 0xeb9f, 1, 0, 24, 0, len(types), len(types), len(strings)) + types + strings
assert extract(b'prefix' + btf + b'suffix') == btf
for malformed in (b'not BTF', btf[:-1], btf + btf):
    try:
        extract(malformed)
    except ValueError:
        pass
    else:
        raise AssertionError('Truncated or ambiguous BTF accepted')
reference = json.loads(Path('compat/oneplus-6.12.38/abi.json').read_text())
with tempfile.TemporaryDirectory(prefix='uidfake-btf-test-') as tmp:
    output = Path(tmp) / 'vmlinux'
    subprocess.run(['python3', 'scripts/kernel-image-btf.py', '--unpack',
                    'compat/oneplus-6.12.38/vmlinux.btf.xz', str(output)], check=True)
    assert hashlib.sha256(output.read_bytes()).hexdigest() == reference['btf_sha256']
    bad = Path(tmp) / 'bad.xz'
    bad.write_bytes(lzma.compress(b'wrong kernel BTF'))
    rejected = Path(tmp) / 'rejected'
    result = subprocess.run(['python3', 'scripts/kernel-image-btf.py', '--unpack',
                             str(bad), str(rejected)], capture_output=True)
    assert result.returncode != 0 and not rejected.exists()
print('OK: reference BTF checksum, bounded table extraction and mismatch rejection')
