# OnePlus / Wild 6.12.38 ABI variant

This directory records the public build inputs for an **additional**
`android16-6.12-oneplus-6.12.38` module. It does not replace the standard
`android16-6.12` binary or provide universal support for every 6.12 kernel.

Reference: [WildKernels run 38894819716](https://github.com/WildKernels/OnePlus_KernelSU_SUSFS/actions/runs/38894819716),
OP15r job 116952723905, artifact `AK3_OP15r_A16_android16-6.12.38_KSUN_33389_SuSFS_v2.3.0.zip`.
The distributed Image has SHA256
`e8bc9ba3f6d556afb50b7f69ff4cd699f81afd2c3bcddab2c8d395019e84e5dd`
and GNU build ID `17975ac76c1fafe0c23e057f7d865b962878279d`.

`kernel.config` was extracted from that public Image, not from a user's private
diagnostic archive. `abi.json` contains the exported CRCs needed by UIDFAKE,
extracted with `scripts/kernel-image-abi.py`, plus provenance. It is deliberately
not an arbitrary replacement Module.symvers.

## Reproduction and validation

Run `.github/workflows/build-oneplus-61238.yml` on the test branch. Its script
`scripts/build-oneplus-61238.sh` pins the OnePlus common source, compiler and Rust
toolchain and reproduces the upstream SYSVIPC header layout. The kernel build
generates its own `vmlinux.symvers`; the module is compiled against those native
outputs with GENDWARFKSYMS and extended modversions retained. No final kernel
Image is built or shipped. Module instructions target generic ARMv8-A, not a
specific OnePlus CPU.

`vmlinux.btf.xz` is debug type information extracted from the same authenticated
public Image (SHA256 is recorded in `abi.json`), not a kernel executable or a
private device file. It provides the actual target's BTF base for external-module
debug metadata without rebuilding/linking another kernel. Reproduce it with:

```sh
python3 scripts/kernel-image-btf.py --extract /path/to/reference/Image compat/oneplus-6.12.38/vmlinux.btf.xz
```

The helper checks both the complete Image SHA256 and the extracted BTF SHA256.

The packaging job checks the source manifest and every imported symbol CRC
against the original Image using `scripts/check-oneplus-abi.py`. Missing or
different CRCs stop packaging. Neither the loader nor the build rewrites CRCs,
vermagic or kernel checks. Kernel/module signature and ABI admission are still
the kernel's responsibility.

Automatic selection accepts either the authenticated reference Image identity
(GNU build ID plus full kernel banner and complete candidate CRC coverage), or
a unique candidate matching build-ID-verified live reference modules. A Linux
version string alone never selects this extra ABI variant. The old standard
6.12 path remains available for ordinary GKI kernels.

Successful compilation and offline matching do not substitute for a reboot and
functional HMA/HMA-OSS test on the target device.
