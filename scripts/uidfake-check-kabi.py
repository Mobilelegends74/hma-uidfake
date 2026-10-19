#!/usr/bin/env python3
"""Check the actual Android KABI macros used by the integrated task field."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

kernel = Path(sys.argv[1])
sched = (kernel / 'include/linux/sched.h').read_text()
task_start = sched.index('struct task_struct {')
marker = sched.index('/* UIDFAKE identity: native KABI reservation */', task_start)
depth = 0
for line in sched[task_start:marker].splitlines():
    if re.match(r'\s*#\s*(if|ifdef|ifndef)\b', line):
        depth += 1
    elif re.match(r'\s*#\s*endif\b', line):
        depth -= 1
assert depth == 0, 'UIDFAKE task field is inside a conditional feature branch'
match = re.search(r'/\* UIDFAKE identity: native KABI reservation \*/\n'
                  r'(#if.*?\n#endif)', sched, re.S)
if not match or 'ANDROID_KABI_USE' not in match.group(1):
    print('UIDFAKE KABI: no Android reservation, normal conditional task field')
    sys.exit(0)
block = match.group(1)
slot = re.search(r'ANDROID_KABI_USE\((\d+),', block).group(1)
with tempfile.TemporaryDirectory(prefix='uidfake-kabi-') as tmp:
    tmp = Path(tmp)
    (tmp / 'linux').mkdir()
    (tmp / 'linux/compiler.h').write_text('')
    (tmp / 'linux/stringify.h').write_text(
        '#define __stringify_1(x) #x\n#define __stringify(x) __stringify_1(x)\n')
    prefix = ('#include <stdint.h>\n#include <stddef.h>\n'
              'typedef uint64_t u64; typedef uint32_t u32;\n'
              '#define CONFIG_ANDROID_KABI_RESERVE 1\n'
              '#define CONFIG_HMA_UIDFAKE_BUILTIN 1\n'
              f'#include "{kernel.resolve()}/include/linux/android_kabi.h"\n')
    before = f'struct probe {{ int first; ANDROID_KABI_RESERVE({slot}); long last; }};\n'
    after = 'struct probe { int first;\n' + block + '\nlong last; };\n'
    baseline, integrated = tmp / 'before.c', tmp / 'after.c'
    baseline.write_text(prefix + before)
    integrated.write_text(prefix + after)
    options = ['cc', '-E', '-P', '-D__GENKSYMS__', '-I' + str(tmp)]
    a = subprocess.check_output(options + [str(baseline)], text=True)
    b = subprocess.check_output(options + [str(integrated)], text=True)
    assert re.sub(r'\s+', '', a) == re.sub(r'\s+', '', b), 'genksyms ABI changed'
    layout = prefix + before.replace('probe', 'baseline') + after
    layout += ('_Static_assert(sizeof(struct probe)==sizeof(struct baseline),"size");\n'
               '_Static_assert(offsetof(struct probe,last)==offsetof(struct baseline,last),"offset");\n'
               'int main(void) { return 0; }\n')
    integrated.write_text(layout)
    subprocess.run(['cc', '-std=gnu11', '-Wall', '-Werror', '-I' + str(tmp),
                    str(integrated), '-o', str(tmp / 'layout')], check=True)
    subprocess.run([str(tmp / 'layout')], check=True)
    print(f'UIDFAKE KABI slot {slot}: size, trailing offsets and genksyms representation preserved')
