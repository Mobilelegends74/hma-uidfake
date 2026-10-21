#!/usr/bin/env python3
"""Exercise only shell doubles; never invoke the actual kernel loader."""
import os
import pathlib
import subprocess
import tempfile

source = pathlib.Path('module/manual-inject.sh').read_text()
assert 'finit_module' in source
assert 'kmi-tool" load ' in source
assert 'rmmod' not in source and 'insmod' not in source
assert 'disable_load' in source
assert 'nohup "$moddir/sync-tool"' in source
assert 'LOCK_EX | LOCK_NB' in pathlib.Path('src/tools/kmi_main.cpp').read_text()
assert 'LOCK_EX | LOCK_NB' in pathlib.Path('src/tools/main.cpp').read_text()

with tempfile.TemporaryDirectory(prefix='uidfake-manual-test-') as tmp:
    root = pathlib.Path(tmp)
    mod = root / 'module'
    bin_dir = root / 'bin'
    mod.mkdir()
    bin_dir.mkdir()
    (mod / 'manual-inject.sh').write_text(source)
    (mod / 'load.sh').write_text(pathlib.Path('module/load.sh').read_text())
    (mod / 'module.prop').write_text('id=hma-uidfake\ndescription=initial\n')
    for name, body in {
        'id': 'echo 0', 'lsmod': ':', 'nohup': 'exit 0',
    }.items():
        path = bin_dir / name
        path.write_text('#!/bin/sh\n' + body + '\n')
        path.chmod(0o700)
    tool = mod / 'kmi-tool'
    tool.write_text('#!/bin/sh\nprintf "%s\\n" "$*" >> "${0%/*}/calls"\necho "[ABI_KERNEL_MISMATCH] simulated CRC mismatch"\nexit 1\n')
    tool.chmod(0o700)
    env = dict(os.environ, PATH=str(bin_dir) + ':' + os.environ['PATH'])
    def run(*args):
        return subprocess.run(['sh', str(mod / 'manual-inject.sh'), *args], env=env, capture_output=True, text=True)
    for args in [(), ('../../bad',), ('android12-5.10;touch bad',), ('android12-5.10', 'extra')]:
        assert run(*args).returncode != 0
    assert not (mod / 'calls').exists()
    result = run('android12-5.10')
    assert result.returncode == 1, result.stdout + result.stderr
    assert '[ABI_KERNEL_MISMATCH]' in result.stdout
    assert (mod / 'calls').read_text().splitlines() == ['load ' + str(mod / 'ko/android12-5.10_arm64_hma_uidfake.ko')]
    assert 'ABI_KERNEL_MISMATCH' in (mod / 'state/load.reason').read_text()
    assert not (mod / 'state/selected-kmi').exists()
    # Busy/already-loaded must not call the kernel helper a second time.
    (bin_dir / 'lsmod').write_text('#!/bin/sh\necho "hma_uidfake 123 0"\n')
    assert '[ALREADY_LOADED]' in run('android14-6.1').stdout
    assert len((mod / 'calls').read_text().splitlines()) == 1
    (bin_dir / 'lsmod').write_text('#!/bin/sh\n:\n')
    tool.write_text('#!/bin/sh\nexit 0\n')
    result = run('android14-6.1')
    assert result.returncode == 0, result.stdout + result.stderr
    assert '[LOADED]' in result.stdout
    assert (mod / 'state/selected-kmi').read_text().strip() == 'android14-6.1'
    assert not (mod / 'state/load.reason').read_text().strip()
print('OK: manual candidate allowlist, rejection reason, no stacking, accepted load; loader mocked throughout')
