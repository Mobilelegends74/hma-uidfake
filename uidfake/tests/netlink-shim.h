/* Generic-netlink test adapter: validates real upload handler state machines. */
#include "host-shim.h"
#define LINUX_VERSION_CODE 0x060100
#define KERNEL_VERSION(a,b,c) (((a)<<16)|((b)<<8)|(c))
#define GENL_ADMIN_PERM 1
#define THIS_MODULE NULL
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define NLMSG_GOODSIZE 4096
#define kfree free
static u32 euid;
static int init_net, foreign_net;
#define current_euid() (euid)
#define GLOBAL_ROOT_UID 0
#define uid_eq(a,b) ((a)==(b))
#define net_eq(a,b) ((a)==(b))
struct nlattr { void *data; u32 len; };
struct genlhdr { u32 version; };
struct genl_info { struct genlhdr *genlhdr; struct nlattr **attrs; u32 snd_portid; int *net; };
#define genl_info_net(i) ((i)->net)
#define nla_data(a) ((a)->data)
#define nla_len(a) ((a)->len)
struct sk_buff { int unused; };
struct genl_ops { int cmd, flags; int (*doit)(struct sk_buff *, struct genl_info *); };
struct genl_multicast_group { const char *name; };
struct genl_family {
    const char *name; u32 version, maxattr, resv_start_op; void *module;
    const struct genl_ops *ops; size_t n_ops;
    const struct genl_multicast_group *mcgrps; size_t n_mcgrps;
};
static struct sk_buff *genlmsg_new(size_t n, int flag) { (void)n; (void)flag; return calloc(1,sizeof(struct sk_buff)); }
static void *genlmsg_put_reply(struct sk_buff *s, struct genl_info *i, struct genl_family *f, int x, int cmd) {
    (void)i; (void)f; (void)x; (void)cmd; return s;
}
static int nla_put(struct sk_buff *s, int a, size_t n, const void *d) { (void)s; (void)a; (void)n; (void)d; return 0; }
#define nlmsg_free free
static void genlmsg_end(struct sk_buff *s, void *h) { (void)s; (void)h; }
static int genlmsg_reply(struct sk_buff *s, struct genl_info *i) { (void)i; free(s); return 0; }
static int genl_register_family(struct genl_family *f) { (void)f; return 0; }
static void genl_unregister_family(struct genl_family *f) { (void)f; }
static u32 crc32_le(u32 crc, const u8 *s, size_t n) {
    size_t i; unsigned int b;
    for (i=0;i<n;i++) { crc ^= s[i]; for(b=0;b<8;b++) crc=(crc>>1) ^ ((0u-(crc&1u))&0xedb88320u); }
    return crc;
}
