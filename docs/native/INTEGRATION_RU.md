# Встраивание HMA UIDFAKE в ядро

[Главная](../../README.md) · [English](INTEGRATION_EN.md) · [Диагностика](TROUBLESHOOTING.md)

## 1. Что понадобится

- Исходники и рабочая сборочная среда **вашего** ядра. Проверенные полные сборки: ARM64 Android 14 / Linux 6.1.118 (OnePlus 13R) и 6.1.145 (GKI).
- Python 3.10+, GNU patch, C-компилятор для host-тестов. Для сборки ядра — его штатный Clang/LLVM и Android build/Kleaf либо обычный Kbuild.
- HMA/HMA-OSS с настроенными правилами и необходимым ему способом внедрения. UIDFAKE дополняет, но не заменяет скрытие PackageManager.
- Для автоматической синхронизации — root и привилегированный запуск службы после загрузки. Адаптер для KernelSU/KernelSU Next service.d включён; для другого root/init нужен собственный boot-adapter.
- Совместимое устройство, резервная копия boot и способ восстановления. Не проверяйте новую интеграцию сразу на единственном рабочем boot без резервной копии.

**Ядро не зависит от версии KernelSU или SUSFS.** Они не скачиваются этим комплектом: сохраните свою сборку и свою политику обновления этих компонентов. Проверки новых версий всё равно обязательны.

Ветка содержит также исходники прежнего LKM и его инструмента: из src/tools собирается companion. Не запускайте сборку/установку LKM для нативного варианта.

## 2. Получение комплекта

```bash
git clone --branch kernel-patches --single-branch https://github.com/Mobilelegends74/hma-uidfake.git
cd hma-uidfake
git rev-parse HEAD
sha256sum --check patches/SHA256SUMS
python3 scripts/uidfake-test-native.py
```

Сохраните SHA комплекта в метаданных своей сборки. Для воспроизводимых production-сборок используйте проверенный commit, а не незаметно меняющийся HEAD комплекта. Это не закрепляет версии KernelSU/SUSFS.

## 3. Подключение: выберите ОДИН способ

Сначала примените остальные патчи вашей сборки, меняющие task_struct, например Droidspaces/SYSVIPC/SLIM_SCHED. Затем UIDFAKE сможет выбрать действительно свободный KABI-слот. Не применяйте одновременно готовый патч и интегратор.

### A. Проверяемый интегратор — рекомендуется для изменённых деревьев

Из каталога комплекта; замените абсолютные пути своими:

```bash
UIDFAKE_KERNEL=/absolute/path/to/kernel/common
python3 scripts/uidfake-native-integrate.py "$UIDFAKE_KERNEL" \
  --patch /absolute/path/to/generated-hma-uidfake.patch
python3 scripts/uidfake-check-kabi.py "$UIDFAKE_KERNEL"
```

Скрипт добавляет исходники, Kconfig/Kbuild, заголовок и точки вызова в kernel/sys.c, block/ioprio.c, kernel/user.c, kernel/cred.c, fs/open.c и include/linux/sched.h. Все исходные контексты проверяются **до записи**. Отсутствие свободного безусловного KABI-слота или несовпадение API — ошибка, а не повод отключать проверки. Повторный запуск не дублирует интеграцию; ненулевой экспортированный патч сохраняется.

Экспортированный патч предназначен для повторного применения к **такому же исходному состоянию** дерева. Он не включает ваши KernelSU/SUSFS/Droidspaces-патчи.

### B. Готовый полный патч — только для соответствующей базы

Выберите файл по [таблице баз](../../patches/README.md). Не применяйте оба файла.

```bash
cd /absolute/path/to/kernel/common
patch --dry-run --fuzz=0 -p1 < /absolute/path/to/hma-uidfake/patches/reference/gki-android14-6.1.145.patch
patch --fuzz=0 -p1 < /absolute/path/to/hma-uidfake/patches/reference/gki-android14-6.1.145.patch
```

Для OnePlus замените имя файла на oneplus-android14-6.1.118.patch. Если dry-run не проходит, используйте способ A или вручную адаптируйте API с проверкой. **Не используйте принудительное пропускание hunks, отключение CFI/SELinux или замену защитных проверок ради компиляции.**

## 4. Конфигурация и сборка

В вашем defconfig/фрагменте конфигурации:

```text
CONFIG_HMA_UIDFAKE_BUILTIN=y
```

Kconfig — bool, зависит от NET и выбирает CRC32. Отдельного .ko не будет. Имя опции нельзя заменить на CONFIG_UIDFAKE или включить только obj-y без точек вызова.

Используйте **существующий** pipeline устройства. Для обычного Kbuild пример после подготовки своей конфигурации:

```bash
UIDFAKE_OUT=/absolute/path/to/kernel/out
./scripts/config --file "$UIDFAKE_OUT/.config" --enable HMA_UIDFAKE_BUILTIN
make O="$UIDFAKE_OUT" ARCH=arm64 LLVM=1 olddefconfig
grep '^CONFIG_HMA_UIDFAKE_BUILTIN=y$' "$UIDFAKE_OUT/.config"
make O="$UIDFAKE_OUT" ARCH=arm64 LLVM=1 -j"$(nproc)" Image
```

Для Kleaf/Bazel меняется исходный defconfig/поддерживаемый fragment **до конфигурирования**, а сборка запускается штатной целью. Не редактируйте сгенерированный sandbox .config и не отключайте KMI/ABI проверки. Заголовки компонента используют srctree и поддерживают out-of-tree O=.

Универсальный набор команд скачивания toolchain/сборки/прошивки для всех OEM не существует. Сохраните требуемые вашей веткой flags, signing и boot-format. В этой ветке не добавляется module-version Bypass.

## 5. Откуда берётся sync-tool

Обязательна **userspace**-синхронизация. Чистый kernel-патч сам по себе не может получать зашифрованный JSON HMA после разблокировки.

### Вариант 1: готовый проверенный companion

Скрипт упаковки ниже скачивает существующий v0.3.6-fork, проверяет SHA-256 всего ZIP и извлекает только sync-tool с отдельной проверкой SHA-256. Не устанавливает модуль, WebUI или lkmloader. Контрольные суммы находятся в scripts/uidfake-stage-anykernel.sh и NOTICE.

### Вариант 2: собрать companion из исходников

Нужны Android NDK r27c, CMake и nlohmann-json3-dev (в Ubuntu/Debian). Сборка **только** src/tools не компилирует LKM и не требует его submodule.

```bash
sudo apt-get install cmake ninja-build nlohmann-json3-dev
UIDFAKE_NDK=/absolute/path/to/android-ndk-r27c
cmake -S src/tools -B out/native-sync -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$UIDFAKE_NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=24 -DANDROID_STL=c++_static \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH
cmake --build out/native-sync --target sync-tool
```

Передайте out/native-sync/sync-tool вторым аргументом упаковщика. Проверяется ARM64 ELF; его SHA-256 сохраняется в архиве. Для самостоятельно собранного файла это проверка целостности, **не внешняя аттестация происхождения**. Сохраняйте совместимость с kaux protocol v4: нативный и userspace-заголовки в этой ветке совпадают.

## 6. AnyKernel3 и автоматический запуск

Из каталога комплекта. Подготовьте чистый AnyKernel3 для вашего устройства; установщик ожидает один стандартный якорь “# boot install”. При другой структуре он остановится: адаптируйте установку до прошивки boot, не обходите preflight.

```bash
UIDFAKE_AK3=/absolute/path/to/AnyKernel3
UIDFAKE_PATCH=/absolute/path/to/generated-hma-uidfake.patch
# Готовый checksum-verified companion:
bash scripts/uidfake-stage-anykernel.sh "$UIDFAKE_AK3" "" "$UIDFAKE_PATCH"
# Альтернатива: собственная сборка companion:
# bash scripts/uidfake-stage-anykernel.sh "$UIDFAKE_AK3" out/native-sync/sync-tool "$UIDFAKE_PATCH"

python3 scripts/uidfake-record-build.py \
  /absolute/path/to/Image /absolute/path/to/.config \
  /absolute/path/to/System.map "$UIDFAKE_AK3"
(cd "$UIDFAKE_AK3" && zip -qr /absolute/path/to/kernel_UIDFAKE.zip . -x '.git/*')
python3 scripts/uidfake-verify-zip.py /absolute/path/to/kernel_UIDFAKE.zip
```

Для готового патча способа B передайте его путь вместо UIDFAKE_PATCH. record-build принимает **сырой ARM64 Image**, а не boot.img/Image.gz; находит UIDFAKE marker, проверяет финальную конфигурацию и символы. Если включаете дополнительные Images (например, собственный ранее существовавший Bypass-Image), они тоже должны содержать UIDFAKE. Этот комплект не создаёт и не требует такой режим.

При установке ZIP из загруженного Android с root и доступным /data/adb установщик:

1. Проверяет доступ к /data/adb и отсутствие известного активного UIDFAKE LKM.
2. Устанавливает sync-tool и supervisor в /data/adb/uidfake-kernel.
3. Добавляет /data/adb/service.d/97-uidfake-kernel.sh для уже существующего root service.d runner.
4. Только затем продолжает штатную прошивку AnyKernel3.

После загрузки supervisor запускает sync-tool и перезапускает его при выходе. Companion ожидает доступности данных и следит за изменениями правил/пакетов. Ручное создание правил UIDFAKE не требуется. Существующие правила не удаляются.

## 7. Без AnyKernel3 / другой root

Установите ARM64 sync-tool и scripts/uidfake-kernel-service.sh как service.sh в /data/adb/uidfake-kernel. Каталог должен принадлежать root и иметь 0700, оба исполняемых файла — root и 0755. Организуйте **один** привилегированный запуск service.sh после загрузки через ваш init/root runner. Путь BASE в supervisor и место установки должны совпадать.

В Android init могут потребоваться отдельный SELinux domain, labels и точечные разрешения для чтения выбранных конфигураций/пакетов и generic netlink. Готовой универсальной sepolicy для всех OEM нет. Не переводите устройство в permissive. Если service.d не поддерживается, простого копирования файла недостаточно.

## 8. Проверка и откат

[Полный чек-лист, приоритеты источников, диагностика и откат](TROUBLESHOOTING.md). Проверьте запуск после перезагрузки, обновление и отзыв правил, isolated/app-zygote процессы и compat-приложения. Отсутствие исключений или наличие строки “ready” не доказывает корректность всех хуков.
