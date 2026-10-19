/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_HMA_UIDFAKE_H
#define _LINUX_HMA_UIDFAKE_H
#include <linux/types.h>
struct cred;
struct file;
#ifdef CONFIG_HMA_UIDFAKE_BUILTIN
bool hma_uidfake_hidden(unsigned int uid);
unsigned int hma_uidfake_translate(unsigned int uid);
unsigned int hma_uidfake_bucket(unsigned int uid);
unsigned int hma_uidfake_hash_bits(void);
void hma_uidfake_creds(const struct cred *old, const struct cred *new);
void hma_uidfake_open(struct file *file);
#else
static inline bool hma_uidfake_hidden(unsigned int uid) { return false; }
static inline unsigned int hma_uidfake_translate(unsigned int uid) { return uid; }
static inline void hma_uidfake_creds(const struct cred *old,
				   const struct cred *new) { }
static inline void hma_uidfake_open(struct file *file) { }
#endif
#endif
