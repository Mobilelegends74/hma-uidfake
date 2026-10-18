#!/system/bin/sh
MODDIR=${0%/*}
LOG="$MODDIR/state/sync.log"
mkdir -p "$MODDIR/state"
mkdir -p /data/adb/hma-uidfake
chmod 0700 /data/adb/hma-uidfake
# Respect explicit opt-out; otherwise load automatically on normal boot.
if [ -e /data/adb/hma-uidfake/disable_load ]; then
  echo "[load] disabled by owner marker (not starting sync daemon)" >>"$LOG"
  exit 0
fi
. "$MODDIR/load.sh"
if uidfake_load "$MODDIR" >>"$LOG" 2>&1; then
  if lsmod | grep -q '^hma_uidfake '; then
    # service.sh is invoked once on boot; this process follows HMA-OSS changes.
    [ -x "$MODDIR/sync-tool" ] && "$MODDIR/sync-tool" >>"$LOG" 2>&1 &
  fi
fi
