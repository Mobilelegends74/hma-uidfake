#!/usr/bin/env python3
"""Exercise the actual helper initializer against host doubles, no module load."""
import pathlib
import re
import subprocess
import tempfile

source = pathlib.Path('src/policy.c').read_text()
start = source.index('static int (*g_kern_path)')
end = source.index('static bool uidfake_tag_pending_here', start)
helper = source[start:end]
assert not re.search(r'(?<![\w])path_put\s*\(', source)
assert source.count('g_path_put(&p);') == 3
assert source.count('uidfake_lookup("path_put")') == 1
init = source[source.index('int policy_init(void)'):]
assert init.index('apk_paths_init()') < init.index('policy_reset()')
with tempfile.TemporaryDirectory(prefix='uidfake-path-helpers-') as tmp:
    root = pathlib.Path(tmp)
    runner = root / 'test.c'
    runner.write_text('''
#include <assert.h>
#include <errno.h>
#include <string.h>
struct path { int held; };
static int missing, acquired, released;
static int test_kern_path(const char *name, unsigned int flags, struct path *p) {
  (void)name; (void)flags; p->held = 1; ++acquired; return 0;
}
static void test_path_put(const struct path *p) { assert(p->held); ++released; }
static unsigned long uidfake_lookup(const char *name) {
  if (!strcmp(name, "kern_path")) return missing == 1 ? 0 : (unsigned long)test_kern_path;
  assert(!strcmp(name, "path_put"));
  return missing == 2 ? 0 : (unsigned long)test_path_put;
}
#define pr_err(...) ((void)0)
''' + helper + '''
int main(void) {
  for (missing = 1; missing <= 2; missing++) {
    assert(apk_paths_init() == -ENOENT);
    assert(acquired == 0 && released == 0);
  }
  missing = 0;
  assert(apk_paths_init() == 0);
  for (int i = 0; i < 3; i++) {
    struct path p;
    assert(g_kern_path("apk", 0, &p) == 0);
    g_path_put(&p);
  }
  assert(acquired == 3 && released == acquired);
  return 0;
}
''')
    exe = root / 'test'
    subprocess.run(['cc', '-std=gnu11', '-Wall', '-Werror', str(runner), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('OK: path helper resolution, missing-helper failure, paired reference release and no direct path_put calls')
