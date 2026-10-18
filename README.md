# HMA UID Fake — audited multi-KMI WebUI Action branch

This branch extends the proven owner build `0.3.3-audit` with the open-source WebUI, native rules, and verified source-level kernel status count from the actual policy RCU snapshot.

**Kernel module compilation targets (8 original KMIs):** android12-5.10, android13-5.10, android13-5.15, android14-5.15, android14-6.1, android15-6.6, android16-6.12, android17-6.18. Compiling a target does not mean an arbitrary vendor kernel is boot-tested.

- WebUI's "Rules active in kernel" reads `kaux_status.policy_pairs`, rather than a transient `sync-tool` process's empty cache. The 4+4 values are syscall-hook counts, not UID counts.
- The KernelSU installer picks a KMI using the release string. It **never** forces an incompatible fallback and refuses an ambiguous vendor-only KMI.
- Automatic load happens in post-fs-data/service on the **normal reboot after installation**; no `allow_load` marker or additional reboot needed. Already-running kernel modules cannot safely be upgraded until that normal reboot.
- Manual emergency opt-out for the *next* boot: `su -c 'touch /data/adb/hma-uidfake/disable_load'`. To restore automatic load: delete that marker then reboot. This marker is an opt-OUT, not a prerequisite.
- An HMA-OSS policy is used unless a standalone config `/data/adb/hma-uidfake/config.json` has been explicitly saved; standalone mode takes precedence and the WebUI warns before it does.
- WebUI saves via stdin; no plaintext temp files. No opaque 0.4.0 ELF binaries or external updater are included.

For installation/recovery details, see [module/README.md](module/README.md) and [0.4.0 archive review](docs/legacy-040-review.md).
