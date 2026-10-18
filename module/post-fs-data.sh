#!/system/bin/sh
# Automatic boot-time load; no activation marker or second reboot needed.
MODDIR=${0%/*}
LOG="$MODDIR/state/sync.log"
mkdir -p "$MODDIR/state"
. "$MODDIR/load.sh"
uidfake_load "$MODDIR" >>"$LOG" 2>&1
