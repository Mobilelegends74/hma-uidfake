#!/system/bin/sh
# Read-only diagnostics. Never source configs or invoke a module loader.
umask 077

capture() {
  dest="$1"
  shift
  # Bound both execution time and output, retaining command errors in the file.
  # timeout is provided by Android toybox; missing commands are nonfatal.
  (if timeout 25 "$@"; then rc=0; else rc=$?; fi; printf '\n[exit=%s]\n' "$rc") 2>&1 |
    tail -c 4194304 > "$dest"
}

copy_small() {
  src="$1"
  dest="$2"
  limit="$3"
  if [ ! -f "$src" ] || [ ! -r "$src" ]; then
    printf 'Unavailable: %s\n' "$src" >> "$work/manifest.txt"
    return
  fi
  # Bound the actual read as well as regular-file size (sysfs/procfs often
  # report zero, and a symlink must not bypass the collection limit).
  if ! head -c "$((limit + 1))" "$src" > "$dest" 2>> "$work/manifest.txt"; then
    printf 'Copy failed: %s\n' "$src" >> "$work/manifest.txt"
    rm -f -- "$dest"
    return
  fi
  size=$(stat -c %s "$dest" 2>/dev/null) || return
  if [ "$size" -gt "$limit" ]; then
    rm -f -- "$dest"
    printf 'Skipped oversized file: %s\n' "$src" >> "$work/manifest.txt"
  fi
}

cleanup() {
  # Only the explicit, privately created staging directory; never the export.
  if [ -n "${work:-}" ] && [ -d "$work" ]; then
    rm -r -- "$work"
  fi
  if [ -n "${export_tmp:-}" ]; then
    rm -f -- "$export_tmp"
  fi
}

# Android entry point. No caller-controlled paths or collection commands.
if [ "$#" -ne 0 ]; then
  echo 'Run diagnostics.sh without arguments' >&2
  exit 2
fi
if [ "$(id -u)" != 0 ]; then
  echo 'Root access is required' >&2
  exit 1
fi
command -v timeout >/dev/null 2>&1 || { echo 'Missing timeout utility' >&2; exit 1; }
moddir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd) || exit 1
base=/data/adb/hma-uidfake
mkdir -p "$base" || exit 1
work=$(mktemp -d "$base/diagnostic-stage-XXXXXX") || exit 1
export_tmp=
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
mkdir -p "$work/logs" "$work/configs" "$work/kernel" "$work/references" || exit 1
printf '%s\n' 'HMA UID Fake diagnostics v1' \
  'PRIVATE: contains package names, rules, device/build details and kernel logs.' \
  'No APK data, credentials, serial properties or full logcat are collected.' \
  'Collection does not load/unload modules or change policy.' > "$work/manifest.txt"

capture "$work/device.txt" sh -c '
  date; id; uname -a; cat /proc/version
  for key in ro.product.model ro.product.device ro.build.display.id ro.build.version.release ro.build.version.sdk; do
    printf "%s=" "$key"; getprop "$key"
  done
  getenforce
'
copy_small "$moddir/module.prop" "$work/module.prop" 65536
for name in kmi.log selected-kmi load.reason kernel-load.log sync.log; do
  if [ -r "$moddir/state/$name" ]; then
    tail -c 2097152 "$moddir/state/$name" > "$work/logs/$name" 2>> "$work/manifest.txt"
  else
    printf 'Missing log: %s\n' "$name" >> "$work/manifest.txt"
  fi
done
capture "$work/status.json.txt" "$moddir/sync-tool" --status
capture "$work/duck-rules.txt" "$moddir/sync-tool" --list com.eltavine.duckdetector
capture "$work/packages.json.txt" "$moddir/sync-tool" --packages
capture "$work/kmi-current.txt" "$moddir/kmi-tool" select "$moddir"
copy_small /proc/config.gz "$work/kernel/config.gz" 1048576
capture "$work/kernel/modules.txt" cat /proc/modules
capture "$work/kernel/dmesg.txt" dmesg
capture "$work/logs/android-filtered.txt" logcat -b all -d -t 2000 -v threadtime -s \
  'uidfake:*' 'sync-tool:*' 'KernelSU:*' 'AndroidRuntime:E'
for name in disable_load; do
  [ ! -e "$base/$name" ] || printf 'Owner marker present: %s\n' "$name" >> "$work/manifest.txt"
done

# Exact rule files only; do not walk app data or collect backups/credentials.
copy_small "$base/config.json" "$work/configs/uidfake.json" 1048576
copy_small "$moddir/config.json" "$work/configs/legacy-uidfake.json" 1048576
copy_small /data/user/0/com.tsng.hidemyapplist/files/config.json "$work/configs/hma.json" 1048576
index=0
for dir in /data/misc/hide_my_applist_* /data/system/hide_my_applist_*; do
  [ -d "$dir" ] || continue
  index=$((index + 1))
  [ "$index" -le 8 ] || break
  printf 'hma-oss-%s: %s\n' "$index" "$dir" >> "$work/manifest.txt"
  copy_small "$dir/config.json" "$work/configs/hma-oss-$index.json" 1048576
  for cache in preset_cache_v2.json preset_cache.json; do
    copy_small "$dir/$cache" "$work/configs/hma-oss-$index-$cache" 2097152
  done
done

# Build-ID notes and a bounded sample of live module files permit offline ABI
# checks. Files are reference evidence, never candidates for loading.
count=0
for note in /sys/module/*/notes/.note.gnu.build-id; do
  [ -r "$note" ] || continue
  count=$((count + 1))
  [ "$count" -le 1024 ] || break
  name=${note#/sys/module/}
  name=${name%%/*}
  copy_small "$note" "$work/references/$name.build-id" 4096
  capture "$work/references/$name.taint" cat "/sys/module/$name/taint"
done
count=0
for dir in /vendor_dlkm/lib/modules /system_dlkm/lib/modules /vendor/lib/modules /odm/lib/modules /system/lib/modules; do
  [ -d "$dir" ] || continue
  capture "$work/kernel/module-files-$(printf '%s' "$dir" | tr / _).txt" \
    find "$dir/" -maxdepth 4 -type f -name '*.ko*'
  # Prefer informative network modules. Only sample ones with a live note.
  for name in cfg80211 rfkill bluetooth; do
    [ "$count" -lt 3 ] || break
    [ -f "$work/references/$name.ko" ] && continue
    [ -r "/sys/module/$name/notes/.note.gnu.build-id" ] || continue
    file=$(timeout 10 find "$dir/" -maxdepth 4 -type f -name "$name.ko" 2>/dev/null | head -n 1)
    [ -n "$file" ] || continue
    copy_small "$file" "$work/references/$name.ko" 8388608
    [ ! -f "$work/references/$name.ko" ] || count=$((count + 1))
  done
done

# Create and validate in root-private storage first. A failed export never
# claims success and never leaves a partial archive presented as complete.
archive="$work/diagnostics.tar.gz"
timeout 60 tar -czf "$archive" -C "$work" \
  manifest.txt device.txt module.prop status.json.txt duck-rules.txt \
  packages.json.txt kmi-current.txt logs configs kernel references || exit 1
timeout 30 tar -tzf "$archive" >/dev/null || exit 1
output_dir=/storage/emulated/0/Download
mkdir -p "$output_dir" || { echo 'Unlock device and allow access to Download' >&2; exit 1; }
export_tmp=$(mktemp "$output_dir/.uidfake-export-XXXXXX") || exit 1
cp "$archive" "$export_tmp" || exit 1
output="$output_dir/uidfake-diagnostics-$(date +%Y%m%d-%H%M%S)-${work##*-}.tar.gz"
mv "$export_tmp" "$output" || exit 1
export_tmp=
printf '{"ok":true,"path":"%s"}\n' "$output"
