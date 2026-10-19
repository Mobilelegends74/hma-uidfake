#!/usr/bin/env bash
# Package the source patch and automatically installed Android sync companion.
set -euo pipefail
ak3="${1:?pass AnyKernel3 directory}"
local_tool="${2:-}"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source_patch="${3:-$root/uidfake/hma-uidfake-native.patch}"
test -s "$source_patch"
artifact="$(mktemp)"
trap 'rm -f "$artifact"' EXIT
mkdir -p "$ak3/uidfake"
if [ -n "$local_tool" ]; then
  test -s "$local_tool"
  cp "$local_tool" "$ak3/uidfake/sync-tool"
else
  curl -fLsS --retry 5 --connect-timeout 25 \
    https://github.com/Mobilelegends74/hma-uidfake/releases/download/v0.3.6-fork/hma-uidfake-fork-0.3.6.zip \
    -o "$artifact"
  echo "d0315a3acde5cb11969aad6fa76be0d42bbbbcbfb225f61410017d08791272f9  $artifact" | sha256sum -c -
  unzip -p "$artifact" sync-tool > "$ak3/uidfake/sync-tool"
  echo "efb4c77c11ea55e961024e6fe4d6fd1999dc281be2e11db3646939c546e3382f  $ak3/uidfake/sync-tool" | sha256sum -c -
fi
python3 - "$ak3/uidfake/sync-tool" <<'PY'
import sys
from pathlib import Path
import hashlib
p = Path(sys.argv[1])
data = p.read_bytes()
assert data[:6] == b'\x7fELF\x02\x01' and int.from_bytes(data[18:20], 'little') == 183, 'sync-tool must be a little-endian ARM64 ELF'
p.with_suffix('.sha256').write_text(hashlib.sha256(data).hexdigest() + '\n')
PY
chmod 755 "$ak3/uidfake/sync-tool"
cp "$root/scripts/uidfake-kernel-service.sh" "$ak3/uidfake/service.sh"
cp "$source_patch" "$ak3/uidfake/hma-uidfake-native.patch"
cp "$root/docs/native/ARCHITECTURE.md" "$ak3/uidfake/README.md"
cp "$root/uidfake/LICENSE" "$root/uidfake/NOTICE" "$ak3/uidfake/"
python3 - "$ak3/anykernel.sh" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1])
s = p.read_text()
anchor = '# boot install\n'
if s.count(anchor) != 1:
    raise SystemExit('Unexpected AnyKernel3 boot-install anchor')
if '# UIDFAKE companion preflight:' in s:
    raise SystemExit('UIDFAKE installer already staged; use a clean AnyKernel3 checkout')
install = '''
# UIDFAKE companion preflight: do not flash a silently incomplete installation.
[ -d /data/adb ] && [ -w /data/adb ] ||
  abort "UIDFAKE: flash from booted rooted Android with writable /data/adb"
for module in /data/adb/modules/*; do
  [ -d "$module" ] || continue
  [ -f "$module/disable" ] && continue
  [ -f "$module/remove" ] && continue
  for legacy_ko in "$module"/ko/*hma_uidfake.ko "$module"/hma_uidfake.ko; do
    if [ -f "$legacy_ko" ]; then
      abort "UIDFAKE: disable the separate UIDFAKE LKM module before flashing"
    fi
  done
done
mkdir -p /data/adb/uidfake-kernel /data/adb/service.d ||
  abort "UIDFAKE: cannot create sync directories"
cp -f uidfake/sync-tool /data/adb/uidfake-kernel/sync-tool ||
  abort "UIDFAKE: cannot install sync-tool"
cp -f uidfake/service.sh /data/adb/uidfake-kernel/service.sh ||
  abort "UIDFAKE: cannot install service"
cp -f uidfake/service.sh /data/adb/service.d/97-uidfake-kernel.sh ||
  abort "UIDFAKE: cannot install boot service"
chmod 0700 /data/adb/uidfake-kernel
chmod 0755 /data/adb/uidfake-kernel/sync-tool /data/adb/uidfake-kernel/service.sh /data/adb/service.d/97-uidfake-kernel.sh
ui_print "UIDFAKE built-in and automatic HMA sync installed"
'''
p.write_text(s.replace(anchor, install + '\n' + anchor))
PY
echo "UIDFAKE: native patch and Android sync companion packaged"
