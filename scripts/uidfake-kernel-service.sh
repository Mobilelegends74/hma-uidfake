#!/system/bin/sh
# Boot adapter for the existing Android service.d runner. No module or insmod.
BASE=/data/adb/uidfake-kernel
[ -x "$BASE/sync-tool" ] || exit 1
if [ "${1:-}" != --supervise ]; then
  "$BASE/service.sh" --supervise </dev/null >/dev/null 2>&1 &
  exit 0
fi
PIDFILE="$BASE/supervisor.pid"
if [ -f "$PIDFILE" ]; then
  read -r prior < "$PIDFILE"
  case "$prior" in
    ''|*[!0-9]*) ;;
    *) if kill -0 "$prior" 2>/dev/null &&
       tr '\0' ' ' < "/proc/$prior/cmdline" | grep -q 'uidfake-kernel/service.sh'; then
         exit 0
       fi ;;
  esac
fi
echo "$$" > "$PIDFILE"
trap 'rm -f "$PIDFILE"; exit 0' INT TERM EXIT
# Keep the previous start's log; retry failures after unlock/boot.
while :; do
  [ ! -f "$BASE/sync.log" ] || mv -f "$BASE/sync.log" "$BASE/sync.previous.log"
  "$BASE/sync-tool" > "$BASE/sync.log" 2>&1
  echo "sync-tool exited ($?); restarting in 15 seconds" >> "$BASE/sync.log"
  sleep 15
done
