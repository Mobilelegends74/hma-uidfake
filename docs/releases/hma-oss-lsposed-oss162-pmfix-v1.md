# HMA-OSS LSPosed — PM Exception Fix v1

**Модифицированный HMA-OSS для LSPosed**, основанный на официальной версии **oss-162**. Сборка устраняет некорректную обработку исключений Android PackageManager при скрытии приложений.

> **Важно:** это **APK приложения HMA-OSS со встроенным модулем LSPosed**, а не ZIP-модуль HMA UID Fake для KernelSU, и не обновление Zygisk-версии HMA-OSS.

### Исправления

- Исправлены перехваты `getPackageInfoInternal` и `getApplicationInfoInternal`.
- Четыре перехвата LSPosed в обеих ветках обработки версий Android переведены с `hookBefore` на `hookAfter`.
- Системные исключения, включая `SecurityException`, сохраняются: если оригинальная функция завершилась ошибкой, модуль не подменяет её результат на `null`.
- Исправлено расхождение поведения API `getPackageInfo`/`getApplicationInfo`, проявлявшееся в Duck Detector как `Platform behavior mismatch observed` при работе HMA-OSS через LSPosed.
- Остальные механизмы HMA-OSS не менялись; HMA UID Fake, его хуки и правила синхронизации также не менялись.

### Проверка на устройстве

**REDMAGIC (NX769J), Android 15, ядро 6.1.145-android14, KernelSU Next, HMA UID Fake + HMA-OSS LSPosed.**

До исправления: Duck Detector фиксировал предупреждение `Dangerous Apps` из-за повторяемых несоответствий `getPackageInfo` и `getApplicationInfo` (по 8 целей), при этом LKM HMA UID Fake уже был загружен, 83 пары правил синхронизированы.

**После установки модифицированного APK пользователь подтвердил прохождение всех проверок Duck Detector без обнаружений.** Это полевой тест на одной конфигурации, а не гарантия результата на всех устройствах или во всех проверяющих приложениях.

### Установка

1. Скачайте файл `HMA-OSS-LSPosed-oss162-PM-ExceptionFix-v1.apk` из раздела **Assets** этого релиза.
2. Перед заменой HMA-OSS **экспортируйте конфигурацию и сохраните резервную копию**.
3. APK подписан **тестовым ключом GitHub Actions**, отличающимся от официального. Поэтому Android может запретить установку поверх официального HMA-OSS с тем же идентификатором пакета. Не удаляйте работающее приложение, пока не убедитесь, что сможете восстановить настройки.
4. После установки активируйте HMA-OSS в LSPosed и убедитесь, что включена необходимая область видимости (System Framework). Не запускайте одновременно два бэкенда HMA-OSS (LSPosed и Zygisk).
5. Перезагрузите устройство и проверьте работу скрытия приложений в своём сценарии.

### Исходный код и происхождение изменений

- Оригинал: [frknkrc44/HMA-OSS](https://github.com/frknkrc44/HMA-OSS), тег [oss-162](https://github.com/frknkrc44/HMA-OSS/releases/tag/oss-162), коммит `b52a10d609d069e82e578d97b4c25a8f4c5d822a`.
- [Скрипт воспроизводимого изменения](https://github.com/Mobilelegends74/hma-uidfake/blob/experiment/hmaoss-lsposed-pm-exceptions/tools/hma_oss_lsposed_patch.py).
- [Описание механизма ошибки](https://github.com/Mobilelegends74/hma-uidfake/blob/experiment/hmaoss-lsposed-pm-exceptions/docs/hma-oss-lsposed-pm.md).
- Сборка осуществляется из исходников оригинального проекта; сторонние бинарные компоненты не добавлялись.

### Совместимость и ограничения

- **Подтверждено пользователем:** REDMAGIC NX769J / Android 15 / HMA-OSS LSPosed + HMA UID Fake.
- На других устройствах, прошивках и версиях Android результат не подтверждён.
- Это отдельная поддерживаемая сообществом модификация LSPosed-ветки. Официальные новые версии HMA-OSS продолжают развиваться в направлении Zygisk.
- Оригинальные права на HMA-OSS и условия лицензии сохраняются.

**Тип релиза:** экспериментальный, с успешным пользовательским тестом.
