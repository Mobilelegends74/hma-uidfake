# Совместимость / Compatibility

Все **8 Android/KMI-целей ARM64** LKM-модуля представлены полными нативными патчами.
Общий движок один; контекст Android common и макросы KABI отличаются.
The policy engine is shared, but patch contexts and KABI APIs differ between trees.

| Android/KMI | Полный патч / Complete patch |
|---|---|
| android12-5.10 | [Patch](../../patches/reference/android12-5.10.patch) |
| android13-5.10 | [Patch](../../patches/reference/android13-5.10.patch) |
| android13-5.15 | [Patch](../../patches/reference/android13-5.15.patch) |
| android14-5.15 | [Patch](../../patches/reference/android14-5.15.patch) |
| android14-6.1 | [Patch](../../patches/reference/android14-6.1.patch) |
| android15-6.6 | [Patch](../../patches/reference/android15-6.6.patch) |
| android16-6.12 | [Patch](../../patches/reference/android16-6.12.patch) |
| android17-6.18 | [Patch](../../patches/reference/android17-6.18.patch) |

Точные LTS-коммиты: [targets.json](../../patches/targets.json). Два старых reference-патча
6.1 сохранены дополнительно; это не предел поддержки. These immutable commits are
source-test fixtures, **not pinned KernelSU/SUSFS dependencies**.

## Проверки / Validation levels

- Все 10 reference-баз: применение без fuzz, идемпотентность интегратора, размер
  и смещения поля KABI. Each patch is checked against its recorded source commit.
- Восемь DDK-образов `ghcr.io/ylarod/ddk:<KMI>-20260828`, как у LKM-модуля:
  реальная компиляция **CONFIG_HMA_UIDFAKE_BUILTIN=y** в built-in archive и
  kernel/sys.o, block/ioprio.o, kernel/user.o, kernel/cred.o, fs/open.o.
  This is native Kbuild compilation, not an external module or syntax-only test.
- Host-регрессии движка и netlink; проверка формата упаковки.

[Реальные результаты CI / Actual CI results](https://github.com/Mobilelegends74/hma-uidfake/actions/workflows/native-patches.yml?query=branch%3Akernel-patches).
DDK snapshots and recorded LTS reference snapshots are separate source checks;
the integrator handles each, without pretending they are identical commits.
Artifacts contain source patches, configs and reports, **not flashable kernels**.

**Это не полная линковка Image, ABI-сертификация восьми vendor-ядер или тест восьми устройств.**
Full Image builds and user-reported device tests exist only for the
[two 6.1 integrations](../../README_EN.md#validated-integrations).
Every additional vendor/ROM requires its normal full build, ABI checks and boot test.

## KABI

Older headers use `__GENKSYMS__`; host checks compare symbol representation and layout.
Newer 6.12/6.18 headers use DWARF-aware KABI unions: layout is checked, but full
gendwarfksyms/KMI certification requires the kernel's normal ABI build. Some
reserve fields are unconditional without CONFIG_ANDROID_KABI_RESERVE; integration
detects the actual header, not just a Linux version. Occupied slots are not reused.

Применяйте один подходящий патч либо [интегратор](../../scripts/uidfake-native-integrate.py),
а не всё сразу. Vendor/feature-деревья: интегратор после остальных task_struct-патчей.
[Инструкция RU](INTEGRATION_RU.md) · [Guide EN](INTEGRATION_EN.md).

HMA/HMA-OSS synchronization and protocol are shared across targets. Root boot
runners, SELinux access and config paths remain Android/OEM-dependent. KMI support
does not prove every user profile, isolated process or sandbox configuration.
This branch creates no releases/tags and leaves the LKM Latest untouched.
