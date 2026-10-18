# Android 14 / Linux 6.1 build verification

This branch is isolated from main. It runs the upstream GitHub Actions workflow with its KMI matrix limited to `android14-6.1`.

Source base: `Anatdx/hma-uidfake` commit `dcc8c1993f355de083d1beca834da423c0ae2098` (v0.3.3-era code).

The result is **not** source-identical to the external HMA UID Fake v0.4.0+1c71b6c archive. Do not label it v0.4.0 or assume compatibility with every OEM kernel without a runtime test.
