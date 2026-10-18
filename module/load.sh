#!/system/bin/sh
# Common KMI-specific loader for post-fs-data and service.
# Kernel hooks cannot be safely replaced in a live kernel; an already loaded
# instance is not unloaded or stacked on top of.
uidfake_load() {
  _moddir="$1"
  _ko="$_moddir/ko/hma_uidfake.ko"

  if [ -e /data/adb/hma-uidfake/disable_load ]; then
    echo "[load] disabled by owner marker"
    return 0
  fi
  if [ ! -f "$_ko" ]; then
    echo "[load] no compatible kernel module selected; refusing cross-KMI load"
    return 1
  fi
  if lsmod | grep -q '^hma_uidfake '; then
    echo "[load] kernel module already loaded; will not stack or hot-replace"
    return 0
  fi

  if [ -x /data/adb/ksud ]; then
    /data/adb/ksud insmod "$_ko"
    _rc=$?
    echo "[load] ksud insmod rc=$_rc"
    if lsmod | grep -q '^hma_uidfake '; then
      return 0
    fi
  fi

  if [ -x "$_moddir/lkmloader" ]; then
    "$_moddir/lkmloader" "$_ko"
    _rc=$?
    echo "[load] bundled lkmloader rc=$_rc"
    if lsmod | grep -q '^hma_uidfake '; then
      return 0
    fi
  fi
  echo "[load] could not load compatible module; inspect state/sync.log and dmesg"
  return 1
}
