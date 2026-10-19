#include "host-shim.h"
#include "policy.c"
#include "core.c"
int netlink_init(void) { return 0; }

static void reset_task(struct task_struct *task, u32 uid)
{
    memset(task, 0, sizeof(*task));
    task->group_leader = task;
    current = task;
    fsuid = uid;
}

int main(void)
{
    struct task_struct task, child, sibling;
    struct cred root = { 0 }, app = { 10555 }, isolated = { 99001 };
    struct kaux_status status;
    u32 rules[] = { 10555, 10666, 10555, 10667, 10777, 10666,
                    0, 10888, 10555, 0, 10555, 1000, 10555, 10555 };
    u32 invalid_apk[] = { 1, 0, 10555, UINT32_MAX, UINT32_MAX };
    u32 blob[32] = { 1, 0, 10555, 20, 15 };
    struct super_block sb = { 3 };
    struct inode dir_inode = { 1234, &sb }, apk_inode = { 5678, &sb };
    struct dentry dir = { &dir_inode, NULL }, apk = { &apk_inode, &dir };
    struct file file = { { &apk } };
    dir.parent = &dir;
    known_path.dentry = &dir;
    memcpy((char *)blob + 20, "/data/app/test", 15);
    assert(!hma_uidfake_init());
    reset_task(&task, 10555);
    assert(!hma_uidfake_hidden(10666));
    assert(!policy_apply(rules, sizeof(rules) / 8));
    assert(hma_uidfake_hidden(10666));
    assert(hma_uidfake_hidden(10667));
    assert(hma_uidfake_hidden(10888));
    assert(!hma_uidfake_hidden(0));
    assert(!hma_uidfake_hidden(1000));
    assert(!hma_uidfake_hidden(10555));
    assert(!hma_uidfake_hidden(10668));
    fsuid = 10700;
    assert(!hma_uidfake_hidden(10666));
    assert(hma_uidfake_hidden(10888));
    fsuid = 1000;
    assert(!hma_uidfake_hidden(10888));
    /* Caller matching follows the upstream app-id contract across users. */
    fsuid = 110555;
    assert(hma_uidfake_hidden(10666));
    fail_alloc = 1;
    assert(policy_apply(rules, 1) == -ENOMEM);
    fail_alloc = 0;
    assert(hma_uidfake_hidden(10666));
    assert(policy_apply(NULL, POLICY_MAX_PAIRS + 1) == -E2BIG);
    /* Successful credentials, inherited identity and a later UID change. */
    reset_task(&task, 0);
    hma_uidfake_creds(&root, &app);
    assert(task.hma_uidfake_app == 556);
    hma_uidfake_creds(&app, &isolated);
    assert(task.hma_uidfake_app == 556);
    child = task; child.group_leader = &child; current = &child;
    fsuid = 99001;
    assert(hma_uidfake_hidden(10666));
    assert(!uidfake_apk_apply(blob, 36));
    uidfake_status_get(&status);
    assert(status.apk_inodes == 1 && !status.apk_failed);
    assert(uidfake_apk_apply(invalid_apk, sizeof(invalid_apk)) == -EINVAL);
    /* A fresh isolated child is attributed from its app's code directory. */
    reset_task(&task, 99001);
    hma_uidfake_creds(&root, &isolated);
    sibling = task; sibling.group_leader = &task; sibling.next = NULL;
    task.next = &sibling;
    hma_uidfake_open(&file);
    assert(task.hma_uidfake_app == 556 && sibling.hma_uidfake_app == 556);
    assert(!dentry_refs && hma_uidfake_hidden(10666));
    /* Query before code load closes the attribution window permanently. */
    reset_task(&task, 99001);
    hma_uidfake_creds(&root, &isolated);
    assert(!hma_uidfake_hidden(10666));
    hma_uidfake_open(&file);
    assert(!task.hma_uidfake_app);
    /* Empty uploads revoke both tables, and failed updates keep old state. */
    assert(!policy_apply(NULL, 0));
    reset_task(&task, 10555);
    assert(!hma_uidfake_hidden(10666));
    {
        u32 drop[] = { 1, 1, 3, 1234, 0 };
        assert(!uidfake_apk_apply(drop, sizeof(drop)));
    }
    uidfake_status_get(&status);
    assert(!status.apk_inodes && !status.policy_pairs);
    assert(!rcu_depth && !dentry_refs);
    policy_free();
    kvfree(rcu_dereference(apks));
    puts("UIDFAKE native core: policy, isolation, revocation and malformed-input tests passed");
    return 0;
}
