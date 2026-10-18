#!/system/bin/sh
# Audited loader shell adaptation from the owner-provided 0.4.0 archive.
# This script is invoked ONLY AFTER the explicit allow_load gate.
uidfake_load() {
  local moddir="$1"
  local ko="$moddir/ko/hma_uidfake.ko"
  local rc
  [ -f "$ko" ] || { echo "[load] no matching kernel module"; return 1; }
  if lsmod | grep -q '^hma_uidfake '; then
    echo "[load] existing hma_uidfake in kernel; replacing it requires a reboot"
    return 0
  fi
  if [ -x /data/adb/ksud ]; then
    /data/adb/ksud insmod "$ko"
    rc=$?
    echo "[load] ksud rc=$rc"
  elif [ -x "$moddir/lkmloader" ]; then
    "$moddir/lkmloader" "$ko"
    rc=$?
    echo "[load] lkmloader rc=$rc"
  else
    echo "[load] no loader found"
    return 1
  fi
  if ! lsmod | grep -q '^hma_uidfake '; then
    echo "[load] kernel module not loaded; check KMI and loader logs"
    return 1
  fi
  return 0
}
