#!/usr/bin/env python3
"""Compile and exercise the actual native core with ASan/UBSan on the host."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
core = root / 'uidfake/kernel/core.c'
with tempfile.TemporaryDirectory(prefix='uidfake-tests-') as tmp:
    tmp = Path(tmp)
    # Provide only the headers requested by the actual production source.
    headers = ''.join(p.read_text() for p in core.parent.glob('*.c'))
    for name in re.findall(r'^#include <([^>]+)>', headers, re.M):
        p = tmp / name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text('/* host API is declared by host-shim.h */\n')
    (tmp / 'linux/types.h').write_text('/* types supplied by host shim */\n')
    for test in ['core-test.c', 'netlink-test.c', 'policy-stress.c']:
        binary = tmp / test.removesuffix('.c')
        subprocess.run([
            'cc', '-std=gnu11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
            '-Wno-unused-parameter', '-fsanitize=address,undefined',
            '-fno-omit-frame-pointer',
            '-I' + str(tmp), '-I' + str(root / 'uidfake/tests'),
            '-I' + str(core.parent), '-I' + str(core.parent / 'include'),
            str(root / 'uidfake/tests' / test), '-o', str(binary)
        ], check=True)
        subprocess.run([str(binary)], check=True)
