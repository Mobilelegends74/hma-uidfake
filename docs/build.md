# Build and tests

Each KMI builds with the compiler its kernel was built with: clang CFI and the shadow call stack
come from that compiler, so mixing is not an option. kbuild links the module itself and writes its
objects into its `M=` directory, so each KMI builds in `build/kmi/<kmi>/`, a directory holding only
symlinks to `src/`.

The version and `versionCode` in the packed `module.prop` come from the git tag (falling back to the
values in `module/module.prop` for a tree without git). `-DUG_USE_PREBUILT_KO=ON` packs the modules
already in `build/ko`, which is how the CI packaging job assembles a zip from separately built KMI
jobs.

The zip also carries `sync-tool` (built from `src/tools`) and `lkmloader` (built from the
`external/lkmloader` submodule, an upstream MIT project pinned by `.gitmodules` + gitlink).
The bundled legacy `lkmloader` is not called by installation or boot scripts. Since 0.3.7,
`kmi-tool` selects a verified candidate and loads the unchanged file using `finit_module(flags=0)`.
Clone with `--recurse-submodules`, or run `git submodule update --init`.

## Tests

- `scripts/run-hosttest.sh` compiles the real `src/policy.c` against the `linux/*` shims in
  `scripts/hosttest/` and checks every configured `(caller, target)` pair, and the same for the ABX
  reader against the first bytes of a real `packages.xml`. Real sources, so word sizes, field order
  and the encoding of a branch written into kernel text are covered.
- `scripts/branch_encode_test.c` checks the branch encoding on its own, in user space.
- `scripts/config_paths_test.cpp` builds a tree and checks the rule-source specs: an exact file, an
  HMA-OSS data directory named with a random suffix, and specs that match nothing yet.
- `scripts/rules_test.cpp` drives both rule formats with configs instead of a device: HMA's hidden
  set and built-in list, and HMA-OSS's decision chain with its opposite list and presets.
- `scripts/lookup_model.py` states what a query touches as a function of `(caller, target)` and the
  load counts, which the C cannot assert about itself.
- `scripts/check-undefined.sh` checks every undefined symbol against the DDK's per-KMI
  `Module.symvers`, the same table kbuild uses; `scripts/test-kmi-map.sh` checks `uname -r` to KMI
  mapping and that the zip carries the result.
- `scripts/run-clang-tidy.sh` analyses both halves: the module with the flags kbuild really
  compiled with, taken from its `.cmd` files, and the userspace helper on its own.
- `scripts/check-format.sh` covers what neither tool reaches -- the CMake files, the shell scripts,
  the module template, the workflow -- and runs `clang-format --dry-run` over every source file. The
  module and the test sources use the kernel's `.clang-format`, `src/tools/` uses LLVM's.

## uidbench

Not part of the build. Cross-compile and run it as an app uid that hides:

```bash
$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android34-clang \
  -O2 -static -o uidbench src/tools/uidbench.c
```

It samples the hidden, absent and unhooked cases in the same round and reports paired deltas, t
statistics and the sample count an attacker would need for a 5 sigma decision.
