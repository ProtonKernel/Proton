/*
* Copyright (c) 2020 Samsung Electronics Co., Ltd.
*            http://www.samsung.com/
*
* EXYNOS - EL3 monitor support
* Author: Jang Hyunsung <hs79.jang@samsung.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/

#include <asm/cacheflush.h>
#include <asm/memory.h>
#include <soc/samsung/exynos-smc.h>
#include <linux/module.h>
#include <soc/samsung/exynos-el3_mon.h>

static int __init exynos_kernel_text_info(void)
{
	int ret = 0;
	unsigned long ktext_start_va = 0;
	unsigned long ktext_start_pa = 0;
	unsigned long ktext_end_va = 0;
	unsigned long ktext_end_pa = 0;

	/* Get virtual addresses of kernel text */
	ktext_start_va = (unsigned long)_text;
	if (!ktext_start_va) {
		pr_err("%s: [ERROR] Kernel text start address is invalid\n",
				__func__);
		return 0;
	}

	ktext_end_va = (unsigned long)_etext;
	if (!ktext_end_va) {
		pr_err("%s: [ERROR] Kernel text end address is invalid\n",
				__func__);
		return 0;
	}

	/* Translate VA to PA */
	ktext_start_pa = (unsigned long)__pa_symbol(_text);
	ktext_end_pa = (unsigned long)__pa_symbol(_etext);

	pr_info("%s: Kernel text start VA(%#lx), PA(%#lx)\n",
			__func__, ktext_start_va, ktext_start_pa);
	pr_info("%s: Kernel text end VA(%#lx), PA(%#lx)\n",
			__func__, ktext_end_va, ktext_end_pa);

	/* I-cache flush to the PoC */
	flush_icache_range(ktext_start_va, ktext_end_va);

	// Use SMC for saving kernel text info in EL3.
	ret = exynos_smc(SMC_CMD_PUSH_KERNEL_TEXT_INFO,
			ktext_start_pa,
			ktext_end_pa,
			1);
	return ret;

}
core_initcall(exynos_kernel_text_info);
