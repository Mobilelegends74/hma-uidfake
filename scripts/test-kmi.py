#!/usr/bin/env python3
"""No kernel modules are loaded. Exercise real ELF parsing and selection in fixtures."""
import gzip
import os
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
KMIS = ['android12-5.10', 'android13-5.10', 'android13-5.15', 'android14-5.15',
        'android14-6.1', 'android15-6.6', 'android16-6.12', 'android17-6.18']


def elf(path, kmi, name='hma_uidfake', crc=True, machine=183, token=b'a'):
    version = kmi.split('-', 1)[1]
    branch = kmi.split('-', 1)[0]
    note = struct.pack('<III', 4, 20, 3) + b'GNU\0' + token * 20
    items = [('.shstrtab', b'', 3), ('.modinfo',
             f'name={name}\0vermagic={version}.123-{branch}-8 SMP preempt mod_unload modversions aarch64\0'.encode(), 1),
             ('.note.gnu.build-id', note, 7)]
    if crc:
        items += [('__versions', struct.pack('<Q', 0x12345678) + b'module_layout\0'.ljust(56, b'\0'), 1)]
    strings = b'\0' + b''.join(n.encode() + b'\0' for n, _, _ in items)
    items[0] = (items[0][0], strings, 3)
    blob = bytearray(64)
    sections = [bytes(64)]
    for name, data, kind in items:
        off = len(blob)
        blob.extend(data)
        sections.append(struct.pack('<IIQQQQIIQQ', strings.index(name.encode()), kind, 0, 0, off, len(data), 0, 0, 1, 0))
    off = len(blob)
    blob.extend(b''.join(sections))
    blob[:64] = struct.pack('<16sHHIQQQIHHHHHH', b'\x7fELF\x02\x01\x01' + bytes(9), 1, machine, 1, 0, 0, off, 0, 64, 0, 0, 64, len(sections), 1)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(blob)
    return note


with tempfile.TemporaryDirectory(prefix='uidfake-kmi-test-') as tmp:
    tmp = Path(tmp)
    tool = tmp / 'kmi-tool'
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-O1',
                    '-DUIDFAKE_KMI_TESTING', str(ROOT / 'src/tools/kmi.cpp'),
                    str(ROOT / 'src/tools/kmi_main.cpp'), '-lz', '-o', str(tool)], check=True)
    count = 0

    def case(release, wanted='', code='', cfg='', hints=(), mutate=None, machine='aarch64'):
        global count
        count += 1
        base = tmp / str(count)
        mod = base / 'module'
        root = base / 'root'
        (root / 'proc').mkdir(parents=True)
        (root / 'proc/version').write_text('Linux version ' + release)
        (root / 'proc/config.gz').write_bytes(gzip.compress(cfg.encode()))
        for kmi in KMIS:
            elf(mod / 'ko' / (kmi + '_arm64_hma_uidfake.ko'), kmi)
        for i, (kmi, loaded, matching) in enumerate(hints):
            name = 'vendor' + str(i)
            note = elf(root / 'vendor_dlkm/lib/modules' / (name + '.ko'), kmi, name=name)
            if loaded:
                path = root / 'sys/module' / name / 'notes/.note.gnu.build-id'
                path.parent.mkdir(parents=True)
                path.write_bytes(note if matching else b'wrong-build-id')
        if mutate:
            mutate(mod, root)
        result = subprocess.run([str(tool), 'fixture', str(mod), str(root), release, machine], capture_output=True, text=True)
        assert result.stdout.strip() == wanted, (release, result.stdout, result.stderr, wanted)
        assert (result.returncode == 0) == bool(wanted), result.stderr
        if code:
            assert '[' + code + ']' in result.stderr, result.stderr
        print('PASS', count, release, wanted or code)

    for kmi in KMIS:
        branch, version = kmi.split('-', 1)
        case(version + '.123-' + branch + '-8-gabcdef', kmi)
    case('5.15.211-g093e3da978e7', code='KMI_UNKNOWN')
    case('5.15.211-custom', code='KMI_UNKNOWN', mutate=lambda m, r:
         (r / 'proc/version').write_text('Linux version 5.15.211-custom compiler android14 clang'))
    case('6.6.123-custom', code='KMI_UNKNOWN')  # Even a lone matching Linux family is not proof.
    case('5.15.211-custom', 'android13-5.15', cfg='CONFIG_LOCALVERSION="-android13-8"\n')
    case('5.15.211-custom', 'android13-5.15', hints=[('android13-5.15', True, True)])
    case('5.15.211-custom', code='KMI_UNKNOWN', hints=[('android13-5.15', False, True)])
    case('5.15.211-custom', code='KMI_UNKNOWN', hints=[('android13-5.15', True, False)])
    case('5.15.211-custom', code='KMI_AMBIGUOUS', hints=[('android13-5.15', True, True), ('android14-5.15', True, True)])
    case('5.15.211-android13-8', code='KMI_AMBIGUOUS', cfg='CONFIG_LOCALVERSION="-android14-8"\n')
    case('5.15.211-android13-8', code='MODVERSIONS', cfg='# CONFIG_MODVERSIONS is not set\n')
    case('5.15.211-android13-8', code='MODULE_METADATA', mutate=lambda m, r: elf(m / 'ko/android13-5.15_arm64_hma_uidfake.ko', 'android14-5.15'))
    case('5.15.211-android13-8', code='MODULE_METADATA', mutate=lambda m, r: elf(m / 'ko/android13-5.15_arm64_hma_uidfake.ko', 'android13-5.15', crc=False))
    case('5.15.211-android13-8', code='MODULE_ELF', mutate=lambda m, r: elf(m / 'ko/android13-5.15_arm64_hma_uidfake.ko', 'android13-5.15', machine=62))
    case('5.15.211-android13-8', code='MODULE_ELF', mutate=lambda m, r: (m / 'ko/android13-5.15_arm64_hma_uidfake.ko').write_bytes(b'bad'))
    case('5.15.211-android13-8', code='KMI_MISSING', mutate=lambda m, r: (m / 'ko/android13-5.15_arm64_hma_uidfake.ko').unlink())
    case('5.15.211-android13-8', code='ARCH', machine='x86_64')

    # Real compiled .ko metadata, when called by the pack CI after eight builds.
    real = ROOT / 'build/ko'
    if real.exists():
        for kmi in KMIS:
            release = kmi.split('-', 1)[1] + '.123-' + kmi.split('-', 1)[0] + '-8'
            root = tmp / 'real-root'
            root.mkdir(exist_ok=True)
            result = subprocess.run([str(tool), 'fixture', str(real.parent), str(root), release, 'aarch64'], capture_output=True, text=True)
            assert result.returncode == 0 and result.stdout.strip() == kmi, (kmi, result.stderr)
            print('PASS actual built ELF', kmi)
    # Boot-path mock: a kernel rejection must not reach ksud/lkmloader or
    # try another candidate. These are shell doubles, never real load syscalls.
    boot = tmp / 'boot'
    (boot / 'ko').mkdir(parents=True)
    (boot / 'module.prop').write_text('id=test\ndescription=old\n')
    (boot / 'ko/android13-5.15_arm64_hma_uidfake.ko').write_bytes(b'fixture')
    mock = boot / 'kmi-tool'
    mock.write_text('#!/bin/sh\necho "$1" >>"$0.calls"\n'
                    'case "$1" in select) echo android13-5.15; exit 0;; esac\n'
                    'echo "[KERNEL_REJECTED] CRC mismatch" >&2\nexit 1\n')
    mock.chmod(0o755)
    script = '. "$1"; lsmod() { :; }; uidfake_load "$2"'
    result = subprocess.run(['sh', '-c', script, 'test', str(ROOT / 'module/load.sh'), str(boot)], capture_output=True, text=True)
    assert result.returncode != 0, result.stdout
    assert (boot / 'kmi-tool.calls').read_text().splitlines() == ['select', 'load']
    assert '[KERNEL_REJECTED] CRC mismatch' in (boot / 'module.prop').read_text()
    # Preserve the kernel-rejection status even without sync-tool starting.
    assert (boot / 'state/load.reason').read_text().startswith('[KERNEL_REJECTED]')
    print('PASS boot rejection: one selected candidate, no patched-loader fallback, persistent reason')
print('All KMI selection tests passed; no LKM load syscall was exercised.')
