#!/usr/bin/env python3
"""Packaging regression test with a SYNTHETIC, NON-BOOTABLE Image fixture.

Uses the real checksum-pinned companion, but does not compile or boot a kernel.
All synthetic outputs stay inside an automatically removed temporary directory.
"""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import zipfile

root = Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix='uidfake-package-format-') as directory:
    tmp = Path(directory)
    ak3 = tmp / 'ak3'
    ak3.mkdir()
    installer = ak3 / 'anykernel.sh'
    installer.write_text('#!/system/bin/sh\n# boot install\ntrue\n')
    image = tmp / 'synthetic-Image-NOT-BOOTABLE'
    data = bytearray(4096)
    data[56:60] = b'ARMd'
    image.write_bytes(data + b'hma_uidfake: native built-in policy ready')
    config = tmp / 'synthetic.config'
    config.write_text('CONFIG_HMA_UIDFAKE_BUILTIN=y\n')
    symbols = tmp / 'synthetic.System.map'
    symbols.write_text('0000000000000000 T hma_uidfake_translate\n')
    patch = root / 'patches/reference/gki-android14-6.1.145.patch'
    subprocess.run(['bash', str(root / 'scripts/uidfake-stage-anykernel.sh'),
                    str(ak3), '', str(patch)], check=True)
    subprocess.run(['sh', '-n', str(installer)], check=True)
    subprocess.run([sys.executable, str(root / 'scripts/uidfake-record-build.py'),
                    str(image), str(config), str(symbols), str(ak3)], check=True)
    archive = tmp / 'format-test_UIDFAKE.zip'
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
        for path in ak3.rglob('*'):
            if path.is_file():
                z.write(path, path.relative_to(ak3))
    subprocess.run([sys.executable, str(root / 'scripts/uidfake-verify-zip.py'), str(archive)], check=True)
    companion = tmp / 'custom-sync-tool'
    shutil.copyfile(ak3 / 'uidfake/sync-tool', companion)
    other = tmp / 'custom-ak3'
    other.mkdir()
    (other / 'anykernel.sh').write_text('#!/system/bin/sh\n# boot install\ntrue\n')
    subprocess.run(['bash', str(root / 'scripts/uidfake-stage-anykernel.sh'),
                    str(other), str(companion), str(patch)], check=True)
    repeat = subprocess.run(['bash', str(root / 'scripts/uidfake-stage-anykernel.sh'),
                             str(ak3), str(companion), str(patch)], capture_output=True)
    assert repeat.returncode != 0 and installer.read_text().count('# UIDFAKE companion preflight:') == 1
    config.write_text('# CONFIG_HMA_UIDFAKE_BUILTIN is not set\n')
    invalid = subprocess.run([sys.executable, str(root / 'scripts/uidfake-record-build.py'),
                              str(image), str(config), str(symbols), str(ak3)], capture_output=True)
    assert invalid.returncode != 0
print('Packaging-format tests passed (synthetic NON-BOOTABLE fixture; not a kernel/device test)')
