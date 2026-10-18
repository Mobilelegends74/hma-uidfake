#!/system/bin/sh
# Opt-in is retained from audited f5c4e860 branch.
MODDIR=${0%/*}
LOG="$MODDIR/state/sync.log"
mkdir -p "$MODDIR/state"
if [ ! -f /data/adb/hma-uidfake/allow_load ]; then
  echo "[load] SAFE MODE: no kernel module load (opt-in marker missing)" >>"$LOG"
  exit 0
fi
. "$MODDIR/load.sh"
uidfake_load "$MODDIR" >>"$LOG" 2>&1
