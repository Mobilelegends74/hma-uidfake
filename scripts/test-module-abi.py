#!/usr/bin/env python3
"""CI auditor regression checks. No kernel loads or target execution."""
from pathlib import Path
import runpy

api = runpy.run_path('scripts/module-abi.py')
for value in (b'', b'not ELF', b'\x7fELF\x02\x01' + bytes(80)):
    try:
        api['inspect'](value)
    except ValueError:
        pass
    else:
        raise AssertionError('Invalid module accepted')
candidate = {'crcs': {'module_layout': 0xe976b219, '_printk': 42}}
api['compare'](candidate, dict(candidate['crcs'], unused_export=7))
for expected in ({'module_layout': 0xe976b219},
                 {'module_layout': 0x12345678, '_printk': 42}):
    try:
        api['compare'](candidate, expected)
    except ValueError:
        pass
    else:
        raise AssertionError('Incomplete / incompatible ABI accepted')
for path in Path('build/ko').glob('*.ko'):
    result = api['inspect'](path.read_bytes())
    assert result['info']['name'] == 'hma_uidfake', path
    assert 'static_key_count' not in result['imports'], path
    assert 'path_put' not in result['imports'], path
    print('PASS actual packaged ELF', path.name, len(result['crcs']))
print('OK: ABI auditor format rejection, missing exports and CRC mismatch')
