#!/usr/bin/env python3
"""Check full reference patches, KABI and idempotence against immutable sources.

Downloads only integration call-site files, not full kernel/toolchain trees.
This is a source compatibility check, not a kernel compilation or device test.
"""
import base64
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import urllib.request

root = Path(__file__).resolve().parent.parent
files = ['kernel/sys.c', 'block/ioprio.c', 'kernel/user.c', 'kernel/cred.c',
         'fs/open.c', 'include/linux/sched.h', 'include/linux/android_kabi.h',
         'drivers/misc/Kconfig', 'drivers/misc/Makefile',
         'arch/arm64/configs/gki_defconfig']
references = [
    ('gki-android14-6.1.145', 'https://android.googlesource.com/kernel/common/+/266f523db5be6cf93b3a2c4f58251dfa65768e35/', True),
    ('oneplus-android14-6.1.118', 'https://raw.githubusercontent.com/OnePlusOSS/android_kernel_common_oneplus_sm8650/7b9a23054ab0ada2886ed34a2137b9d0312891ff/', False),
]


def run(*args, cwd=None):
    subprocess.run(args, cwd=cwd, check=True)


def snapshot(tree):
    return {str(p.relative_to(tree)): p.read_bytes()
            for p in tree.rglob('*') if p.is_file()}


for name, url, encoded in references:
    with tempfile.TemporaryDirectory(prefix='uidfake-reference-') as directory:
        tree = Path(directory) / 'kernel'
        tree.mkdir()

        def download(relative):
            location = url + relative + ('?format=TEXT' if encoded else '')
            for attempt in range(3):
                try:
                    with urllib.request.urlopen(location, timeout=45) as response:
                        data = response.read()
                    if encoded:
                        data = base64.b64decode(data, validate=True)
                    assert data and b'<html' not in data[:128].lower(), location
                    target = tree / relative
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(data)
                    return
                except Exception:
                    if attempt == 2:
                        raise
                    time.sleep(attempt + 1)

        with ThreadPoolExecutor(max_workers=4) as pool:
            list(pool.map(download, files))
        patch = root / 'patches/reference' / (name + '.patch')
        run('patch', '--dry-run', '--batch', '--fuzz=0', '-p1', '-i', str(patch), cwd=tree)
        run('patch', '--batch', '--fuzz=0', '-p1', '-i', str(patch), cwd=tree)
        run(sys.executable, str(root / 'scripts/uidfake-check-kabi.py'), str(tree))
        before = snapshot(tree)
        export = Path(directory) / 'unexpected.patch'
        run(sys.executable, str(root / 'scripts/uidfake-native-integrate.py'),
            str(tree), '--patch', str(export))
        assert snapshot(tree) == before, 'Reference patch and integrator disagree'
        assert not export.exists(), 'Idempotent integration unexpectedly exported changes'
        print(f'{name}: complete patch, KABI and idempotence passed', flush=True)
