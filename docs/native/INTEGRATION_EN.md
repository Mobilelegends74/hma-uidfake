# Native HMA UIDFAKE integration

[Home](../../README_EN.md) · [Русский](INTEGRATION_RU.md) · [Diagnostics](TROUBLESHOOTING.md)

## Requirements and boundaries

Use your kernel's existing toolchain, configuration and build pipeline. Full builds were validated on ARM64 Android 14 / Linux 6.1.118 (OnePlus 13R) and 6.1.145 (GKI). Other kernels/architectures need independent validation. Host requirements: Python 3.10+, GNU patch and a C compiler. Building the optional companion requires Android NDK r27c, CMake and nlohmann-json3-dev.

UIDFAKE complements HMA/HMA-OSS; it does not replace PackageManager filtering or the app's injection requirements. Native kernel code does not depend on a specific KernelSU/SUSFS version. The kit does not download or freeze those components.

Root and a privileged boot runner are required for automatic Android rule synchronization. The kernel cannot safely parse encrypted app configuration itself. An existing KernelSU/KernelSU Next service.d runner is supported by the supplied adapter. Other init/root distributions need their own adapter and possibly targeted SELinux policy; never disable SELinux/CFI to make this work.

Back up boot and verify device/KMI/firmware compatibility and recovery before flashing. A module ZIP is not a kernel patch. Do not install the legacy LKM on top of built-in UIDFAKE.

## Obtain and validate the kit

```bash
git clone --branch kernel-patches --single-branch https://github.com/Mobilelegends74/hma-uidfake.git
cd hma-uidfake
git rev-parse HEAD
sha256sum --check patches/SHA256SUMS
python3 scripts/uidfake-test-native.py
```

Record the kit commit in your build metadata; pin a tested kit commit for reproducibility. This does not pin KernelSU or SUSFS. Existing legacy src/module sources remain for provenance; only src/tools is needed for a source-built Android companion.

## Choose exactly one integration method

Apply other task_struct-changing patches (Droidspaces/SYSVIPC/SLIM_SCHED) **first**. Then select a genuinely free KABI slot. Do not apply both the reference patch and the integrator.

### Recommended: checked semantic integration

```bash
UIDFAKE_KERNEL=/absolute/path/to/kernel/common
python3 scripts/uidfake-native-integrate.py "$UIDFAKE_KERNEL" \
  --patch /absolute/path/to/generated-hma-uidfake.patch
python3 scripts/uidfake-check-kabi.py "$UIDFAKE_KERNEL"
```

This installs the component, Kconfig/Kbuild, internal header and native call sites. Every source anchor is checked before any source is written. Missing APIs or an unconditional free Android KABI slot cause an explicit error. Re-running is idempotent. The generated patch applies to the same starting tree; it does not include your other feature patches.

### Alternative: a complete reference patch

Choose **one** file from the [base matrix](../../patches/README.md). From the matching clean kernel root:

```bash
patch --dry-run --fuzz=0 -p1 < /absolute/path/to/hma-uidfake/patches/reference/gki-android14-6.1.145.patch
patch --fuzz=0 -p1 < /absolute/path/to/hma-uidfake/patches/reference/gki-android14-6.1.145.patch
```

Use oneplus-android14-6.1.118.patch for its documented vendor base. If dry-run fails, use the integrator or review/adapt the source API. Never drop rejected hunks, bypass security checks or force an occupied KABI slot.

## Configure and build

Set **CONFIG_HMA_UIDFAKE_BUILTIN=y** in your actual defconfig/fragment. The option is bool, depends on NET and selects CRC32. Merely replacing obj-m with obj-y or copying files is insufficient: call sites must also be installed.

Keep your device's existing build process. For ordinary Kbuild, after preparing its existing output configuration:

```bash
UIDFAKE_OUT=/absolute/path/to/kernel/out
./scripts/config --file "$UIDFAKE_OUT/.config" --enable HMA_UIDFAKE_BUILTIN
make O="$UIDFAKE_OUT" ARCH=arm64 LLVM=1 olddefconfig
grep '^CONFIG_HMA_UIDFAKE_BUILTIN=y$' "$UIDFAKE_OUT/.config"
make O="$UIDFAKE_OUT" ARCH=arm64 LLVM=1 -j"$(nproc)" Image
```

For Kleaf/Bazel, update its source defconfig/supported fragment **before** configuration and build with its normal target. Do not edit sandbox-generated .config or disable KMI/ABI checks. Header paths support out-of-tree O= builds. Toolchains, signing and boot formats remain device-specific. This kit does not add a module-version Bypass.

## Android synchronization companion

The packaging helper defaults to extracting sync-tool from the existing v0.3.6-fork release after checking both archive and binary SHA-256. It does **not** install its module, WebUI or lkmloader. The checksum-pinned version matches kaux protocol v4; source provenance is in uidfake/NOTICE.

Alternatively build only src/tools from this checkout (no LKM submodule needed):

```bash
sudo apt-get install cmake ninja-build nlohmann-json3-dev
UIDFAKE_NDK=/absolute/path/to/android-ndk-r27c
cmake -S src/tools -B out/native-sync -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$UIDFAKE_NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=24 -DANDROID_STL=c++_static \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH
cmake --build out/native-sync --target sync-tool
```

The supplied native and userspace protocol headers are identical. A custom binary must preserve the same protocol/layout. The packer checks ARM64 ELF and records its SHA-256; that proves archive integrity, not external attestation of your custom build.

## AnyKernel3 packaging

Prepare a clean device-appropriate AnyKernel3 checkout with one standard “# boot install” anchor. A different layout is rejected; adapt your installer before flashing rather than removing preflight checks.

```bash
UIDFAKE_AK3=/absolute/path/to/AnyKernel3
UIDFAKE_PATCH=/absolute/path/to/generated-hma-uidfake.patch
bash scripts/uidfake-stage-anykernel.sh "$UIDFAKE_AK3" "" "$UIDFAKE_PATCH"
# Or use your compiled companion:
# bash scripts/uidfake-stage-anykernel.sh "$UIDFAKE_AK3" out/native-sync/sync-tool "$UIDFAKE_PATCH"

python3 scripts/uidfake-record-build.py \
  /absolute/path/to/Image /absolute/path/to/.config \
  /absolute/path/to/System.map "$UIDFAKE_AK3"
(cd "$UIDFAKE_AK3" && zip -qr /absolute/path/to/kernel_UIDFAKE.zip . -x '.git/*')
python3 scripts/uidfake-verify-zip.py /absolute/path/to/kernel_UIDFAKE.zip
```

For reference-patch integration, pass that patch instead of the generated one. record-build requires the actual **raw ARM64 Image**, final .config and System.map from the same build, not boot.img/Image.gz. Additional Images you already ship must also contain UIDFAKE; this kit does not introduce Bypass mode.

Flash from booted rooted Android with writable /data/adb. Before the normal boot flash, the installer checks for known active UIDFAKE LKMs and installs root-owned sync-tool/service.sh under /data/adb/uidfake-kernel, plus /data/adb/service.d/97-uidfake-kernel.sh. It does not create an independent native JSON policy or erase existing rules. After boot, the existing root service runner starts the supervisor; it restarts sync-tool after failures while the tool waits for data/unlock and watches rule/package changes.

## Without AnyKernel3 / other root distributions

Install the ARM64 sync-tool and scripts/uidfake-kernel-service.sh (as service.sh) under /data/adb/uidfake-kernel. Use root ownership, directory mode 0700 and executable modes 0755. Arrange **one** privileged boot invocation of service.sh through your init/root runner. The supervisor's BASE must match installation paths.

Android init may require a dedicated SELinux domain/labels and narrow rules for selected config/package reads and generic netlink. There is no universal OEM sepolicy or boot-runner ABI. Do not use permissive mode. Copying a service.d script without a compatible runner does not start synchronization.

## Validate and recover

Follow the [diagnostic and rollback checklist](TROUBLESHOOTING.md): reboot/startup, rule changes and removal, isolated/app-zygote identity and compat applications. Compilation success, a ready marker or absence of logged errors does not prove every hook is correct. Built-in UIDFAKE cannot be unloaded: kernel rollback requires restoring a known-good compatible boot image.
