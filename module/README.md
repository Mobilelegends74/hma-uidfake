# HMA UID Fake – audited WebUI build

Source: last owner Action branch `build/android14-6.1-compat` (commit `f5c4e860`), with the standalone WebUI and source changes from `SHuiTU68/hma-uidfake`. The kernel code is still 0.3.3-era. No closed-source 0.4.0 ELF is embedded.

## Safety: explicit kernel loading

Installing the ZIP does not by itself opt in to running kernel hooks. To enable loading from the next boot, the device owner must deliberately create:

```
su -c 'mkdir -p /data/adb/hma-uidfake && touch /data/adb/hma-uidfake/allow_load'
```

Remove the marker and reboot to avoid further loading. **Removing the marker does NOT unload any kernel hooks already installed.** If the marker was already created in a previous test it still counts as opt-in: remove it before installing if you want a fully inert first boot. Have boot recovery available.

## WebUI / rules

KernelSU Next module WebUI is built into `webroot/` and calls the bundled `sync-tool`. It supports native rule lists, templates, a package picker, status and HMA/HMA-OSS config source observation. It needs no Zygisk. Native config is `/data/adb/hma-uidfake/config.json`; once created it takes priority over HMA/HMA-OSS as source. The panel asks for confirmation before this change.

The WebUI sends config via stdin, without persistent temporary policy files. The daemon monitors config changes and rebuilds caller/target UID pairs. Review the active source and module status before assuming a policy is effective.

Only Android 14-6.1 KMI is packaged by this audited workflow. `customize.sh` selects exact matching KMI or keeps a `.try` copy without loading it. No external auto-updater is embedded.
