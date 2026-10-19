<div align="center">

<img src="docs/assets/banner.svg" alt="HMA UIDFAKE" width="820">

# HMA UIDFAKE — native kernel patches

**Built into Image: no .ko, LKM loader or separate UIDFAKE root module.**

[![Patch checks](https://img.shields.io/github/actions/workflow/status/Mobilelegends74/hma-uidfake/native-patches.yml?branch=kernel-patches&label=Patch%20checks&style=flat-square)](https://github.com/Mobilelegends74/hma-uidfake/actions/workflows/native-patches.yml)
[![GPL-2.0](https://img.shields.io/badge/License-GPL--2.0-blue?style=flat-square)](LICENSE)

[Русский](README.md) · **English**

[![Guide](https://img.shields.io/badge/Guide-Kernel_integration-6d43d6?style=for-the-badge&logo=linux&logoColor=white)](docs/native/INTEGRATION_EN.md)
[![Sources](https://img.shields.io/badge/Download-Sources_and_patches-2e9c82?style=for-the-badge)](https://github.com/Mobilelegends74/hma-uidfake/archive/refs/heads/kernel-patches.zip)
[![LKM branch](https://img.shields.io/badge/Back-LKM_module-555555?style=for-the-badge)](https://github.com/Mobilelegends74/hma-uidfake/tree/main)

</div>

## Contents

A kernel-builder kit: native UIDFAKE policy engine, complete patches, checked source integration, regression tests and automatic HMA/HMA-OSS rule synchronization.

The kernel component uses **CONFIG_HMA_UIDFAKE_BUILTIN=y**. A privileged Android userspace companion parses app configuration; the kernel cannot safely parse encrypted app files itself. Use the checksum-verified existing Android sync-tool or build it from the included sources.

**This branch is not a module-install ZIP or a universal flashable kernel.** LKM releases remain on [main](https://github.com/Mobilelegends74/hma-uidfake/tree/main). This branch never publishes releases or updates Latest; its CI has read-only permissions.

| Task | Entry point |
|---|---|
| Complete build, boot-sync and packaging instructions | [Integration guide](docs/native/INTEGRATION_EN.md) |
| Russian guide | [Инструкция](docs/native/INTEGRATION_RU.md) |
| Standalone reference patches | [Patch selection](patches/README.md) |
| Modified kernel trees | [Checked integrator](scripts/uidfake-native-integrate.py) |
| Native source, Kconfig and Kbuild | [uidfake/kernel](uidfake/kernel) |
| Architecture and compatibility limits | [Architecture](docs/native/ARCHITECTURE.md) |
| HMA configuration, diagnostics and rollback | [Troubleshooting](docs/native/TROUBLESHOOTING.md) |

## Validated integrations

- [OnePlus 13R, Android 14 / Linux 6.1.118](https://github.com/Mobilelegends74/OnePlus_KernelSU_SUSFS/releases/tag/op13r-6.1.118-2026-10-19-build90-1-uidfake).
- [GKI Android 14 / Linux 6.1.145](https://github.com/Mobilelegends74/GKI_KernelSU_SUSFS/releases/tag/gki-android14-6.1.145-2026-10-19-build53-1-uidfake).

Both integrations were compiled and the user reported working on their device. This is not validation of every hook, ROM, isolated-process case or architecture. Other Linux/Android versions still require independent full builds and device tests.

Before flashing, confirm device/KMI compatibility, back up boot and keep a tested recovery path. Do not load a separate UIDFAKE LKM alongside this built-in implementation. HMA/HMA-OSS retain their own injection requirements.

GPL-2.0. See [NOTICE](uidfake/NOTICE) for source and companion provenance.
