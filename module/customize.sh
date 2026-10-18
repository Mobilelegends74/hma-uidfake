#!/system/bin/sh
SKIPUNZIP=0

ui_print "- HMA UID Fake fork"

# Read Android system language at install time (independent from manager locale).
SYS_LOCALE="$(getprop persist.sys.locale 2>/dev/null)"
[ -n "$SYS_LOCALE" ] || SYS_LOCALE="$(getprop persist.sys.locales 2>/dev/null)"
[ -n "$SYS_LOCALE" ] || SYS_LOCALE="$(getprop persist.sys.language 2>/dev/null)"
[ -n "$SYS_LOCALE" ] || SYS_LOCALE="$(getprop ro.product.locale 2>/dev/null)"
[ -n "$SYS_LOCALE" ] || SYS_LOCALE="$(getprop ro.product.locale.language 2>/dev/null)"
# Prefer Android's active configuration when available. On some vendor ROMs,
# persist.sys.locale may be empty or stale, while 'am get-config' reflects
# the language currently used by the system UI (e.g. ru-rRU).
ACTIVE_CONFIG_LOCALE="$(am get-config 2>/dev/null | grep '^config:' | grep -oE '[a-z]{2}-r[A-Z]{2}' | head -n1)"
if [ -n "$ACTIVE_CONFIG_LOCALE" ]; then
  SYS_LOCALE="$ACTIVE_CONFIG_LOCALE"
else
  SETTINGS_LOCALE="$(settings get system system_locales 2>/dev/null)"
  case "$SETTINGS_LOCALE" in
    ""|null|undefined) ;;
    *) SYS_LOCALE="$SETTINGS_LOCALE" ;;
  esac
fi
SYS_LOCALE="${SYS_LOCALE%%,*}"
case "$SYS_LOCALE" in
  ru|ru-*|ru_*) UI_LOCALE=ru ;;
  *) UI_LOCALE=en ;;
esac
if [ -d "$MODPATH/webroot" ]; then
  printf 'window.HMA_UIDFAKE_INSTALL_LOCALE = "%s";\n' "$UI_LOCALE" > "$MODPATH/webroot/locale.js"
fi
if [ "$UI_LOCALE" = ru ]; then
  ui_print "- WebUI: русский (системный язык Android)"
else
  ui_print "- WebUI: English (Android system language)"
fi

# 6.1.138-android14-11-g0c3559bcd85-ab14529422 -> android14-6.1
# The branch and the kernel version are two different things: the "11" in "android14-11" is a KMI
# generation, not a kernel version, and a 5.15 kernel from the android14 branches reports
# "android14-...". It is the branch that decides between two KMIs sharing a kernel version
# (android13-5.15 and android14-5.15), so both parts are read here.
kmi_from_uname() {
  _r="$(uname -r)"
  _branch="$(echo "$_r" | grep -oE 'android[0-9]+' | head -n1)"
  _ver="$(echo "$_r" | grep -oE '^[0-9]+\.[0-9]+' | head -n1)"

  if [ -n "$_branch" ]; then
    # The device names its branch, so only that branch's module will do: one from another branch
    # has a different vermagic and the kernel would refuse it anyway.
    echo "${_branch}-${_ver}"
    return
  fi

  # No branch in the string (a vendor kernel): fall back to the kernel version alone, and say so
  # when more than one KMI matches it.
  _cands=""
  for _c in $(ls "$MODPATH/ko" 2>/dev/null | sed 's/_arm64_hma_uidfake\.ko$//' | sort -u); do
    [ "${_c#*-}" = "$_ver" ] && _cands="$_cands $_c"
  done
  _cands="${_cands# }"
  _count="$(echo "$_cands" | wc -w)"
  if [ "$_count" -ne 1 ]; then
    [ "$_count" -gt 1 ] && ui_print "! Ambiguous vendor kernel: $_cands (no automatic loading)" >&2
    echo ""
    return
  fi
  echo "${_cands%% *}"
}

KMI="$(kmi_from_uname)"
SRC=""
[ -n "$KMI" ] && [ -f "$MODPATH/ko/${KMI}_arm64_hma_uidfake.ko" ] && SRC="$MODPATH/ko/${KMI}_arm64_hma_uidfake.ko"

if [ -n "$SRC" ]; then
  cp -f "$SRC" "$MODPATH/ko/hma_uidfake.ko"
  for f in "$MODPATH"/ko/*_arm64_hma_uidfake.ko; do
    [ -e "$f" ] && rm -f "$f"
  done
  ui_print "- $(uname -r) -> $(basename "$SRC")"
else
  # Nothing here can load: the kernel refuses another KMI's vermagic, and the structures this
  # module reads differ between them. Keep the closest build for anyone who wants to try it.
  _want_ver="$(uname -r | grep -oE '^[0-9]+\.[0-9]+' | head -n1)"
  for _f in "$MODPATH"/ko/*-"$_want_ver"_arm64_hma_uidfake.ko; do
    [ -e "$_f" ] || continue
    cp -f "$_f" "$MODPATH/ko/hma_uidfake.ko.try"
    break
  done
  ui_print "! No ko for $(uname -r) (KMI ${KMI:-unknown}). ko/ has: $(ls "$MODPATH/ko" | tr '\n' ' ')"
  ui_print "! A build for another KMI will not load, and forcing it in can crash or bootloop the"
  ui_print "! device. Build your own (docs/build.md) and report the device, its KMI and uname -r."
  ui_print "! No automatic loading is attempted for unmatched kernels."
fi

set_perm_recursive "$MODPATH" 0 0 0755 0644
for f in "$MODPATH"/*.sh; do chmod 0755 "$f"; done
[ -f "$MODPATH/ko/hma_uidfake.ko" ] && chmod 0644 "$MODPATH/ko/hma_uidfake.ko"
[ -f "$MODPATH/sync-tool" ] && chmod 0755 "$MODPATH/sync-tool"
[ -f "$MODPATH/lkmloader" ] && chmod 0755 "$MODPATH/lkmloader"

if [ "$UI_LOCALE" = ru ]; then
  ui_print "- Автозагрузка после одной обычной перезагрузки."
  ui_print "- Ручная активация allow_load не требуется."
  ui_print "- Отключение: su -c 'touch /data/adb/hma-uidfake/disable_load'"
  ui_print "- Готово"
else
  ui_print "- Compatible kernels load automatically after the normal reboot."
  ui_print "- No manual allow_load marker is required."
  ui_print "- To disable next boot: su -c 'touch /data/adb/hma-uidfake/disable_load'"
  ui_print "- Done"
fi
