# Static review of owner-provided HMA UID Fake 0.4.0 ZIP

Source: previously uploaded `HMA UID Fake-0.4.0_1c71b6c.zip`.
SHA-256: `ad505ae7510c0780b86797e445258c643d35ad0391eb41b33f237fc2828b07c2`.

## Evidence from archive, not inferred source code

The archive contains stripped arm64 `sync-tool`, `lkmloader`, eight KMI-specific `hma_uidfake.ko` files, and readable boot/install scripts; **no source files** and **no WebUI**. The KMI `android14-6.1` ELF contains named local symbols `uid_inline_install`, `inline_install`, `setuid_inline_install`, `uf_find_user_hook`, `uid_tables_install`, `setuid_tables_install` and multiple `uid_tier` and `setuid_tier` selection messages. The archived README mentions permanent inline hooks and fallback paths. These are indicators of a new design, **not enough to reconstruct reliable or safe equivalent kernel C code**.

Observed shell-side changes: reusable `load.sh`, installed `ksud insmod` preference, bundled `lkmloader` fallback when ksud is absent, and never stacking a second loaded instance. Those shell-only behaviors are adapted here behind the preexisting explicit opt-in marker. The archived package auto-loaded hooks; this branch intentionally does not.

The archive's original module.prop pointed to an external update URL. That URL and all opaque binaries are intentionally omitted. The copied WebUI instead comes from an identifiable public GPL-2.0 fork and was reviewed separately.

No boot test on physical devices, full behavioral equivalence audit or source-level proof of version 0.4.0 is claimed. Kernel inline hooks may persist until reboot; use a tested recovery path before opting in.
