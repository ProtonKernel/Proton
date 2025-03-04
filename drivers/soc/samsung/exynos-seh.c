/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - Smart Exception Handler
 * Author: Jang Hyunsung <hs79.jang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <asm/cacheflush.h>
#include <asm/memory.h>

#include <soc/samsung/exynos-smc.h>
#include <soc/samsung/exynos-el3_mon.h>

static char *smc_debug_mem;
static struct page *page_ptr;


static void exynos_smart_exception_handler(unsigned int id,
				unsigned long elr, unsigned long esr,
				unsigned long sctlr, unsigned long ttbr,
				unsigned long far, unsigned long x6,
				unsigned int offset)
{
	int i;
	unsigned long *ptr;
	unsigned long tmp;

	pr_err("========================================="
		"=========================================\n");

	if (id)
		pr_err("%s: There has been an unexpected exception from "
			"a LDFW which has smc id 0x%x\n\n", __func__, id);
	else
		pr_err("%s: There has been an unexpected exception from "
			"the EL3 monitor.\n\n", __func__);

	if (id) {
		pr_err("elr_el1   : 0x%016lx, \tesr_el1  : 0x%016lx\n",
								elr, esr);
		pr_err("sctlr_el1 : 0x%016lx, \tttbr_el1 : 0x%016lx\n",
								sctlr, ttbr);
		pr_err("far_el1   : 0x%016lx, \tlr (EL1) : 0x%016lx\n\n",
								far, x6);
	} else {
		pr_err("elr_el3   : 0x%016lx, \tesr_el3  : 0x%016lx\n",
								elr, esr);
		pr_err("sctlr_el3 : 0x%016lx, \tttbr_el3 : 0x%016lx\n",
								sctlr, ttbr);
		pr_err("far_el3   : 0x%016lx, \tscr_el3  : 0x%016lx\n\n",
								far, x6);
	}

	if ((offset > 0x0 && offset < (PAGE_SIZE * 2))
			&& !(offset % 0x8) && (smc_debug_mem)) {

		/* Flush  smc_debug_mem for cache coherency */
		flush_dcache_page(page_ptr);

		tmp = (unsigned long)smc_debug_mem;
		tmp += (unsigned long)offset;
		ptr = (unsigned long *)tmp;

		for (i = 0; i < 15; i++) {
			pr_err("x%02d : 0x%016lx, \tx%02d : 0x%016lx\n",
					i * 2, ptr[i * 2],
					i * 2 + 1, ptr[i * 2 + 1]);
		}
		pr_err("x%02d : 0x%016lx\n", i * 2,  ptr[i * 2]);
	}

	pr_err("\n[WARNING] IT'S GOING TO CAUSE KERNEL PANIC FOR DEBUGGING.\n\n");

	pr_err("========================================="
		"=========================================\n");
	/* make kernel panic */
	BUG();

	/* SHOULD NOT be here */
	while(1);
}

static int  __init exynos_set_seh_address(void)
{
	unsigned long addr = (unsigned long)exynos_smart_exception_handler;
	unsigned long ret;
	char *phys;
	unsigned int size = PAGE_SIZE * 2;

	pr_info("%s: send smc call with SMC_CMD_SET_SEH_ADDRESS.\n", __func__);

	/* Set debug mem */
	page_ptr = alloc_pages(GFP_KERNEL, 1);
	if (!page_ptr) {
		pr_err("%s: alloc_page for smc_debug failed.\n", __func__);
		return 0;
	}

	smc_debug_mem = page_address(page_ptr);

	if (!smc_debug_mem) {
		pr_err("%s: kmalloc for smc_debug failed.\n", __func__);
		return 0;
	}

	/* to map & flush memory */
	memset(smc_debug_mem, 0x00, size);
	flush_dcache_page(page_ptr);

	phys = (char *)virt_to_phys(smc_debug_mem);
	pr_err("%s: alloc kmem for smc_dbg virt: 0x%p phys: 0x%p size: %d.\n",
			__func__, smc_debug_mem, phys, size);
	ret = exynos_smc(SMC_CMD_SET_DEBUG_MEM, (u64)phys, (u64)size, 0);

	/* correct return value is input size */
	if (ret != size) {
		pr_err("%s: Can not set the address to el3 monitor. "
				"ret = 0x%lx. free the kmem\n", __func__, ret);
		__free_pages(page_ptr, 1);
		return 0;
	}

	ret = exynos_smc(SMC_CMD_SET_SEH_ADDRESS, addr, 0, 0);

	/* return value not zero means failure */
	if (ret)
		pr_err("%s: did not set the seh address to el3 monitor. "
				"ret = 0x%lx.\n", __func__, ret);
	else
		pr_err("%s: set the seh address to el3 monitor well.\n",
							__func__);

	return 0;
}
module_init(exynos_set_seh_address);
MODULE_LICENSE("GPL");
