#!/usr/bin/env python3
"""Exercise only shell doubles; never invoke the actual kernel loader."""
import os
import pathlib
import subprocess
import tempfile

source = pathlib.Path('module/manual-inject.sh').read_text()
assert 'SYS_finit_module' in pathlib.Path('src/tools/kmi_main.cpp').read_text()
assert 'kmi-tool" manual-load ' in source
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
    tool.write_text('#!/bin/sh\nprintf "%s\\n" "$*" >> "${0%/*}/calls"\necho "[MANUAL_DIRECT] preflight skipped"\necho "[KERNEL_REJECTED] simulated kernel rejection"\nexit 1\n')
    tool.chmod(0o700)
    env = dict(os.environ, PATH=str(bin_dir) + ':' + os.environ['PATH'])
    def run(*args):
        return subprocess.run(['sh', str(mod / 'manual-inject.sh'), *args], env=env, capture_output=True, text=True)
    for args in [(), ('../../bad',), ('android12-5.10;touch bad',), ('android12-5.10', 'extra')]:
        assert run(*args).returncode != 0
    assert not (mod / 'calls').exists()
    result = run('android12-5.10')
    assert result.returncode == 1, result.stdout + result.stderr
    assert '[KERNEL_REJECTED]' in result.stdout
    assert (mod / 'calls').read_text().splitlines() == ['manual-load ' + str(mod / 'ko/android12-5.10_arm64_hma_uidfake.ko')]
    assert '[KERNEL_REJECTED]' in (mod / 'state/load.reason').read_text()
    assert '[MANUAL_DIRECT]' not in (mod / 'state/load.reason').read_text()
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

# Compile the real loader entry point, replacing only its syscall boundary.
# Deliberately invalid ELF bytes must reach that boundary in manual mode,
# while the automatic path must stop first. NO real finit_module is executed.
with tempfile.TemporaryDirectory(prefix='uidfake-direct-loader-test-') as tmp:
    root = pathlib.Path(tmp)
    (root / 'ko').mkdir()
    (root / 'state').mkdir()
    wrapper = root / 'loader.cpp'
    entry = pathlib.Path('src/tools/kmi_main.cpp').resolve()
    wrapper.write_text('''
#include "kmi.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/file.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <cstdarg>
static long fake_syscall(long nr, ...) {
  va_list ap; va_start(ap, nr);
  const int fd = va_arg(ap, int);
  const char *params = va_arg(ap, const char *);
  const int flags = va_arg(ap, int);
  va_end(ap);
  if (nr != SYS_finit_module || fd < 0 || !params || params[0] || flags != 0)
    return 99;
  std::cerr << "MOCK_FINIT_MODULE_FLAGS_0\\n";
  errno = ENOEXEC;
  return -1;
}
#define syscall fake_syscall
''' + '#include "' + str(entry) + '"\n')
    tool = root / 'loader'
    subprocess.run(['g++', '-std=c++23', '-Isrc/tools', str(wrapper),
                    'src/tools/kmi.cpp', '-lz', '-o', str(tool)], check=True)
    for kmi in ['android12-5.10', 'android13-5.10', 'android13-5.15', 'android14-5.15',
                'android14-6.1', 'android15-6.6', 'android16-6.12', 'android17-6.18',
                'android16-6.12-oneplus-6.12.38']:
        candidate = root / 'ko' / (kmi + '_arm64_hma_uidfake.ko')
        candidate.write_bytes(b'deliberately invalid ELF; test only')
        result = subprocess.run([str(tool), 'manual-load', str(candidate)], capture_output=True, text=True)
        assert '[MANUAL_DIRECT]' in result.stderr, result.stderr
        assert 'MOCK_FINIT_MODULE_FLAGS_0' in result.stderr, result.stderr
        assert '[KERNEL_REJECTED]' in result.stderr, result.stderr
        assert result.returncode == 1
        result = subprocess.run([str(tool), 'load', str(candidate)], capture_output=True, text=True)
        assert 'MOCK_FINIT_MODULE_FLAGS_0' not in result.stderr
        assert result.returncode == 1
    bad = root / 'ko' / 'unpackaged.ko'
    bad.write_bytes(b'bad')
    result = subprocess.run([str(tool), 'manual-load', str(bad)], capture_output=True, text=True)
    assert '[INVALID_CANDIDATE]' in result.stderr
    assert 'MOCK_FINIT_MODULE_FLAGS_0' not in result.stderr
print('OK: all nine manual choices bypass actual userspace preflight; automatic path still gated; kernel syscall mocked')
