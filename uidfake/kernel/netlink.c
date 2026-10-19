// SPDX-License-Identifier: GPL-2.0
/*
 * netlink.c - the channel from privileged userspace into the kernel: the policy,
 * the caller apk table and a status reply. The wire format is include/kaux.h,
 * which the tool includes as well, so the two ends cannot drift apart.
 */

#include "uidfake.h"
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/version.h> /* LINUX_VERSION_CODE for the resv_start_op guard */
#include <linux/crc32.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/cred.h>
#include <net/net_namespace.h>
#include <net/genetlink.h>

#include "kaux.h"

#define MAX_BLOB_BYTES 32768

/* A policy larger than one message arrives in pages and is held here until its
 * last page and its CRC have been seen: the live policy is replaced in one step,
 * or not at all. */
static DEFINE_MUTEX(g_staged_lock);
/* The apply runs on the one buffer both ends share, so a second commit cannot
 * start until the first has finished reading it: two uploads at once (two
 * helpers, or a retry that overlapped) used to copy into that buffer while it
 * was being parsed. The staging lock is not the one to hold for this -- it must
 * be dropped before the apply, or an install hangs on the next page. */
static DEFINE_MUTEX(g_apply_lock);
static u32 *g_staged;
/* The blob being applied, while the staging buffer is free for the next upload. */
static u32 *g_apply;
static u32 g_staged_kind;
static u32 g_staged_total; /* bytes */
static u32 g_staged_len; /* bytes written so far */
static u32 g_staged_crc;
static u32 g_staged_owner;
static bool g_staged_active;

/* How big a staged blob may be (KAUX_KIND_* and its layout are in kaux.h). */
/* What the two ends actually send: a policy is at most POLICY_MAX_PAIRS pairs of two
 * u32 (512 KiB) and an apk set at most 10000 entries of 16 bytes plus their paths
 * (~800 KiB), so 2 MiB is twice what fits and 4 MiB was four times it -- a wasted six
 * megabytes of kernel memory on a phone, allocated on the first upload and held until
 * the module goes away. */
#define KAUX_STAGED_BYTES (2u * 1024u * 1024u)

/* The helper gets the same value from zlib: crc32_le(~0, ..) ^ ~0 is zlib's
 * crc32(0, ..). Both are byte-wise, so the endianness of either side is not part
 * of the agreement. */
static u32 kaux_crc32(const u8 *data, size_t bytes)
{
	return crc32_le(~0u, data, bytes) ^ ~0u;
}

/*
 * The command ids moved when the staged upload was added, and a helper of the
 * other version would read a ping as a set: the family version is checked, not
 * assumed.
 */
/* Defined with its ops below; the status reply names it. */
static struct genl_family kaux_family;

static int kaux_version(struct genl_info *info)
{
	if (!uid_eq(current_euid(), GLOBAL_ROOT_UID) ||
	    !net_eq(genl_info_net(info), &init_net))
		return -EPERM;
	return info->genlhdr->version == KAUX_FAMILY_VERSION ? 0 :
							       -EPROTONOSUPPORT;
}

static int kaux_blob(struct genl_info *info, const u32 **p, u32 *len)
{
	if (kaux_version(info))
		return -EPROTONOSUPPORT;
	if (!info->attrs[KAUX_ATTR_BLOB])
		return -EINVAL;
	*p = nla_data(info->attrs[KAUX_ATTR_BLOB]);
	*len = nla_len(info->attrs[KAUX_ATTR_BLOB]);
	if (*len < 4 || (*len & 3) || *len > MAX_BLOB_BYTES)
		return -EINVAL;
	return 0;
}

/* blob: u32 total_pairs, u32 total_words, u32 crc32 */
static int kaux_set_begin(struct sk_buff *skb, struct genl_info *info)
{
	const struct kaux_begin *b;
	const u32 *p;
	u32 len;
	int rc = 0;

	mutex_lock(&g_staged_lock);

	/*
	 * The staging buffer is allocated on the first upload, not at load: it is
	 * megabytes now, and the module is loaded while /data is barely there.
	 */
	if (g_staged == NULL)
		g_staged = kvzalloc(KAUX_STAGED_BYTES, GFP_KERNEL);
	if (g_staged == NULL) {
		rc = -ENOMEM;
		goto out;
	}

	if (kaux_blob(info, &p, &len) || len < sizeof(*b)) {
		rc = -EINVAL;
		goto out;
	}
	b = (const struct kaux_begin *)p;
	if (b->kind > KAUX_KIND_MAX || b->bytes > KAUX_STAGED_BYTES ||
	    (b->kind == KAUX_KIND_POLICY &&
	     (b->bytes % 8u || b->bytes / 8u > POLICY_MAX_PAIRS)) ||
	    (b->kind == KAUX_KIND_APKS && b->bytes < 4)) {
		rc = -EINVAL;
		goto out;
	}

	g_staged_kind = b->kind;
	g_staged_total = b->bytes;
	g_staged_len = 0;
	g_staged_crc = b->crc32;
	g_staged_owner = info->snd_portid;
	g_staged_active = true;

out:
	mutex_unlock(&g_staged_lock);
	return rc;
}

/* blob: u32 seq, u32 npairs, then npairs * (caller, target) */
static int kaux_set_page(struct sk_buff *skb, struct genl_info *info)
{
	const u32 *p;
	u32 len, offset, bytes;
	int rc = 0;

	mutex_lock(&g_staged_lock);

	if (kaux_blob(info, &p, &len) || len < 8) {
		rc = -EINVAL;
		goto out;
	}
	offset = p[0];
	bytes = p[1];
	if (len < 8 || bytes > len - 8) {
		rc = -EINVAL;
		goto out;
	}
	if (!g_staged_active || g_staged_owner != info->snd_portid ||
	    offset != g_staged_len ||
	    bytes > g_staged_total - g_staged_len) {
		rc = -EINVAL;
		goto out;
	}

	memcpy((u8 *)g_staged + offset, p + 2, bytes);
	g_staged_len += bytes;

out:
	mutex_unlock(&g_staged_lock);
	return rc;
}

static int kaux_set_commit(struct sk_buff *skb, struct genl_info *info)
{
	u32 kind = 0, bytes = 0;
	int rc = 0;

	if (kaux_version(info))
		return -EPROTONOSUPPORT;

	mutex_lock(&g_apply_lock);
	mutex_lock(&g_staged_lock);
	if (!g_staged_active || g_staged_owner != info->snd_portid ||
	    g_staged_len != g_staged_total) {
		rc = -EINVAL;
		goto out;
	}
	if (kaux_crc32((const u8 *)g_staged, g_staged_len) != g_staged_crc) {
		pr_err("uidfake: staged blob crc mismatch, nothing applied\n");
		rc = -EINVAL;
		goto out;
	}
	/*
	 * The blob is moved out of the staging buffer and applied with the lock
	 * dropped. Applying it resolves paths and touches inodes, and that can
	 * sleep: an apply inside this lock blocks the next page of the next upload,
	 * which is what turned an install into a hang.
	 */
	if (g_apply == NULL)
		g_apply = kvzalloc(KAUX_STAGED_BYTES, GFP_KERNEL);
	if (g_apply == NULL) {
		rc = -ENOMEM;
		goto out;
	}
	kind = g_staged_kind;
	bytes = g_staged_total;
	memcpy(g_apply, g_staged, bytes);
	g_staged_total = 0;
	g_staged_len = 0;
	g_staged_active = false;

out:
	mutex_unlock(&g_staged_lock);
	if (rc == 0) {
		if (kind == KAUX_KIND_POLICY) {
			pr_info("uidfake: netlink policy: %u pair(s) in pages\n",
				bytes / 8u);
			if (bytes % 8u || bytes / 8u > POLICY_MAX_PAIRS)
				rc = -EINVAL;
			else
				rc = policy_apply(g_apply, bytes / 8u);
		} else {
			pr_info("uidfake: netlink apks: %u byte(s) in pages\n",
				bytes);
			rc = uidfake_apk_apply(g_apply, bytes);
		}
	}
	mutex_unlock(&g_apply_lock);
	return rc;
}

static int kaux_ping(struct sk_buff *skb, struct genl_info *info)
{
	if (kaux_version(info))
		return -EPROTONOSUPPORT;

	/* A command id that lands here instead of where it belongs would otherwise
   * look like a success, because a ping is answered with an ACK like anything
   * else. */
	pr_info("uidfake: netlink ping\n");
	return 0;
}

/*
 * How the module is doing, as one fixed structure (kaux.h): what it hooked, and
 * what it failed to. This is the only command that answers with data, and the
 * only one a user ends up seeing -- the tool puts it in the module's description
 * line, which is where KernelSU and Magisk show a module's state.
 */
static int kaux_status(struct sk_buff *skb, struct genl_info *info)
{
	struct kaux_status st;
	struct sk_buff *out;
	void *hdr;

	if (kaux_version(info))
		return -EPROTONOSUPPORT;

	uidfake_status_get(&st);

	out = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	if (!out)
		return -ENOMEM;
	hdr = genlmsg_put_reply(out, info, &kaux_family, 0, KAUX_CMD_STATUS);
	if (hdr == NULL || nla_put(out, KAUX_ATTR_STATUS, sizeof(st), &st)) {
		nlmsg_free(out);
		return -EMSGSIZE;
	}
	genlmsg_end(out, hdr);
	return genlmsg_reply(out, info);
}

static const struct genl_ops kaux_ops[] = {
	{ .cmd = KAUX_CMD_PING, .flags = GENL_ADMIN_PERM, .doit = kaux_ping },
	{ .cmd = KAUX_CMD_STATUS,
	  .flags = GENL_ADMIN_PERM,
	  .doit = kaux_status },
	{ .cmd = KAUX_CMD_STAGE_BEGIN,
	  .flags = GENL_ADMIN_PERM,
	  .doit = kaux_set_begin },
	{ .cmd = KAUX_CMD_STAGE_CHUNK,
	  .flags = GENL_ADMIN_PERM,
	  .doit = kaux_set_page },
	{ .cmd = KAUX_CMD_STAGE_COMMIT,
	  .flags = GENL_ADMIN_PERM,
	  .doit = kaux_set_commit },
};

static const struct genl_multicast_group kaux_mcgrps[] = {
	{ .name = "events" }
};

static struct genl_family kaux_family = {
	.name = KAUX_FAMILY_NAME,
	.version = KAUX_FAMILY_VERSION,
	.maxattr = KAUX_ATTR_MAX,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	.resv_start_op = KAUX_CMD_MAX + 1,
#endif
	.module = THIS_MODULE,
	.ops = kaux_ops,
	.n_ops = ARRAY_SIZE(kaux_ops),
	.mcgrps = kaux_mcgrps,
	.n_mcgrps = ARRAY_SIZE(kaux_mcgrps),
};

int netlink_init(void)
{
	int rc;

	rc = genl_register_family(&kaux_family);

	pr_info("uidfake: netlink family '%s' register rc=%d\n",
		KAUX_FAMILY_NAME, rc);
	return rc;
}

void netlink_exit(void)
{
	genl_unregister_family(&kaux_family);
	kvfree(g_staged);
	kvfree(g_apply);
	g_staged = NULL;
	g_apply = NULL;
}
