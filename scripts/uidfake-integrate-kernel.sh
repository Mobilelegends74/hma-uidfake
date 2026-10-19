#!/usr/bin/env bash
# Native in-tree UIDFAKE, using checked source hooks rather than LKM patching.
set -euo pipefail
kernel="${1:?pass common kernel source directory}"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
test -f "$kernel/arch/arm64/configs/gki_defconfig"
python3 "$root/scripts/uidfake-native-integrate.py" "$kernel" \
  --patch "$root/uidfake/hma-uidfake-native.patch"
grep -qx 'CONFIG_HMA_UIDFAKE_BUILTIN=y' "$kernel/arch/arm64/configs/gki_defconfig" ||
  printf '\nCONFIG_HMA_UIDFAKE_BUILTIN=y\n' >> "$kernel/arch/arm64/configs/gki_defconfig"
