#include "netlink-shim.h"
#include "policy.c"
#include "core.c"
#include "netlink.c"

int main(void)
{
    struct task_struct task;
    struct genlhdr hdr = { KAUX_FAMILY_VERSION };
    struct nlattr blob = { NULL, 0 }, *attrs[KAUX_ATTR_MAX + 1] = { NULL, &blob };
    struct genl_info info = { &hdr, attrs, 42, &init_net };
    u32 pairs[] = { 10555, 10666 };
    struct kaux_begin begin = { KAUX_KIND_POLICY, sizeof(pairs), 0 };
    u32 page[] = { 0, sizeof(pairs), 10555, 10666 };
    task.hma_uidfake_app=556; task.group_leader=&task; task.next=NULL;
    current=&task; fsuid=10555;
    assert(!hma_uidfake_init());
    assert(kaux_crc32((const u8 *)"123456789",9)==0xcbf43926u);
    begin.crc32 = kaux_crc32((const u8 *)pairs, sizeof(pairs));
    blob.data=&begin; blob.len=sizeof(begin);
    euid=1000;
    assert(kaux_version(&info)==-EPERM);
    euid=0; info.net=&foreign_net;
    assert(kaux_version(&info)==-EPERM);
    info.net=&init_net;
    hdr.version++;
    assert(kaux_version(&info)==-EPROTONOSUPPORT);
    hdr.version--;
    assert(!kaux_set_begin(NULL,&info));
    blob.data=page; blob.len=sizeof(page);
    info.snd_portid=99;
    assert(kaux_set_page(NULL,&info)==-EINVAL);
    info.snd_portid=42;
    assert(!kaux_set_page(NULL,&info));
    assert(!hma_uidfake_hidden(10666)); /* uncommitted upload invisible */
    info.snd_portid=99;
    assert(kaux_set_commit(NULL,&info)==-EINVAL);
    info.snd_portid=42;
    assert(!kaux_set_commit(NULL,&info));
    assert(hma_uidfake_hidden(10666));
    assert(kaux_set_commit(NULL,&info)==-EINVAL); /* no replay */
    begin.crc32++;
    blob.data=&begin; blob.len=sizeof(begin);
    assert(!kaux_set_begin(NULL,&info));
    blob.data=page; blob.len=sizeof(page);
    assert(!kaux_set_page(NULL,&info));
    assert(kaux_set_commit(NULL,&info)==-EINVAL);
    assert(hma_uidfake_hidden(10666));
    /* The shipped helper sends bytes=0 to revoke every pair. */
    begin.bytes=0; begin.crc32=0;
    blob.data=&begin; blob.len=sizeof(begin);
    assert(!kaux_set_begin(NULL,&info));
    assert(!kaux_set_commit(NULL,&info));
    assert(!hma_uidfake_hidden(10666));
    begin.bytes=7;
    assert(kaux_set_begin(NULL,&info)==-EINVAL);
    begin.bytes=KAUX_STAGED_BYTES+1;
    assert(kaux_set_begin(NULL,&info)==-EINVAL);
    assert(!kaux_ping(NULL,&info));
    assert(!kaux_status(NULL,&info));
    netlink_exit();
    policy_free();
    assert(!rcu_depth && !dentry_refs);
    puts("UIDFAKE netlink: CRC, ownership, authorization, empty revocation and replay tests passed");
    return 0;
}
