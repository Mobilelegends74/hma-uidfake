# HMA UID Fake

Makes a hidden uid look like it does not exist:

```
getpriority(PRIO_USER, uid)       -> -ESRCH
ioprio_get(IOPRIO_WHO_USER, uid)  -> -EINVAL
setpriority / ioprio_set          -> same
```

The rules come from the app a process was born from, so an isolated child or anything that called
`setuid()` cannot choose which of them apply. The repository README has the rest.

`customize.sh` picks the `ko/` entry matching `uname -r`, renames it to `ko/hma_uidfake.ko` and
removes the rest. With nothing for your kernel it leaves the closest build as
`ko/hma_uidfake.ko.try`, prints why another KMI's build will not load (vermagic, and the structures
this module reads), and points at `docs/build.md`. `post-fs-data.sh` loads the module as early as it
can and `service.sh` retries; both use the bundled `lkmloader` and log its exit code to
`state/sync.log` (`lkmloader ... debug=1` names things in dmesg for 60 seconds).

## Audited test branch (not original 0.4.0)

This experimental fork packages the 0.3.3-era source with an explicit opt-in gate.
After installation, **no kernel module is loaded** until the owner manually creates
`/data/adb/hma-uidfake/allow_load` and reboots. Root is required to create the marker.
Do not opt in unless you have a tested rescue path from a bootloop. Deleting the marker
prevents loading on the next boot; it does not unload already patched kernel hooks.
This archive is unrelated to any unsigned Telegram ZIP calling itself 0.4.0.
