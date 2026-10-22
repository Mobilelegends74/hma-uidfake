# Ветки и архив исходников

- `main` — опубликованный код и документация.
- `develop` — постоянная ветка исправлений и тестовых сборок. Создана переименованием `test/oneplus-61238-updates-0315` с коммита `325c827`, из которого собран релиз 0.3.16.
- `kernel-patches` — самостоятельные патчи интеграции в ядро и инструкции.

Работа ведётся новыми коммитами в `develop`, без отдельных веток для каждой сборки. После проверки выбранный коммит включается в `main`; в релиз переносится проверенный артефакт без повторной сборки. Номер версии и versionCode задаются до тестовой сборки.

## Архив на 22 октября 2026

Перед очисткой веток их последние коммиты закреплены тегами `archive/2026-10-22/<прежнее имя ветки>`. Теги сохраняют исходники и достижимую историю, но не создают новых релизов и не запускают релизную сборку.

| Прежняя ветка | Исходники архивного коммита |
|---|---|
| `build/android14-6.1-compat` | [f5c4e86](https://github.com/Mobilelegends74/hma-uidfake/tree/archive/2026-10-22/build/android14-6.1-compat) |
| `experiment/hmaoss-lsposed-pm-exceptions` | [2518d79](https://github.com/Mobilelegends74/hma-uidfake/tree/archive/2026-10-22/experiment/hmaoss-lsposed-pm-exceptions) |
| `feature/ru-en-webui-fork-readme` | [5821320](https://github.com/Mobilelegends74/hma-uidfake/tree/archive/2026-10-22/feature/ru-en-webui-fork-readme) |
| `feature/webui-audited-build-v2` | [399b41c](https://github.com/Mobilelegends74/hma-uidfake/tree/archive/2026-10-22/feature/webui-audited-build-v2) |
| `release/v0.3.6-fork` | [fa72fe4](https://github.com/Mobilelegends74/hma-uidfake/tree/archive/2026-10-22/release/v0.3.6-fork) |
| `test/context-help-status-v036` | [d1648f1](https://github.com/Mobilelegends74/hma-uidfake/tree/archive/2026-10-22/test/context-help-status-v036) |
| `test/diagnostics-038` | [b8f0ea6](https://github.com/Mobilelegends74/hma-uidfake/tree/archive/2026-10-22/test/diagnostics-038) |
| `test/kmi-custom-kernel-v036` | [58204e2](https://github.com/Mobilelegends74/hma-uidfake/tree/archive/2026-10-22/test/kmi-custom-kernel-v036) |
| `test/manual-direct-0313` | [a34e3fb](https://github.com/Mobilelegends74/hma-uidfake/tree/archive/2026-10-22/test/manual-direct-0313) |
| `test/manual-inject-0312` | [24f5688](https://github.com/Mobilelegends74/hma-uidfake/tree/archive/2026-10-22/test/manual-inject-0312) |
| `test/oneplus-61238-updates-0315` | [325c827](https://github.com/Mobilelegends74/hma-uidfake/tree/archive/2026-10-22/test/oneplus-61238-updates-0315) |
| `test/source-mode-039` | [8e31782](https://github.com/Mobilelegends74/hma-uidfake/tree/archive/2026-10-22/test/source-mode-039) |
| `test/static-key-0314` | [6680a1f](https://github.com/Mobilelegends74/hma-uidfake/tree/archive/2026-10-22/test/static-key-0314) |

Существующие релизные теги, вложения Releases и записи GitHub Actions не удалялись. Для артефактов и журналов Actions продолжает действовать обычный срок хранения GitHub: архивный Git-тег сам по себе их не продлевает.

Ветка `develop` получает актуальную документацию из `main`; архивный тег выше навсегда сохраняет точную исходную точку последнего релиза.
