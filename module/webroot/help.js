/* HMA UID Fake fork: contextual help. Read-only descriptions, never changes policy. */
"use strict";
window.HMA_CONTEXT_HELP = Object.freeze({
  "home": {
    "en": {
      "title": "Module overview",
      "intro": "The home screen shows whether the kernel module is responding and which policy is active. Its rule count is read from the kernel, not from a temporary userspace counter.",
      "sections": [
        {
          "title": "Kernel module",
          "text": "Reports whether the kernel answers through kaux. A successful response does not by itself prove that every requested hook was installed."
        },
        {
          "title": "Rules active in kernel",
          "text": "The count of live caller–target UID pairs supplied by HMA, HMA-OSS or a standalone policy. It is NOT the number of manually configured WebUI rules."
        },
        {
          "title": "Rule source",
          "text": "The active config source. When it says hma-oss, 'Manage apps: no rules yet' is normal: the WebUI shows only its own independent app rules."
        },
        {
          "title": "Syscall hooks / APK inodes",
          "text": "Native and compat indicate hooked syscall table entries. APK inodes count tracked application code files, not hidden apps."
        },
        {
          "title": "Main menu",
          "text": "Choose HMA/HMA-OSS or Manual explicitly on the home screen. Saving manual rules never switches mode. In HMA-OSS mode they remain inactive saved data; deleting the selected config clears the previous UID policy."
        }
      ]
    },
    "ru": {
      "title": "Главный экран",
      "intro": "Здесь показано, отвечает ли модуль ядра и откуда загружены правила. Количество правил берётся непосредственно из ядра, а не из временного счётчика приложения.",
      "sections": [
        {
          "title": "Kernel module",
          "text": "Показывает наличие ответа ядра через kaux. Сам по себе ответ не означает, что все требуемые перехваты установлены."
        },
        {
          "title": "Rules active in kernel",
          "text": "Число активных пар UID «проверяющее приложение — скрываемое приложение», полученных из HMA, HMA-OSS или отдельных правил. Это НЕ количество правил, вручную созданных в WebUI."
        },
        {
          "title": "Rule source",
          "text": "Источник политики. Если выбран hma-oss, надпись «Manage apps: no rules yet» нормальна: здесь отображаются только самостоятельные правила WebUI."
        },
        {
          "title": "Syscall hooks / APK inodes",
          "text": "Native и compat — количество перехваченных системных вызовов. APK inodes — отслеживаемые файлы кода приложений, а не число скрытых программ."
        },
        {
          "title": "Разделы",
          "text": "Источник выбирается отдельно на главном экране: HMA/HMA-OSS или Ручной. Сохранение ручных правил не переключает режим. В режиме HMA-OSS они остаются неактивной резервной копией; при удалении выбранного конфига старая UID-политика очищается."
        }
      ]
    }
  },
  "apps": {
    "en": {
      "title": "Manage apps",
      "intro": "Creates or removes independent per-app hiding rules. Choose the caller (the app performing the check) and then choose which target applications it must not see.",
      "sections": [
        {
          "title": "Important when using HMA-OSS",
          "text": "Existing HMA-OSS rules are synchronized into the kernel automatically. They are not listed as editable WebUI entries. The list can say 'no rules yet' even when the kernel has active rules."
        },
        {
          "title": "Saving a rule",
          "text": "Choose HMA/HMA-OSS or Manual explicitly on the home screen. Saving manual rules never switches mode. In HMA-OSS mode they remain inactive saved data; deleting the selected config clears the previous UID policy."
        },
        {
          "title": "Search and filters",
          "text": "Search by package name or UID. All, With rule, User and System only filter the displayed list; they do not change existing kernel rules."
        }
      ]
    },
    "ru": {
      "title": "Manage apps — управление",
      "intro": "Здесь создаются самостоятельные правила: выбирается проверяющее приложение (кто ищет), затем приложения, которые оно не должно видеть.",
      "sections": [
        {
          "title": "Важно при HMA-OSS",
          "text": "Правила HMA-OSS автоматически поступают в ядро, но не появляются здесь как редактируемые записи. Поэтому «no rules yet» может отображаться одновременно с десятками активных правил ядра."
        },
        {
          "title": "Что произойдёт при сохранении",
          "text": "Источник выбирается отдельно на главном экране: HMA/HMA-OSS или Ручной. Сохранение ручных правил не переключает режим. В режиме HMA-OSS они остаются неактивной резервной копией; при удалении выбранного конфига старая UID-политика очищается."
        },
        {
          "title": "Поиск и фильтры",
          "text": "Поиск работает по имени пакета и UID. All, With rule, User и System меняют только видимый список, но не действующие правила ядра."
        }
      ]
    }
  },
  "app": {
    "en": {
      "title": "App rule",
      "intro": "Configures what one selected caller app can see. A caller is the app making UID checks; targets are the apps you want hidden from that caller.",
      "sections": [
        {
          "title": "Default / Blacklist / Whitelist",
          "text": "Default follows the module-wide setting. Blacklist hides listed targets; Whitelist hides everything except listed targets. An empty whitelist can hide all targets."
        },
        {
          "title": "Hide everything",
          "text": "Hides every eligible target from this caller. Use carefully: it can affect how the target app and its integrations behave."
        },
        {
          "title": "Include system apps",
          "text": "Enables filtering system packages for this caller. Hiding Android framework packages can break app or device behavior."
        },
        {
          "title": "Hidden apps / Templates",
          "text": "Pick individual hidden targets or reuse named target lists. Remove rule deletes this caller's independent rule only."
        },
        {
          "title": "HMA-OSS precedence",
          "text": "Saving your first independent rule switches the module's active policy source away from HMA-OSS. The confirmation is deliberate."
        }
      ]
    },
    "ru": {
      "title": "App rule — правило приложения",
      "intro": "Настройка того, какие UID видит конкретное приложение. Проверяющее приложение — это кто выполняет поиск; скрываемые — те, кого оно не должно находить.",
      "sections": [
        {
          "title": "Default / Blacklist / Whitelist",
          "text": "Default использует общий режим. Blacklist скрывает перечисленные приложения; Whitelist оставляет видимыми только перечисленные. Пустой белый список способен скрыть всё."
        },
        {
          "title": "Hide everything",
          "text": "Скрывает от этого приложения все подходящие UID. Используйте осторожно: это может повлиять на работу целевого приложения."
        },
        {
          "title": "Include system apps",
          "text": "Разрешает скрывать системные пакеты. Скрытие компонентов Android может вызвать ошибки работы приложений или системы."
        },
        {
          "title": "Hidden apps / Templates",
          "text": "Можно выбрать отдельные пакеты или использовать готовые шаблоны. Remove rule удаляет только самостоятельное правило этого проверяющего приложения."
        },
        {
          "title": "Приоритет HMA-OSS",
          "text": "Первое сохранение самостоятельного правила переключает источник политики с HMA-OSS. Предупреждение появляется не случайно."
        }
      ]
    }
  },
  "templates": {
    "en": {
      "title": "Templates",
      "intro": "Reusable named lists of target apps. Attach a template to multiple caller rules instead of selecting the same hidden packages repeatedly.",
      "sections": [
        {
          "title": "New template",
          "text": "Creates an empty group. Add the target packages through Members after creation."
        },
        {
          "title": "How it applies",
          "text": "A template changes kernel policy only when a caller rule references it. Having a template alone does not hide anything."
        },
        {
          "title": "Source priority",
          "text": "Templates belong to the standalone WebUI JSON policy; they do not edit HMA-OSS's own templates."
        }
      ]
    },
    "ru": {
      "title": "Templates — шаблоны",
      "intro": "Это именованные наборы скрываемых приложений. Один шаблон можно использовать в нескольких правилах вместо повторного выбора одинаковых пакетов.",
      "sections": [
        {
          "title": "New template",
          "text": "Создаёт пустую группу. После создания добавьте нужные пакеты через Members."
        },
        {
          "title": "Как применяются",
          "text": "Один лишь шаблон ничего не скрывает. Он работает, когда выбран в правиле проверяющего приложения."
        },
        {
          "title": "Приоритет источников",
          "text": "Это шаблоны самостоятельной JSON-политики WebUI. Они не изменяют шаблоны внутри HMA-OSS."
        }
      ]
    }
  },
  "template": {
    "en": {
      "title": "Template details",
      "intro": "Edit the name and membership of one reusable target-app group.",
      "sections": [
        {
          "title": "Name",
          "text": "Renaming also updates references in this module's standalone rules."
        },
        {
          "title": "Members",
          "text": "Pick packages to include in the group. A caller using the template hides these targets according to its rule mode."
        },
        {
          "title": "Delete template",
          "text": "Removes the group and references to it from standalone caller rules. It does not uninstall apps or delete HMA-OSS templates."
        }
      ]
    },
    "ru": {
      "title": "Настройки шаблона",
      "intro": "Управление названием и составом одного набора приложений.",
      "sections": [
        {
          "title": "Name",
          "text": "При переименовании ссылки на шаблон в самостоятельных правилах модуля обновляются."
        },
        {
          "title": "Members",
          "text": "Выберите пакеты группы. Проверяющее приложение, использующее шаблон, скрывает эти пакеты согласно своему режиму."
        },
        {
          "title": "Delete template",
          "text": "Удаляет группу и её упоминания из самостоятельных правил. Приложения не удаляются, шаблоны HMA-OSS не затрагиваются."
        }
      ]
    }
  },
  "picker": {
    "en": {
      "title": "Select apps",
      "intro": "Choose package names that will be part of the current rule or template. You can search by package name or UID.",
      "sections": [
        {
          "title": "Switches",
          "text": "Select or deselect a package. System packages are included in the listing but can be filtered according to your rule settings."
        },
        {
          "title": "Clear / Done",
          "text": "Clear resets the current selection. Done returns the selection to the previous screen; the parent rule then validates and writes the policy."
        }
      ]
    },
    "ru": {
      "title": "Выбор приложений",
      "intro": "Выберите пакеты для текущего правила или шаблона. Поиск доступен по имени пакета и UID.",
      "sections": [
        {
          "title": "Переключатели",
          "text": "Включают и исключают пакет из выбора. Системные приложения присутствуют в списке, но их скрытие регулируется настройками правила."
        },
        {
          "title": "Clear / Done",
          "text": "Clear сбрасывает текущий выбор. Done возвращает выбранные пакеты в предыдущее меню для дальнейшего сохранения и проверки."
        }
      ]
    }
  },
  "pickerTemplates": {
    "en": {
      "title": "Choose templates",
      "intro": "Select reusable template groups to apply to this caller.",
      "sections": [
        {
          "title": "Templates",
          "text": "Each selected template adds its member packages to the caller's targets. Removing a template from the selection leaves the template itself intact."
        },
        {
          "title": "Done",
          "text": "Choose HMA/HMA-OSS or Manual explicitly on the home screen. Saving manual rules never switches mode. In HMA-OSS mode they remain inactive saved data; deleting the selected config clears the previous UID policy."
        }
      ]
    },
    "ru": {
      "title": "Выбор шаблонов",
      "intro": "Выберите готовые шаблоны, которые нужно применить к правилу этого приложения.",
      "sections": [
        {
          "title": "Как работают",
          "text": "Каждый выбранный шаблон добавляет свои пакеты к скрываемым целям. Удаление шаблона из выбора не удаляет сам шаблон."
        },
        {
          "title": "Done",
          "text": "Источник выбирается отдельно на главном экране: HMA/HMA-OSS или Ручной. Сохранение ручных правил не переключает режим. В режиме HMA-OSS они остаются неактивной резервной копией; при удалении выбранного конфига старая UID-политика очищается."
        }
      ]
    }
  },
  "settings": {
    "en": {
      "title": "Settings",
      "intro": "Controls appearance and default behavior of the independent WebUI policy. These settings do not rewrite the original HMA-OSS configuration.",
      "sections": [
        {
          "title": "Theme",
          "text": "System follows Android light/dark mode. Light and Dark override it for this WebUI."
        },
        {
          "title": "Default mode",
          "text": "Blacklist hides only listed targets. Whitelist hides everything except listed targets; a blank whitelist may hide all."
        },
        {
          "title": "Hide system apps too",
          "text": "Allows hiding Android framework and system packages. This is off by default to reduce the risk of malfunction."
        },
        {
          "title": "Raw config",
          "text": "Directly edit the JSON document. Back up first and use it only if you understand the schema."
        },
        {
          "title": "Backup & restore",
          "text": "Export or restore the independent JSON policy, not a full backup of HMA-OSS."
        },
        {
          "title": "Critical",
          "text": "Saving standalone settings can override policy coming from HMA/HMA-OSS. Read the confirmation dialog before switching."
        }
      ]
    },
    "ru": {
      "title": "Settings — настройки",
      "intro": "Настройки внешнего вида и общего поведения самостоятельной политики WebUI. Оригинальная конфигурация HMA-OSS не редактируется.",
      "sections": [
        {
          "title": "Theme",
          "text": "System — тема Android; Light — светлая; Dark — тёмная."
        },
        {
          "title": "Default mode",
          "text": "Blacklist скрывает перечисленные пакеты. Whitelist скрывает всё, кроме указанных; пустой список способен скрыть все."
        },
        {
          "title": "Hide system apps too",
          "text": "Разрешает скрывать системные компоненты Android. По умолчанию отключено, так как может нарушить работу программ и системы."
        },
        {
          "title": "Raw config",
          "text": "Прямое редактирование JSON. Сделайте копию и используйте только при понимании формата."
        },
        {
          "title": "Backup & restore",
          "text": "Экспорт и восстановление самостоятельной конфигурации, а не полная копия HMA-OSS."
        },
        {
          "title": "Важно",
          "text": "Сохранение отдельных правил может перекрыть политику HMA/HMA-OSS. Внимательно прочитайте предупреждение."
        }
      ]
    }
  },
  "raw": {
    "en": {
      "title": "Raw config",
      "intro": "An advanced editor for this module's standalone JSON policy. It is not a view of the live HMA-OSS configuration.",
      "sections": [
        {
          "title": "Edit / Revert",
          "text": "Edit the JSON text, or restore it from the last loaded config with Revert."
        },
        {
          "title": "Validate & save",
          "text": "Parses and validates the document, saves a backup beside it and pushes the new standalone policy. A malformed document is rejected."
        },
        {
          "title": "Source precedence",
          "text": "Choose HMA/HMA-OSS or Manual explicitly on the home screen. Saving manual rules never switches mode. In HMA-OSS mode they remain inactive saved data; deleting the selected config clears the previous UID policy."
        }
      ]
    },
    "ru": {
      "title": "Raw config — JSON",
      "intro": "Расширенный редактор самостоятельной JSON-политики этого модуля, а не просмотр активного файла настроек HMA-OSS.",
      "sections": [
        {
          "title": "Редактирование / Revert",
          "text": "Измените JSON вручную или восстановите последний загруженный вариант кнопкой Revert."
        },
        {
          "title": "Validate & save",
          "text": "Проверяет формат, сохраняет резервную копию рядом с файлом и отправляет новую политику ядру. Ошибочный JSON будет отклонён."
        },
        {
          "title": "Приоритет",
          "text": "Источник выбирается отдельно на главном экране: HMA/HMA-OSS или Ручной. Сохранение ручных правил не переключает режим. В режиме HMA-OSS они остаются неактивной резервной копией; при удалении выбранного конфига старая UID-политика очищается."
        }
      ]
    }
  },
  "backup": {
    "en": {
      "title": "Backup & restore",
      "intro": "Copy or restore this module's standalone WebUI JSON configuration. HMA-OSS app data and HMA-OSS presets are not included in this export.",
      "sections": [
        {
          "title": "Current config",
          "text": "The JSON displayed here can be copied to a safe place. Copy path gives the filesystem location."
        },
        {
          "title": "Restore",
          "text": "Paste a previously exported JSON and choose Validate & write. The tool checks the content before replacing anything."
        },
        {
          "title": "Caution",
          "text": "Choose HMA/HMA-OSS or Manual explicitly on the home screen. Saving manual rules never switches mode. In HMA-OSS mode they remain inactive saved data; deleting the selected config clears the previous UID policy."
        }
      ]
    },
    "ru": {
      "title": "Backup & restore — копии",
      "intro": "Здесь копируются и восстанавливаются самостоятельные настройки WebUI в формате JSON. Данные приложения HMA-OSS и его пресеты в этот экспорт не входят.",
      "sections": [
        {
          "title": "Current config",
          "text": "Содержимое JSON можно скопировать и сохранить отдельно. Copy path показывает путь к файлу."
        },
        {
          "title": "Restore",
          "text": "Вставьте ранее сохранённый JSON и нажмите Validate & write. Перед заменой настройки проверяются."
        },
        {
          "title": "Осторожно",
          "text": "Восстановленная самостоятельная политика перекрывает правила HMA-OSS. Сохраните резервную копию и проверьте источник правил."
        }
      ]
    }
  },
  "about": {
    "en": {
      "title": "About",
      "intro": "HMA UID Fake fork is an open-source KernelSU kernel module that complements HMA / HMA-OSS by filtering selected UID-related system calls.",
      "sections": [
        {
          "title": "Source",
          "text": "The Source entry links to github.com/Mobilelegends74/hma-uidfake, where the code, release builds and documentation are maintained."
        },
        {
          "title": "License",
          "text": "GPL-2.0. The fork retains credit to the original author and other upstream contributors."
        },
        {
          "title": "How it works",
          "text": "Explains the UID-layer design, what remains visible through Java APIs, and where independent configuration files are stored."
        }
      ]
    },
    "ru": {
      "title": "About — о модуле",
      "intro": "HMA UID Fake fork — открытый модуль ядра для KernelSU, дополняющий HMA/HMA-OSS фильтрацией отдельных системных вызовов, связанных с UID.",
      "sections": [
        {
          "title": "Source",
          "text": "Ведёт на github.com/Mobilelegends74/hma-uidfake — репозиторий с исходниками, релизами и документацией."
        },
        {
          "title": "License",
          "text": "GPL-2.0. Сохранены указания на автора оригинала и участников исходных проектов."
        },
        {
          "title": "How it works",
          "text": "Объясняет принцип работы на уровне ядра, ограничения Java API и расположение самостоятельной конфигурации."
        }
      ]
    }
  },
  "help": {
    "en": {
      "title": "How it works",
      "intro": "The module makes selected kernel UID existence checks behave as though the configured target UID is absent, but only for matching callers.",
      "sections": [
        {
          "title": "Coverage",
          "text": "Choose HMA/HMA-OSS or Manual explicitly on the home screen. Saving manual rules never switches mode. In HMA-OSS mode they remain inactive saved data; deleting the selected config clears the previous UID policy."
        },
        {
          "title": "Not a replacement for HMA-OSS",
          "text": "PackageManager, ActivityManager, icons and Java framework behavior are outside the scope of UID Fake. Use HMA-OSS for those surfaces."
        },
        {
          "title": "Policy source",
          "text": "Choose HMA/HMA-OSS or Manual explicitly on the home screen. Saving manual rules never switches mode. In HMA-OSS mode they remain inactive saved data; deleting the selected config clears the previous UID policy."
        },
        {
          "title": "Kernel risk",
          "text": "Different vendor kernels may reject the .ko or fail at hook installation. Keep recovery available; support for a KMI in CI is not a device-boot guarantee."
        }
      ]
    },
    "ru": {
      "title": "How it works — принцип работы",
      "intro": "Модуль меняет ответы отдельных системных вызовов: для выбранного проверяющего приложения скрываемые UID выглядят отсутствующими.",
      "sections": [
        {
          "title": "Перехваты",
          "text": "Источник выбирается отдельно на главном экране: HMA/HMA-OSS или Ручной. Сохранение ручных правил не переключает режим. В режиме HMA-OSS они остаются неактивной резервной копией; при удалении выбранного конфига старая UID-политика очищается."
        },
        {
          "title": "Не замена HMA-OSS",
          "text": "PackageManager, ActivityManager, значки и Java API не скрываются этим ядровым модулем. Для них нужен отдельный уровень фильтрации HMA-OSS."
        },
        {
          "title": "Источники правил",
          "text": "Источник выбирается отдельно на главном экране: HMA/HMA-OSS или Ручной. Сохранение ручных правил не переключает режим. В режиме HMA-OSS они остаются неактивной резервной копией; при удалении выбранного конфига старая UID-политика очищается."
        },
        {
          "title": "Риски ядра",
          "text": "На несовместимом ядре загрузка .ko или установка хуков могут завершиться ошибкой. Поддержка KMI при сборке не гарантирует загрузку на каждом устройстве."
        }
      ]
    }
  }
});
