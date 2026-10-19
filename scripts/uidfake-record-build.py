#!/usr/bin/env python3
"""Validate a built ARM64 Image and record configuration/symbols in AnyKernel3."""
import argparse
import hashlib
from pathlib import Path
import shutil

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('image', type=Path)
p.add_argument('config', type=Path)
p.add_argument('system_map', type=Path)
p.add_argument('anykernel', type=Path)
a = p.parse_args()
image = a.image.read_bytes()
assert image[56:60] == b'ARMd', 'Pass an uncompressed ARM64 Image, not boot.img/Image.gz'
assert b'hma_uidfake: native built-in policy ready' in image, 'UIDFAKE marker missing'
assert 'CONFIG_HMA_UIDFAKE_BUILTIN=y' in a.config.read_text().splitlines(), 'UIDFAKE not built in final config'
symbols = [line for line in a.system_map.read_text().splitlines()
           if any(name in line for name in ('hma_uidfake', 'uidfake_policy', 'kaux_family'))]
assert any('hma_uidfake_' in line for line in symbols), 'UIDFAKE symbols missing from System.map'
payload = a.anykernel / 'uidfake'
payload.mkdir(parents=True, exist_ok=True)
if a.image.resolve() != (a.anykernel / 'Image').resolve():
    shutil.copyfile(a.image, a.anykernel / 'Image')
if a.config.resolve() != (payload / 'kernel.config').resolve():
    shutil.copyfile(a.config, payload / 'kernel.config')
(payload / 'build-verification.txt').write_text(
    'CONFIG_HMA_UIDFAKE_BUILTIN=y\nImage SHA256: ' + hashlib.sha256(image).hexdigest()
    + '\n' + '\n'.join(symbols) + '\n')
print('UIDFAKE: actual ARM64 Image, final config and System.map verified and packaged')
