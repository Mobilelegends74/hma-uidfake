#!/usr/bin/env python3
"""Generate or check complete patches for the module's Android/KMI matrix."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile
from uidfake_sources import ROOT, targets, fetch

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--target', help='Target ID; omitted = all reference targets')
p.add_argument('--generate', action='store_true', help='Export patch snapshots from the native source integrator')
p.add_argument('--workdir', type=Path, help='Keep downloaded/integrated fixtures here for diagnosis')
a = p.parse_args()
selected = [t for t in targets() if a.target is None or t['id'] == a.target]
assert selected, 'Unknown target'
failures = []


def run(*args, cwd=None):
    subprocess.run(args, cwd=cwd, check=True)


def snapshot(tree):
    return {str(p.relative_to(tree)): p.read_bytes() for p in tree.rglob('*') if p.is_file()}


for target in selected:
    try:
        with tempfile.TemporaryDirectory(prefix='uidfake-reference-') as temp:
            tree = (a.workdir / target['id']) if a.workdir else Path(temp) / 'kernel'
            assert not tree.exists(), f'Refusing to overwrite fixture: {tree}'
            tree.mkdir(parents=True)
            fetch(target, tree)
            patch = ROOT / target['patch']
            if a.generate:
                run(sys.executable, str(ROOT / 'scripts/uidfake-native-integrate.py'),
                    str(tree), '--patch', str(patch))
            else:
                run('patch', '--dry-run', '--batch', '--fuzz=0', '-p1', '-i', str(patch), cwd=tree)
                run('patch', '--batch', '--fuzz=0', '-p1', '-i', str(patch), cwd=tree)
            # Validate export syntax too (notably source files without final LF).
            run('patch', '--reverse', '--dry-run', '--batch', '--fuzz=0',
                '-p1', '-i', str(patch), cwd=tree)
            run(sys.executable, str(ROOT / 'scripts/uidfake-check-kabi.py'), str(tree))
            before = snapshot(tree)
            repeat = Path(temp) / 'unexpected.patch'
            run(sys.executable, str(ROOT / 'scripts/uidfake-native-integrate.py'), str(tree), '--patch', str(repeat))
            assert snapshot(tree) == before and not repeat.exists(), 'Non-idempotent integration'
            print(f'{target["id"]}: full patch, KABI and idempotence passed', flush=True)
    except Exception as error:
        failures.append(target['id'])
        print(f'{target["id"]}: FAILED: {error}', file=sys.stderr, flush=True)
if failures:
    raise SystemExit('Unsupported/failed targets: ' + ', '.join(failures))
