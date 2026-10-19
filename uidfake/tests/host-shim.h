/* Host-only shims; core.c itself is compiled unchanged by the test runner. */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint64_t u64;
typedef uint8_t u8;
typedef u32 kuid_t;
static int namespace_token;
#define init_user_ns namespace_token
#define current_user_ns() (&namespace_token)
#define __rcu
#define __init
#define late_initcall(fn)
#define GFP_KERNEL 0
#define CONFIG_COMPAT 1
#define IS_ENABLED(x) (x)
#define READ_ONCE(x) (x)
#define DEFINE_MUTEX(x) int x
#define lockdep_is_held(x) (*(x))
#define cmpxchg(p, a, b) __sync_val_compare_and_swap(p, a, b)
#define rcu_dereference(p) (p)
#define rcu_dereference_protected(p, c) (assert(c), (p))
#define rcu_assign_pointer(p, v) ((p) = (v))
#define pr_info(...) ((void)0)
#define pr_err(...) ((void)0)
#define __kuid_val(x) (x)
#define LOOKUP_FOLLOW 0
static int rcu_depth, fail_alloc;
static void rcu_read_lock(void) { rcu_depth++; }
static void rcu_read_unlock(void) { assert(rcu_depth > 0); rcu_depth--; }
static void synchronize_rcu(void) { assert(!rcu_depth); }
static void mutex_lock(int *p) { assert(!*p); *p = 1; }
static void mutex_unlock(int *p) { assert(*p); *p = 0; }
static void *kvzalloc(size_t n, int flag) {
    (void)flag;
    return fail_alloc ? NULL : calloc(1, n);
}
#define kvfree free
#define kfree free
#define kcalloc(n, s, f) kvzalloc((n) * (s), (f))
#define kvcalloc(n, s, f) kvzalloc((n) * (s), (f))
#define kvmalloc_array(n, s, f) kvzalloc((n) * (s), (f))
#define kzalloc(n, f) kvzalloc(n, f)
#define xchg(p,v) __extension__ ({ __typeof__(*(p)) old_ = *(p); *(p)=(v); old_; })
#define unlikely(x) (x)
#define likely(x) (x)
#define pr_warn(...) ((void)0)
static inline u32 ilog2(u32 x) { return 31u - __builtin_clz(x); }
struct user_struct { int unused; };
#define KUIDT_INIT(x) (x)
static struct user_struct *find_user(u32 uid) {
    static struct user_struct live;
    return uid >= 10000 && uid < 20000 ? &live : NULL;
}
static void free_uid(struct user_struct *user) { (void)user; }
static u32 host_hash_bits = 7;
static bool host_hash_multiply;
static unsigned int hma_uidfake_hash_bits(void) { return host_hash_bits; }
static unsigned int hma_uidfake_bucket(unsigned int uid) {
    if (host_hash_multiply)
        return (u32)(uid * 0x61c88647u) >> (32 - host_hash_bits);
    return ((uid >> host_hash_bits) + uid) & ((1u << host_hash_bits) - 1);
}
#define sort(p, n, s, cmp, swap) qsort(p, n, s, cmp)
static size_t strscpy(char *dst, const char *src, size_t n) {
    size_t len = strlen(src);
    assert(len < n); memcpy(dst, src, len + 1); return len;
}
struct cred { u32 fsuid; };
struct task_struct {
    u32 hma_uidfake_app;
    struct task_struct *group_leader, *next;
};
static struct task_struct *current;
static u32 fsuid;
#define current_fsuid() (fsuid)
#define for_each_thread(leader, t) for ((t) = (leader)->next; (t); (t) = (t)->next)
struct super_block { dev_t s_dev; };
struct inode { u64 i_ino; struct super_block *i_sb; };
struct dentry { struct inode *inode; struct dentry *parent; };
struct path { struct dentry *dentry; };
struct file { struct path f_path; };
static struct path known_path;
static int dentry_refs;
static struct inode *d_inode(struct dentry *d) { return d->inode; }
static struct dentry *dget(struct dentry *d) { dentry_refs++; return d; }
static void dput(struct dentry *d) { (void)d; dentry_refs--; assert(dentry_refs >= 0); }
static struct dentry *dget_parent(struct dentry *d) { return dget(d->parent); }
#define IS_ROOT(d) ((d)->parent == (d))
static u32 new_encode_dev(dev_t d) { return (u32)d; }
static int kern_path(const char *p, int flags, struct path *out) {
    (void)flags;
    if (strcmp(p, "/data/app/test")) return -ENOENT;
    *out = known_path; return 0;
}
static void path_put(struct path *p) { (void)p; }
