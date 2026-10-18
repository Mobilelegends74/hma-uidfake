#!/system/bin/sh
# Load the module if post-fs-data did not, then start sync-tool: it watches HMA's config,
# evaluates it and pushes the resulting policy over netlink, forever.
MODDIR=${0%/*}
KO="$MODDIR/ko/hma_uidfake.ko"
LOG="$MODDIR/state/sync.log"
mkdir -p "$MODDIR/state"
# Safety gate: never load a new kernel hook during boot without explicit owner opt-in.
# The marker is outside modules/, so a module update cannot silently opt in.
if [ ! -f /data/adb/hma-uidfake/allow_load ]; then
  echo "[load] SAFE MODE: no kernel module load (opt-in marker missing)" >>"$LOG"
  exit 0
fi


if [ -f "$KO" ] && [ -x "$MODDIR/lkmloader" ] && ! lsmod | grep -q '^hma_uidfake '; then
  "$MODDIR/lkmloader" "$KO" >>"$LOG" 2>&1
  echo "[load] lkmloader rc=$? $(date '+%m-%d %H:%M:%S')" >>"$LOG"
fi

[ -x "$MODDIR/sync-tool" ] && "$MODDIR/sync-tool" >>"$LOG" 2>&1 &
