#!/system/bin/sh
MODDIR=${0%/*}
LOG="$MODDIR/state/sync.log"
mkdir -p "$MODDIR/state"
# Native standalone policy lives outside modules so updates cannot erase it.
mkdir -p /data/adb/hma-uidfake
chmod 0700 /data/adb/hma-uidfake
if [ ! -f /data/adb/hma-uidfake/allow_load ]; then
  echo "[load] SAFE MODE: not starting kernel hooks or policy daemon" >>"$LOG"
  exit 0
fi
. "$MODDIR/load.sh"
uidfake_load "$MODDIR" >>"$LOG" 2>&1
[ -x "$MODDIR/sync-tool" ] && "$MODDIR/sync-tool" >>"$LOG" 2>&1 &
