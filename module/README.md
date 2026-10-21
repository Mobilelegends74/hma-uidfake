# HMA UID Fake — multi-KMI WebUI, automatic load

The module includes eight original KMI targets: android12-5.10, android13-5.10, android13-5.15, android14-5.15, android14-6.1, android15-6.6, android16-6.12, android17-6.18.

Install the ZIP in KernelSU Next and perform **one normal reboot**. No extra `allow_load` step or second reboot is necessary. The installer selects a matching kernel module; if none can be selected, it deliberately refuses loading rather than forcing a different KMI. A compatible KMI is necessary but does not itself prove full device compatibility.

In WebUI, "Rules active in kernel" reflects the actual live RCU policy count. "Syscall hooks" is a separate number. A blank or unavailable count means the new kernel status response could not be read; it must not be shown as zero.

The selected kernel module is loaded at post-fs-data/service. An already loaded older version is not forcibly unloaded/replaced during installation. To verify, check `lsmod | grep hma_uidfake` and `tail -n 50 /data/adb/modules/hma-uidfake/state/sync.log`. The background sync tool tracks HMA/HMA-OSS config changes.

**Recovery:** kernel-inline hooks can cause boot failure on incompatible systems. Make a backup of your current boot image and confirm working recovery procedures. To inhibit loading on the next boot, create `/data/adb/hma-uidfake/disable_load`; remove it to re-enable. Removing a marker never unloads hooks already live in the kernel. You can also disable/remove the module through KernelSU or recovery when possible.

The default rule source is HMA/HMA-OSS. The standalone file `/data/adb/hma-uidfake/config.json` is used only after explicitly selecting Manual mode. Saving/importing that file does not switch modes or override external synchronization. Missing selected configs clear the previous UID policy.

If the module is absent, **Manual inject** offers the eight packaged KMIs. After explicit risk confirmation it skips UIDFAKE's KMI/ABI preflight and passes the chosen file to `finit_module(flags=0)`. The kernel can still reject it. It does not flash a kernel, force-load, rewrite CRC/vermagic, replace a loaded module or persist an unchecked boot override. An incompatible module may crash the device. Automatic boot selection retains its checks.

The bottom **Save diagnostics** button exports a private archive to Download, including the manual attempt log. Review it before sharing; it contains package names and rules.

This is still an experimental kernel-hook module; Action compilation cannot substitute for physical boot tests of every kernel.
