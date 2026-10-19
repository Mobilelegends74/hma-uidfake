# 0.3.6-fork TEST-KMI — custom kernel identification

Historical test-branch instructions: the device-tested implementation was promoted to **0.3.7-fork**. For the published package use [release instructions](releases/v0.3.7-fork.md); references below to unchanged main/releases describe the earlier TEST-KMI stage.

Тестовая ветка: `test/kmi-custom-kernel-v036`. Это не новый релиз.
ZIP и имя модуля помечены **TEST-KMI**. Main, теги и v0.3.6-fork не изменяются.

## Выбор KMI и допуск ядром — разные этапы

1. Android KMI берётся из release работающего ядра (`uname`, release-токен
   `/proc/version`) или `CONFIG_LOCALVERSION` из `/proc/config.gz`.
2. Если этих сведений нет, ищутся системные `.ko` в стандартных каталогах
   `/vendor_dlkm`, `/system_dlkm`, `/vendor`, `/odm`, `/system`. Файл учитывается
   только если его `.note.gnu.build-id` совпадает побайтно с note **уже загруженного**
   модуля в `/sys/module/<name>/notes/.note.gnu.build-id`. Старый файл в vendor,
   имя устройства, версия Android/SDK или исходный BRANCH доказательством не являются.
3. Требуется единственная непротиворечивая Android/KMI-ветка и соответствующий
   пакетный ARM64 ELF с именем `hma_uidfake`, той же веткой и Linux major.minor,
   `modversions` и неповреждённой таблицей CRC, содержащей `module_layout`.
   Каждому сильному импортируемому символу также должна соответствовать CRC-запись.
   KMI бинарника записывается отдельной `.modinfo uidfake_kmi`: DDK vermagic иногда
   не содержит Android-ветку. При наличии обоих полей они не должны противоречить.
4. Выбранный **неизменённый** файл передаётся `finit_module(..., flags=0)`.
   Ядро проверяет vermagic, CRC импортируемых символов, подпись и политики загрузки
   до вызова инициализации модуля. Отказ не ведёт к попытке другой KMI.

### Дополнение TEST-KMI: ABI-отпечаток без Android-суффикса

Если Android-ветку не указывает даже vermagic проверенных системных LKM,
детектор сопоставляет CRC с пакетными кандидатами той же Linux major.minor:

- Нужен `CONFIG_MODVERSIONS=y` из работающего ядра.
- Reference должен быть ARM64, иметь полную таблицу CRC, совпадающий live build-id
  и доступный `/sys/module/<name>/taint` **без F** (forced load).
- CRC всех принятых references объединяются; конфликт хотя бы одного символа
  блокирует вывод (`ABI_CONFLICT`). Одинаковые загруженные модули не учитываются дважды.
- Для кандидата обязательны совпадение `module_layout`, как минимум 8 общих CRC
  и отсутствие любых обнаруженных несовпадений. 8 — консервативный минимальный
  порог идентификации, **не доказательство полной совместимости**.
- Должен остаться ровно один кандидат; при нескольких совпадениях нет выбора
  по количеству совпавших символов или номеру Android. Недостаточные данные
  дают `ABI_UNCONFIRMED`, несколько кандидатов — `KMI_AMBIGUOUS`.
- Символы, отсутствующие в reference, явно считаются `uncovered`. Их и остальные
  ограничения проверяет штатный загрузчик ядра; данные `.ko` не изменяются.

`state/kmi.log` теперь содержит счётчики сканирования и для каждого кандидата
`common`, `mismatched`, `uncovered`, `module_layout`, а также имена первых
несовпавших символов. При `KERNEL_REJECTED` остаются прежние строгие ограничения.
Эта процедура выбирает кандидат сборки, а не устанавливает историческую ветку
исходников и не подтверждает работу хуков на OEM-ядре.

Таким образом, идентификация ветки ещё **не означает бинарную совместимость**.
Только успешный штатный допуск означает прохождение проверок загрузчика ядра;
он также не доказывает правильность всех хуков и OEM-изменённых внутренних структур.
Компиляция и host-тесты не заменяют проверку на устройстве.

В тестовой сборке дополнительно исправлена среда DDK: восстановлены отключённые
ею `modpost check_exports` и привязка символов к экспортирующему модулю. Иначе
получались пустые CRC-секции, непригодные для заявленной строгой проверки.
Сборка не подавляет ошибки отсутствующих экспортов и не выдумывает CRC.

Все восемь KMI остаются в ZIP и переопределяются при каждой загрузке, в том числе
после OTA. Один обычный reboot, без ручного allow_load. Уже загруженный модуль
не выгружается и не заменяется на лету. HMA/HMA-OSS синхронизация не менялась.

## Защитные ограничения

- Нет выбора по одному `5.15`, даже если пакет содержит лишь один такой файл.
- Нет привязки к Xiaomi, LineageOS, vermeer или номеру 5.15.211.
- Нет принудительного insmod, игнорирования CRC/signature, правки vermagic,
  преобразования импортов в абсолютные адреса или перебора `.ko` загрузкой.
- TEST-KMI boot path **не вызывает** `ksud insmod` или bundled lkmloader:
  их переписывающие механизмы не используются. Legacy binary пока остаётся
  в пакете для сохранения структуры сборки, но не вызывается скриптами.
- Если KMI неизвестен, неоднозначен или метаданные недоступны, загрузки нет.
  Не распаковывайте/переименовывайте `.ko` для обхода этого ограничения.
- Сжатые системные reference `.ko.xz/.ko.zst` сейчас не декодируются; отсутствие
  доступных проверяемых данных приводит к безопасному отказу, не догадке.

## Диагностика

Причина появляется непосредственно в описании модуля и сохраняется независимо
от запуска sync-tool:

- `state/kmi.log` — результат определения ветки и источники доказательств;
- `state/selected-kmi` — текущий выбор; очищается перед повторным определением;
- `state/kernel-load.log` — errno штатного загрузчика;
- `state/load.reason` — последняя причина отказа;
- `state/sync.log` — журнал загрузки/хуков/синхронизации.

`KMI_UNKNOWN`: нет надёжной ветки; `KMI_AMBIGUOUS`: сведения противоречат друг другу;
`KMI_MISSING`: нужного файла нет; `MODULE_METADATA`/`MODULE_ELF`: повреждённый или
несоответствующий кандидат; `MODVERSIONS`: ядро отключило LKM/CRC;
`KERNEL_REJECTED`: ядро отказало, подробности в dmesg. Успех определяется не одним
наличием LKM: sync-tool продолжает проверять активные хуки/политику.

## Проверка на LineageOS 23.2 / vermeer

1. Сохраните рабочий ZIP 0.3.6-fork и способ отключения модуля через recovery/KernelSU.
   Установите ZIP с **TEST-KMI** из Artifacts, затем выполните обычную перезагрузку.
2. Проверьте описание модуля. Не меняйте KMI вручную, не запускайте другие loaders.
3. Соберите от root:

```sh
uname -a
cat /proc/version
cat /data/adb/modules/hma-uidfake/state/kmi.log
cat /data/adb/modules/hma-uidfake/state/selected-kmi
cat /data/adb/modules/hma-uidfake/state/kernel-load.log
tail -n 160 /data/adb/modules/hma-uidfake/state/sync.log
lsmod | grep hma_uidfake
dmesg | grep -iE 'uidfake|module_layout|version magic|disagrees about version|Unknown symbol|signature'
```

4. Если загрузился, измените правило HMA-OSS для тестового приложения, перезапустите
   его и проверьте обновление политики и нужные UID-проверки; пришлите логи и результат.
5. Если `KMI_UNKNOWN`, пришлите kmi.log: `BRANCH=android13-5.15` и `KMI_GENERATION=8`
   из LineageOS исходников — подсказка сопровождающему, **не разрешение грузить**
   бинарник DDK. Может потребоваться LKM, собранный именно для установленного ядра.
6. При `KERNEL_REJECTED` не используйте force; нужны errno и соответствующие строки dmesg.

Отключение следующей загрузки: `su -c 'touch /data/adb/hma-uidfake/disable_load'`.
Это не выгружает уже работающий LKM. Для полного возврата к прежней версии
переустановите сохранённый ZIP и перезагрузитесь.

## Автоматические проверки

`python3 scripts/test-kmi.py`: ELF fixtures всех восьми KMI, кастомный uname,
доказанная/ложная provenance, конфликт веток, неверные ABI/архитектура/CRC metadata,
обрезанный ELF, отсутствие кандидата, mock отказа ядра без fallback. В pack job
дополнительно проверяются настоящие `.ko` из восьми DDK-сборок. Host-тесты не вызывают
реальную загрузку модулей. `.github/workflows/build.yml` публикует **только Artifacts**,
с `contents: read`; шагов создания релиза/тега в тестовом workflow нет.

English: automatic KMI discovery requires explicit running-kernel metadata or a
build-id-verified already-loaded system module. A unique identity selects an intact
ARM64 versioned candidate; the kernel then performs normal admission checks through
finit_module with zero flags. No forced fallback is permitted. Unsupported or
insufficient evidence fails closed with diagnostics; device validation is still required.
