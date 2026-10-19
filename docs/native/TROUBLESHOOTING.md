# Проверка, синхронизация и откат / Validation and recovery

[Русская инструкция](INTEGRATION_RU.md) · [English guide](INTEGRATION_EN.md)

## До прошивки / Before flashing

- Финальная .config содержит CONFIG_HMA_UIDFAKE_BUILTIN=y. Не только defconfig.
- Используйте Image, .config и System.map **одной сборки**; выполните record-build и verify-zip из инструкции.
- Проверьте KernelSU/SUSFS/NoMount и защитные опции, которые требуются вашему ядру; не отключайте их для обхода ошибок интеграции.
- Подготовьте резервную копию boot, правильный слот и проверенный способ восстановления, совместимые с устройством.
- Отключите прежний отдельный UIDFAKE LKM и перезагрузитесь перед использованием встроенного варианта. Нельзя безопасно заменить уже загруженный LKM нативным движком в той же сессии.

Use the final config, actual Image and System.map from one build. Confirm device/KMI/boot compatibility. Disable a previous UIDFAKE LKM and reboot; never run both implementations. Keep kernel protections and a tested recovery path.

## После перезагрузки / After reboot

В загруженном Android:

```bash
adb shell su -c 'cat /data/adb/uidfake-kernel/sync.log'
adb shell su -c '/data/adb/uidfake-kernel/sync-tool --status'
adb shell su -c '/data/adb/uidfake-kernel/sync-tool --get-config'
adb shell su -c 'getenforce'
```

Формат su/ADB может отличаться у вашего root. Статус проверяет связь с ядром, но не доказывает корректность правил. Не запускайте второй постоянный sync-tool рядом с supervisor. --once тоже обновляет политику; для диагностики начинайте с read-only --status/--get-config.

Log rotation: current attempt is sync.log, previous attempt is sync.previous.log. Inspect Android Logcat, kernel log and SELinux AVCs alongside the helper log. A “ready” string, zero exceptions or a hook-count field alone does not prove actual hiding.

Не публикуйте необработанные config.json, packages.xml и логи с личным списком приложений. Удалите чувствительные данные перед отчётом. Avoid sharing unredacted app inventories/configuration.

## Приоритет источников / Rule-source precedence

Companion выбирает первый подходящий источник:

1. /data/adb/hma-uidfake/config.json — явная самостоятельная политика.
2. /data/adb/modules/hma-uidfake/config.json — legacy native policy.
3. /data/user/0/com.tsng.hidemyapplist/files/config.json (с учётом /data/data bind-path).
4. /data/misc/hide_my_applist_*/config.json.
5. /data/system/hide_my_applist_*/config.json.

Существующая native-конфигурация имеет приоритет над HMA. Даже выключенный legacy-модуль может оставить config.json: это **не означает**, что ядро не работает. Сначала изучите --get-config и log, сделайте резервную копию; не удаляйте правила наугад. Для переноса конфигурации используйте безопасное резервное переименование по выбранному точному пути, а не рекурсивное удаление каталога.

An existing explicit native policy overrides app-derived rules. Disabling the old LKM does not remove its JSON. Inspect the active source before changing anything. user-0 paths do not guarantee all Android work/profile configurations are discovered.

## Типичные проблемы / Common failures

| Симптом / Symptom | Что проверить / Check |
|---|---|
| Missing source anchor | Kernel API/context differs; use source review, not forced patching. |
| No unconditional free KABI slot | Another feature consumed reservations; do not reuse conditional/occupied slots or silently change vendor ABI. |
| Header uidfake.h not found | Preserve component Makefile and include/ tree; use full integrator/patch and the correct kernel root. |
| CONFIG disappears after olddefconfig | Kconfig/parent entries, NET dependency, actual defconfig/fragment, downstream config overrides. |
| No generic-netlink family / connection refused | Correct boot slot/kernel, built-in init, incompatible/old kernel, duplicate legacy LKM, actual config and dmesg. |
| Helper starts then exits | Read sync.log/previous log; protocol, root, data unlock, ELF architecture and SELinux AVC. |
| No sync.log after reboot | service.d runner support, executable modes, service path and root ownership; copying a file is not a boot runner. |
| Wrong active rules | Native JSON precedence, selected HMA source, presets, package UID/inode changes and data availability. |
| Some isolated/compat cases differ | Confirm actual attribution and common syscall path; capture a minimal reproducible app/process case. |
| AnyKernel preflight aborts | Writable /data/adb, disabled legacy module, correct booted-root flashing environment and supported installer anchor. |
| SELinux denial | Identify the exact denied operation/domain and add minimal reviewed OEM policy; do not set permissive. |

## Функциональный чек-лист / Functional checklist

1. Boot/reboot and supervisor startup without manual actions.
2. Test one explicit caller/hidden target pair and a control app that must still see the target UID.
3. Edit HMA/HMA-OSS rules and presets; verify the actual effect updates without reinstalling a module.
4. Remove a rule and verify visibility returns. Explicit empty policy must revoke rules.
5. Exercise all relevant UID-query APIs, native/compat callers and isolated/app-zygote/system-zygote cases used by your workload.
6. Verify ordinary KernelSU/SUSFS/NoMount behavior and SELinux/CFI remain intact.

Положительный результат одного детектора не гарантирует необнаружимость. Native UIDFAKE does not replace Java/PackageManager hiding or guarantee universal detection bypass.

## Откат / Rollback

Встроенный компонент нельзя выгрузить через rmmod. Для полного отключения восстановите **свой совместимый сохранённый boot/kernel** штатным для устройства способом. Не прошивайте произвольный образ из другого релиза/устройства.

Чтобы отключить только автозапуск companion при следующем boot, переместите конкретный service-файл **вне** service.d, сохранив его как резервную копию:

```bash
adb shell su -c 'mv /data/adb/service.d/97-uidfake-kernel.sh /data/adb/uidfake-kernel/service.disabled'
```

Затем перезагрузитесь. Уже работающий процесс и уже загруженная политика от одного перемещения файла не исчезают. На следующем boot ядровая политика начинается пустой, если другой источник не запустит uploader. Не используйте для нативного режима legacy-маркер disable_load: его обслуживает старый LKM-инсталлятор, а не этот service.

This move disables the supplied adapter's **next** boot invocation; it neither unloads the built-in component nor immediately revokes a live policy. Back up any config before changing it. Do not recursively delete /data/adb or unrelated modules. Restore the service to its original path to re-enable this adapter after confirming a compatible native kernel.
