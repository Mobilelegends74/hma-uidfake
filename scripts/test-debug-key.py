#!/usr/bin/env python3
"""Compile the real main.c against host doubles; no kernel loading/patching."""
import pathlib
import subprocess
import tempfile

entry = pathlib.Path('src/main.c').resolve()
with tempfile.TemporaryDirectory(prefix='uidfake-debug-key-') as tmp:
    root = pathlib.Path(tmp)
    include = root / 'include'
    (include / 'linux').mkdir(parents=True)
    for name in ['kernel.h', 'module.h', 'workqueue.h']:
        (include / 'linux' / name).write_text('#include "test_kernel.h"\n')
    (include / 'uidfake.h').write_text('#include "test_kernel.h"\n')
    (include / 'test_kernel.h').write_text('''
#ifndef TEST_KERNEL_H
#define TEST_KERNEL_H
#include <assert.h>
#include <stdbool.h>
#include <errno.h>
#define __init
#define __exit
#define HZ 100
struct static_key_false { bool enabled; };
extern struct static_key_false uidfake_debug_key;
static int enables, disables, cancels, installs, removes, frees;
#define UF_DEBUG_ON() (uidfake_debug_key.enabled)
#define static_branch_enable(key) ((key)->enabled = true, ++enables)
#define static_branch_disable(key) ((key)->enabled = false, ++disables)
#define static_key_enabled(...) forbidden_static_key_count_dependency()
struct work_struct { int unused; };
struct delayed_work { bool pending; unsigned long delay; };
#define DECLARE_DELAYED_WORK(name, callback) struct delayed_work name
static void schedule_delayed_work(struct delayed_work *work, unsigned long delay) {
  work->pending = true; work->delay = delay;
}
static void cancel_delayed_work_sync(struct delayed_work *work) {
  work->pending = false; ++cancels;
}
#define module_param_named(...)
#define MODULE_PARM_DESC(...)
#define MODULE_LICENSE(...)
#define MODULE_INFO(...)
#define MODULE_AUTHOR(...)
#define MODULE_DESCRIPTION(...)
#define module_init(...)
#define module_exit(...)
static void test_log(const char *fmt, ...) { (void)fmt; }
#define pr_info(...) test_log(__VA_ARGS__)
static int policy_init(void) { return 0; }
static void policy_free(void) { ++frees; }
static void netlink_init(void) {}
static void netlink_exit(void) {}
static int hooks_install(void) { ++installs; return 1; }
static void hooks_remove(void) { ++removes; }
#endif
''')
    runner = root / 'test.c'
    runner.write_text('#include "' + str(entry) + '"\n' + '''
int main(void) {
  assert(!UF_DEBUG_ON());
  uidfake_debug_init(false);
  assert(!UF_DEBUG_ON() && enables == 0 && !uidfake_debug_work.pending);
  uidfake_debug_init(true);
  assert(UF_DEBUG_ON() && enables == 1);
#ifdef UF_DEBUG_ALWAYS
  assert(!uidfake_debug_work.pending);
  assert(uidfake_debug);
#else
  assert(uidfake_debug_work.pending);
  assert(uidfake_debug_work.delay == 60UL * HZ);
  uidfake_debug_work.pending = false; /* the host runs the queued callback */
  uidfake_debug_off(0);
  assert(!UF_DEBUG_ON() && disables == 1);
  uidfake_debug_off(0);
  assert(disables == 1); /* off is idempotent */
#endif
  assert(uidfake_init() == 0);
  assert(installs == 1);
  uidfake_exit();
  assert(cancels == 1 && removes == 1 && frees == 1);
  return 0;
}
''')
    for always in [False, True]:
        exe = root / ('always' if always else 'timed')
        subprocess.run(['cc', '-std=gnu11', '-Werror=implicit-function-declaration',
                        '-I' + str(include), *(['-DUF_DEBUG_ALWAYS'] if always else []),
                        str(runner), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
print('OK: real main.c debug off/on, 60-second timer, idempotent shutdown, always-on build and cleanup; kernel APIs mocked')
