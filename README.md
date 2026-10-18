<div align="center">

<img src="docs/assets/banner.svg" alt="HMA UID Fake fork" width="820">

# HMA UID Fake fork

**Ядровой модуль для KernelSU Next, дополняющий скрытие приложений HMA / HMA-OSS.**

[![Build](https://img.shields.io/github/actions/workflow/status/Mobilelegends74/hma-uidfake/build.yml?branch=main&label=Build&style=flat-square)](https://github.com/Mobilelegends74/hma-uidfake/actions/workflows/build.yml)
[![Latest Release](https://img.shields.io/github/v/release/Mobilelegends74/hma-uidfake?label=Latest%20release&style=flat-square)](https://github.com/Mobilelegends74/hma-uidfake/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/Mobilelegends74/hma-uidfake/total?label=Downloads&style=flat-square)](https://github.com/Mobilelegends74/hma-uidfake/releases)
[![License](https://img.shields.io/badge/License-GPL--2.0-blue?style=flat-square)](LICENSE)
[![Architecture](https://img.shields.io/badge/Arch-arm64-5a81d8?style=flat-square)](#требования)
[![Languages](https://img.shields.io/badge/WebUI-Русский%20%2F%20English-3bba9d?style=flat-square)](#веб-интерфейс-ruen)

**Русский** · [English](README_EN.md)

[![Скачать последнюю версию](https://img.shields.io/badge/⬇_Скачать_последнюю_версию-ZIP-2e9c82?style=for-the-badge)](https://github.com/Mobilelegends74/hma-uidfake/releases/latest/download/hma-uidfake-fork-latest.zip)

[Релизы и изменения](https://github.com/Mobilelegends74/hma-uidfake/releases) · [Сообщить о проблеме](https://github.com/Mobilelegends74/hma-uidfake/issues) · [GitHub Actions](https://github.com/Mobilelegends74/hma-uidfake/actions)

</div>

## О проекте

**HMA UID Fake fork** — модуль для ядра Android, который дополняет HMA и HMA-OSS. Приложения могут проверять существование других приложений не только через PackageManager, но и через системные вызовы, связанные с UID. Даже когда HMA скрывает пакет на уровне Android, ответы ядра способны выдать его существование.

Модуль синхронизирует правила HMA / HMA-OSS с ядром и изменяет ответы отдельных системных вызовов **для приложений, которым положено не видеть выбранные UID**. Это дополнительный уровень приватности, а не замена HMA-OSS.

Развитие форка основано на открытом исходном проекте [KodateMitsuru/hma-uidfake](https://github.com/KodateMitsuru/hma-uidfake); WebUI и конфигурационный бэкенд адаптированы из открытой ветки [SHuiTU68/hma-uidfake](https://github.com/SHuiTU68/hma-uidfake). Исправления совместимости, сборки и русская локализация сопровождаются [Mobilelegends74](https://github.com/Mobilelegends74). Исходные лицензии и сведения об авторах сохраняются.

## Возможности

| Функция | Описание |
|---|---|
| **Фильтрация UID в ядре** | Корректировка результатов `getpriority`, `setpriority`, `ioprio_get` и `ioprio_set` для скрываемых UID согласно правилам. |
| **HMA и HMA-OSS** | Считывание конфигурации и применяемых пресетов; автоматическая синхронизация политики. |
| **Native + compat** | Поддержка основных и 32-битных совместимых перехватов там, где их позволяет установить ядро. |
| **Контекст приложения** | Сопоставление правил с исходным приложением процесса, включая определённые случаи изолированных процессов и смены UID. |
| **WebUI без отдельного APK** | Состояние ядра, число активных правил, управление приложениями, шаблоны и настройки. |
| **Русский / English** | Язык WebUI выбирается по **системному языку Android при установке**; все прочие языки — английский. |
| **Резервное копирование** | Импорт, экспорт и ручное редактирование JSON-конфигурации; запись через `sync-tool`. |
| **Автоматический запуск** | Загрузка подходящего LKM при обычной перезагрузке после установки, без ручного `allow_load`. |
| **8 вариантов KMI** | Ядра GKI / KMI от Android 12 до Android 17; автоматический выбор совместимой сборки. |

## Требования

- Android с **root-доступом KernelSU / KernelSU Next** и возможностью загружать LKM.
- **ARM64** и ядро, соответствующее одному из поддерживаемых KMI.
- HMA или HMA-OSS — для автоматического импорта существующих правил скрытия. Возможны и самостоятельные правила.
- Перед установкой желательно иметь резервную копию загрузочного образа и способ восстановления после неудачной загрузки.

### Поддерживаемые KMI для сборки

| Ветка Android | KMI ядра |
|---|---|
| Android 12 | `android12-5.10` |
| Android 13 | `android13-5.10`, `android13-5.15` |
| Android 14 | `android14-5.15`, `android14-6.1` |
| Android 15 | `android15-6.6` |
| Android 16 | `android16-6.12` |
| Android 17 | `android17-6.18` |

**Важно:** наличие сборки для KMI не означает подтверждённую работу на каждом устройстве с этим ядром. OEM-изменения и несовместимые хуки могут препятствовать загрузке. Для неизвестного или неоднозначного KMI установщик **не подставляет другой модуль ядра автоматически**.

## Установка

1. Скачайте **[последний установочный ZIP](https://github.com/Mobilelegends74/hma-uidfake/releases/latest/download/hma-uidfake-fork-latest.zip)**.
2. Откройте KernelSU Next → **Модули** → **Установить из хранилища** и выберите ZIP.
3. Установщик определит KMI текущего ядра и **системный язык Android**. Русский Android → русский WebUI, любой другой язык → английский.
4. Перезагрузите устройство **один раз** обычным способом. Дополнительный файл `allow_load` создавать не нужно.
5. Откройте **WebUI** модуля в KernelSU Next и проверьте статус ядра и активные правила.

**Обновление:** устанавливайте новый ZIP поверх старой версии и перезагружайте устройство. Загруженный в ядро LKM нельзя безопасно заменить «на лету». При смене системного языка повторная установка обновит язык интерфейса.

## Веб-интерфейс (RU/EN)

Веб-интерфейс открывается непосредственно из KernelSU Next. Он показывает фактическое количество активных правил из ядра, а не счётчик нового процесса `sync-tool`. Показатель `Syscall hooks: 4 native, 4 compat` обозначает **число установленных перехватов**, не число скрытых UID.

Можно искать приложения, назначать правила, работать с именованными шаблонами, копировать и восстанавливать конфигурацию, переключать тёмную/светлую тему и изучать текущий источник правил.

> **Не меняйте источник случайно:** сохранение самостоятельной JSON-конфигурации создаёт отдельную политику с приоритетом над HMA/HMA-OSS. WebUI предупреждает об этом перед первым сохранением.

## Совместимость и проверки

Пользовательское тестирование проводилось на **OnePlus 13R (Android 15, android14-6.1)** и **REDMAGIC NX769J (Android 15, android14-6.1)** с KernelSU Next и HMA-OSS. В обоих случаях ядровой модуль загрузился, правила синхронизировались, а указанные проверки Duck Detector завершались без Danger/Warning при подходящей конфигурации HMA-OSS.

Это **не гарантия необнаружимости** другими приложениями: результат зависит от конкретной прошивки, способа внедрения HMA-OSS и проверяемых API. Особенно важно: HMA-OSS через LSPosed может иметь отдельные несоответствия PackageManager, которые не решаются ядровым UID Fake.

Отдельный экспериментальный APK с исправлением HMA-OSS LSPosed: [HMA-OSS LSPosed PM Exception Fix](https://github.com/Mobilelegends74/hma-uidfake/releases/tag/hma-oss-lsposed-oss162-pmfix-v1). **Это другое приложение, не установочный ZIP UID Fake.**

## Безопасность и восстановление

- Модуль перехватывает системные вызовы ядра: несовместимость способна вызвать нестабильность или циклические перезагрузки.
- Для отключения загрузки при следующем запуске: `su -c 'touch /data/adb/hma-uidfake/disable_load'`. Для возврата автозагрузки удалите маркер и перезагрузите телефон.
- Этот маркер **не выгружает уже загруженный модуль**. Если система не загружается, используйте предусмотренное KernelSU/recovery восстановление.
- Не изменяйте `ko/hma_uidfake.ko` вручную на модуль из другого KMI.
- Программа не заменяет Java-фильтрацию HMA-OSS; скрытие значков или пакетов от PackageManager в ядровом модуле не предусмотрено.

## Документация

[История изменений](CHANGELOG.md) · [Инструкция по установке и восстановлению](module/README.md) · [Сборка из исходников](docs/build.md) · [Архитектура](docs/design.md) · [GitHub Issues](https://github.com/Mobilelegends74/hma-uidfake/issues)

При сообщении об ошибке приложите версию Android, `uname -r`, модель устройства, версию KernelSU, сообщение из WebUI и обезличенный `state/sync.log`.

## Авторы и лицензия

- [KodateMitsuru](https://github.com/KodateMitsuru/hma-uidfake) — оригинальная ядровая реализация.
- [SHuiTU68](https://github.com/SHuiTU68/hma-uidfake) — открытые улучшения интерфейса и самостоятельной конфигурации.
- [Mobilelegends74](https://github.com/Mobilelegends74) — сборки и совместимость KMI, исправления статуса WebUI, автозагрузка, двуязычный интерфейс, поддержка форка.

Лицензия: **[GNU GPL-2.0](LICENSE)**. Перед изменением или распространением учитывайте условия используемых компонентов.

---
<div align="center"><b>HMA UID Fake fork</b> · Kernel-level UID privacy · Open source · RU / EN</div>
