# HMA UID Fake — multi-KMI WebUI, automatic load

The module includes eight original KMI targets: android12-5.10, android13-5.10, android13-5.15, android14-5.15, android14-6.1, android15-6.6, android16-6.12, android17-6.18.

Install the ZIP in KernelSU Next and perform **one normal reboot**. No extra `allow_load` step or second reboot is necessary. The installer selects a matching kernel module; if none can be selected, it deliberately refuses loading rather than forcing a different KMI. A compatible KMI is necessary but does not itself prove full device compatibility.

In WebUI, "Rules active in kernel" reflects the actual live RCU policy count. "Syscall hooks" is a separate number. A blank or unavailable count means the new kernel status response could not be read; it must not be shown as zero.

The selected kernel module is loaded at post-fs-data/service. An already loaded older version is not forcibly unloaded/replaced during installation. To verify, check `lsmod | grep hma_uidfake` and `tail -n 50 /data/adb/modules/hma-uidfake/state/sync.log`. The background sync tool tracks HMA/HMA-OSS config changes.

**Recovery:** kernel-inline hooks can cause boot failure on incompatible systems. Make a backup of your current boot image and confirm working recovery procedures. To inhibit loading on the next boot, create `/data/adb/hma-uidfake/disable_load`; remove it to re-enable. Removing a marker never unloads hooks already live in the kernel. You can also disable/remove the module through KernelSU or recovery when possible.

The WebUI's standalone policy file `/data/adb/hma-uidfake/config.json` overrides HMA-OSS rules; it is not created until you choose to save new independent rules. Normal installs use HMA-OSS as before.

This is still an experimental kernel-hook module; Action compilation cannot substitute for physical boot tests of every kernel.
