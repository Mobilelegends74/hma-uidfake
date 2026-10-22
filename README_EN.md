<div align="center">

<img src="docs/assets/banner.svg" alt="HMA UID Fake fork" width="820">

# HMA UID Fake fork

**Kernel-side UID visibility guard for KernelSU Next — a companion to HMA and HMA-OSS.**

[![Build](https://img.shields.io/github/actions/workflow/status/Mobilelegends74/hma-uidfake/build.yml?branch=main&label=Build&style=flat-square)](https://github.com/Mobilelegends74/hma-uidfake/actions/workflows/build.yml)
[![Latest release](https://img.shields.io/badge/Latest_release-v0.3.16-orange?style=flat-square)](https://github.com/Mobilelegends74/hma-uidfake/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/Mobilelegends74/hma-uidfake/total.svg?style=flat-square&label=downloads&cacheSeconds=300)](https://github.com/Mobilelegends74/hma-uidfake/releases)
[![GPL-2.0](https://img.shields.io/badge/License-GPL--2.0-blue?style=flat-square)](LICENSE)
[![Languages](https://img.shields.io/badge/WebUI-Русский%20%2F%20English-3bba9d?style=flat-square)](#webui--languages)

[Русская версия](README.md) · **English**

[![Download latest ZIP](https://img.shields.io/badge/⬇_Download_latest_version-ZIP-2e9c82?style=for-the-badge)](https://github.com/Mobilelegends74/hma-uidfake/releases/download/v0.3.16/hma-uidfake-fork-0.3.16-test.zip)

[![Kernel patches](https://img.shields.io/badge/Kernel_patches-Built--in_UIDFAKE-6d43d6?style=for-the-badge&logo=linux&logoColor=white)](https://github.com/Mobilelegends74/hma-uidfake/tree/kernel-patches)

[Release notes](https://github.com/Mobilelegends74/hma-uidfake/releases) · [Report an issue](https://github.com/Mobilelegends74/hma-uidfake/issues) · [GitHub Actions](https://github.com/Mobilelegends74/hma-uidfake/actions)

</div>

## Overview

Development uses the permanent `develop` branch; `main` holds published code and `kernel-patches` holds standalone patches. [Branch and source archive](docs/branches.md).

**HMA UID Fake fork** is an Android kernel module that complements **HMA / HMA-OSS** app hiding. A package can be hidden from Android's PackageManager while still leaving detectable UID traces through kernel system calls. This module synchronizes HMA / HMA-OSS policies to the kernel and changes selected UID lookup results **only for configured callers**.

It is an additional privacy layer, **not a replacement for HMA-OSS**. The fork builds upon [KodateMitsuru/hma-uidfake](https://github.com/KodateMitsuru/hma-uidfake), with open WebUI/configuration work adapted from [SHuiTU68/hma-uidfake](https://github.com/SHuiTU68/hma-uidfake). Maintained by [Mobilelegends74](https://github.com/Mobilelegends74), with upstream attribution and licensing preserved.

## Features

| Feature | Details |
|---|---|
| **Kernel UID filtering** | Adjusts `getpriority`, `setpriority`, `ioprio_get`, and `ioprio_set` results according to the caller's hiding policy. |
| **HMA / HMA-OSS integration** | Imports configuration, presets and effective hiding rules; synchronizes updates. |
| **Native + compat hooks** | Native and 32-bit compatibility interception where supported by the kernel. |
| **Caller identity** | Tracks the application associated with a process in supported isolated-process and UID-changing scenarios. |
| **WebUI in KernelSU** | Kernel status, active rule count, per-app rules, templates, themes, JSON backup/restore, and contextual `?` explanations on 12 screens. |
| **Russian / English explanations** | Feature names stay in English. Contextual `?` help, About / How it works explanations and the compact KernelSU status follow **Android's system language at installation time**; other languages use English. |
| **Minimal module status** | The KernelSU module list displays only `✅ Working` or `❌ Not working`, with details available inside WebUI. |
| **Automatic start** | Loads the selected KMI build after one normal reboot, without a manual `allow_load` gate. |
| **8 KMI targets + 6.12.38 ABI** | Android 12 through Android 17 GKI/KMI builds plus a separate 6.12.38 ABI variant. |

## Requirements and supported KMIs

Requires **ARM64**, root with **KernelSU / KernelSU Next** capable of loading an LKM, and a kernel matching one of the included targets. HMA or HMA-OSS is recommended for importing existing app visibility rules; independent rules are optional. Keep a known boot-image backup/recovery path.

| Android branch | Kernel KMI |
|---|---|
| Android 12 | `android12-5.10` |
| Android 13 | `android13-5.10`, `android13-5.15` |
| Android 14 | `android14-5.15`, `android14-6.1` |
| Android 15 | `android15-6.6` |
| Android 16 | `android16-6.12` |
| Android 17 | `android17-6.18` |

**Note:** passing CI for a given KMI does not prove compatibility with every vendor kernel. The installer refuses unmatched or ambiguous KMI strings instead of force-loading a different kernel module.

## Installation

Since **0.3.7-fork**, custom kernels without an Android suffix can be identified using CRC references from build-ID-verified live modules. Only a unique candidate with no observed CRC mismatch is selected; insufficient evidence is rejected. Kernel admission checks are never bypassed. See [changes](CHANGELOG.md).

1. [**Download the latest flashable ZIP**](https://github.com/Mobilelegends74/hma-uidfake/releases/download/v0.3.16/hma-uidfake-fork-0.3.16-test.zip).
2. In KernelSU Next, open **Modules** → **Install from storage** and select the ZIP.
3. The installer detects both the kernel KMI and the Android **system language**. Russian Android → Russian help, About text and status; all other languages → English. Feature labels remain in English.
4. Perform **one normal reboot**. No `allow_load` file and no second reboot are required.
5. Open the module's WebUI in KernelSU Next and verify the active kernel hooks and rule count.

To update, install the new ZIP over the old module and reboot; kernel hooks cannot be safely hot-swapped. Reinstallation also refreshes the selected WebUI language if your Android system language has changed.

## WebUI & languages

The WebUI shows the **actual number of live kernel policy pairs**, rather than a zero from an unrelated, fresh `sync-tool --status` instance. The status **4 native / 4 compat** describes syscall hook counts, not hidden UID counts.

Search and manage apps, create and edit templates, copy and restore policies, change the theme, and inspect the current rule source. All major settings and confirmation dialogs are localized.

**Rule source:** HMA/HMA-OSS is the default. Explicitly select **Manual** to apply independent rules. Saving or importing JSON does not switch modes; saved manual settings remain inactive in external mode. If the selected config disappears, the previous UID policy is cleared. Technical package identifiers and config keys are never translated.

**Save diagnostics:** the bottom home-menu button exports logs, configs and kernel details to an archive in Download. Nothing is uploaded automatically. The archive contains package names and rules; share it privately.

**Manual inject:** shown when the kernel module is absent. Select a packaged KMI and confirm **Accept risk — load**. The selected `.ko` reaches the kernel loader without UIDFAKE's KMI/ABI preflight; kernel admission checks still apply and may reject it. This is a one-shot attempt, not flashing a kernel. Normal automatic selection runs again after reboot. Compatibility is not guaranteed.

**Release 0.3.16:** an additional 6.12.38 ABI variant, path_put dependency fix, aligned Manual inject choices filtered by the running Linux branch, and stable-only root-manager updates. [Changes](docs/releases/v0.3.16-fork.md).

**Updates:** install this ZIP manually over older versions and reboot. Compatible root managers will offer subsequent ordinary releases with a higher versionCode; prereleases are excluded.

## Compatibility and limitations

User-side testing confirmed successful LKM loading and rule synchronization on **OnePlus 13R (Android 15, android14-6.1)** and **REDMAGIC NX769J (Android 15, android14-6.1)** with KernelSU Next and HMA-OSS. The reported Duck Detector checks produced zero Danger/Warning findings with appropriate HMA-OSS configurations.

This is **not a guarantee against other detectors**. Results depend on firmware, injector/backend mode, and the API being examined. HMA-OSS LSPosed may separately expose inconsistent PackageManager exception semantics; UID Fake's kernel hooks cannot solve that API-level problem.

A separate experimental [HMA-OSS LSPosed PM Exception Fix APK](https://github.com/Mobilelegends74/hma-uidfake/releases/tag/hma-oss-lsposed-oss162-pmfix-v1) is available. **That APK is not this KernelSU ZIP.**

## Recovery and safety

- Kernel interception can crash or bootloop incompatible kernels. Back up the boot image and ensure recovery is available before installing.
- To **disable autoload on the next boot**: `su -c 'touch /data/adb/hma-uidfake/disable_load'`. Remove the marker and reboot to restore autoload. This does not unload already active kernel hooks.
- Never force-load a `.ko` for a different KMI.
- Manual injection of an incompatible `.ko` can hang or reboot the device and lose unsaved data, especially on kernels that ignore CRC mismatches. Prepare a recovery path first.
- This module does not hook Java PackageManager, hide launcher icons, or replace general HMA/HMA-OSS framework filtering.

## Documentation

[Changelog](CHANGELOG.md) · [Install/recovery notes](module/README.md) · [Build](docs/build.md) · [Design](docs/design.md) · [Issues](https://github.com/Mobilelegends74/hma-uidfake/issues)

When reporting an issue, include reproduction steps and the archive from **Save diagnostics**, after reviewing it for private data.

## Credits and license

- [KodateMitsuru](https://github.com/KodateMitsuru/hma-uidfake) — original kernel module.
- [SHuiTU68](https://github.com/SHuiTU68/hma-uidfake) — open WebUI and native policy management.
- [Mobilelegends74](https://github.com/Mobilelegends74) — KMI compatibility builds, WebUI fixes, autoload restoration, RU/EN localization and fork maintenance.

Distributed under [GNU GPL-2.0](LICENSE); third-party components retain their respective notices and licenses.

---
<div align="center"><b>HMA UID Fake fork</b> · Kernel-level UID privacy · Open source · RU / EN</div>
