#!/system/bin/sh
# Explicit packaged KMI selection, never a force loader or arbitrary file path.
umask 077
moddir=${0%/*}
case "${1:-}" in
  android12-5.10|android13-5.10|android13-5.15|android14-5.15|android14-6.1|android15-6.6|android16-6.12|android17-6.18) candidate=$1 ;;
  *) echo '[INVALID_CANDIDATE] Choose a packaged KMI'; exit 2 ;;
esac
[ "$#" -eq 1 ] || exit 2
[ "$(id -u)" = 0 ] || { echo '[ROOT_REQUIRED] Open from the root manager'; exit 1; }
mkdir -p "$moddir/state" || exit 1
. "$moddir/load.sh"
if [ -e /data/adb/hma-uidfake/disable_load ]; then
  echo '[DISABLED] Owner disabled loading; manual selection does not override this'
  exit 1
fi
if [ -d /sys/module/hma_uidfake ] || lsmod | grep -q '^hma_uidfake '; then
  echo '[ALREADY_LOADED] Module is present; no stacking or hot replacement'
  exit 1
fi
printf '%s\n' "$candidate" > "$moddir/state/manual-requested-kmi"
# The helper repeats the complete automatic compatibility checks, checks the
# exact filename, and uses finit_module(flags=0). No bypass for manual mode.
if "$moddir/kmi-tool" load "$moddir/ko/${candidate}_arm64_hma_uidfake.ko" > "$moddir/state/manual-load.log" 2>&1; then
  printf '%s\n' "$candidate" > "$moddir/state/selected-kmi"
  printf '' > "$moddir/state/load.reason"
  # The daemon holds its own exclusive lock. Never kill an existing syncer.
  nohup "$moddir/sync-tool" </dev/null >> "$moddir/state/sync.log" 2>&1 &
  echo '[LOADED] Kernel accepted the module; rule synchronization started'
else
  reason=$(head -n 1 "$moddir/state/manual-load.log" | cut -c 1-220)
  uidfake_status "$moddir" "${reason:-[LOAD_FAILED] Manual load rejected}"
  cat "$moddir/state/manual-load.log"
  exit 1
fi
