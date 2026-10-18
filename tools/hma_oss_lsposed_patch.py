#!/usr/bin/env python3
"""Minimal, checked fix for HMA-OSS oss-162's LSPosed PackageManager hooks.

This modifies an upstream checkout; it is NOT a modification to hma_uidfake.ko.
Replaces only four premature *getPackageInfoInternal/*getApplicationInfoInternal
hookBefore entries with hookAfter, preserving any framework exception.

Pinned source: frknkrc44/HMA-OSS oss-162 @
b52a10d609d069e82e578d97b4c25a8f4c5d822a
"""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys


METHODS = ("getPackageInfoInternal", "getApplicationInfoInternal")


def patch(source: str) -> str:
    """Fail instead of quietly producing an APK from unfamiliar upstream code."""
    for method in METHODS:
        p = re.compile(
            r'(?m)^(?P<indent>[ \t]*)hooks \+= findMethod\('
            r'(?:COMPUTER_ENGINE_CLASS|clazzToHook)\) \{\n'
            r'[ \t]*name == "' + re.escape(method) + r'"\n'
            r'[ \t]*\}\.hookBefore \{ param ->'
        )
        def correct(m: re.Match[str]) -> str:
            return (
                m.group(0).replace("}.hookBefore { param ->",
                                    "}.hookAfter { param ->") +
                "\n" + m.group("indent") +
                "    if (param.hasThrowable()) return@hookAfter"
            )
        source, n = p.subn(correct, source)
        if n != 2:
            raise RuntimeError(
                f"{method}: expected exactly 2 guarded Xposed backend hooks; got {n}"
            )

    for method in METHODS:
        # Detect unexpected upstream layout or accidental regression.
        guarded = re.findall(
            r'name == "' + re.escape(method) + r'"\n'
            r'\s*\}\.hookAfter \{ param ->\n'
            r'\s*if \(param\.hasThrowable\(\)\) return@hookAfter',
            source,
        )
        if len(guarded) != 2:
            raise RuntimeError(f"{method}: exception guards missing after patch")

    return source


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "checkout",
        help="HMA-OSS oss-162 repository checkout (not the UID Fake repository)",
    )
    args = parser.parse_args()

    path = (
        Path(args.checkout)
        / "xposed/src/main/java/icu/nullptr/hidemyapplist/xposed/hook/PmsHookTargetBase.kt"
    )
    if not path.is_file():
        raise RuntimeError(f"Source file missing: {path}")
    original = path.read_text(encoding="utf-8")
    edited = patch(original)
    if original == edited:
        raise RuntimeError("No source modifications applied")
    path.write_text(edited, encoding="utf-8")
    print(
        "HMA-OSS LSPosed oss-162: patched 4 PackageManager hooks to preserve "
        "original SecurityException / other framework exceptions."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
