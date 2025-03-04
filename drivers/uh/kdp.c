#include <asm-generic/sections.h>
#include <linux/mm.h>
#include "../../mm/slab.h"
#include <linux/slub_def.h>
#include <linux/binfmts.h>

#include <linux/kdp.h>
#include <linux/mount.h>
#include <linux/cred.h>
#include <linux/security.h>
#include <linux/init_task.h>
#include "../../fs/mount.h"

#define VERITY_PARAM_LENGTH 20
#define KDP_CRED_SYS_ID 1000

/* security/selinux/include/objsec.h */
struct task_security_struct {
	u32 osid;               /* SID prior to last execve */
	u32 sid;                /* current SID */
	u32 exec_sid;           /* exec SID */
	u32 create_sid;         /* fscreate SID */
	u32 keycreate_sid;      /* keycreate SID */
	u32 sockcreate_sid;     /* fscreate SID */
	void *bp_cred;
};
/* security/selinux/hooks.c */
struct task_security_struct init_sec __kdp_ro;

bool kdp_enable __kdp_ro = false;
static int __check_verifiedboot __kdp_ro = 0;

static char verifiedbootstate[VERITY_PARAM_LENGTH];

void __init kdp_init(void)
{
	struct kdp_init cred;

	memset((void *)&cred, 0, sizeof(kdp_init));
	cred._srodata		= (u64)__start_rodata;
	cred._erodata		= (u64)__end_rodata;
	cred.credSize		= sizeof(struct cred_kdp);
	cred.sp_size		= sizeof(struct task_security_struct);
	cred.pgd_mm			= offsetof(struct mm_struct, pgd);
	cred.uid_cred		= offsetof(struct cred, uid);
	cred.euid_cred		= offsetof(struct cred, euid);
	cred.gid_cred		= offsetof(struct cred, gid);
	cred.egid_cred		= offsetof(struct cred, egid);

	cred.bp_pgd_cred	= offsetof(struct cred_kdp, bp_pgd);
	cred.bp_task_cred	= offsetof(struct cred_kdp, bp_task);
	cred.type_cred		= offsetof(struct cred_kdp, type);
	cred.security_cred	= offsetof(struct cred, security);
	cred.usage_cred		= offsetof(struct cred_kdp, use_cnt);
	cred.cred_task		= offsetof(struct task_struct, cred);
	cred.mm_task		= offsetof(struct task_struct, mm);

	cred.pid_task		= offsetof(struct task_struct, pid);
	cred.rp_task		= offsetof(struct task_struct, real_parent);
	cred.comm_task		= offsetof(struct task_struct, comm);
	cred.bp_cred_secptr	= offsetof(struct task_security_struct, bp_cred);
	cred.verifiedbootstate	= (u64)verifiedbootstate;

	uh_call(UH_APP_KDP, KDP_INIT, (u64)&cred, 0, 0, 0);
}

static int __init verifiedboot_state_setup(char *str)
{
	strlcpy(verifiedbootstate, str, sizeof(verifiedbootstate));

	if (!strncmp(verifiedbootstate, "orange", sizeof("orange")))
		__check_verifiedboot = 1;
	return 0;
}
__setup("androidboot.verifiedbootstate=", verifiedboot_state_setup);

inline bool is_kdp_kmem_cache(struct kmem_cache *s)
{
	if (s->name &&
		(!strncmp(s->name, CRED_JAR_RO, strlen(CRED_JAR_RO)) ||
		 !strncmp(s->name, TSEC_JAR, strlen(TSEC_JAR)) ||
		 !strncmp(s->name, VFSMNT_JAR, strlen(VFSMNT_JAR))))
		return true;
	else
		return false;
}

#ifdef CONFIG_KDP_CRED
/*------------------------------------------------
 * CRED
 *------------------------------------------------
 */
struct cred_kdp_init {
	atomic_t use_cnt;
	struct ro_rcu_head ro_rcu_head_init;
};

struct cred_kdp_init init_cred_use_cnt = {
	.use_cnt					= ATOMIC_INIT(4),
	.ro_rcu_head_init.non_rcu	= 0,
	.ro_rcu_head_init.bp_cred	= (void *)0,
	.ro_rcu_head_init.reflected_cred	= (void *)0,
};

struct cred_kdp init_cred_kdp __kdp_ro = {
	.use_cnt		= (atomic_t *)&init_cred_use_cnt,
	.bp_task		= &init_task,
	.bp_pgd			= (void *)0,
	.type			= 0,
};

static struct kmem_cache *cred_jar_ro;
static struct kmem_cache *tsec_jar;
static struct kmem_cache *usecnt_jar;

/* Dummy constructor to make sure we have separate slabs caches. */
static void cred_ctor(void *data) {}
static void sec_ctor(void *data) {}
static void usecnt_ctor(void *data) {}

void __init kdp_cred_init(void)
{
	if (kdp_enable) {
		cred_jar_ro = kmem_cache_create("cred_jar_ro", sizeof(struct cred_kdp),
					0, SLAB_HWCACHE_ALIGN|SLAB_PANIC|SLAB_ACCOUNT, cred_ctor);
		if (!cred_jar_ro)
			panic("Unable to create RO Cred cache\n");

		tsec_jar = kmem_cache_create("tsec_jar", sizeof(struct task_security_struct),
					0, SLAB_HWCACHE_ALIGN|SLAB_PANIC|SLAB_ACCOUNT, sec_ctor);
		if (!tsec_jar)
			panic("Unable to create RO security cache\n");

		usecnt_jar = kmem_cache_create("usecnt_jar", sizeof(atomic_t) + sizeof(struct ro_rcu_head),
					0, SLAB_HWCACHE_ALIGN|SLAB_PANIC|SLAB_ACCOUNT, usecnt_ctor);
		if (!usecnt_jar)
			panic("Unable to create use count jar\n");

#ifdef CONFIG_RUSTUH_KDP
		uh_call(UH_APP_KDP, JARRO_TSEC_SIZE, (u64)cred_jar_ro->size, (u64)tsec_jar->size, 0, 0);
#else
		uh_call(UH_APP_KDP, CRED_INIT, (u64)cred_jar_ro->size, (u64)tsec_jar->size, 0, 0);
#endif
	}
}

unsigned int kdp_get_usecount(struct cred *cred)
{
	int ret = is_kdp_protect_addr((unsigned long)cred);

	if (ret == PROTECT_INIT)
		return (unsigned int)atomic_read(init_cred_kdp.use_cnt);
	else if (ret == PROTECT_KMEM)
		return (unsigned int)atomic_read(((struct cred_kdp *)cred)->use_cnt);
	else
		return atomic_read(&cred->usage);
}

inline struct cred *get_new_cred(struct cred *cred)
{
	int ret = is_kdp_protect_addr((unsigned long)cred);

	if (ret == PROTECT_INIT)
		atomic_inc(init_cred_kdp.use_cnt);
	else if (ret == PROTECT_KMEM)
		atomic_inc(((struct cred_kdp *)cred)->use_cnt);
	else
		atomic_inc(&cred->usage);

	return cred;
}

/* copy logic - include/linux/cred.h */
inline void put_cred(const struct cred *_cred)
{
	struct cred *cred = (struct cred *) _cred;
	int ret = is_kdp_protect_addr((unsigned long)cred);

	if (cred) {
		validate_creds(cred);
		if (ret == PROTECT_INIT) {
			if (atomic_dec_and_test(init_cred_kdp.use_cnt))
				__put_cred(cred);
		} else if (ret == PROTECT_KMEM) {
			if (atomic_dec_and_test(((struct cred_kdp *)cred)->use_cnt))
				__put_cred(cred);
		} else {
			if (atomic_dec_and_test(&(cred)->usage))
				__put_cred(cred);
		}
	}
}

/* match for kernel/cred.c function */
inline void set_cred_subscribers(struct cred *cred, int n)
{
#ifdef CONFIG_DEBUG_CREDENTIALS
	atomic_set(&cred->subscribers, n);
#endif
}

/* Check whether the address belong to Cred Area */
int is_kdp_protect_addr(unsigned long addr)
{
	struct kmem_cache *s;
	struct page *page;
	void *objp = (void *)addr;

	if (!objp)
		return 0;

	if (!kdp_enable)
		return 0;

	if ((addr == ((unsigned long)&init_cred)) ||
		(addr == ((unsigned long)&init_sec)))
		return PROTECT_INIT;

	page = virt_to_head_page(objp);
	s = page->slab_cache;
	if (s && (s == cred_jar_ro || s == tsec_jar))
		return PROTECT_KMEM;

	return 0;
}

/* We use another function to free protected creds. */
void put_rocred_rcu(struct rcu_head *rcu)
{
	struct cred *cred = container_of(rcu, struct ro_rcu_head, rcu)->bp_cred;

	if (atomic_read(((struct cred_kdp *)cred)->use_cnt) != 0)
			panic("RO_CRED: put_rocred_rcu() sees %p with usage %d\n",
					cred, atomic_read(((struct cred_kdp *)cred)->use_cnt));

	security_cred_free(cred);
	key_put(cred->session_keyring);
	key_put(cred->process_keyring);
	key_put(cred->thread_keyring);
	key_put(cred->request_key_auth);
	if (cred->group_info)
		put_group_info(cred->group_info);
	free_uid(cred->user);
	put_user_ns(cred->user_ns);
	if (((struct cred_kdp *)cred)->use_cnt)
		kmem_cache_free(usecnt_jar, (void *)((struct cred_kdp *)cred)->use_cnt);
	kmem_cache_free(cred_jar_ro, cred);
}

/* prepare_ro_creds - Prepare a new set of credentials which is protected by KDP */
struct cred *prepare_ro_creds(struct cred *old, int kdp_cmd, u64 p)
{
	u64 pgd = (u64)(current->mm ? current->mm->pgd : swapper_pg_dir);
	struct cred_kdp temp_old __aligned(256);
	struct cred_kdp *new_ro = NULL;
	struct cred_param param_data __aligned(64);
	void *use_cnt_ptr = NULL;
	void *rcu_ptr = NULL;
	void *tsec = NULL;

	new_ro = kmem_cache_alloc(cred_jar_ro, GFP_KERNEL);
	if (!new_ro)
		panic("[%d] : kmem_cache_alloc() failed", kdp_cmd);

	use_cnt_ptr = kmem_cache_alloc(usecnt_jar, GFP_KERNEL);
	if (!use_cnt_ptr)
		panic("[%d] : Unable to allocate usage pointer\n", kdp_cmd);

	// get_usecnt_rcu
	rcu_ptr = (struct ro_rcu_head *)((atomic_t *)use_cnt_ptr + 1);
	((struct ro_rcu_head *)rcu_ptr)->bp_cred = (void *)new_ro;

	tsec = kmem_cache_alloc(tsec_jar, GFP_KERNEL);
	if (!tsec)
		panic("[%d] : Unable to allocate security pointer\n", kdp_cmd);

	// make cred_kdp 'temp_old'
	if ((u64)current->cred == (u64)&init_cred)
		memcpy(&temp_old, &init_cred_kdp, sizeof(struct cred_kdp));
	else
		memcpy(&temp_old, current->cred, sizeof(struct cred_kdp));

	memcpy(&temp_old, old, sizeof(struct cred));

	// init
	memset((void *)&param_data, 0, sizeof(struct cred_param));
	param_data.cred = &temp_old;
	param_data.cred_ro = new_ro;
	param_data.use_cnt_ptr = use_cnt_ptr;
	param_data.sec_ptr = tsec;
	param_data.type = kdp_cmd;
	param_data.use_cnt = (u64)p;

	READ_ONCE(param_data);
	READ_ONCE(temp_old);

	uh_call(UH_APP_KDP, PREPARE_RO_CRED, (u64)&param_data, (u64)current, (u64)&init_cred, (u64)&init_cred_kdp);
	if (kdp_cmd == CMD_COPY_CREDS) {
		if ((new_ro->bp_task != (void *)p) ||
			new_ro->cred.security != tsec ||
			new_ro->use_cnt != use_cnt_ptr) {
			panic("[%d]: KDP Call failed task=0x%lx:0x%lx, sec=0x%lx:0x%lx, usecnt=0x%lx:0x%lx",
					kdp_cmd, new_ro->bp_task, (void *)p,
					new_ro->cred.security, tsec, new_ro->use_cnt, use_cnt_ptr);
		}
	} else {
		if ((new_ro->bp_task != current) ||
			(current->mm && new_ro->bp_pgd != (void *)pgd) ||
			(new_ro->cred.security != tsec) ||
			(new_ro->use_cnt != use_cnt_ptr)) {
			panic("[%d]: KDP Call failed task=0x%lx:0x%lx, sec=0x%lx:0x%lx, usecnt=0x%lx:0x%lx, pgd=0x%lx:0x%lx",
					kdp_cmd, new_ro->bp_task, current, new_ro->cred.security, tsec,
					new_ro->use_cnt, use_cnt_ptr, new_ro->bp_pgd, (void *)pgd);
		}
	}

	GET_ROCRED_RCU(new_ro)->non_rcu = old->non_rcu;
	GET_ROCRED_RCU(new_ro)->reflected_cred = 0;
	atomic_set(new_ro->use_cnt, 2);

	set_cred_subscribers((struct cred *)new_ro, 0);
	get_group_info(new_ro->cred.group_info);
	get_uid(new_ro->cred.user);
	get_user_ns(new_ro->cred.user_ns);

#ifdef CONFIG_KEYS
	key_get(new_ro->cred.session_keyring);
	key_get(new_ro->cred.process_keyring);
	key_get(new_ro->cred.thread_keyring);
	key_get(new_ro->cred.request_key_auth);
#endif

	validate_creds((struct cred *)new_ro);
	return (struct cred *)new_ro;
}

/* security/selinux/hooks.c */
static bool is_kdp_tsec_jar(unsigned long addr)
{
	struct kmem_cache *s;
	struct page *page;
	void *objp = (void *)addr;

	if (!objp)
		return false;

	page = virt_to_head_page(objp);
	s = page->slab_cache;
	if (s && s == tsec_jar)
		return true;
	return false;
}

static inline int chk_invalid_kern_ptr(u64 tsec)
{
	return (((u64)tsec >> 39) != (u64)0x1FFFFFF);
}

void kdp_free_security(unsigned long tsec)
{
	if (!tsec || chk_invalid_kern_ptr(tsec))
		return;

	if (is_kdp_tsec_jar(tsec))
		kmem_cache_free(tsec_jar, (void *)tsec);
	else
		kfree((void *)tsec);
}

void kdp_assign_pgd(struct task_struct *p)
{
	u64 pgd = (u64)(p->mm ? p->mm->pgd : swapper_pg_dir);

	uh_call(UH_APP_KDP, SET_CRED_PGD, (u64)p->cred, (u64)pgd, 0, 0);
}

struct task_security_struct init_sec __kdp_ro;
static inline unsigned int cmp_sec_integrity(const struct cred *cred, struct mm_struct *mm)
{
	if (cred == &init_cred) {
		if (init_cred_kdp.bp_task != current)
			printk(KERN_ERR "[KDP] init_cred_kdp.bp_task: 0x%lx, current: 0x%lx\n",
							init_cred_kdp.bp_task, current);

		if (mm && (init_cred_kdp.bp_pgd != swapper_pg_dir) && (init_cred_kdp.bp_pgd != mm->pgd ))
			printk(KERN_ERR "[KDP] mm: 0x%lx, init_cred_kdp.bp_pgd: 0x%lx, swapper_pg_dir: %p, mm->pgd: 0x%lx\n",
							mm, init_cred_kdp.bp_pgd, swapper_pg_dir, mm->pgd);

		return ((init_cred_kdp.bp_task != current) ||
				(mm && (!(in_interrupt() || in_softirq())) &&
				(init_cred_kdp.bp_pgd != swapper_pg_dir) &&
				(init_cred_kdp.bp_pgd != mm->pgd)));
	} else {
		if (((struct cred_kdp *)cred)->bp_task != current)
			printk(KERN_ERR "[KDP] cred->bp_task: 0x%lx, current: 0x%lx\n",
						((struct cred_kdp *)cred)->bp_task, current);

		if (mm && (((struct cred_kdp *)cred)->bp_pgd != swapper_pg_dir) &&
			(((struct cred_kdp *)cred)->bp_pgd != mm->pgd))
			printk(KERN_ERR "[KDP] mm: 0x%lx, cred->bp_pgd: 0x%lx, swapper_pg_dir: %p, mm->pgd: 0x%lx\n",
							mm, ((struct cred_kdp *)cred)->bp_pgd, swapper_pg_dir, mm->pgd);

		return ((((struct cred_kdp *)cred)->bp_task != current) ||
				(mm && (!(in_interrupt() || in_softirq())) &&
				(((struct cred_kdp *)cred)->bp_pgd != swapper_pg_dir) &&
				(((struct cred_kdp *)cred)->bp_pgd != mm->pgd)));
	}
	// Want to not reaching
	return 1;
}

static inline bool is_kdp_invalid_cred_sp(u64 cred, u64 sec_ptr)
{
	struct task_security_struct *tsec = (struct task_security_struct *)sec_ptr;
	u64 cred_size = sizeof(struct cred_kdp);
	u64 tsec_size = sizeof(struct task_security_struct);

	if (cred == (u64)&init_cred)
		cred_size = sizeof(struct cred);

	if ((cred == (u64)&init_cred) && (sec_ptr == (u64)&init_sec))
		return false;

	if (!is_kdp_protect_addr(cred) ||
		!is_kdp_protect_addr(cred + cred_size) ||
		!is_kdp_protect_addr(sec_ptr) ||
		!is_kdp_protect_addr(sec_ptr + tsec_size)) {
		printk(KERN_ERR, "[KDP] cred: %d, cred + sizeof(cred): %d, sp: %d, sp + sizeof(tsec): %d",
				is_kdp_protect_addr(cred),
				is_kdp_protect_addr(cred + cred_size),
				is_kdp_protect_addr(sec_ptr),
				is_kdp_protect_addr(sec_ptr + tsec_size));
		return true;
	}

	if ((u64)tsec->bp_cred != cred) {
		printk(KERN_ERR, "[KDP] %s: tesc->bp_cred: %lx, cred: %lx\n",
				__func__, (u64)tsec->bp_cred, cred);
		return true;
	}

	return false;
}

inline int kdp_restrict_fork(struct filename *path)
{
	struct cred *shellcred;
	const struct cred_kdp *cred_kdp = (const struct cred_kdp *)(current->cred);

	if (!strcmp(path->name, "/system/bin/patchoat") ||
		!strcmp(path->name, "/system/bin/idmap2")) {
		return 0;
	}

	if ((cred_kdp->type) >> 1 & 1) {
		shellcred = prepare_creds();
		if (!shellcred)
			return 1;

		shellcred->uid.val = 2000;
		shellcred->gid.val = 2000;
		shellcred->euid.val = 2000;
		shellcred->egid.val = 2000;

		commit_creds(shellcred);
	}
	return 0;
}
#endif

/* This function is related to Namespace */
#ifdef CONFIG_KDP_NS
static unsigned int cmp_ns_integrity(void)
{
	struct kdp_mount *root = NULL;
	struct nsproxy *nsp = NULL;

	if (in_interrupt() || in_softirq())
		return 0;

	nsp = current->nsproxy;
	if (!ns_protect || !nsp || !nsp->mnt_ns)
		return 0;

	root = (struct kdp_mount *)current->nsproxy->mnt_ns->root;
	if (root != (struct kdp_mount *)((struct kdp_vfsmount *)root->mnt)->bp_mount) {
		printk(KERN_ERR "[KDP] NameSpace Mismatch %lx != %lx\n nsp: 0x%lx, mnt_ns: 0x%lx\n",
				root, ((struct kdp_vfsmount *)root->mnt)->bp_mount, nsp, nsp->mnt_ns);
		return 1;
	}

	return 0;
}

/*------------------------------------------------
 * Namespace
 *------------------------------------------------
 */
unsigned int ns_protect __kdp_ro = 0;
static int dex2oat_count = 0;
static DEFINE_SPINLOCK(mnt_vfsmnt_lock);

static struct super_block *rootfs_sb __kdp_ro = NULL;
static struct super_block *sys_sb __kdp_ro = NULL;
static struct super_block *odm_sb __kdp_ro = NULL;
static struct super_block *vendor_sb __kdp_ro = NULL;
static struct super_block *art_sb __kdp_ro = NULL;
static struct super_block *crypt_sb	__kdp_ro = NULL;
static struct super_block *dex2oat_sb	__kdp_ro = NULL;
static struct super_block *adbd_sb		__kdp_ro = NULL;
static struct kmem_cache *vfsmnt_cache __read_mostly;

extern int __is_kdp_recovery;

void cred_ctor_vfsmount(void *data)
{
	/* Dummy constructor to make sure we have separate slabs caches. */
}
void __init kdp_mnt_init(void)
{
	struct ns_param nsparam;

	vfsmnt_cache = kmem_cache_create("vfsmnt_cache", sizeof(struct kdp_vfsmount),
				0, SLAB_HWCACHE_ALIGN | SLAB_PANIC, cred_ctor_vfsmount);

	if (!vfsmnt_cache)
		panic("Failed to allocate vfsmnt_cache\n");

	memset((void *)&nsparam, 0, sizeof(struct ns_param));
	nsparam.ns_buff_size = (u64)vfsmnt_cache->size;
	nsparam.ns_size = (u64)sizeof(struct kdp_vfsmount);
	nsparam.bp_offset = (u64)offsetof(struct kdp_vfsmount, bp_mount);
	nsparam.sb_offset = (u64)offsetof(struct kdp_vfsmount, mnt.mnt_sb);
	nsparam.flag_offset = (u64)offsetof(struct kdp_vfsmount, mnt.mnt_flags);
	nsparam.data_offset = (u64)offsetof(struct kdp_vfsmount, mnt.data);

	uh_call(UH_APP_KDP, NS_INIT, (u64)&nsparam, 0, 0, 0);
}

void __init kdp_init_mount_tree(struct vfsmount *mnt)
{
	if (!rootfs_sb)
		uh_call(UH_APP_KDP, SET_NS_SB_VFSMOUNT, (u64)&rootfs_sb, (u64)mnt, KDP_SB_ROOTFS, 0);
}

bool is_kdp_vfsmnt_cache(unsigned long addr)
{
	static void *objp;
	static struct kmem_cache *s;
	static struct page *page;

	objp = (void *)addr;

	if (!objp)
		return false;

	page = virt_to_head_page(objp);
	s = page->slab_cache;
	if (s && s == vfsmnt_cache)
		return true;
	return false;
}

static int kdp_check_sb_mismatch(struct super_block *sb)
{
	if (__is_kdp_recovery || __check_verifiedboot)
		return 0;

	if ((sb != rootfs_sb) && (sb != sys_sb) && (sb != odm_sb)
		&& (sb != vendor_sb) && (sb != art_sb) && (sb != crypt_sb)
		&& (sb != dex2oat_sb) && (sb != adbd_sb))
		return 1;

	return 0;
}

int is_kdp_priv_task(void)
{
	struct cred *cred = (struct cred *)current_cred();

	if (cred->uid.val <= (uid_t)KDP_CRED_SYS_ID ||
		cred->euid.val <= (uid_t)KDP_CRED_SYS_ID ||
		cred->gid.val <= (gid_t)KDP_CRED_SYS_ID ||
		cred->egid.val <= (gid_t)KDP_CRED_SYS_ID) {
		return 1;
	}

	return 0;
}

static int kdp_check_path_mismatch(struct kdp_vfsmount *vfsmnt)
{
	int i = 0;
	int ret = -1;
	char *buf = NULL;
	char *path_name = NULL;
	const char* skip_path[] = {
		"/com.android.runtime",
		"/com.android.conscrypt",
		"/com.android.art",
		"/com.android.adbd",
		"/com.android.sdkext",
	};

	if (!vfsmnt->bp_mount) {
		printk(KERN_ERR "vfsmnt->bp_mount is NULL");
		return -ENOMEM;
	}

	buf = kzalloc(PATH_MAX, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	path_name = dentry_path_raw(vfsmnt->bp_mount->mnt_mountpoint, buf, PATH_MAX);
	if (IS_ERR(path_name))
		goto out;

	for (; i < ARRAY_SIZE(skip_path); ++i) {
		if (!strncmp(path_name, skip_path[i], strlen(skip_path[i]))) {
			ret = 0;
			break;
		}
	}
out:
	kfree(buf);

	return ret;
}

int invalid_drive(struct linux_binprm *bprm)
{
	struct super_block *sb =  NULL;
	struct vfsmount *vfsmnt = NULL;

	vfsmnt = bprm->file->f_path.mnt;
	if (!vfsmnt || !is_kdp_vfsmnt_cache((unsigned long)vfsmnt)) {
		printk(KERN_ERR "[KDP] Invalid Drive : %s, vfsmnt: 0x%lx\n",
				bprm->filename, (unsigned long)vfsmnt);
		return 1;
	}

	if (!kdp_check_path_mismatch((struct kdp_vfsmount *)vfsmnt)) {
		return 0;
	}

	sb = vfsmnt->mnt_sb;

	if (kdp_check_sb_mismatch(sb)) {
		printk(KERN_ERR "[KDP] Superblock Mismatch -> %s vfsmnt: 0x%lx, mnt_sb: 0x%lx",
				bprm->filename, (unsigned long)vfsmnt, (unsigned long)sb);
		printk(KERN_ERR "[KDP] Superblock list : 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx\n",
				(unsigned long)rootfs_sb, (unsigned long)sys_sb, (unsigned long)odm_sb,
				(unsigned long)vendor_sb, (unsigned long)art_sb, (unsigned long)crypt_sb,
				(unsigned long)dex2oat_sb, (unsigned long)adbd_sb);
		return 1;
	}

	return 0;
}

inline void kdp_set_mnt_root_sb(struct vfsmount *mnt, struct dentry *mnt_root, struct super_block *mnt_sb)
{
	uh_call(UH_APP_KDP, SET_NS_ROOT_SB, (u64)mnt, (u64)mnt_root, (u64)mnt_sb, 0);
}

inline void kdp_assign_mnt_flags(struct vfsmount *mnt, int flags)
{
	uh_call(UH_APP_KDP, SET_NS_FLAGS, (u64)mnt, (u64)flags, 0, 0);
}

inline void kdp_clear_mnt_flags(struct vfsmount *mnt, int flags)
{
	int f = mnt->mnt_flags;

	f &= ~flags;
	kdp_assign_mnt_flags(mnt, f);
}

void kdp_set_mnt_flags(struct vfsmount *mnt, int flags)
{
	int f = mnt->mnt_flags;

	f |= flags;
	kdp_assign_mnt_flags(mnt, f);
}

void kdp_set_ns_data(struct vfsmount *mnt, void *data)
{
	uh_call(UH_APP_KDP, SET_NS_DATA, (u64)mnt, (u64)data, 0, 0);
}

int kdp_mnt_alloc_vfsmount(struct mount *mnt)
{
	struct kdp_vfsmount *vfsmnt = NULL;

	vfsmnt = kmem_cache_alloc(vfsmnt_cache, GFP_KERNEL);
	if (!vfsmnt)
		return 1;

	spin_lock(&mnt_vfsmnt_lock);
#ifdef CONFIG_RUSTUH_KDP
	uh_call(UH_APP_KDP, SET_NS_BP, (u64)vfsmnt, (u64)mnt, 0, 0);
#else
	uh_call(UH_APP_KDP, ALLOC_VFSMOUNT, (u64)vfsmnt, (u64)mnt, 0, 0);
#endif
	((struct kdp_mount *)mnt)->mnt = (struct vfsmount *)vfsmnt;
	spin_unlock(&mnt_vfsmnt_lock);

	return 0;
}

void kdp_free_vfsmount(void *objp)
{
	kmem_cache_free(vfsmnt_cache, objp);
}

static void kdp_populate_sb(char *mount_point, struct vfsmount *mnt)
{
	if (!mount_point || !mnt)
		return;

	if (!odm_sb && !strncmp(mount_point, KDP_MOUNT_PRODUCT, KDP_MOUNT_PRODUCT_LEN))
		uh_call(UH_APP_KDP, SET_NS_SB_VFSMOUNT, (u64)&odm_sb, (u64)mnt, KDP_SB_ODM, 0);
	else if (!sys_sb && !strncmp(mount_point, KDP_MOUNT_SYSTEM, KDP_MOUNT_SYSTEM_LEN))
		uh_call(UH_APP_KDP, SET_NS_SB_VFSMOUNT, (u64)&sys_sb, (u64)mnt, KDP_SB_SYS, 0);
	else if (!sys_sb && !strncmp(mount_point, KDP_MOUNT_SYSTEM2, KDP_MOUNT_SYSTEM2_LEN))
		uh_call(UH_APP_KDP, SET_NS_SB_VFSMOUNT, (u64)&sys_sb, (u64)mnt, KDP_SB_SYS, 0);
	else if (!vendor_sb && !strncmp(mount_point, KDP_MOUNT_VENDOR, KDP_MOUNT_VENDOR_LEN))
		uh_call(UH_APP_KDP, SET_NS_SB_VFSMOUNT, (u64)&vendor_sb, (u64)mnt, KDP_SB_VENDOR, 0);
	else if (!art_sb && !strncmp(mount_point, KDP_MOUNT_ART, KDP_MOUNT_ART_LEN - 1))
		uh_call(UH_APP_KDP, SET_NS_SB_VFSMOUNT, (u64)&art_sb, (u64)mnt, KDP_SB_ART, 0);
	else if (!crypt_sb && !strncmp(mount_point, KDP_MOUNT_CRYPT, KDP_MOUNT_CRYPT_LEN - 1))
		uh_call(UH_APP_KDP, SET_NS_SB_VFSMOUNT, (u64)&crypt_sb, (u64)mnt, KDP_SB_CRYPT, 0);
	else if (!dex2oat_sb && !strncmp(mount_point, KDP_MOUNT_DEX2OAT, KDP_MOUNT_DEX2OAT_LEN - 1))
		uh_call(UH_APP_KDP, SET_NS_SB_VFSMOUNT, (u64)&dex2oat_sb, (u64)mnt, KDP_SB_DEX2OAT, 0);
	else if (!dex2oat_count && !strncmp(mount_point, KDP_MOUNT_DEX2OAT, KDP_MOUNT_DEX2OAT_LEN)) {
		uh_call(UH_APP_KDP, SET_NS_SB_VFSMOUNT, (u64)&dex2oat_sb, (u64)mnt, KDP_SB_DEX2OAT, 0);
		dex2oat_count++;
	}
	else if (!adbd_sb && !strncmp(mount_point, KDP_MOUNT_ADBD, KDP_MOUNT_ADBD_LEN - 1))
		uh_call(UH_APP_KDP, SET_NS_SB_VFSMOUNT, (u64)&adbd_sb, (u64)mnt, KDP_SB_ADBD, 0);
}

int kdp_do_new_mount(struct vfsmount *mnt, struct path *path)
{
	char *buf = NULL;
	char *dir_name;

	buf = kzalloc(PATH_MAX, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	dir_name = dentry_path_raw(path->dentry, buf, PATH_MAX);
	if (!sys_sb || !odm_sb || !vendor_sb || !art_sb || !crypt_sb || !dex2oat_sb || !dex2oat_count || !adbd_sb)
		kdp_populate_sb(dir_name, mnt);

	kfree(buf);

	return 0;
}
#endif

#ifdef CONFIG_KDP_CRED
/* Main function to verify cred security context of a process */
int security_integrity_current(void)
{
	const struct cred *cur_cred = current_cred();

	rcu_read_lock();
	if (kdp_enable &&
			(is_kdp_invalid_cred_sp((u64)cur_cred, (u64)cur_cred->security)
			|| cmp_sec_integrity(cur_cred, current->mm)
#ifdef CONFIG_KDP_NS
			|| cmp_ns_integrity())) {
#else
			)) {
#endif
		rcu_read_unlock();
		panic("KDP CRED PROTECTION VIOLATION\n");
	}
	rcu_read_unlock();
	return 0;
}
#endif
