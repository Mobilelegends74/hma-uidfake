/* SPDX-License-Identifier: GPL-2.0 */
#ifndef HMA_UIDFAKE_INTERNAL_H
#define HMA_UIDFAKE_INTERNAL_H
#include <linux/types.h>
#include "kaux.h"
#define POLICY_MAX_PAIRS 65536u
#define UF_APK_MAX 10000u
#define POLICY_WAY 8
#define POLICY_PROBE_MAX 4
#define POLICY_MIN_LINES 16
#define POLICY_MAX_LINES 32768
#define POLICY_MAX_CALLERS 4096
#define POLICY_REPL_BITS 12
#define POLICY_REPL_BASE 20001u
#define POLICY_REPL_MAX (1u << POLICY_REPL_BITS)
#define POLICY_ID_NONE 0xffffffffu
#define POLICY_APP_ID_MIN 10000u
#define POLICY_APP_ID_SPAN 10000u
#define POLICY_WILD_FLAG (1u << 15)
#define UF_APP_SPAN POLICY_APP_ID_SPAN
struct uid_pair { u32 target; u16 repl_k, mask_id; };
u32 policy_lookup_as(uid_t caller, uid_t target);
u32 policy_live_pairs(void);
u32 uidfake_policy_translate(u32 original, u32 target, u32 app);
int policy_init(void);
void policy_free(void);
int policy_apply(const u32 *pairs, u32 npairs);
int uidfake_apk_apply(const u32 *blob, u32 len);
void uidfake_status_get(struct kaux_status *out);
int netlink_init(void);
#endif
