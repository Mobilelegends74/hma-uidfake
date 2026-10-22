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


def elf(path, kmi, name='hma_uidfake', crc=True, machine=183, token=b'a', import_crc=True, embedded=True, bare=False, extended=False, fingerprint=None, variant=''):
    version = kmi.split('-', 1)[1]
    branch = kmi.split('-', 1)[0]
    note = struct.pack('<III', 4, 20, 3) + b'GNU\0' + token * 20
    release = version + '.123-custom' if bare else version + '.123-' + branch + '-8'
    info = f'name={name}\0vermagic={release} SMP preempt mod_unload modversions aarch64\0'
    if embedded:
        info += f'uidfake_kmi={kmi}\0'
    if variant:
        info += f'uidfake_variant={variant}\0'
    items = [('.shstrtab', b'', 3), ('.modinfo', info.encode(), 1),
             ('.note.gnu.build-id', note, 7)]
    items += [('.strtab', b'\0_printk\0', 3),
              ('.symtab', bytes(24) + struct.pack('<IBBHQQ', 1, 16, 0, 0, 0, 0), 2)]
    if crc:
        records = {'module_layout': 0x12345678}
        if import_crc:
            records['_printk'] = 0x87654321
        if fingerprint:
            records.update(fingerprint)
        versions = b''.join(struct.pack('<Q', value) + symbol.encode().ljust(56, b'\0') for symbol, value in records.items())
        if extended:
            names = b''.join(symbol.encode() + b'\0' for symbol in records) + b'\0'
            crcs = b''.join(struct.pack('<I', value) for value in records.values())
            items += [('__version_ext_names', names, 1), ('__version_ext_crcs', crcs, 1)]
        else:
            items += [('__versions', versions, 1)]
    strings = b'\0' + b''.join(n.encode() + b'\0' for n, _, _ in items)
    items[0] = (items[0][0], strings, 3)
    blob = bytearray(64)
    sections = [bytes(64)]
    for name, data, kind in items:
        off = len(blob)
        blob.extend(data)
        link = 4 if name == '.symtab' else 0
        entsize = 24 if name == '.symtab' else 0
        sections.append(struct.pack('<IIQQQQIIQQ', strings.index(name.encode()), kind, 0, 0, off, len(data), link, 0, 1, entsize))
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
    case('5.15.211-android13-android14', code='KMI_AMBIGUOUS')
    case('5.15.211-android13-8', code='KMI_AMBIGUOUS', cfg='CONFIG_LOCALVERSION="-android14-8"\n')
    case('5.15.211-android13-8', code='MODVERSIONS', cfg='# CONFIG_MODVERSIONS is not set\n')
    case('5.15.211-android13-8', code='MODULE_METADATA', mutate=lambda m, r: elf(m / 'ko/android13-5.15_arm64_hma_uidfake.ko', 'android14-5.15'))
    case('5.15.211-android13-8', code='MODULE_METADATA', mutate=lambda m, r: elf(m / 'ko/android13-5.15_arm64_hma_uidfake.ko', 'android13-5.15', crc=False))
    case('5.15.211-android13-8', code='MODULE_METADATA', mutate=lambda m, r: elf(m / 'ko/android13-5.15_arm64_hma_uidfake.ko', 'android13-5.15', import_crc=False))
    case('5.15.211-android13-8', 'android13-5.15', mutate=lambda m, r: elf(m / 'ko/android13-5.15_arm64_hma_uidfake.ko', 'android13-5.15', bare=True))
    case('5.15.211-android13-8', code='MODULE_METADATA', mutate=lambda m, r: elf(m / 'ko/android13-5.15_arm64_hma_uidfake.ko', 'android13-5.15', bare=True, embedded=False))
    case('6.12.123-android16-8', 'android16-6.12', mutate=lambda m, r: elf(m / 'ko/android16-6.12_arm64_hma_uidfake.ko', 'android16-6.12', extended=True))
    case('5.15.211-android13-8', code='MODULE_ELF', mutate=lambda m, r: elf(m / 'ko/android13-5.15_arm64_hma_uidfake.ko', 'android13-5.15', machine=62))
    case('5.15.211-android13-8', code='MODULE_ELF', mutate=lambda m, r: (m / 'ko/android13-5.15_arm64_hma_uidfake.ko').write_bytes(b'bad'))
    case('5.15.211-android13-8', code='KMI_MISSING', mutate=lambda m, r: (m / 'ko/android13-5.15_arm64_hma_uidfake.ko').unlink())
    case('5.15.211-android13-8', code='ARCH', machine='x86_64')

    def abi_fixture(mod, root, ambiguous=False, mismatch=False, sparse=False,
                    stale=False, forced=False, conflict=False, extended=False):
        fp = {f'abi_symbol_{i}': i + 100 for i in range(8 if not sparse else 1)}
        for kmi in ('android13-5.15', 'android14-5.15'):
            values = dict(fp)
            if kmi == 'android14-5.15' and not ambiguous:
                values['module_layout'] = 0xdeadbeef
            elf(mod / 'ko' / (kmi + '_arm64_hma_uidfake.ko'), kmi,
                fingerprint=values, bare=True, extended=extended)
        for n in range(2 if conflict else 1):
            name = 'reference' + str(n)
            values = dict(fp)
            if mismatch or n:
                values['abi_symbol_0'] = 0xbad
            note = elf(root / 'vendor_dlkm/lib/modules' / (name + '.ko'),
                       'android13-5.15', name=name, fingerprint=values,
                       bare=True, token=bytes([n + 1]), extended=extended)
            live = root / 'sys/module' / name
            (live / 'notes').mkdir(parents=True)
            (live / 'notes/.note.gnu.build-id').write_bytes(b'stale' if stale else note)
            (live / 'taint').write_text('F' if forced else 'OE')

    abi_config = 'CONFIG_MODVERSIONS=y\n'
    case('5.15.211-custom', 'android13-5.15', cfg=abi_config, mutate=abi_fixture)
    case('5.15.211-custom', 'android13-5.15', cfg=abi_config,
         mutate=lambda m, r: abi_fixture(m, r, extended=True))
    case('5.15.211-custom', code='KMI_AMBIGUOUS', cfg=abi_config,
         mutate=lambda m, r: abi_fixture(m, r, ambiguous=True))
    case('5.15.211-custom', code='ABI_UNCONFIRMED', cfg=abi_config,
         mutate=lambda m, r: abi_fixture(m, r, mismatch=True))
    case('5.15.211-custom', code='ABI_UNCONFIRMED', cfg=abi_config,
         mutate=lambda m, r: abi_fixture(m, r, sparse=True))
    case('5.15.211-custom', code='KMI_UNKNOWN', cfg=abi_config,
         mutate=lambda m, r: abi_fixture(m, r, stale=True))
    case('5.15.211-custom', code='KMI_UNKNOWN', cfg=abi_config,
         mutate=lambda m, r: abi_fixture(m, r, forced=True))
    case('5.15.211-custom', code='KMI_UNKNOWN', mutate=abi_fixture)
    case('5.15.211-custom', code='ABI_CONFLICT', cfg=abi_config,
         mutate=lambda m, r: abi_fixture(m, r, conflict=True))

    def profile_fixture(mod, root, mismatch=False, wrong_id=False, missing=False):
        fingerprint = {'module_layout': 0x12345678, '_printk': 0x87654321}
        folder = mod / 'kernel-profiles'
        folder.mkdir()
        lines = ['build_id ' + 'ab' * 20, 'kmi android12-5.10', 'banner Linux version 5.10.269-custom']
        for name, crc in fingerprint.items():
            if missing and name == '_printk':
                continue
            lines.append(f'crc {name} {crc + int(mismatch):08x}')
        (folder / 'test.abi').write_text('\n'.join(lines) + '\n')
        (root / 'sys/kernel').mkdir(parents=True)
        note = struct.pack('<III', 4, 20, 3) + b'GNU\0' + bytes.fromhex(('cd' if wrong_id else 'ab') * 20)
        (root / 'sys/kernel/notes').write_bytes(note)
        (root / 'dev').mkdir()
        (root / 'dev/kmsg').write_text('vendor: disagrees about version of symbol x, but ignore...\n')

    case('5.10.269-custom', 'android12-5.10', mutate=profile_fixture)
    case('5.10.269-custom', code='ABI_KERNEL_MISMATCH', mutate=lambda m, r: profile_fixture(m, r, mismatch=True))
    case('5.10.269-custom', code='PROFILE_ID_UNVERIFIED', mutate=lambda m, r: profile_fixture(m, r, wrong_id=True))
    case('5.10.269-custom', code='ABI_KERNEL_MISMATCH', mutate=lambda m, r: profile_fixture(m, r, missing=True))

    def bypass_fixture(mod, root):
        (root / 'dev').mkdir()
        (root / 'dev/kmsg').write_text('vendor: disagrees about version of symbol x, but ignore...\n')
    case('5.10.269-android12-9', code='ABI_CHECKS_BYPASSED', mutate=bypass_fixture)

    variant_id = 'android16-6.12-oneplus-6.12.38'
    def variant_fixture(mod, root, legacy=False, ambiguous=False, stale=False, profile=False, wrong_id=False):
        fp = {f'abi_symbol_{i}': i + 100 for i in range(8)}
        elf(mod / 'ko/android16-6.12_arm64_hma_uidfake.ko', 'android16-6.12',
            fingerprint=fp, extended=True)
        values = dict(fp, module_layout=0x12345678 if ambiguous else 0xe976b219)
        elf(mod / 'ko' / (variant_id + '_arm64_hma_uidfake.ko'), 'android16-6.12',
            fingerprint=values, extended=True, variant='oneplus-6.12.38')
        if profile:
            folder = mod / 'kernel-profiles'
            folder.mkdir()
            lines = ['build_id ' + 'ab' * 20, 'kmi android16-6.12',
                     'candidate ' + variant_id, 'banner Linux version 6.12.38-android16-OP-WILD']
            for name, crc in dict(values, _printk=0x87654321).items():
                lines.append(f'crc {name} {crc:08x}')
            (folder / 'oneplus.abi').write_text('\n'.join(lines) + '\n')
            (root / 'sys/kernel').mkdir(parents=True)
            (root / 'sys/kernel/notes').write_bytes(struct.pack('<III', 4, 20, 3) + b'GNU\0' + bytes.fromhex(('cd' if wrong_id else 'ab') * 20))
            return
        note = elf(root / 'vendor_dlkm/lib/modules/reference.ko', 'android16-6.12',
                   name='reference', fingerprint=fp if legacy else values, extended=True)
        live = root / 'sys/module/reference'
        (live / 'notes').mkdir(parents=True)
        (live / 'notes/.note.gnu.build-id').write_bytes(b'stale' if stale else note)
        (live / 'taint').write_text('OE')

    case('6.12.38-android16-OP-WILD', variant_id, cfg=abi_config, mutate=variant_fixture)
    case('6.12.38-android16-OP-WILD', 'android16-6.12', cfg=abi_config,
         mutate=lambda m, r: variant_fixture(m, r, legacy=True))
    case('6.12.38-android16-OP-WILD', code='KMI_AMBIGUOUS', cfg=abi_config,
         mutate=lambda m, r: variant_fixture(m, r, ambiguous=True))
    case('6.12.38-android16-OP-WILD', 'android16-6.12', cfg=abi_config,
         mutate=lambda m, r: variant_fixture(m, r, stale=True))
    case('6.12.38-android16-OP-WILD', variant_id,
         mutate=lambda m, r: variant_fixture(m, r, profile=True))
    case('6.12.38-android16-OP-WILD', code='PROFILE_ID_UNVERIFIED',
         mutate=lambda m, r: variant_fixture(m, r, profile=True, wrong_id=True))

    # Real compiled .ko metadata, when called by the pack CI after eight builds.
    real = Path(os.environ.get('UIDFAKE_TEST_KO_DIR', ROOT / 'build/ko'))
    if real.exists():
        for kmi in KMIS:
            release = kmi.split('-', 1)[1] + '.123-' + kmi.split('-', 1)[0] + '-8'
            root = tmp / 'real-root'
            root.mkdir(exist_ok=True)
            result = subprocess.run([str(tool), 'fixture', str(real.parent), str(root), release, 'aarch64'], capture_output=True, text=True)
            assert result.returncode == 0 and result.stdout.strip() == kmi, (kmi, result.stderr)
            print('PASS actual built ELF', kmi)
        # Optional local replay of tester-supplied binaries. Never bundle or
        # upload device files: CI runs only the synthetic ABI regressions.
        reference = os.environ.get('UIDFAKE_TEST_REFERENCE_KO')
        note = os.environ.get('UIDFAKE_TEST_REFERENCE_NOTE')
        if reference and note:
            import shutil
            root = tmp / 'device-replay'
            (root / 'proc').mkdir(parents=True)
            (root / 'proc/config.gz').write_bytes(gzip.compress(b'CONFIG_MODVERSIONS=y\n'))
            dst = root / 'vendor_dlkm/lib/modules/cfg80211.ko'
            dst.parent.mkdir(parents=True)
            shutil.copyfile(reference, dst)
            live = root / 'sys/module/cfg80211'
            (live / 'notes').mkdir(parents=True)
            shutil.copyfile(note, live / 'notes/.note.gnu.build-id')
            # OE comes from the tester's /proc/modules snapshot; this is a
            # replay fixture, not an additional observation on their phone.
            (live / 'taint').write_text('OE')
            result = subprocess.run([str(tool), 'fixture', str(real.parent), str(root),
                                     '5.15.211-g093e3da978e7', 'aarch64'], capture_output=True, text=True)
            print(result.stderr)
            assert result.returncode == 0 and result.stdout.strip() == 'android13-5.15', result.stderr
            print('PASS local device ABI replay; no LKM was loaded')
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
