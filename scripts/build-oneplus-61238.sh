#!/usr/bin/env bash
# Build against source-generated symbol CRCs. Never rewrite a candidate's CRCs.
set -euo pipefail
project=$PWD
task_root="${RUNNER_TEMP:?}/oneplus-61238"
mkdir -p "$task_root/source" "$task_root/clang" "$task_root/rust" "$task_root/clang-tools" "$task_root/build-tools"
cache=https://github.com/WildKernels/OnePlus_KernelSU_SUSFS/releases/download/toolchain-cache
source_commit=2eeabf09aa84e35236fdb82aba9b59b18b04ff9c
curl --fail --location --retry 3 "https://github.com/OnePlusOSS/android_kernel_common_oneplus_sm8845/archive/$source_commit.tar.gz" |
  tar -xz --strip-components=1 -C "$task_root/source"
curl --fail --location --retry 3 "$cache/clang-7cb95284aba215c2e1bbb70f545867f3d9295d58.tar.gz.partaa" -o "$task_root/clang.aa"
curl --fail --location --retry 3 "$cache/clang-7cb95284aba215c2e1bbb70f545867f3d9295d58.tar.gz.partab" -o "$task_root/clang.ab"
cat "$task_root/clang.aa" "$task_root/clang.ab" |
  tar -xz --wildcards --strip-components=1 -C "$task_root/clang" '*/clang-r547379/*'
curl --fail --location --retry 3 "$cache/rust-6ff98fecc15373d056564f1ad13508f812d90fa9.tar.gz" |
  tar -xz --strip-components=1 -C "$task_root/rust"
curl --fail --location --retry 3 "$cache/clang-tools-8d2e8177f56ed0543cc8866ecad6f9f32f25fffe.tar.gz" |
  tar -xz --strip-components=1 -C "$task_root/clang-tools"
curl --fail --location --retry 3 "$cache/build-tools-4ba19a61c7a0d63603e0d1c84eb2b610bff706a4.tar.gz" |
  tar -xz --strip-components=1 -C "$task_root/build-tools"
clang_bin=$(find "$task_root/clang" -path '*/clang-r547379/bin/clang' -print -quit)
rust_bin=$(find "$task_root/rust" -path '*/1.82.0/bin/rustc' -print -quit)
bindgen_bin=$(find "$task_root/clang-tools" -path '*/bin/bindgen' -print -quit)
export PAHOLE=$(find "$task_root/build-tools" -path '*/bin/pahole' -print -quit)
test -x "$clang_bin" && test -x "$rust_bin" && test -x "$bindgen_bin" && test -x "$PAHOLE"
export PATH="$(dirname "$clang_bin"):$(dirname "$rust_bin"):$(dirname "$bindgen_bin"):$PATH"
export LIBCLANG_PATH="$(dirname "$clang_bin")/../lib"
export ARCH=arm64 LLVM=1 LLVM_IAS=1 CROSS_COMPILE=aarch64-linux-gnu-
export KBUILD_BUILD_USER=OnePlus KBUILD_BUILD_HOST=ubuntu-build KBUILD_BUILD_VERSION=1
export KBUILD_BUILD_TIMESTAMP='Fri Oct 16 12:02:38 UTC 2026'
export KCFLAGS='-D__ANDROID_COMMON_KERNEL__ -mcpu=oryon-1 -O2'
export KCPPFLAGS='-DCONFIG_OPTIMIZE_INLINING'
clang --version
rustc --version
bindgen --version
"$PAHOLE" --version
cd "$task_root/source"
# Match the upstream task_struct layout change used for SYSVIPC. This is
# source/header compatibility, not a CRC substitution or loader bypass.
curl --fail --location --retry 3 https://raw.githubusercontent.com/WildKernels/kernel_patches/44b23a7601b98a3c548e2584e9f294d7b8b2d19b/common/droidspaces/fix_sysvipc_kabi_a16-6.12.patch -o "$task_root/sysvipc.patch"
patch -p1 --fuzz=0 < "$task_root/sysvipc.patch"
mkdir -p out
cp "$project/compat/oneplus-6.12.38/kernel.config" out/.config
make O=out olddefconfig
scripts/diffconfig "$project/compat/oneplus-6.12.38/kernel.config" out/.config
for required in CONFIG_GENDWARFKSYMS=y CONFIG_EXTENDED_MODVERSIONS=y CONFIG_CFI_CLANG=y CONFIG_CFI_ICALL_NORMALIZE_INTEGERS=y CONFIG_RUST=y; do
  grep -qx "$required" out/.config || { echo "Required config lost: $required"; exit 1; }
done
make -j"$(nproc)" O=out modules_prepare kernel/module/version.o
layout=$(awk '$1 == "#SYMVER" && $2 == "module_layout" { value=$3 } END { print value }' out/kernel/module/.version.o.cmd)
echo "Initial source-generated module_layout=$layout"
if [ "$layout" != 0xe976b219 ]; then
  export KBUILD_GENDWARFKSYMS_STABLE=1
  touch kernel/module/version.c
  make -j"$(nproc)" O=out kernel/module/version.o
  layout=$(awk '$1 == "#SYMVER" && $2 == "module_layout" { value=$3 } END { print value }' out/kernel/module/.version.o.cmd)
fi
echo "Source-generated module_layout=$layout; target=0xe976b219"
test "$layout" = 0xe976b219
# Native kernel modpost creates the real symvers. Do not substitute CRCs from
# another kernel/header tree or edit the emitted module version sections.
make -j"$(nproc)" O=out Image
# UIDFAKE imports built-in exports only. Preserve the exact native modpost
# output; compiling unrelated vendor/Rust LKMs is not a prerequisite.
test -s out/vmlinux.symvers
cp out/vmlinux.symvers out/Module.symvers
test -s out/Module.symvers
stage="$task_root/stage/android16-6.12"
mkdir -p "$stage" "$project/out-oneplus"
for f in "$project"/src/*; do ln -s "$f" "$stage/$(basename "$f")"; done
make O=out M="$stage" KMI=android16-6.12 UIDFAKE_VARIANT=oneplus-6.12.38 modules
cp "$stage/hma_uidfake.ko" "$project/out-oneplus/android16-6.12-oneplus-6.12.38_arm64_hma_uidfake.ko"
cp out/Module.symvers "$project/out-oneplus/kernel-Module.symvers"
cp out/.config "$project/out-oneplus/kernel.config"
printf '%s\n' "source=$source_commit" "module_layout=$layout" "gendwarf_stable=${KBUILD_GENDWARFKSYMS_STABLE:-0}" > "$project/out-oneplus/provenance.txt"
(cd "$project" && git ls-files -z src ':!src/tools/**' | sort -z | xargs -0 sha256sum) > "$project/out-oneplus/src.manifest.sha256"
