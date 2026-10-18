# Experimental HMA-OSS LSPosed exception-preserving APK

This is a separate Android APK experiment. **It is not part of the HMA UID Fake
kernel module, and does not modify its LKM, scripts, or policy format.**

## Diagnosis

User-observed on REDMAGIC / Android 15:
- HMA-OSS oss-162 LSPosed + HMA UID Fake: Duck Detector finds 8 repeated
  `getPackageInfo` and `getApplicationInfo` cross-user permission exceptions.
- Same hiding rules, switching HMA-OSS backend to Zygisk: warning disappears.
- HMA UID Fake (KMI android14-6.1) was loaded and had 83 synchronized rules.

In HMA-OSS oss-162, `PmsHookTargetBase.kt` hooks
`getPackageInfoInternal` and `getApplicationInfoInternal` **before** the original
Android framework methods execute. For a hidden package it returns null
before the platform can enforce cross-user `MATCH_ANY_USER` permissions.
Duck Detector's controlled checks correctly expect `SecurityException`
before a target-package lookup. The premature null can be surfaced as
`NameNotFoundException` by the public PackageManager wrapper.

The later Zygisk implementation hooks these methods **after** calling the
original and skips filtering if the original threw an exception.

## Patch scope

The checker edits exactly four LSPosed hooks for the two methods (two Android
SDK branches). It uses `hookAfter` and checks `param.hasThrowable()` before
filtering, keeping existing SecurityException and other exceptions untouched.
It leaves `shouldFilterApplication` and all other hooks unchanged.

Source pin: `frknkrc44/HMA-OSS` at tag `oss-162`,
commit `b52a10d609d069e82e578d97b4c25a8f4c5d822a`.

This is a **candidate fix**, not a physical-device verified result.
The CI only compiles the APK; complete behavioral coverage requires
RedMagic OS 15/real LSPosed, including package visibility and
permission-exception regression tests. It is not guaranteed to remove
all detection signals.

## Installation warning

The experimental APK is **debug signed**, not signed with upstream
HMA-OSS's private release key. Android will normally block installing
this APK as an update to the official HMA-OSS release. Do not uninstall
your working HMA-OSS Zygisk manager just to test this without a verified
config export and restore procedure. The Zygisk manager and LSPosed app
share the same package name, and switching implementation may require
re-registering scopes and rebooting.

No malicious or third-party prebuilt APK is shipped. Build logs and
the patch appear in the GitHub Action.

Upstream links:
- https://github.com/frknkrc44/HMA-OSS/releases/tag/oss-162
- https://github.com/frknkrc44/HMA-OSS/blob/oss-162/xposed/src/main/java/icu/nullptr/hidemyapplist/xposed/hook/PmsHookTargetBase.kt
- https://github.com/frknkrc44/HMA-OSS/blob/oss-173/zygote/src/main/java/org/frknkrc44/hma_oss/zygote/hook/PmsHookTarget33.kt
- https://github.com/eltavine/Duck-Detector-Refactoring/blob/f3f430956efb/feature/dangerousapps/data/src/main/kotlin/com/eltavine/duckdetector/features/dangerousapps/data/probes/hma/HmaPlatformCalls.kt
