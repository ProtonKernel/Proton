// SPDX-License-Identifier: GPL-2.0-only
/*
 * sec_debug_extra_info.c
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/sec_debug.h>
#include <linux/stacktrace.h>
#include <asm/stacktrace.h>
#include <asm/esr.h>

#include "sec_debug_internal.h"
#include "sec_debug_extra_info_keys.c"

#define SDEI_DEBUG	(0)

#define MAX_CALL_ENTRY		128

static bool exin_ready;
static struct sec_debug_shared_buffer *sh_buf;
static void *slot_end_addr;

static char *ftype_items[MAX_ITEM_VAL_LEN] = {
	"UNDF", "BAD", "WATCH", "KERN", "MEM",
	"SPPC", "PAGE", "AUF", "EUF", "AUOF",
	"BUG", "SERR", "SEA"
};

static bool is_exin_module_initialized(void)
{
	if (sh_buf->magic[3] == SEC_DEBUG_SHARED_MAGIC3)
		exin_ready = true;

	return exin_ready;
}

/*****************************************************************/
/*                        UNIT FUNCTIONS                         */
/*****************************************************************/
static int get_val_len(const char *s)
{
	if (s)
		return strnlen(s, MAX_ITEM_VAL_LEN);

	return 0;
}

static int get_key_len(const char *s)
{
	return strnlen(s, MAX_ITEM_KEY_LEN);
}

static void *get_slot_addr(int idx)
{
	return (void *)secdbg_base_built_get_ncva(sh_buf->sec_debug_sbidx[idx].paddr);
}

static void *secdbg_exin_get_slot_end_addr(struct sec_debug_sb_index *sbidx)
{
	return (void *)secdbg_base_built_get_ncva(sbidx->paddr +
				((phys_addr_t)sbidx->size * (phys_addr_t)sbidx->nr));
}

static int get_max_len(void *p)
{
	int i;

	if (!slot_end_addr)
		slot_end_addr = secdbg_exin_get_slot_end_addr(&sh_buf->sec_debug_sbidx[SLOT_END]);

	if (p < get_slot_addr(SLOT_32) || (p >= slot_end_addr)) {
		pr_crit("%s: addr(%px) is not in range\n", __func__, p);

		return 0;
	}

	for (i = SLOT_32 + 1; i < NR_SLOT; i++)
		if (p < get_slot_addr(i))
			return sh_buf->sec_debug_sbidx[i - 1].size;

	return sh_buf->sec_debug_sbidx[SLOT_END].size;
}

static void *__get_item(int slot, unsigned int idx)
{
	void *p, *base;
	unsigned int size, nr;

	size = sh_buf->sec_debug_sbidx[slot].size;
	nr = sh_buf->sec_debug_sbidx[slot].nr;

	if (nr <= idx) {
		pr_crit("%s: SLOT %d has %d items (%d)\n",
					__func__, slot, nr, idx);

		return NULL;
	}

	base = get_slot_addr(slot);
	p = (void *)(base + (idx * size));

	return p;
}

static void *search_item_by_key(const char *key, int start, int end)
{
	int s, i, keylen = get_key_len(key);
	void *p;
	char *keyname;
	unsigned int max;

	if (!sh_buf || !is_exin_module_initialized()) {
		pr_info("%s: (%s) extra_info is not ready\n", __func__, key);

		return NULL;
	}

	for (s = start; s < end; s++) {
		if (sh_buf->sec_debug_sbidx[s].cnt)
			max = sh_buf->sec_debug_sbidx[s].cnt;
		else
			max = sh_buf->sec_debug_sbidx[s].nr;

		for (i = 0; i < max; i++) {
			p = __get_item(s, i);
			if (!p)
				break;

			keyname = (char *)p;
			if (keylen != get_key_len(keyname))
				continue;

			if (!strncmp(keyname, key, keylen))
				return p;
		}
	}

	return NULL;
}

static void *get_item(const char *key)
{
	return search_item_by_key(key, SLOT_32, NR_MAIN_SLOT);
}

static char *get_item_val(const char *key)
{
	void *p;

	p = get_item(key);
	if (!p) {
		pr_crit("%s: no key %s\n", __func__, key);

		return NULL;
	}

	return ((char *)p + MAX_ITEM_KEY_LEN);
}

static int is_ocp;

static int is_key_in_blocklist(const char *key)
{
	char blkey[][MAX_ITEM_KEY_LEN] = {
		"KTIME", "BAT", "FTYPE", "ODR", "DDRID",
		"PSITE", "ASB", "ASV", "IDS",
	};

	int nr_blkey, keylen, i;

	keylen = get_key_len(key);
	nr_blkey = ARRAY_SIZE(blkey);

	for (i = 0; i < nr_blkey; i++)
		if (!strncmp(key, blkey[i], keylen))
			return 1;

	if (!strncmp(key, "OCP", keylen)) {
		if (is_ocp)
			return 1;
		is_ocp = 1;
	}

	return 0;
}

static DEFINE_SPINLOCK(keyorder_lock);

static void set_key_order(const char *key)
{
	void *p;
	char *v;
	char tmp[MAX_ITEM_VAL_LEN] = {0, };
	int max = MAX_ITEM_VAL_LEN;
	int len_prev, len_remain, len_this;

	if (is_key_in_blocklist(key))
		return;

	spin_lock(&keyorder_lock);

	p = get_item("ODR");
	if (!p) {
		pr_crit("%s: fail to find %s\n", __func__, key);

		goto unlock_keyorder;
	}

	max = get_max_len(p);
	if (!max) {
		pr_crit("%s: fail to get max len %s\n", __func__, key);

		goto unlock_keyorder;
	}

	v = get_item_val(p);

	/* keep previous value */
	len_prev = get_val_len(v);
	if ((!len_prev) || (len_prev >= MAX_ITEM_VAL_LEN))
		len_prev = MAX_ITEM_VAL_LEN - 1;

	snprintf(tmp, len_prev + 1, "%s", v);

	/* calculate the remained size */
	len_remain = max;

	/* get_item_val returned address without key */
	len_remain -= MAX_ITEM_KEY_LEN;

	/* need comma */
	len_this = get_key_len(key) + 1;
	len_remain -= len_this;

	/* put last key at the first of ODR */
	/* +1 to add NULL (by snprintf) */
	snprintf(v, len_this + 1, "%s,", key);

	/* -1 to remove NULL between KEYS */
	/* +1 to add NULL (by snprintf) */
	snprintf((char *)(v + len_this), len_remain + 1, "%s", tmp);

unlock_keyorder:
	spin_unlock(&keyorder_lock);
}

static void set_item_val(const char *key, const char *fmt, ...)
{
	va_list args;
	void *p;
	char *v;
	int max = MAX_ITEM_VAL_LEN;

	p = get_item(key);
	if (!p) {
		pr_crit("%s: fail to find %s\n", __func__, key);

		return;
	}

	max = get_max_len(p);
	if (!max) {
		pr_crit("%s: fail to get max len %s\n", __func__, key);

		return;
	}

	v = get_item_val(p);
	if (!get_val_len(v)) {
		va_start(args, fmt);
		vsnprintf(v, max, fmt, args);
		va_end(args);

		set_key_order(key);
	}
}

void secdbg_exin_set_fault(enum secdbg_exin_fault_type type,
				    unsigned long addr, struct pt_regs *regs)
{

	phys_addr_t paddr = 0;

	if (regs) {
		pr_crit("%s = %s / 0x%lx\n", __func__, ftype_items[type], addr);

		set_item_val("FTYPE", "%s", ftype_items[type]);
		set_item_val("FAULT", "0x%lx", addr);
		set_item_val("PC", "%pS", regs->pc);
		set_item_val("LR", "%pS",
					 compat_user_mode(regs) ?
					 regs->compat_lr : regs->regs[30]);

		if (type == UNDEF_FAULT && addr >= kimage_voffset) {
			paddr = virt_to_phys((void *)addr);

			pr_crit("%s: 0x%x / 0x%x\n", __func__,
				upper_32_bits(paddr), lower_32_bits(paddr));
//			exynos_pmu_write(EXYNOS_PMU_INFORM8, lower_32_bits(paddr));
//			exynos_pmu_write(EXYNOS_PMU_INFORM9, upper_32_bits(paddr));
		}
	}
}

void secdbg_exin_set_bug(const char *file, unsigned int line)
{
	set_item_val("BUG", "%s:%u", file, line);
}

void secdbg_exin_set_regs(struct pt_regs *regs)
{
	char fbuf[MAX_ITEM_VAL_LEN];
	int offset = 0, i;
	char *v;

	v = get_item_val("REGS");
	if (!v) {
		pr_crit("%s: no REGS in items\n", __func__);
		return;
	}

	if (get_val_len(v)) {
		pr_crit("%s: already %s in REGS\n", __func__, v);
		return;
	}

	memset(fbuf, 0, MAX_ITEM_VAL_LEN);

	pr_crit("%s: set regs\n", __func__);

	offset += sprintf(fbuf + offset, "pc:%llx/", regs->pc);
	offset += sprintf(fbuf + offset, "sp:%llx/", regs->sp);
	offset += sprintf(fbuf + offset, "pstate:%llx/", regs->pstate);

	for (i = 0; i < 31; i++)
		offset += sprintf(fbuf + offset, "x%d:%llx/", i, regs->regs[i]);

	set_item_val("REGS", fbuf);
}

static void __print_stack_trace(char *outbuf, int maxlen, struct stack_trace *trace)
{
	unsigned int i;
	int offset = 0;

	if (!trace->nr_entries)
		return;

	for (i = 0; i < trace->nr_entries; i++) {
		if (offset)
			offset += scnprintf(outbuf + offset, maxlen - offset, ":");

		offset += scnprintf(outbuf + offset, maxlen - offset, "%ps", (void *)trace->entries[i]);
	}
}

void secdbg_exin_set_backtrace(struct pt_regs *regs)
{
	char fbuf[MAX_ITEM_VAL_LEN];
	unsigned long entry[MAX_CALL_ENTRY];
	struct stack_trace trace = {0,};
	char *v;

	if (regs)
		secdbg_exin_set_regs(regs);

	v = get_item_val("STACK");
	if (!v) {
		pr_crit("%s: no STACK in items\n", __func__);
		return;
	}

	if (get_val_len(v)) {
		pr_crit("%s: already %s in STACK\n", __func__, v);
		return;
	}

	memset(fbuf, 0, MAX_ITEM_VAL_LEN);

	pr_crit("%s\n", __func__);

	trace.max_entries = MAX_CALL_ENTRY;
	trace.entries = entry;

	if (regs)
		save_stack_trace_regs(regs, &trace);
	else
		save_stack_trace(&trace);

	__print_stack_trace(fbuf, MAX_ITEM_VAL_LEN, &trace);

	set_item_val("STACK", fbuf);
}

void secdbg_exin_set_backtrace_task(struct task_struct *tsk)
{
	char fbuf[MAX_ITEM_VAL_LEN];
	char buf[64];
	struct stackframe frame;
	int offset = 0;
	int sym_name_len;
	char *v;

	if (!tsk) {
		pr_crit("%s: no TASK, quit\n", __func__);
		return;
	}

	if (!try_get_task_stack(tsk)) {
		pr_crit("%s: fail to get task stack, quit\n", __func__);
		return;
	}

	v = get_item_val("STACK");
	if (!v) {
		pr_crit("%s: no STACK in items\n", __func__);
		goto out;
	}

	if (get_val_len(v)) {
		pr_crit("%s: already %s in STACK\n", __func__, v);
		goto out;
	}

	memset(fbuf, 0, MAX_ITEM_VAL_LEN);

	pr_crit("sec_debug_store_backtrace_task\n");

	if (tsk == current) {
		start_backtrace(&frame,
				(unsigned long)__builtin_frame_address(0),
				(unsigned long)secdbg_exin_set_backtrace_task);
	} else {
		/*
		 * task blocked in __switch_to
		 */
		start_backtrace(&frame,
				thread_saved_fp(tsk),
				thread_saved_pc(tsk));
	}

	do {
		unsigned long where = frame.pc;

		snprintf(buf, sizeof(buf), "%ps", (void *)where);
		sym_name_len = strlen(buf);

		if (offset + sym_name_len > MAX_ITEM_VAL_LEN)
			break;

		if (offset)
			offset += scnprintf(fbuf + offset, MAX_ITEM_VAL_LEN - offset, ":");

		offset += scnprintf(fbuf + offset, MAX_ITEM_VAL_LEN - offset, "%s", buf);
	} while (!unwind_frame(tsk, &frame));

	set_item_val("STACK", fbuf);

out:
	put_task_stack(tsk);
}

void secdbg_exin_set_dpm_timeout(const char *devname)
{
	set_item_val("DPM", "%s", devname);
}

void secdbg_exin_set_esr(unsigned int esr)
{
	set_item_val("ESR", "%s (0x%08x)",
	esr_get_class_string(esr), esr);
}

void secdbg_exin_built_set_merr(const char *merr)
{
	set_item_val("MER", "%s", merr);
}

void secdbg_exin_built_set_hint(unsigned long hint)
{
	if (hint)
		set_item_val("HINT", "%llx", hint);
}

void secdbg_exin_built_set_decon(unsigned int err)
{
	set_item_val("DCN", "%08x", err);
}

void secdbg_exin_set_ufs_error(const char *str)
{
	set_item_val("ETC", "%s", str);
}

void secdbg_exin_built_set_zswap(const char *str)
{
	set_item_val("ETC", "%s", str);
}

void secdbg_exin_built_set_aud(const char *str)
{
	set_item_val("AUD", "%s", str);
}

#define MAX_UNFZ_VAL_LEN (240)

void secdbg_exin_set_unfz(const char *comm, int pid)
{
	void *p;
	char *v;
	char tmp[MAX_UNFZ_VAL_LEN] = {0, };
	int max = MAX_UNFZ_VAL_LEN;
	int len_prev, len_remain, len_this;

	p = get_item("UNFZ");
	if (!p) {
		pr_crit("%s: fail to find %s\n", __func__, comm);

		return;
	}

	max = get_max_len(p);
	if (!max) {
		pr_crit("%s: fail to get max len %s\n", __func__, comm);

		return;
	}

	v = get_item_val(p);

	/* keep previous value */
	len_prev = get_val_len(v);
	if ((!len_prev) || (len_prev >= MAX_UNFZ_VAL_LEN))
		len_prev = MAX_UNFZ_VAL_LEN - 1;

	snprintf(tmp, len_prev + 1, "%s", v);

	/* calculate the remained size */
	len_remain = max;

	/* get_item_val returned address without key */
	len_remain -= MAX_ITEM_KEY_LEN;

	/* put last key at the first of ODR */
	/* +1 to add NULL (by snprintf) */
	if (pid < 0)
		len_this = scnprintf(v, len_remain, "%s/", comm);
	else
		len_this = scnprintf(v, len_remain, "%s:%d/", comm, pid);

	/* -1 to remove NULL between KEYS */
	/* +1 to add NULL (by snprintf) */
	snprintf((char *)(v + len_this), len_remain - len_this, "%s", tmp);
}

char *secdbg_exin_get_unfz(void)
{
	void *p;
	int max = MAX_UNFZ_VAL_LEN;

	p = get_item("UNFZ");
	if (!p) {
		pr_crit("%s: fail to find\n", __func__);

		return NULL;
	}

	max = get_max_len(p);
	if (!max) {
		pr_crit("%s: fail to get max len\n", __func__);

		return NULL;
	}

	return get_item_val(p);
}

int secdbg_extra_info_built_init(void)
{

	pr_info("%s: start\n", __func__);

	sh_buf = secdbg_base_built_get_debug_base(SDN_MAP_EXTRA_INFO);
	if (!sh_buf) {
		pr_err("%s: No extra info buffer\n", __func__);
		return -EFAULT;
	}

	if (!is_exin_module_initialized())
		pr_info("%s: exin_module has not been initialized yet\n", __func__);

	pr_info("%s: done\n", __func__);

	return 0;
}

