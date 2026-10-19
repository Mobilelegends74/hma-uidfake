#include "host-shim.h"
#include "policy.c"
#include "core.c"
int netlink_init(void) { return 0; }

static u32 rng = 0x12345678;
static u32 random_word(void)
{
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return rng;
}

int main(void)
{
    enum { N = 3000 };
    u32 pairs[N * 2];
    struct task_struct task = { 0, &task, NULL };
    u32 round, query, j, checked = 0;
    current = &task;
    assert(!hma_uidfake_init());
    for (round = 0; round < 12; round++) {
        host_hash_bits = round % 3 == 0 ? 3 : round % 3 == 1 ? 7 : 8;
        host_hash_multiply = round % 2;
        for (j = 0; j < N; j++) {
            pairs[j*2] = random_word() % 9 ? 10000 + random_word() % 16 : 0;
            pairs[j*2+1] = 10000 + random_word() % 512;
        }
        assert(!policy_apply(pairs, N));
        for (query = 0; query < 2000; query++) {
            u32 target = 10000 + random_word() % 1024;
            bool expected = false;
            fsuid = 10000 + random_word() % 32;
            for (j = 0; j < N; j++)
                expected |= pairs[j*2+1] == target &&
                            (pairs[j*2] == 0 || pairs[j*2] == fsuid);
            expected &= target != fsuid;
            assert(hma_uidfake_hidden(target) == expected);
            checked++;
        }
        assert(!policy_apply(NULL, 0));
        assert(!hma_uidfake_hidden(10099));
    }
    /* Every target has a distinct >64-caller mask: catches pool sizing errors. */
    for (j = 0; j < 650; j++) {
        pairs[j*2] = 10000+j;
        pairs[j*2+1] = 12000+j;
    }
    assert(!policy_apply(pairs, 650));
    for (j = 0; j < 650; j++) {
        fsuid=10000+j;
        assert(hma_uidfake_hidden(12000+j));
        assert(!hma_uidfake_hidden(12000+(j+1)%650));
    }
    /* Exercise the full supported allocation and sorting boundary. */
    {
        u32 *max = calloc(POLICY_MAX_PAIRS * 2, sizeof(u32));
        assert(max);
        for (j = 0; j < POLICY_MAX_PAIRS; j++) {
            max[j*2] = 10555;
            max[j*2+1] = 20000 + j;
        }
        assert(!policy_apply(max, POLICY_MAX_PAIRS));
        fsuid = 10555;
        assert(hma_uidfake_hidden(20000));
        assert(hma_uidfake_hidden(20000 + POLICY_MAX_PAIRS - 1));
        assert(!hma_uidfake_hidden(20000 + POLICY_MAX_PAIRS));
        free(max);
    }
    policy_free();
    assert(!rcu_depth && !dentry_refs);
    printf("UIDFAKE policy: %u differential queries and 65536-pair capacity passed\n", checked);
    return 0;
}
