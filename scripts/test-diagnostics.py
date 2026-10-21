#!/usr/bin/env python3
"""Exercise collector helpers without root, Android data or any kernel load."""
import pathlib
import subprocess
import tempfile

source = pathlib.Path('module/diagnostics.sh').read_text()
helpers = source.split('# Android entry point.')[0]
with tempfile.TemporaryDirectory(prefix='uidfake-diag-test-') as tmp:
    root = pathlib.Path(tmp)
    helper = root / 'helpers.sh'
    helper.write_text(helpers)
    (root / 'small').write_bytes(b'example\x00binary\n')
    (root / 'large').write_bytes(b'x' * 1025)
    (root / 'large-link').symlink_to(root / 'large')
    script = '''
set -eu
. "$1"
work="$2"
copy_small "$work/small" "$work/copy" 1024
cmp "$work/small" "$work/copy"
copy_small "$work/large" "$work/oversized" 1024
test ! -e "$work/oversized"
copy_small "$work/large-link" "$work/linked" 1024
test ! -e "$work/linked"
copy_small "$work/absent" "$work/missing" 1024
test ! -e "$work/missing"
capture "$work/error.txt" sh -c 'echo expected-error >&2; exit 7'
grep -q expected-error "$work/error.txt"
grep -q '\\[exit=7\\]' "$work/error.txt"
capture "$work/missing-command.txt" /uidfake-command-that-does-not-exist
grep -q '\\[exit=127\\]' "$work/missing-command.txt"
capture "$work/bounded.txt" sh -c 'head -c 5000000 /dev/zero'
test "$(wc -c < "$work/bounded.txt")" -eq 4194304
tar -czf "$work/archive.tar.gz" -C "$work" copy error.txt manifest.txt
tar -tzf "$work/archive.tar.gz" >/dev/null
test "$(stat -c %a "$work/archive.tar.gz")" = 600
'''
    subprocess.run(['sh', '-c', script, 'test', str(helper), tmp], check=True)
    assert 'Unavailable:' in (root / 'manifest.txt').read_text()
    assert (root / 'small').read_bytes() == b'example\x00binary\n'

assert 'kmi-tool" select ' in source
assert 'kmi-tool" load ' not in source
assert '--set-config' not in source
assert 'insmod' not in source and 'rmmod' not in source
assert 'trap cleanup EXIT' in source
assert 'tar -tzf "$archive"' in source
assert 'printf \'{"ok":true' in source
assert 'logcat -b all -d -t 2000' in source
assert '"$output_dir/.uidfake-export-XXXXXX"' in source
print('OK: bounded binary copies, symlinks, missing data, command errors, archive integrity, private permissions and read-only collection')
