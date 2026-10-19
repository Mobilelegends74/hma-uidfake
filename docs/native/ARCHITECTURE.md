# Native UIDFAKE: architecture, provenance and limits

GPL-2.0. [Sources and authors](../../uidfake/NOTICE) · [Russian guide](INTEGRATION_RU.md) · [English guide](INTEGRATION_EN.md).

## Kernel and userspace boundary

The kernel component is a bool Kconfig feature: core.o, policy.o and netlink.o
are linked into Image with CONFIG_HMA_UIDFAKE_BUILTIN=y. It needs NET and CRC32;
it does not link against KernelSU, SUSFS or a particular root implementation.
KernelSU service.d is one Android **boot adapter**, not a kernel dependency.

The root Android companion parses encrypted/credential-protected app JSON after
data becomes available and publishes complete policies over generic netlink.
It watches rule/package/preset changes. The supervisor restarts it after exit.
A kernel cannot safely replace that userspace responsibility with Kconfig.
The helper is not a .ko, LKM loader or installable UIDFAKE root module.

## Source integration

| Call site | Role |
|---|---|
| kernel/sys.c | Translate PRIO_USER queries in getpriority/setpriority |
| block/ioprio.c | Translate IOPRIO_WHO_USER queries in ioprio_get/ioprio_set |
| kernel/user.c | Typed access to the real UID hash bucket and hash size |
| kernel/cred.c | Capture app identity before publishing validated credentials |
| fs/open.c | Attribute isolated processes from successfully checked APK opens |
| include/linux/sched.h | Fork-inherited app identity in an available KABI reservation |
| drivers/misc/Kconfig, Makefile | Standard in-tree configuration and build |

The common C syscall paths also serve compat ABIs. This is not runtime table
hooking: there is no syscall-table write, instruction decoder, f_op replacement,
kprobe dependency or spare TIF flag. Existing argument, capability, LSM and RCU
paths remain. Non-initial user namespaces retain normal UID behavior.

The policy engine preserves pooled per-caller masks, wildcard/app-id callers,
RCU snapshots and same-bucket UID replacement. ARM64 keeps the branchless csel
replacement path. Native bounds fixes cover caller-mask capacity and UID-hash
lookup wrapping; malformed policy/APK updates and allocation failure preserve
the preceding complete snapshot. An explicit empty policy revokes rules.

The dedicated task field is inherited by forks. Credentials record app origin;
APK inode mappings support attribution of isolated processes. App-zygote,
system-zygote, compat and unusual sandbox cases still require real-device tests.
No blanket timing-indistinguishability or undetectability claim is made.

## KABI

For Android kernels, the integrator scans task_struct preprocessor nesting and
selects an **unconditional free** ANDROID_KABI_RESERVE slot, using ANDROID_KABI_USE.
It avoids reservations already consumed by SYSVIPC/Droidspaces/SLIM_SCHED.
On tested modified 6.1 trees it used slot 5; clean reference trees use slot 8.
Host checks compare size and trailing offsets using the tree's real KABI macros;
older headers also preserve __GENKSYMS__ representation. New DWARF-aware headers
need the target's full ABI build for CRC certification. See [matrix](COMPATIBILITY.md).

If Android KABI is present but no safe reservation is available, integration
fails instead of silently changing vendor ABI. Non-Android trees can use a
conditional field before the architecture-dependent trailing thread_struct;
their module ABI and architecture compatibility are not claimed from this.
Headers provide CONFIG=n stubs; native source include paths support O= builds.

## Protocol and security

The userspace src/include/kaux.h and native kernel/include/kaux.h are identical:
kaux protocol v4, including the 108-byte status layout. Uploads require root and
GENL_ADMIN_PERM in the initial network namespace. The implementation validates
CRC, lengths, alignment, ownership and page order before publishing snapshots.
No unsafe writable proc file, arbitrary kernel-memory upload or text patcher is
introduced. A different companion/protocol layout must not be mixed blindly.

The default companion comes from v0.3.6-fork / source
fa72fe4fa93b26a336bf8c421ba60a108a54c253. The full archive checksum is
d0315a3acde5cb11969aad6fa76be0d42bbbbcbfb225f61410017d08791272f9;
the ARM64 sync-tool checksum is
efb4c77c11ea55e961024e6fe4d6fd1999dc281be2e11db3646939c546e3382f.
The packer verifies both before use and extracts no module/loader.

## What is and is not validated

Full compiled integrations and user-reported device success:

- [OnePlus 13R Linux 6.1.118](https://github.com/Mobilelegends74/OnePlus_KernelSU_SUSFS/releases/tag/op13r-6.1.118-2026-10-19-build90-1-uidfake).
- [GKI Linux 6.1.145](https://github.com/Mobilelegends74/GKI_KernelSU_SUSFS/releases/tag/gki-android14-6.1.145-2026-10-19-build53-1-uidfake).

The kit CI runs native core/netlink sanitizer tests, 24,000 differential queries,
65,536-pair capacity and patch/KABI/idempotence checks on ten exact references.
Eight DDK jobs compile the built-in component and all five modified C call sites.
They do **not** link every full Image, boot an emulator or validate every hook.
Source-anchor checks are not a universal Android/Linux compatibility promise.
The native component retains kernel protections; it does not require SELinux,
CFI, module-version or ABI enforcement to be disabled.

Multi-user configuration discovery is limited by the companion's user-0 HMA
paths. Android app-id expansion does not prove arbitrary profile discovery.
Unusual OEM encrypted-data layouts and root boot runners require adaptation.
The native kit has no separate installed WebUI/module-list entry by default.

## Release separation

kernel-patches is source-only. Its workflow has contents:read and contains no
tag, GitHub Release, LKM build or release-normalization jobs. The inherited
release-writing workflows were removed **only from this branch**. main retains
its existing LKM releases; adding the README button creates no new version/tag
and does not replace the latest release or any release asset.
