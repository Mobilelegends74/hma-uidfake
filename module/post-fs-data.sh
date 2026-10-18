#!/system/bin/sh
# Load the module before the framework starts: the reads a child does before its window
# closes have to be covered. lkmloader is the loader: it is shipped here, it needs no root
# solution to be installed, and it is what resolves the ko's undefined symbols from
# /proc/kallsyms and, when the kernel names the vermagic it wants, writes that in and
# retries.
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

[ -f "$KO" ] || exit 0
lsmod | grep -q '^hma_uidfake ' && exit 0

[ -x "$MODDIR/lkmloader" ] || { echo "[load] no lkmloader" >>"$LOG"; exit 0; }
"$MODDIR/lkmloader" "$KO" >>"$LOG" 2>&1
_rc=$?
echo "[load] lkmloader rc=$_rc $(date '+%m-%d %H:%M:%S')" >>"$LOG"
lsmod | grep -q '^hma_uidfake ' || echo "[load] not loaded; no ko for this kernel? see the customize.sh note" >>"$LOG"
