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

# Keep every candidate for reboot/OTA re-detection; no .ko.try or manual force path.
chmod 0755 "$MODPATH/kmi-tool"
. "$MODPATH/load.sh"
if uidfake_select "$MODPATH"; then
  ui_print "- KMI: $_selected_kmi; unmodified module will be checked by the kernel after reboot."
else
  ui_print "! No verified compatible module selected. See state/kmi.log and module status."
  ui_print "! Android version/source BRANCH alone cannot establish binary compatibility."
  ui_print "! Installation preserved all candidates; no cross-KMI loading will be attempted."
fi

set_perm_recursive "$MODPATH" 0 0 0755 0644
for f in "$MODPATH"/*.sh; do chmod 0755 "$f"; done
[ -f "$MODPATH/ko/hma_uidfake.ko" ] && chmod 0644 "$MODPATH/ko/hma_uidfake.ko"
[ -f "$MODPATH/sync-tool" ] && chmod 0755 "$MODPATH/sync-tool"
[ -f "$MODPATH/lkmloader" ] && chmod 0755 "$MODPATH/lkmloader"
[ -f "$MODPATH/kmi-tool" ] && chmod 0755 "$MODPATH/kmi-tool"

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
