# HMA UID Fake – audited WebUI Action branch

This branch extends the **owner's verified 0.3.3-audit Android14-6.1 build** with the open-source WebUI and standalone configuration backend from [SHuiTU68/hma-uidfake](https://github.com/SHuiTU68/hma-uidfake).

The `getpriority`, `ioprio_get` and UID hook code remains the audited 0.3.3 implementation. The user-supplied 0.4.0 ZIP was examined statically but its newer closed inline-hook implementation was **not** copied into the codebase or binaries. This branch does **not** claim 0.4.0 hook parity or guaranteed detector bypass.

The kernel module load is **opt-in** via `/data/adb/hma-uidfake/allow_load`; see [module/README.md](module/README.md). The standalone WebUI native config is kept at `/data/adb/hma-uidfake/config.json` and takes precedence once saved, even when HMA-OSS remains installed. The WebUI warns about that transition and uses stdin instead of temporary config files.

The workflow builds and validates a ZIP for KMI `android14-6.1`. There is no external update URL, no unsigned imported binaries, and no automatic merge to the default branch.

[Detailed provenance and 0.4.0 audit](docs/legacy-040-review.md)
