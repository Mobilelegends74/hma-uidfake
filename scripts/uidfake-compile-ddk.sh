#!/usr/bin/env bash
# Real ARM64 built-in and call-site compilation, not an external module build.
set -euo pipefail
kmi=${1:?Android KMI required}
kit=$(cd "$(dirname "$0")/.." && pwd)
src="/opt/ddk/src/$kmi"
out="/opt/ddk/kdir/$kmi"
report="$kit/out/$kmi"
test -f "$src/Makefile"
test -f "$out/.config"
mkdir -p "$report"
cp "$out/.config" "$report/config.before"
python3 "$kit/scripts/uidfake-native-integrate.py" "$src" --patch "$report/native.patch"
python3 "$kit/scripts/uidfake-check-kabi.py" "$src"
"$src/scripts/config" --file "$out/.config" -e HMA_UIDFAKE_BUILTIN
make -C "$src" O="$out" ARCH=arm64 LLVM=1 LLVM_IAS=1 olddefconfig
grep -qx 'CONFIG_HMA_UIDFAKE_BUILTIN=y' "$out/.config"
# Never remove kernel protections to pass compilation.
python3 - "$report/config.before" "$out/.config" <<'PY'
from pathlib import Path
import sys
before, after = (Path(x).read_text().splitlines() for x in sys.argv[1:])
for line in before:
    if line.endswith('=y') and line.startswith(('CONFIG_CFI', 'CONFIG_SECURITY_SELINUX=',
            'CONFIG_ARM64_PTR_AUTH', 'CONFIG_ANDROID_KABI', 'CONFIG_MODVERSIONS=')):
        assert line in after, 'Protection changed: ' + line
PY
make -C "$src" O="$out" ARCH=arm64 LLVM=1 LLVM_IAS=1 -j"$(nproc)" \
  drivers/misc/hma_uidfake/ kernel/sys.o block/ioprio.o kernel/user.o kernel/cred.o fs/open.o
test -s "$out/drivers/misc/hma_uidfake/built-in.a"
for name in core policy netlink; do
  test -s "$out/drivers/misc/hma_uidfake/$name.o"
done
llvm-nm "$out/drivers/misc/hma_uidfake/built-in.a" > "$report/native-symbols.txt"
grep -q 'hma_uidfake_hidden' "$report/native-symbols.txt"
cp "$out/.config" "$report/config.after"
{
  echo "KMI=$kmi"
  echo 'DDK=20260828'
  make -s -C "$src" O="$out" ARCH=arm64 LLVM=1 kernelversion
  clang --version
  echo 'PASS: native built-in archive and all five kernel call-site objects compiled.'
  echo 'NOT a complete Image link, full vendor KMI certification or device boot test.'
} > "$report/result.txt"
cat "$report/result.txt"
