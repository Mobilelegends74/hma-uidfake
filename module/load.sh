#!/system/bin/sh
# Shared install/boot selection. Never guess by Linux major.minor or ROM SDK.
uidfake_status() {
  _status_dir="$1"
  _status_reason="$2"
  mkdir -p "$_status_dir/state"
  printf '%s\n' "$_status_reason" >"$_status_dir/state/load.reason"
  _status_label='❌ Not working'
  if grep -q 'HMA_UIDFAKE_INSTALL_LOCALE = "ru"' "$_status_dir/webroot/locale.js" 2>/dev/null; then
    _status_label='❌ Не работает'
  fi
  awk -v message="$_status_label — $_status_reason" '
    /^description=/ { print "description=" message; next }
    { print }
  ' "$_status_dir/module.prop" >"$_status_dir/module.prop.kmi.tmp" &&
    mv -f "$_status_dir/module.prop.kmi.tmp" "$_status_dir/module.prop"
}

uidfake_select() {
  _select_dir="$1"
  mkdir -p "$_select_dir/state"
  printf '' >"$_select_dir/state/selected-kmi"
  if [ ! -x "$_select_dir/kmi-tool" ]; then
    uidfake_status "$_select_dir" '[KMI_TOOL] Missing executable kmi-tool; reinstall TEST-KMI ZIP'
    return 1
  fi
  _selected_kmi="$("$_select_dir/kmi-tool" select "$_select_dir" 2>"$_select_dir/state/kmi.log")"
  _select_rc=$?
  cat "$_select_dir/state/kmi.log"
  if [ "$_select_rc" -ne 0 ]; then
    _reason="$(head -n 1 "$_select_dir/state/kmi.log" | cut -c 1-220)"
    uidfake_status "$_select_dir" "${_reason:-[KMI_UNKNOWN] No verified compatible module}"
    return 1
  fi
  # Treat output as data, never source/eval it. The helper also verifies ELF
  # name, ARM64, branch and intact symbol-version metadata.
  case "$_selected_kmi" in
    android12-5.10|android13-5.10|android13-5.15|android14-5.15|android14-6.1|android15-6.6|android16-6.12|android17-6.18) ;;
    *) uidfake_status "$_select_dir" '[KMI_OUTPUT] Invalid selector result'; return 1 ;;
  esac
  UIDFAKE_KO="$_select_dir/ko/${_selected_kmi}_arm64_hma_uidfake.ko"
  [ -f "$UIDFAKE_KO" ] || return 1
  printf '%s\n' "$_selected_kmi" >"$_select_dir/state/selected-kmi"
  echo "[load] $_selected_kmi selected; kernel ABI admission is still required"
  return 0
}

uidfake_load() {
  _moddir="$1"
  if [ -e /data/adb/hma-uidfake/disable_load ]; then
    uidfake_status "$_moddir" '[DISABLED] Loading disabled by owner marker'
    echo '[load] disabled by owner marker'
    return 1
  fi
  if lsmod | grep -q '^hma_uidfake '; then
    printf '' >"$_moddir/state/load.reason"
    echo '[load] kernel module already loaded; will not stack or hot-replace'
    return 0
  fi
  # Re-evaluate on EVERY boot/OTA, not only at installation. Keep all KMI files.
  uidfake_select "$_moddir" || return 1
  "$_moddir/kmi-tool" load "$UIDFAKE_KO" >"$_moddir/state/kernel-load.log" 2>&1
  _load_rc=$?
  cat "$_moddir/state/kernel-load.log"
  if lsmod | grep -q '^hma_uidfake '; then
    printf '' >"$_moddir/state/load.reason"
    echo '[load] module present; sync-tool must verify hooks and active policy'
    return 0
  fi
  _reason="$(head -n 1 "$_moddir/state/kernel-load.log" | cut -c 1-220)"
  uidfake_status "$_moddir" "${_reason:-[LOAD_FAILED] Module absent after strict load (rc=$_load_rc)}"
  echo '[load] refused/failed; no alternate KMI, patched vermagic or symbol bypass will be attempted'
  return 1
}
