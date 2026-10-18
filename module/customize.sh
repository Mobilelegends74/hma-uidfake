#!/system/bin/sh
SKIPUNZIP=0

ui_print "- HMA UID Fake"

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
  [ "$(echo "$_cands" | wc -w)" -gt 1 ] && ui_print "! $(uname -r) fits $_cands; taking the first"
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
  ui_print "! To try the closest build anyway, unsupported and at your own risk:"
  ui_print "!   su -c 'mv $MODPATH/ko/hma_uidfake.ko.try $MODPATH/ko/hma_uidfake.ko && reboot'"
fi

set_perm_recursive "$MODPATH" 0 0 0755 0644
for f in "$MODPATH"/*.sh; do chmod 0755 "$f"; done
[ -f "$MODPATH/ko/hma_uidfake.ko" ] && chmod 0644 "$MODPATH/ko/hma_uidfake.ko"
[ -f "$MODPATH/sync-tool" ] && chmod 0755 "$MODPATH/sync-tool"
[ -f "$MODPATH/lkmloader" ] && chmod 0755 "$MODPATH/lkmloader"

ui_print "! AUDIT BUILD: kernel autoload is disabled by default."
ui_print "! See module README and BUILD_VERIFICATION.md before opting in."
ui_print "- Done"
