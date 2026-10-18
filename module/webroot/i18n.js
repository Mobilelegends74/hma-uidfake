/* HMA UID Fake fork. Installation captures Android's system language.
 * English is the source language; technical identifiers/JSON keys are unchanged.
 */
"use strict";
const HMA_UI_LOCALE = (() => {
  const stored = window.HMA_UIDFAKE_INSTALL_LOCALE;
  const browser = (navigator.languages && navigator.languages[0]) || navigator.language || "en";
  return /^ru(?:[-_]|$)/i.test(stored || browser) ? "ru" : "en";
})();
document.documentElement.lang = HMA_UI_LOCALE;
const HMA_RU = Object.freeze({
  "About": "О модуле",
  "Add rule": "Добавить правило",
  "All": "Все",
  "App rule": "Правило приложения",
  "Back": "Назад",
  "Backup & restore": "Резервное копирование",
  "Blacklist": "Чёрный список",
  "Blacklist hides the apps a rule lists. Whitelist hides every app except the ones a rule lists -- an app with a rule and nothing listed is hidden from everything, so check the rules before switching.": "Чёрный список скрывает выбранные приложения. Белый список скрывает всё, кроме выбранных. Если в правиле с белым списком ничего не указано, будут скрыты все приложения. Проверьте настройки перед переключением.",
  "Cancel": "Отмена",
  "Clear": "Очистить",
  "Config": "Конфигурация",
  "Copied": "Скопировано",
  "Copy": "Копировать",
  "Copy failed": "Не удалось скопировать",
  "Copy path": "Копировать путь",
  "Create": "Создать",
  "Current config": "Текущая конфигурация",
  "Dark": "Тёмная",
  "Default": "По умолчанию",
  "Default for apps without a mode of their own": "Режим для приложений без индивидуальных настроек",
  "Default mode": "Режим по умолчанию",
  "Delete template": "Удалить шаблон",
  "Done": "Готово",
  "For an app you name as a caller, the kernel reports the uids you list as free, so a scan of /proc or a uid lookup does not turn them up.": "Для выбранного приложения ядро сообщает, что UID указанных скрытых приложений отсутствуют. Так результаты проверки UID согласуются с заданными правилами.",
  "Hidden apps": "Скрываемые приложения",
  "Hide everything": "Скрыть всё",
  "Hide system apps": "Скрывать системные приложения",
  "Hide system apps too": "Скрывать и системные приложения",
  "How it works": "Как это работает",
  "Include system apps": "Включить системные приложения",
  "Independent policy change cancelled": "Переход на отдельные правила отменён",
  "It is a kernel-side guard. It does not hook the Java framework, so an app that asks PackageManager, ActivityManager or a ContentProvider for an installed package can still be told it exists. Hiding an icon, hiding from the launcher and per-hook switches are not implemented here. Treat this as one layer, not a complete hiding system.": "Это защита на уровне ядра. Модуль не перехватывает Java API: PackageManager, ActivityManager и ContentProvider могут сообщить об установленном приложении. Скрытие иконок, записей лаунчера и отдельные Java-хуки не реализованы. Используйте его как дополнительный слой, а не замену HMA-OSS.",
  "Kernel module": "Модуль ядра",
  "Kernel status unavailable": "Статус ядра недоступен",
  "Kernel-side uid guard. Apps you hide here look uninstalled.": "Защита UID на уровне ядра. Скрытые приложения выглядят отсутствующими при проверках UID.",
  "Licence": "Лицензия",
  "Light": "Светлая",
  "Loading...": "Загрузка...",
  "Manage apps": "Управление приложениями",
  "Members": "Участники",
  "Mode": "Режим",
  "Name": "Название",
  "New template": "Новый шаблон",
  "No packages. Is the module installed?": "Список приложений пуст. Проверьте установку модуля.",
  "No rule for this app.": "Для этого приложения правило не задано.",
  "No templates yet. A template is a named list of apps you can give to many rules.": "Шаблонов пока нет. Шаблон — это именованный список приложений, который можно использовать в нескольких правилах.",
  "Nothing matches": "Ничего не найдено",
  "Raw config": "JSON-конфигурация",
  "Remove rule": "Удалить правило",
  "Rename": "Переименовать",
  "Restore": "Восстановить",
  "Revert": "Отменить изменения",
  "Rule source": "Источник правил",
  "Rules active in kernel": "Правил в ядре",
  "Save": "Сохранить",
  "Saved": "Сохранено",
  "Saving will create an independent UID policy. It takes priority over the HMA/HMA-OSS config. Continue?": "Сохранение создаст отдельные правила UID. Они будут иметь приоритет над HMA/HMA-OSS. Продолжить?",
  "Search package or uid": "Поиск пакета или UID",
  "Search template": "Поиск шаблона",
  "Select apps": "Выбрать приложения",
  "Settings": "Настройки",
  "Source": "Исходный код",
  "Summary": "Сводка",
  "Syscall hooks": "Перехваты системных вызовов",
  "System": "Системные",
  "Template": "Шаблон",
  "Templates": "Шаблоны",
  "That template is gone.": "Этот шаблон больше не существует.",
  "The config has to be a JSON object": "Конфигурация должна быть JSON-объектом",
  "The screen layout follows the HMA-OSS app because these settings are familiar in that shape. It is an independent reimplementation for this module, not a copy of its code.": "Интерфейс использует знакомую структуру экранов HMA-OSS, но разработан отдельно для этого модуля без копирования исходников HMA-OSS.",
  "Theme": "Тема",
  "User": "Пользовательские",
  "Validate & save": "Проверить и сохранить",
  "Validate & write": "Проверить и записать",
  "What it does": "Что делает модуль",
  "What it does not do": "Чего модуль не делает",
  "Where the config lives": "Расположение настроек",
  "Whitelist": "Белый список",
  "With rule": "С правилами",
  "answering over kaux": "Связь с ядром через kaux",
  "copy or paste a config": "Копирование и восстановление настроек",
  "copy the config, paste one back": "Скопировать или восстановить настройки",
  "edit the JSON by hand": "Ручное редактирование JSON",
  "hide everything": "скрывать всё",
  "hide system apps from it too": "Также скрывать системные приложения",
  "module information": "Информация о модуле",
  "no ksu.exec: open this page from the KernelSU manager": "Нет доступа к ksu.exec: откройте страницу из KernelSU",
  "no rules yet": "Правил пока нет",
  "no templates yet": "Шаблонов пока нет",
  "none listed": "Ничего не выбрано",
  "none yet": "Пока нет",
  "not answering": "Нет ответа от ядра",
  "not in the package list": "Нет в списке приложений",
  "nothing hidden yet": "Пока ничего не скрывается",
  "off by default: a framework package made absent breaks a device": "По умолчанию выключено: скрытие системных пакетов может нарушить работу Android",
  "paste a config here": "Вставьте сюда конфигурацию",
  "this app sees no other app at all": "Это приложение не видит другие приложения",
  "version unknown": "Версия неизвестна",
  "what this does and does not cover": "Возможности и ограничения",
  "unavailable": "недоступно",
  "none (idle)": "нет (ожидание)",
  " (absent)": " (отсутствует)",
  " · system": " · системное",
  " · user": " · пользовательское",
  " app": " приложение",
  "rule": "правило",
  " rule": " правило",
  "APK inodes": "APK inode",
  "native": "основных",
  "compat": "совместимости",
  "name, e.g. banking": "Название, например bank",
  "A template with that name already exists": "Шаблон с таким названием уже существует",
  "none": "нет",
  "system": "системное",
  "Default follows the module-wide mode (": "По умолчанию используется общий режим модуля (",
  "). Blacklist hides what is listed; whitelist hides everything except what is listed.": "). Чёрный список скрывает перечисленные приложения; белый список скрывает все, кроме перечисленных.",
  "Config format version ": "Версия формата конфигурации ",
  ". Every change is written through sync-tool, which keeps one .bak and pushes the new policy to the kernel in the same step.": ". Каждое изменение сохраняется через sync-tool: создаётся резервная копия .bak и одновременно обновляются правила ядра.",
  " -- outside the modules directory, so a module update does not replace it. One .bak is kept beside it.": " — вне каталога модулей, поэтому обновление не удаляет настройки. Рядом сохраняется копия .bak.",
  "Not valid JSON: ": "Ошибка JSON: ",
  "install: package list unavailable": "Не удалось получить список приложений"
});
function t(value) {
  if (HMA_UI_LOCALE !== "ru" || typeof value !== "string") return value;
  if (Object.prototype.hasOwnProperty.call(HMA_RU, value)) return HMA_RU[value];
  const forms = (n, one, few, many) =>
    n % 10 === 1 && n % 100 !== 11 ? one :
    n % 10 >= 2 && n % 10 <= 4 && (n % 100 < 12 || n % 100 > 14) ? few : many;
  let m;
  if ((m = /^(\d+) apps? with a rule$/.exec(value)))
    return m[1] + " " + forms(+m[1], "приложение с правилом", "приложения с правилом", "приложений с правилами");
  if ((m = /^(\d+) apps?$/.exec(value)))
    return m[1] + " " + forms(+m[1], "приложение", "приложения", "приложений");
  if ((m = /^(\d+) templates?$/.exec(value)))
    return m[1] + " " + forms(+m[1], "шаблон", "шаблона", "шаблонов");
  if ((m = /^(\d+) listed$/.exec(value))) return "Выбрано: " + m[1];
  if ((m = /^(\d+) selected$/.exec(value))) return "Выбрано: " + m[1];
  if ((m = /^version (.+)$/.exec(value))) return "Версия " + m[1];
  if ((m = /^uid (\d+) · (system|user) app$/.exec(value)))
    return "UID " + m[1] + " · " + (m[2] === "system" ? "системное" : "пользовательское") + " приложение";
  if ((m = /^uid (\d+) · system · (.*)$/.exec(value)))
    return "UID " + m[1] + " · системное · " + t(m[2]);
  if ((m = /^uid (\d+) · (.*)$/.exec(value))) return "UID " + m[1] + " · " + t(m[2]);
  if ((m = /^(.+) \+ (\d+) templates?$/.exec(value)))
    return t(m[1]) + " + " + t(m[2] + " templates");
  return value;
}
