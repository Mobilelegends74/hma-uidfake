#!/usr/bin/env python3
"""Reject a source-built candidate unless every version CRC matches the Image."""
import json
from pathlib import Path
import runpy
import sys

abi = runpy.run_path('scripts/module-abi.py')
reference = json.loads(Path('compat/oneplus-6.12.38/abi.json').read_text())
candidate = abi['inspect'](Path(sys.argv[1]).read_bytes())
info = candidate['info']
if (info.get('name') != 'hma_uidfake' or info.get('uidfake_kmi') != reference['kmi']
        or info.get('uidfake_variant') != 'oneplus-6.12.38'
        or not info.get('vermagic', '').startswith('6.12.38')):
    raise ValueError(f'Unexpected native candidate identity: {info}')
abi['compare'](candidate, {name: int(crc, 16) for name, crc in reference['exports'].items()})
if 'static_key_count' in candidate['imports']:
    raise ValueError('Debug static_key_count regression')
lines = [f"build_id {reference['build_id']}", f"kmi {reference['kmi']}",
         f"candidate {reference['candidate']}", f"banner {reference['banner']}",
         f"image_sha256 {reference['image_sha256']}"]
lines += [f'crc {name} {crc:08x}' for name, crc in sorted(candidate['crcs'].items())]
out = Path(sys.argv[2])
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text('\n'.join(lines) + '\n')
print(f"PASS: all {len(candidate['crcs'])} source-generated import CRCs match the original kernel Image")
