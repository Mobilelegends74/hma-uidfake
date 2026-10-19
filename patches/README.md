# Полные патчи / Complete reference patches

**Выберите один способ интеграции и один соответствующий патч. Не применяйте все файлы.**

Каждый файл включает нативные исходники, внутренние заголовки, Kconfig/Kbuild и все точки вызова. Требуется отдельно включить CONFIG_HMA_UIDFAKE_BUILTIN=y и добавить Android companion по [инструкции](../docs/native/INTEGRATION_RU.md).

| Файл | Исходная база для dry-run / Exact reference |
|---|---|
| [gki-android14-6.1.145.patch](reference/gki-android14-6.1.145.patch) | android/kernel/common, android14-6.1.145_r00, commit 266f523db5be6cf93b3a2c4f58251dfa65768e35 |
| [oneplus-android14-6.1.118.patch](reference/oneplus-android14-6.1.118.patch) | OnePlusOSS/android_kernel_common_oneplus_sm8650, commit 7b9a23054ab0ada2886ed34a2137b9d0312891ff, branch oneplus/sm8650_v_15.0.0_oneplus_13r |

These are **clean-tree reference patches**, not all-purpose patches for every vendor source. They use a clean tree's free KABI slot. Droidspaces/SYSVIPC/SLIM_SCHED can consume it: for modified trees use scripts/uidfake-native-integrate.py **after** those feature patches. The integrator exports a new complete patch for your exact pre-integration state and refuses unsafe/missing anchors.

Проверка контрольных сумм из корня комплекта:

```bash
sha256sum --check patches/SHA256SUMS
python3 scripts/uidfake-test-patches.py
```

CI скачивает только нужные файлы этих двух зафиксированных исходных баз, применяет патчи без fuzz, проверяет KABI и повторное применение интегратора. Это не полная сборка каждого ядра. Reference commits here describe patch-test fixtures, **not pinned KernelSU/SUSFS dependencies**.

Рабочие полные интеграции с дополнительными feature-патчами:

- [OnePlus 13R 6.1.118 UIDFAKE](https://github.com/Mobilelegends74/OnePlus_KernelSU_SUSFS/releases/tag/op13r-6.1.118-2026-10-19-build90-1-uidfake).
- [GKI 6.1.145 UIDFAKE](https://github.com/Mobilelegends74/GKI_KernelSU_SUSFS/releases/tag/gki-android14-6.1.145-2026-10-19-build53-1-uidfake).

Their AnyKernel3 ZIPs contain the generated UIDFAKE patch for their actual feature-patched tree. Do not mistake that context-specific patch for the clean reference snapshot. Source origin/license: [NOTICE](../uidfake/NOTICE), GPL-2.0.
