// SPDX-License-Identifier: GPL-2.0

#include <linux/rkp.h>
#include <linux/mm.h>

#include <asm/pgtable.h>
bool rkp_started __rkp_ro = false;
static u64 robuffer_base __rkp_ro;
static u64 robuffer_size __rkp_ro;

static spinlock_t ro_rkp_pages_lock = __SPIN_LOCK_UNLOCKED();
static char ro_pages_stat[PAGE_SIZE] = {0,};
static unsigned int ro_alloc_avail;

/* init/main.c */
void __init rkp_init(void)
{
	struct rkp_init init_data;

	memset((void *)&init_data, 0, sizeof(struct rkp_init));
	/* initialized rkp_init struct */
	init_data.magic = RKP_INIT_MAGIC;
	init_data.vmalloc_start = VMALLOC_START;
#ifndef CONFIG_RKP_FIMC_CHECK
	init_data.no_fimc_verify = 1;
#endif
	init_data.fimc_phys_addr = 0;
	init_data._text = (u64)_text;
	init_data._etext = (u64)_etext;
	init_data._srodata = (u64)__start_rodata;
	init_data._erodata = (u64)__end_rodata;
	init_data.large_memory = 0;

	init_data.vmalloc_end = (u64)high_memory;
	init_data.init_mm_pgd = (u64)__pa(swapper_pg_dir);
	init_data.id_map_pgd = (u64)__pa(idmap_pg_dir);
	init_data.zero_pg_addr = (u64)__pa(empty_zero_page);

#ifdef CONFIG_UNMAP_KERNEL_AT_EL0
	init_data.tramp_pgd = (u64)__pa(tramp_pg_dir);
	init_data.tramp_valias = (u64)TRAMP_VALIAS;
#endif
	uh_call(UH_APP_RKP, RKP_START, (u64)&init_data, (u64)kimage_voffset, 0, 0);
	rkp_started = true;
}

/* init/main.c */
void rkp_deferred_init(void)
{
	uh_call(UH_APP_RKP, RKP_DEFERRED_START, 0, 0, 0, 0);
}

#ifdef CONFIG_RUSTUH_RKP
/* RO BUFFER */
void rkp_robuffer_init(void)
{
	uh_call(UH_APP_RKP, RKP_GET_RO_INFO, (u64)&robuffer_base, (u64)&robuffer_size, 0, 0);
}

/* allocation */
inline phys_addr_t rkp_ro_alloc_phys(int shift)
{
	phys_addr_t alloc_addr = 0;

	uh_call(UH_APP_RKP, RKP_ROBUFFER_ALLOC, (u64)&alloc_addr, 1, 0, 0);

	return alloc_addr;
}

inline void *rkp_ro_alloc(void)
{
	void *addr = NULL;

	uh_call(UH_APP_RKP, RKP_ROBUFFER_ALLOC, (u64)&addr, 1, 0, 0);
	if (!addr)
		return 0;

	return (void *)__phys_to_virt(addr);
}

inline void rkp_ro_free(void *addr)
{
	uh_call(UH_APP_RKP, RKP_ROBUFFER_FREE, (u64)addr, 0, 0, 0);
}

inline bool is_rkp_ro_buffer(u64 addr)
{
	u64 pa = (u64)virt_to_phys(addr);

	if ((robuffer_base <= pa) && (pa < robuffer_base + robuffer_size))
		return true;
	else
		return false;
}
#else
/* RO BUFFER */
void rkp_robuffer_init(void)
{
	uh_call(UH_APP_RKP, RKP_GET_RO_BUFFER, (u64)&robuffer_base, (u64)&robuffer_size, 0, 0);
}

/* allocation */
inline phys_addr_t rkp_ro_alloc_phys(int shift)
{
	unsigned long flags;
	unsigned int i = 0;
	phys_addr_t alloc_addr = 0;
	bool found = false;

	int ro_pages = robuffer_size >> PAGE_SHIFT;

	spin_lock_irqsave(&ro_rkp_pages_lock, flags);
	while (i < ro_pages) {
		if (ro_pages_stat[ro_alloc_avail] == false) {
			found = true;
			break;
		}
		ro_alloc_avail = (ro_alloc_avail + 1) % ro_pages;
		i++;
	}

	if (found) {
		alloc_addr = (phys_addr_t)(robuffer_base + (ro_alloc_avail << PAGE_SHIFT));
		ro_pages_stat[ro_alloc_avail] = true;
		ro_alloc_avail = (ro_alloc_avail + 1) % ro_pages;
	}
	spin_unlock_irqrestore(&ro_rkp_pages_lock, flags);

	return alloc_addr;
}

inline void *rkp_ro_alloc(void)
{
	unsigned long flags;
	unsigned int i = 0;
	void *alloc_addr = NULL;
	bool found = false;

	int ro_pages = robuffer_size >> PAGE_SHIFT;
	u64 va = (u64)phys_to_virt(robuffer_base);

	spin_lock_irqsave(&ro_rkp_pages_lock, flags);
	while (i < ro_pages) {
		if (ro_pages_stat[ro_alloc_avail] == false) {
			found = true;
			break;
		}
		ro_alloc_avail = (ro_alloc_avail + 1) % ro_pages;
		i++;
	}

	if (found) {
		alloc_addr = (void *)(va + (ro_alloc_avail << PAGE_SHIFT));
		ro_pages_stat[ro_alloc_avail] = true;
		ro_alloc_avail = (ro_alloc_avail + 1) % ro_pages;
	}
	spin_unlock_irqrestore(&ro_rkp_pages_lock, flags);

	return alloc_addr;
}

inline void rkp_ro_free(void *free_addr)
{
	unsigned int i;
	unsigned long flags;
	u64 va = (u64)phys_to_virt(robuffer_base);

	i =  ((u64)free_addr - va) >> PAGE_SHIFT;

	spin_lock_irqsave(&ro_rkp_pages_lock, flags);
	ro_pages_stat[i] = 0;
	ro_alloc_avail = i;
	spin_unlock_irqrestore(&ro_rkp_pages_lock, flags);
}

inline bool is_rkp_ro_buffer(u64 addr)
{
	u64 va = (u64)phys_to_virt(robuffer_base);

	if ((va <= addr) && (addr < va + robuffer_size))
		return true;
	else
		return false;
}
#endif
