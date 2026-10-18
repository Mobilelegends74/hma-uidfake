# Android 14 / Linux 6.1 — source-audit test build

Source basis: `Anatdx/hma-uidfake` commit `dcc8c1993f355de083d1beca834da423c0ae2098` (0.3.3-era source).
Branch limited to `android14-6.1`. **Not** original v0.4.0 and not a byte-identical rebuild of an external ZIP.

## Why the module is safe-by-default

The original module loads a kernel hook automatically from `post-fs-data.sh` and retries in `service.sh`.
This branch adds a gate to both stages: without the marker `/data/adb/hma-uidfake/allow_load`,
they skip kernel module loading and do not start policy synchronization.

An owner who has a proven recovery method may opt in using a root shell:

```sh
su -c 'mkdir -p /data/adb/hma-uidfake && touch /data/adb/hma-uidfake/allow_load && chmod 600 /data/adb/hma-uidfake/allow_load'
```

The module starts attempting to hook only **on the next reboot**. Creating the marker alone
does not load anything immediately. For a future boot, remove the marker with root privileges:

```sh
su -c 'rm -f /data/adb/hma-uidfake/allow_load'
```

Removing the marker does **not** undo in-memory kernel hooks; reboot is required.
If the phone cannot boot, an alternate recovery that can disable/remove KernelSU modules is required.
Uninstall/disable the module or remove the marker before reinstalling a changed build.

## Provenance and limitations

The installed metadata identifies an audited 0.3.3 source baseline and contains no remote
`updateJson` URL. The GitHub Actions workflow compiles the kernel module with the Android 14
Linux 6.1 DDK, packages userspace tools and uploads a verified ZIP.

Successful CI only proves compilation and archive structure. It does not prove the hooks
work on a given OEM kernel and does not constitute a security audit of third-party toolchains.
