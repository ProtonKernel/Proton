// SPDX-License-Identifier: GPL-2.0-only
/*
 * sec_debug_ksym.c
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 */

#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/libfdt.h>
#include <asm/memory.h>

#include <linux/sec_debug.h>

#include "sec_debug_internal.h"

#ifdef CONFIG_KALLSYMS_ALL
#define all_var 1
#else
#define all_var 0
#endif

/* Kernel Symbol For Debugging */
static struct device_node *ksyms_np;
static uint64_t ksym_offset;

static uint64_t get_ksym_dt_val(const char *name)
{
	uint64_t ret;

	if (of_property_read_u64(ksyms_np, name, &ret)) {
		pr_crit("%s: fail to get %s\n", __func__, name);
		return -EINVAL;
	}

	return ret + ksym_offset;
}

static void update_kernel_symbol_offset(void)
{
	/* kaslr_offset in arch/arm64/include/asm/memory.h */
	ksym_offset = (uint64_t)kaslr_offset();
}

static int secdbg_ksym_get_dt_data(void)
{
	int ret = 0;
	struct device_node *np;

	np = of_find_node_by_path("/sec_debug_data/kallsyms");
	if (!np) {
		pr_crit("%s: of_find_node_by_path, failed, /sec_debug/kallsyms\n", __func__);
		return -EINVAL;
	}

	pr_crit("%s: set kallsyms dt node\n", __func__);
	ksyms_np = np;

	return ret;
}

void secdbg_ksym_set_kallsyms_info(struct sec_debug_ksyms *ksyms)
{
	struct sec_debug_ksyms *ksyms_data;

	if (secdbg_ksym_get_dt_data()) {
		pr_crit("%s: fail to get kallsyms data in dt, quit\n", __func__);

		return;
	}

	pr_crit("%s: start to get ksyms data\n", __func__);
	update_kernel_symbol_offset();

	ksyms_data = ksyms;

	if (!IS_ENABLED(CONFIG_KALLSYMS_BASE_RELATIVE)) {
		ksyms_data->addresses_pa = __pa(get_ksym_dt_val("kallsyms_addresses"));
		ksyms_data->relative_base = 0x0;
		ksyms_data->offsets_pa = 0x0;
	} else {
		ksyms_data->addresses_pa = 0x0;
		ksyms_data->relative_base = *(uint64_t *)get_ksym_dt_val("kallsyms_relative_base");
		ksyms_data->offsets_pa = __pa(get_ksym_dt_val("kallsyms_offsets"));
	}

	ksyms_data->names_pa = __pa(get_ksym_dt_val("kallsyms_names"));
	ksyms_data->num_syms = (uint64_t)(*(uint32_t *)get_ksym_dt_val("kallsyms_num_syms"));
	ksyms_data->token_table_pa = __pa(get_ksym_dt_val("kallsyms_token_table"));
	ksyms_data->token_index_pa = __pa(get_ksym_dt_val("kallsyms_token_index"));
	ksyms_data->markers_pa = __pa(get_ksym_dt_val("kallsyms_markers"));

	ksyms_data->sect.sinittext = get_ksym_dt_val("_sinittext");
	ksyms_data->sect.einittext = get_ksym_dt_val("_einittext");
	ksyms_data->sect.stext = get_ksym_dt_val("_stext");
	ksyms_data->sect.etext = get_ksym_dt_val("_etext");
	ksyms_data->sect.end = get_ksym_dt_val("_end");

	ksyms_data->kallsyms_all = all_var;

	/* MAGIC1 in sec_debug_internal.h */
	ksyms_data->magic = SEC_DEBUG_MAGIC1;

	pr_info("%s: relbase: %llx\n", __func__, ksyms_data->relative_base);
	pr_info("%s: numsyms: %llx\n", __func__, ksyms_data->num_syms);
	pr_info("%s: pa_markers: %llx\n", __func__, ksyms_data->markers_pa);
	pr_info("%s: pa_names: %llx\n", __func__, ksyms_data->names_pa);

	pr_crit("%s: end to fill ksym data\n", __func__);
}
