#!/usr/bin/env python3
"""Check the final flashable archive rather than just its staging directory."""
import hashlib
from pathlib import Path
import sys
import zipfile

archive = Path(sys.argv[1])
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None, 'ZIP CRC failure'
    required = {'Image', 'anykernel.sh', 'uidfake/sync-tool', 'uidfake/sync-tool.sha256',
                'uidfake/service.sh', 'uidfake/hma-uidfake-native.patch',
                'uidfake/README.md', 'uidfake/LICENSE', 'uidfake/NOTICE',
                'uidfake/kernel.config', 'uidfake/build-verification.txt'}
    assert required <= set(z.namelist()), f'Missing payload: {required - set(z.namelist())}'
    for name in ('Image', 'Bypass-Image'):
        if name not in z.namelist():
            continue
        data = z.read(name)
        assert data[56:60] == b'ARMd' and b'hma_uidfake: native built-in policy ready' in data, name
    assert 'CONFIG_HMA_UIDFAKE_BUILTIN=y' in z.read('uidfake/kernel.config').decode().splitlines()
    tool = z.read('uidfake/sync-tool')
    assert tool[:6] == b'\x7fELF\x02\x01' and int.from_bytes(tool[18:20], 'little') == 183
    assert hashlib.sha256(tool).hexdigest() == z.read('uidfake/sync-tool.sha256').decode().strip()
    assert not any('uidfake' in n.lower() and '.ko' in n.lower() for n in z.namelist())
    assert not any(n.endswith('lkmloader') for n in z.namelist())
    installer = z.read('anykernel.sh').decode()
    assert installer.count('# UIDFAKE companion preflight:') == 1
    assert installer.index('# UIDFAKE companion preflight:') < installer.index('# boot install')
    assert '/data/adb/service.d/97-uidfake-kernel.sh' in installer
    patch = z.read('uidfake/hma-uidfake-native.patch').decode()
    for name in ('kernel/sys.c', 'kernel/user.c', 'kernel/cred.c', 'block/ioprio.c',
                 'fs/open.c', 'include/linux/sched.h', 'drivers/misc/hma_uidfake/core.c'):
        assert '+++ b/' + name in patch, name
    assert 'hma_uidfake-y := core.o policy.o netlink.o' in patch
    assert archive.name.endswith('_UIDFAKE.zip'), 'Use an explicit UIDFAKE archive label'
print('UIDFAKE archive verified; this does not substitute for on-device testing')
print(hashlib.sha256(archive.read_bytes()).hexdigest(), archive.name)
