# Design

## Queries

The four syscalls a uid scanner uses have their entries in `sys_call_table` (and the AArch32 numbers
in `compat_sys_call_table`) redirected, not the syscalls themselves: the hook substitutes the uid
argument and calls the original, which takes its own "no such uid" branch. The caller side comes
from the birth tag rather than from the current uid -- one table load and a `csel` -- so changing
uid cannot move a process into another set of rules. `/proc` and ptrace see the argument as the
caller wrote it, and there is nothing to restore.

A slot is patched only when the value in it is one this module resolved: the entry's own symbol, the
64-bit implementation, the compat wrapper under either of its names, in both the plain and the
jump-table spelling. The number a syscall has is a hint for the first comparison, never the thing
that decides -- a wrong number would otherwise be a hook on someone else's syscall, which is what a
32-bit getpriority used to be before this was a comparison.

## Naming an isolated child

An isolated process gets its uid at birth and nothing else says which app it came from:

1. **Birth.** The id change is watched where the kernel commits it: `task_fix_setuid` is taken over
   in the LSM hook list, chained into the implementation that was there (commoncap's on every kernel
   this is built for). It is called for every uid change, 32-bit callers included, and hands over
   both creds, so nothing has to be sampled around a call. When the framework gives an app uid to a
   fresh process, or `app_zygote` gives an isolated uid to one, the app id goes into bits 40..53 of
   `thread_info.flags` (zero = untagged) and an isolated child also gets a pending bit above that
   field. A task that is named already is left alone: its name came from the one transition that
   gave it its identity.
2. **Where the id change is watched.** The LSM hook at the commit is the one that sees both creds
   at once; when the kernel cannot give it to us (no exported way to move a 6.12 static call, which
   is what a kernel that trims unused ksyms looks like), the id setters in both syscall tables are
   hooked instead, exactly as KernelSU does, and the two ends are read around the call. The status
   says which of the two is in place.
3. **The first file of its code it opens.** The pending bit says a child is waiting. The base.apk of
   every app that has rules has its `->open` replaced with a copy of the inode's
   `file_operations` that differs in that one member, and the record behind the copy is the app id:
   the first open of that file names the whole thread group, with no lookup and no walk. The inode is
   held while its fields are read and only for that -- a path resolves to a dentry, not to a committed
   inode, and the package manager frees the one it is replacing while the helper is still sending the
   new one. Nothing is stored about the inode: a record keeps the numbers it was made for and the
   table to put back, is never handed to another file, and carries this module as the owner of its
   table -- so an open file keeps the module loaded and the table cannot be given back while one is
   still using it. At unload each file is found again from the path it was registered with. The app
   id is taken from inside the uid (`uid % 100000`), because a user id is the high part of it: read
   as a whole number, an app of a secondary user looks like an isolated uid and would never be named.
3. **Before the module loads.** `uidfake_tag_prime()` derives the same tag for every running task
   from its uid. Without it a manual `rmmod`/`insmod` would lose the identity of every running app.

## Patch writes

The kernel text and rodata this touches are read-only, and nothing that makes them writable is
exported to modules. The physical address of the target is translated with the image offset
(`va - kimage_voffset`), the page is mapped through the kernel's own fixmap window, and the write
goes through a nofault copy.

- Every target is inside `[_stext, _end)`; anything else is refused before a byte is written.
- The fixmap address depends on the VA size the kernel was built with, which is not always the one
  this module was built with. Two candidates are tried, this build's and the one derived from the
  kernel's own `vmemmap` (`FIXADDR_TOP = VMEMMAP_START - SZ_32M`), and each is proved before use:
  the bytes at the alias have to be the bytes at the target. Neither proved means nothing is written.
- The page table walk is a second opinion and is calibrated against the image offset once at load. A
  kernel whose `struct mm_struct` or geometry is not this module's makes the walk answer with a
  different page; the offset is the one that does not care, and it is what the write needs.
- What was written is read back, and a hook that is no longer this module's is left alone on unload
  rather than overwritten.

## Diagnostics

Diagnostics sit behind a static key (jump label): with the key off the branch is a NOP.

```
lkmloader hma_uidfake.ko debug=1     # for 60 seconds, then off again
```

What the module is doing is also readable where a user looks: `KAUX_CMD_STATUS` answers with the
entry counts for both tables, how many apk inodes are held and how many of an apply failed, whether
the setuid hook was taken and from which implementation, the geometry this module was built for, and
the last failure. `sync-tool` reads it and writes the one-line summary into the module description,
which is where KernelSU and Magisk show a module's state, and compares the module's geometry against
the running kernel's config (`/proc/config.gz`).

## Invariants

- Never take the `find_user()` hit path: it walks every process at ~1000x the cost of a miss, which
  timing shows. The argument is rewritten instead and the kernel takes its own miss branch.
- The replacement uid hashes into the same bucket as the target (`__uidhashfn(uid) = ((uid >> 7) +
  uid) & 127`), or the chain length would differ from a genuinely absent uid.
- The lookup does constant work: one hash of the target with the kernel's own uid hash (read back
  from `find_user()` when a policy is applied) gives the bucket line and the starting slot; the line
   contents; the probe count is fixed when the policy is laid out (1, 2 or 4), each probe
   reads one slot -- the target's own slot of its own line, then the same slot of the lines
   that follow, which is where the layout puts a target whose line is full -- and one word
   of that slot's mask, the one the caller's own id selects. Which words those are follows
   from the caller and the target and never from the answer; indices are masked, never
   branched on; `cmp`+`csel` picks the bit and the replacement, and the select is a `csel`
   rather than a branch on purpose (a predictor can learn a branch on the answer). Probing
   across lines is what keeps the table the size of the target count rather than of the
   worst collision on one line: 24000 targets need 8192 lines (512 KB) instead of 32768
   (2 MB). The masks are interned, one entry per distinct set of callers, so a policy of
   tens of thousands of pairs keeps a few hundred of them instead of one copy per slot.

   The replacement a hidden target answers with is chosen once, at apply time, from a low
   unassigned range (20001..24096) carrying the target's own uidhash bucket, so a syscall
   that resolves a uid through `find_user()` walks the chain it would for the target. It is
   low because the kernel rejects a small uid several nanoseconds faster than a large one,
   which any caller can measure (`uidbench`, which is why the select is a `csel` too). What
   is left after both is the kernel's own per-value cost variation, the same for these
   values as for any other uid that does not exist. A query touches a function of `(caller, target)` alone --
  `scripts/lookup_model.py` states that function.
- Never touch the syscall's `pt_regs`: the probe sits on `find_user()`, the first place a uid is a
  plain argument register. (arm64 has no in-register syscall entry to hook: no `__do_sys_`/
  `__se_sys_` symbol.)
- The kernel only compares numbers; whatever needs a path, a package name or JSON happens in
  `sync-tool`, and what arrives is checked for shape and size.
- A rejected update changes nothing: a policy that does not fit, a caller that is not an app uid, a
  group larger than the tables -- each is logged and the previous policy stays in force, because
  half a policy is the state that leaks.

## Trust

- The netlink family is `GENL_ADMIN_PERM`: only root can push a policy, both blobs are
  length-checked before they are parsed, and nothing is copied back out except the status.
- HMA's `config.json` decides who is hidden and belongs to HMA's uid; `sync-tool` reads nothing an
  app can write.
- The tag lives in bits 40..53 of `thread_info.flags` and the pending bit in bit 55; both are only
  read-modify-written with those bits masked out. KernelSU's own marker is the standard
  `TIF_SYSCALL_TRACEPOINT` bit, so the two do not share a field.
- The hooked syscalls take at most three arguments, which is what the register object they receive
  covers.
- Normal runs print no addresses; the two init lines that do are behind the debug key.
- The timing a hidden uid still costs is measured, not assumed away: `src/tools/uidbench.c` samples
  the hidden, absent and unhooked cases in one round and reports paired deltas.

## The module's own rules and the WebUI

The rules do not have to come from HMA. The module has a format of its own, and the places a config
can be are checked in a fixed order, the module's own file first:

```
/data/adb/hma-uidfake/config.json          the module's own (Native) -- checked first
/data/adb/modules/hma-uidfake/config.json  the same, when a build keeps it in the module
/data/user/0/com.tsng.hidemyapplist/files/config.json                                   HMA
/data/misc|/data/system/hide_my_applist_*/config.json                                   HMA-OSS
```

The native file sits outside `/data/adb/modules/` on purpose: a module update replaces that tree, and
a hide list is not something an update should carry away. Which format is read is a property of the
path, and each format has its own class (`HmaRules`, `HmaOssRules`, `NativeRules`) behind one
interface, so the rest of the tool does not know which one it is holding. The user-space side needs
no Zygisk and no framework hook: it is the same process that already reads a config and pushes the
pairs over `kaux`, and it only learned one more format to read.

The native format is the one the WebUI writes:

```json
{
  "version": 2,
  "mode": "blacklist",
  "hide_system": false,
  "templates": {"social": ["com.a", "com.b"]},
  "apps": {
    "com.caller": {
      "hide": ["com.target"],
      "templates": ["social"],
      "hide_all": false
    }
  }
}
```

A caller hides its own `hide` list plus the union of the templates it applies. `mode` may be
`whitelist`, which turns that set into the one that stays visible instead, and `hide_all` hides every
app. A system target is left alone unless `hide_system` is on for the pair, because making the
framework itself look absent breaks a device long before it hides an app. A caller never hides
itself. A `version` newer than this tool knows is refused rather than read with a field missing,
which is the failure this whole thing exists to avoid.

The WebUI is served from `module/webroot/` by the KernelSU manager and decides nothing by itself:
every read is one of `sync-tool --status`, `--packages`, `--get-config`, and every write is
`--set-config`, so the page and the running policy cannot disagree about which format a file is in.
Each command prints one JSON document on stdout and logs nothing there (the daemon's log goes to
stderr), so the page reads a result, not a log. `--set-config` reads the document from stdin, checks
it exactly as the reader would, writes it through a temporary file and a rename (keeping one `.bak`),
and pushes it in the same step -- a config the daemon would refuse is never left on disk. On the
device the page runs `sync-tool` through the manager's own `ksu.exec`, so nothing has to be a root
shell over the network.

The page is a home list over sub-screens: a card of kernel status, then Manage apps, Templates,
Backup and restore, How it works, Settings and About. Manage apps is a searchable list of every
installed app (filtered by user/system/with-a-rule), each row carrying a switch that adds or removes
that app's rule; opening a row gives the rule's mode, its `hide_all` and `hide_system`, its own
`hide` list and the templates it applies, drawn from the same app list. A template is a named list of
apps, so renaming or deleting one follows through to every rule that references it -- a rule left
pointing at a name that no longer exists would hide less than its author sees. Settings holds the
theme and the module-wide `mode` and `hide_system`, which are the default for apps that do not
override them.

The screens follow the layout of the HMA-OSS app, because a user coming from it already knows these
settings in that shape. Nothing is taken from it: HMA-OSS is AGPL-3.0 and this module is GPL-2.0, and
its UI is Kotlin against Compose and Fragments rather than a page, so there is no code here to reuse
even if the licences allowed it. What that app offers and a kernel-side uid guard cannot honour --
icon and launcher hiding, per-hook switches, logs, an accessibility service -- is left out rather than
shown as a switch that would do nothing.

## Protocol

Little endian, defined once in `include/kaux.h`, which the module and the tool both include. The
family version is 3 and the kernel rejects a request that does not carry it, so a helper and a
module of different versions cannot read each other's command ids. Version 3 has not been published
before 0.3.0, so it is the layout as it stands.

```
KAUX_CMD_PING         (1)  no payload, ACK only
KAUX_CMD_STAGE_BEGIN  (2)  blob: struct kaux_begin {u32 kind, u32 bytes, u32 crc32}
KAUX_CMD_STAGE_CHUNK  (3)  blob: u32 offset, u32 len, then len bytes
KAUX_CMD_STAGE_COMMIT (4)  no payload: byte count and CRC are checked, then applied
KAUX_CMD_STATUS       (5)  reply: KAUX_ATTR_STATUS = struct kaux_status
```

Family `kaux`, all commands `GENL_ADMIN_PERM`; a blob over 32 KiB is rejected before it is parsed.
A blob goes up in chunks and only becomes live when the commit matches what was announced, so a
half-uploaded policy never takes effect. `struct kaux_status` carries magic, size and version, so a
reader that is not looking at the structure it was built for says so instead of misreading it.

| limit | value |
|---|---|
| `(caller, target)` pairs | 8192 |
| callers | 8192 |
| code dirs (`UF_APK_MAX`) | 10000 |
| netlink blob (`KAUX_STAGED_BYTES`) | 4 MiB |
| netlink message (`MAX_BLOB_BYTES`) | 32 KiB |
