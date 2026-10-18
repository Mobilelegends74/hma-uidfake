#!/usr/bin/env bash
# Check that a device's uname -r maps to the KMI the module was built for.
#
# The branch is what matters: a 5.15 kernel from the android14 branches reports "android14-...",
# and mapping it to android13-5.15 loads a module whose vermagic the kernel refuses. Run this
# after a build: it uses the KMI names that are actually in build/ko.
set -eu
cd "$(dirname "$0")/.."

stage=build/kmi-map
rm -rf "$stage"
mkdir -p "$stage/ko"
for kmi in $(ls build/ko 2>/dev/null | sed 's/_arm64_hma_uidfake\.ko$//' | sort -u); do
  : >"$stage/ko/${kmi}_arm64_hma_uidfake.ko"
done
if [ -z "$(ls -A "$stage/ko")" ]; then
  echo "FAIL: build/ko is empty; build the modules first"
  exit 1
fi

MODPATH=$stage
ui_print() { echo "  [installer] $*"; }
uname() { echo "$CASE"; }
. <(sed -n '/^kmi_from_uname()/,/^}/p' module/customize.sh)

fail=0
check() {
  CASE=$1 desired=$2
  got=$(kmi_from_uname)
  [ "$got" = "$desired" ] || { echo "FAIL $CASE -> '$got', wanted '$desired'"; fail=1; return; }
  if [ -f "$stage/ko/${got}_arm64_hma_uidfake.ko" ]; then
    echo "ok   $CASE -> $got (in the zip)"
  else
    echo "ok   $CASE -> $got (not built; the installer reports it instead of loading another)"
  fi
}

check "5.10.198-android13-8-g1234-ab"                       android13-5.10
check "5.15.149-android13-11-g1-ab"                        android13-5.15
check "5.15.167-android14-12-g1-ab"                        android14-5.15
check "6.1.138-android14-11-g0c3559bcd85-ab14529422"       android14-6.1
check "6.6.30-android15-8-g1-ab"                           android15-6.6
check "6.12.23-android16-6-g1-ab"                          android16-6.12
check "6.18.1-android17-5-g1-ab"                           android17-6.18
check "5.10.198-android12-9-g1-ab"                         android12-5.10
check "4.19.191-g1234ab"                                   ""
check "5.15.167-gvendor-123"                                ""
exit $fail
