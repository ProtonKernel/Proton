/*
 * Exynos PMUCAL debug interface support.
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include "pmucal_cpu.h"
#include "pmucal_local.h"
#include "pmucal_system.h"

#define PMUCAL_DBG_PREFIX	"PMUCAL-DBG"

static void __iomem *latency_base;
static void __iomem *mct_base;
static u32 profile_en;
static u32 profile_en_bit;
static u32 profile_en_offset;
static struct dentry *root;
static struct pmucal_dbg_info *cpu_list;
static struct pmucal_dbg_info *cluster_list;
static struct pmucal_dbg_info *local_list;
static struct pmucal_dbg_info *system_list;
bool pmucal_dbg_init_ok;

static u32 get_curr_mct(void) {
	return __raw_readl(mct_base);
}

void pmucal_dbg_set_emulation(struct pmucal_dbg_info *dbg)
{
	if (dbg->emul_en && !dbg->emul_enabled) {
		exynos_pmu_update(dbg->emul_offset, (1 << dbg->emul_bit), (1 << dbg->emul_bit));
		dbg->emul_enabled = 1;
	} else if (!dbg->emul_en && dbg->emul_enabled) {
		exynos_pmu_update(dbg->emul_offset, (1 << dbg->emul_bit), (0 << dbg->emul_bit));
		dbg->emul_enabled = 0;
	}
	return ;
}

void pmucal_dbg_req_emulation(struct pmucal_dbg_info *dbg, bool en)
{
	if (en) {
		dbg->emul_en++;
	} else {
		if (dbg->emul_en)
			dbg->emul_en--;
		else
			pr_err("%s: imbalanced %s.\n", PMUCAL_DBG_PREFIX, __func__);
	}

	return ;
}

static void pmucal_dbg_record_latency(void __iomem* addr, bool is_on)
{
	u32 start, end, result;

	if (is_on) {
		start = __raw_readl(addr);
		end = get_curr_mct();

		if (end < start)
			result = 0xffffffff - start + end;
		else
			result = end - start;

		__raw_writel(result, addr);
	} else {
		__raw_writel(get_curr_mct(), addr);
	}
}

static void pmucal_dbg_update_stats(struct pmucal_latency *stats, u64 curr_latency, u64 cnt)
{
	if (stats->min > curr_latency)
		stats->min = curr_latency;
	stats->avg = (stats->avg * cnt + curr_latency) / (cnt + 1);
	if (stats->max < curr_latency)
		stats->max = curr_latency;

	return ;
}

void pmucal_dbg_do_profile(struct pmucal_dbg_info *dbg, bool is_on)
{
	u64 curr_on_latency = 0, curr_off_latency = 0, curr_on_latency1 = 0, curr_off_latency1 = 0;
	/* CPU/CLUSTER 	- use MCT (38ns/tick),
	   SYSTEM 	- use APM systick (20ns/tick) */
	u32 factor = (dbg->block_id == BLK_CPU || dbg->block_id == BLK_CLUSTER) ? 38 : 20;
	int i;

	if (!profile_en)
		return;

	if (is_on) {
		if (!dbg->profile_started)
			return;

		if (dbg->block_id == BLK_SYSTEM) {
			/* soc down */
			curr_off_latency = __raw_readl(latency_base + dbg->latency_offset) * 20;
			/* mif down */
			curr_off_latency1 = __raw_readl(latency_base + dbg->latency_offset + 8) * 20;
			/* soc up */
			curr_on_latency = __raw_readl(latency_base + dbg->latency_offset + 4) * 20;
			/* mif up */
			curr_on_latency1 = __raw_readl(latency_base + dbg->latency_offset + 4 + 8) * 20;
		} else {
			curr_off_latency = __raw_readl(latency_base + dbg->latency_offset) * 20;
			curr_on_latency = __raw_readl(latency_base + dbg->latency_offset + 4) * 20;
		}

		/* Update profile when there is no 'early-wakeup'. */
		if (((dbg->block_id == BLK_SYSTEM) && curr_off_latency && curr_off_latency1) ||
				((dbg->block_id != BLK_SYSTEM) && curr_off_latency)) {
			spin_lock(&dbg->profile_lock);

			pmucal_dbg_update_stats(&dbg->off, curr_off_latency, dbg->off_cnt);
			pmucal_dbg_update_stats(&dbg->on, curr_on_latency, dbg->off_cnt);

			if (dbg->block_id == BLK_SYSTEM) {
				pmucal_dbg_update_stats(dbg->off1, curr_off_latency1, dbg->off_cnt);
				pmucal_dbg_update_stats(dbg->on1, curr_on_latency1, dbg->off_cnt);
			}

			if (dbg->aux) {
				if (dbg->block_id == BLK_CPU || dbg->block_id == BLK_CLUSTER)
					pmucal_dbg_record_latency(latency_base + dbg->aux_offset + 4 * (dbg->aux_size - 1), true);

				for (i = 0; i < dbg->aux_size; i++) {
					u64 aux_latency;
					aux_latency = __raw_readl(latency_base + dbg->aux_offset + 4 * i) * factor;
					pmucal_dbg_update_stats(&dbg->aux[i], aux_latency, dbg->off_cnt);
				}
			}

			dbg->off_cnt += 1;

			spin_unlock(&dbg->profile_lock);
		}

		/* clear latency data of this cycle */
		if (dbg->block_id == BLK_SYSTEM) {
			__raw_writel(0, latency_base + dbg->latency_offset);
			__raw_writel(0, latency_base + dbg->latency_offset + 0x4);
			__raw_writel(0, latency_base + dbg->latency_offset + 0x8);
			__raw_writel(0, latency_base + dbg->latency_offset + 0xc);
		} else {
			__raw_writel(0, latency_base + dbg->latency_offset);
			__raw_writel(0, latency_base + dbg->latency_offset + 0x4);
		}
		if (dbg->aux) {
			for (i = 0; i < dbg->aux_size; i++) {
				__raw_writel(0, latency_base + dbg->aux_offset + 4 * i);
			}
		}
	} else {
		if (!dbg->profile_started) {
			dbg->profile_started = true;

			/* clear latency data of this cycle */
			if (dbg->block_id == BLK_SYSTEM) {
				__raw_writel(0, latency_base + dbg->latency_offset);
				__raw_writel(0, latency_base + dbg->latency_offset + 0x4);
				__raw_writel(0, latency_base + dbg->latency_offset + 0x8);
				__raw_writel(0, latency_base + dbg->latency_offset + 0xc);
			} else {
				__raw_writel(0, latency_base + dbg->latency_offset);
				__raw_writel(0, latency_base + dbg->latency_offset + 0x4);
			}
			if (dbg->aux) {
				for (i = 0; i < dbg->aux_size; i++) {
					__raw_writel(0, latency_base + dbg->aux_offset + 4 * i);
				}
			}
		}

		/* Start the off - kernel ~ wfi */
		if (dbg->aux && (dbg->block_id == BLK_CPU || dbg->block_id == BLK_CLUSTER))
			pmucal_dbg_record_latency(latency_base + dbg->aux_offset, false);
	}
}

static void pmucal_dbg_clear_profile(struct pmucal_dbg_info *dbg)
{
	int i;

	spin_lock(&dbg->profile_lock);

	dbg->profile_started = false;
	dbg->on.min = U64_MAX;
	dbg->on.avg = 0;
	dbg->on.max = 0;
	dbg->off.min = U64_MAX;
	dbg->off.avg = 0;
	dbg->off.max = 0;
	if (dbg->block_id == BLK_SYSTEM) {
		dbg->on1->min = U64_MAX;
		dbg->on1->avg = 0;
		dbg->on1->max = 0;
		dbg->off1->min = U64_MAX;
		dbg->off1->avg = 0;
		dbg->off1->max = 0;
	}

	dbg->off_cnt = 0;

	if (dbg->aux) {
		for (i = 0; i < dbg->aux_size; i++) {
			dbg->aux[i].min = U64_MAX;
			dbg->aux[i].avg = 0;
			dbg->aux[i].max = 0;
		}
	}

	spin_unlock(&dbg->profile_lock);

	if (dbg->block_id == BLK_SYSTEM) {
		__raw_writel(0, latency_base + dbg->latency_offset);
		__raw_writel(0, latency_base + dbg->latency_offset + 0x4);
		__raw_writel(0, latency_base + dbg->latency_offset + 0x8);
		__raw_writel(0, latency_base + dbg->latency_offset + 0xc);
	} else {
		__raw_writel(0, latency_base + dbg->latency_offset);
		__raw_writel(0, latency_base + dbg->latency_offset + 0x4);
	}

	if (dbg->aux) {
		for (i = 0; i < dbg->aux_size; i++)
			__raw_writel(0, latency_base + dbg->aux_offset + 4 * i);
	}
}

static ssize_t pmucal_dbg_emul_read(struct file *file, char __user *user_buf,
				size_t count, loff_t *ppos)
{
	struct pmucal_dbg_info *data = file->private_data;
	char buf[80];
	ssize_t ret;

	ret = snprintf(buf, sizeof(buf), "emulation %s.\n", data->emul_en ? "enabled" : "disabled");
	if (ret < 0)
		return ret;

	return simple_read_from_buffer(user_buf, count, ppos, buf, ret);
}

static ssize_t pmucal_dbg_emul_write(struct file *file, const char __user *user_buf,
				size_t count, loff_t *ppos)
{
	struct pmucal_dbg_info *data = file->private_data;
	char buf[32];
	ssize_t len;

	len = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (len < 0)
		return len;

	buf[len] = '\0';

	switch (buf[0]) {
	case '0':
		pmucal_dbg_req_emulation(data, false);
		break;
	case '1':
		pmucal_dbg_req_emulation(data, true);
		break;
	default:
		pr_err("%s %s: Invalid input ['0'|'1']\n", PMUCAL_PREFIX, __func__);
		return -EINVAL;
	}

	return len;
}

static ssize_t pmucal_dbg_show_profile(char *buf, ssize_t ret, struct pmucal_dbg_info *dbg)
{
	int i;
	ssize_t len, orig_ret = ret;

	spin_lock(&dbg->profile_lock);

	len = snprintf(buf + ret, PAGE_SIZE*2 - ret,
			"count = %llu\n", dbg->off_cnt);
	if (len > 0)
		ret += len;

	len = snprintf(buf + ret, PAGE_SIZE*2 - ret,
			"on latency = %llu / %llu / %llu [nsec]\n",
			dbg->on.min, dbg->on.avg, dbg->on.max);
	if (len > 0)
		ret += len;

	len = snprintf(buf + ret, PAGE_SIZE*2 - ret,
			"off latency = %llu / %llu / %llu [nsec]\n",
			dbg->off.min, dbg->off.avg, dbg->off.max);
	if (len > 0)
		ret += len;

	if (dbg->block_id == BLK_SYSTEM) {
		len = snprintf(buf + ret, PAGE_SIZE*2 - ret,
				"on1(mif) latency = %llu / %llu / %llu [nsec]\n",
				dbg->on1->min, dbg->on1->avg, dbg->on1->max);
		if (len > 0)
			ret += len;

		len = snprintf(buf + ret, PAGE_SIZE*2 - ret,
				"off1(mif) latency = %llu / %llu / %llu [nsec]\n",
				dbg->off1->min, dbg->off1->avg, dbg->off1->max);
		if (len > 0)
			ret += len;
	}

	if (dbg->aux) {
		for (i = 0; i < dbg->aux_size; i++) {
			len = snprintf(buf + ret, PAGE_SIZE*2 - ret,
					"\taux[%d] latency = %llu / %llu / %llu [nsec]\n",
					i, dbg->aux[i].min, dbg->aux[i].avg, dbg->aux[i].max);
			if (len > 0)
				ret += len;
		}
	}

	spin_unlock(&dbg->profile_lock);

	return (ret - orig_ret);
}

static ssize_t pmucal_dbg_profile_read(struct file *file, char __user *user_buf,
				size_t count, loff_t *ppos)
{
	char *buf = kmalloc(PAGE_SIZE*2, GFP_KERNEL);
	ssize_t len, ret = 0;
	int i;

	if (!buf)
		return -ENOMEM;

	if (profile_en)
		goto skip_read;

	len = snprintf(buf + ret, PAGE_SIZE*2 - ret, "####################################\n");
	if (len > 0)
		ret += len;

	len = snprintf(buf + ret, PAGE_SIZE*2 - ret, "######CPU######\n");
	if (len > 0)
		ret += len;
	for (i = 0; i < pmucal_cpu_list_size; i++) {
		if (cpu_list[i].off_cnt == 0)
			continue;
		len = snprintf(buf + ret, PAGE_SIZE*2 - ret, "<Core%d>\n", pmucal_cpu_list[i].id);
		if (len > 0)
			ret += len;
		len = pmucal_dbg_show_profile(buf, ret, &cpu_list[i]);
		if (len > 0)
			ret += len;
	}

	len = snprintf(buf + ret, PAGE_SIZE*2 - ret, "######CLUSTER######\n");
	if (len > 0)
		ret += len;
	for (i = 0; i < pmucal_cluster_list_size; i++) {
		if (cluster_list[i].off_cnt == 0)
			continue;
		len = snprintf(buf + ret, PAGE_SIZE*2 - ret, "<Cluster%d>\n", pmucal_cluster_list[i].id);
		if (len > 0)
			ret += len;
		len = pmucal_dbg_show_profile(buf, ret, &cluster_list[i]);
		if (len > 0)
			ret += len;
	}

	len = snprintf(buf + ret, PAGE_SIZE*2 - ret, "######System power mode######\n");
	if (len > 0)
		ret += len;
	for (i = 0; i < pmucal_lpm_list_size; i++) {
		if (system_list[i].off_cnt == 0)
			continue;
		len = snprintf(buf + ret, PAGE_SIZE*2 - ret, "<Powermode%d>\n", pmucal_lpm_list[i].id);
		if (len > 0)
			ret += len;
		len = pmucal_dbg_show_profile(buf, ret, &system_list[i]);
		if (len > 0)
			ret += len;
	}

	len = snprintf(buf + ret, PAGE_SIZE*2 - ret, "######Power Domain######\n");
	if (len > 0)
		ret += len;
	for (i = 0; i < pmucal_pd_list_size; i++) {
		if (local_list[i].off_cnt == 0)
			continue;
		len = snprintf(buf + ret, PAGE_SIZE*2 - ret, "<%s>\n", pmucal_pd_list[i].name);
		if (len > 0)
			ret += len;
		len = pmucal_dbg_show_profile(buf, ret, &local_list[i]);
		if (len > 0)
			ret += len;
	}

	len = snprintf(buf + ret, PAGE_SIZE*2 - ret, "####################################\n");
	if (len > 0)
		ret += len;

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, ret);

skip_read:
	kfree(buf);
	return ret;
}

static ssize_t pmucal_dbg_profile_write(struct file *file, const char __user *user_buf,
				size_t count, loff_t *ppos)
{
	char buf[32];
	ssize_t len;
	int i = 0;

	len = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (len < 0)
		return len;

	buf[len] = '\0';

	switch (buf[0]) {
	case '0':
		exynos_pmu_update(profile_en_offset,
				(1 << profile_en_bit),
				(0 << profile_en_bit));
		profile_en = 0;
		break;
	case '1':
		for (i = 0; i < pmucal_cpu_list_size; i++)
			pmucal_dbg_clear_profile(&cpu_list[i]);
		for (i = 0; i < pmucal_cluster_list_size; i++)
			pmucal_dbg_clear_profile(&cluster_list[i]);
		for (i = 0; i < pmucal_pd_list_size; i++)
			pmucal_dbg_clear_profile(&local_list[i]);
		for (i = 0; i < pmucal_lpm_list_size; i++)
			pmucal_dbg_clear_profile(&system_list[i]);

		exynos_pmu_update(profile_en_offset,
				(1 << profile_en_bit),
				(1 << profile_en_bit));
		profile_en = 1;
		break;
	default:
		return -EINVAL;
	}

	return len;
}

static const struct file_operations pmucal_dbg_emul_fops = {
	.open = simple_open,
	.read = pmucal_dbg_emul_read,
	.write = pmucal_dbg_emul_write,
	.llseek = default_llseek,
};

static const struct file_operations pmucal_dbg_profile_fops = {
	.open = simple_open,
	.read = pmucal_dbg_profile_read,
	.write = pmucal_dbg_profile_write,
	.llseek = default_llseek,
};

static int pmucal_dbg_blk_init(struct device_node *node, u32 block_id,
				u32 idx, struct pmucal_dbg_info *dbg_info)
{
	int ret;

	if (!node || !dbg_info)
		return -ENODEV;

	switch (block_id) {
		case BLK_CPU:
			dbg_info->pmucal_data = &pmucal_cpu_list[idx];
			pmucal_cpu_list[idx].dbg = dbg_info;
			break;
		case BLK_CLUSTER:
			dbg_info->pmucal_data = &pmucal_cluster_list[idx];
			pmucal_cluster_list[idx].dbg = dbg_info;
			break;
		case BLK_SYSTEM:
			dbg_info->pmucal_data = &pmucal_lpm_list[idx];
			pmucal_lpm_list[idx].dbg = dbg_info;
			break;
		case BLK_LOCAL:
			dbg_info->pmucal_data = &pmucal_pd_list[idx];
			pmucal_pd_list[idx].dbg = dbg_info;
			break;
		default:
			return -EINVAL;
	}
	dbg_info->block_id = block_id;

	ret = of_property_read_u32(node, "emul_offset", &dbg_info->emul_offset);
	if (ret)
		return ret;
	ret = of_property_read_u32(node, "emul_bit", &dbg_info->emul_bit);
	if (ret)
		return ret;
	ret = of_property_read_u32(node, "emul_en", &dbg_info->emul_en);
	if (ret)
		return ret;
	dbg_info->emul_enabled = 0;

	ret = of_property_read_u32(node, "latency_offset", &dbg_info->latency_offset);
	if (ret)
		return ret;

	of_property_read_u32(node, "aux_offset", &dbg_info->aux_offset);
	ret = of_property_read_u32(node, "aux_size", &dbg_info->aux_size);
	if (!ret)
		dbg_info->aux = kzalloc(sizeof(struct pmucal_latency) * dbg_info->aux_size, GFP_KERNEL);

	spin_lock_init(&dbg_info->profile_lock);

	pmucal_dbg_clear_profile(dbg_info);

	return 0;
}

int pmucal_dbg_init(void)
{
	struct device_node *node = NULL;
	int ret;
	u32 prop1, prop2, i, f_line;
	pmucal_dbg_init_ok = false;

	node = of_find_node_by_name(NULL, "pmucal_dbg");
	if (!node) {
		f_line = __LINE__;
		goto err_ret;
	}

	ret = of_property_read_u32(node, "latency_base", &prop1);
	if (ret) {
		f_line = __LINE__;
		goto err_ret;
	}

	ret = of_property_read_u32(node, "latency_size", &prop2);
	if (ret) {
		f_line = __LINE__;
		goto err_ret;
	}

	latency_base = ioremap(prop1, prop2);
	if (latency_base == NULL) {
		f_line = __LINE__;
		goto err_ret;
	}

	ret = of_property_read_u32(node, "profile_en", &profile_en);
	if (ret) {
		f_line = __LINE__;
		goto err_ret;
	}

	ret = of_property_read_u32(node, "profile_en_offset", &profile_en_offset);
	if (ret) {
		f_line = __LINE__;
		goto err_ret;
	}

	ret = of_property_read_u32(node, "profile_en_bit", &profile_en_bit);
	if (ret) {
		f_line = __LINE__;
		goto err_ret;
	}

	ret = of_property_read_u32(node, "mct_base", &prop1);
	if (ret) {
		f_line = __LINE__;
		goto err_ret;
	}

	mct_base = ioremap(prop1, 0x10);
	if (mct_base == NULL) {
		f_line = __LINE__;
		goto err_ret;
	}

	/* CPU */
	cpu_list = kzalloc(sizeof(struct pmucal_dbg_info) * pmucal_cpu_list_size, GFP_KERNEL);
	if (!cpu_list) {
		f_line = __LINE__;
		goto err_ret;
	}
	while((node = of_find_node_by_type(node, "pmucal_dbg_cpu"))) {
		ret = of_property_read_u32(node, "id", &i);
		if (ret)
			continue;

		ret = pmucal_dbg_blk_init(node, BLK_CPU, i, &cpu_list[i]);
		if (ret) {
			pr_err("%s %s: pmucal_dbg_blk_init failed. (blk: %d, id: %d)\n",
					PMUCAL_PREFIX, __func__, BLK_CPU, i);
			f_line = __LINE__;
			goto err_ret;
		}
	}

	/* Cluster */
	cluster_list = kzalloc(sizeof(struct pmucal_dbg_info) * pmucal_cluster_list_size, GFP_KERNEL);
	if (!cluster_list) {
		f_line = __LINE__;
		goto err_ret;
	}
	while((node = of_find_node_by_type(node, "pmucal_dbg_cluster"))) {
		ret = of_property_read_u32(node, "id", &i);
		if (ret)
			continue;

		ret = pmucal_dbg_blk_init(node, BLK_CLUSTER, i, &cluster_list[i]);
		if (ret) {
			pr_err("%s %s: pmucal_dbg_blk_init failed. (blk: %d, id: %d)\n",
					PMUCAL_PREFIX, __func__, BLK_CLUSTER, i);
			f_line = __LINE__;
			goto err_ret;
		}
	}

	/* System */
	system_list = kzalloc(sizeof(struct pmucal_dbg_info) * pmucal_lpm_list_size, GFP_KERNEL);
	if (!system_list) {
		f_line = __LINE__;
		goto err_ret;
	}
	while((node = of_find_node_by_type(node, "pmucal_dbg_system"))) {
		ret = of_property_read_u32(node, "id", &i);
		if (ret)
			continue;

		system_list[i].on1 = kzalloc(sizeof(struct pmucal_latency), GFP_KERNEL);
		if (!system_list[i].on1) {
			f_line = __LINE__;
			goto err_ret;
		}
		system_list[i].off1 = kzalloc(sizeof(struct pmucal_latency), GFP_KERNEL);
		if (!system_list[i].off1) {
			f_line = __LINE__;
			goto err_ret;
		}

		ret = pmucal_dbg_blk_init(node, BLK_SYSTEM, i, &system_list[i]);
		if (ret) {
			pr_err("%s %s: pmucal_dbg_blk_init failed. (blk: %d, id: %d)\n",
					PMUCAL_PREFIX, __func__, BLK_SYSTEM, i);
			f_line = __LINE__;
			goto err_ret;
		}
	}

	/* Local */
	local_list = kzalloc(sizeof(struct pmucal_dbg_info) * pmucal_pd_list_size, GFP_KERNEL);
	if (!local_list) {
		f_line = __LINE__;
		goto err_ret;
	}
	while((node = of_find_node_by_type(node, "pmucal_dbg_local"))) {
		ret = of_property_read_u32(node, "id", &i);
		if (ret)
			continue;

		ret = pmucal_dbg_blk_init(node, BLK_LOCAL, i, &local_list[i]);
		if (ret) {
			pr_err("%s %s: pmucal_dbg_blk_init failed. (blk: %d, id: %d)\n",
					PMUCAL_PREFIX, __func__, BLK_LOCAL, i);
			f_line = __LINE__;
			goto err_ret;
		}
	}

	pmucal_dbg_init_ok = true;

	return 0;

err_ret:
	pr_err("%s %s: line %d failed.\n", PMUCAL_PREFIX, __func__, f_line);

	return 0;
}

int pmucal_dbg_debugfs_init(void)
{
	int i = 0;
	struct dentry *dentry;
	struct device_node *node = NULL;
	const char *buf;

	if (!pmucal_dbg_init_ok)
		return 0;

	/* Common */
	root = debugfs_create_dir("pmucal-dbg", NULL);
	if (!root) {
		pr_err("%s %s: could not create debugfs dir\n",
				PMUCAL_PREFIX, __func__);
		return 0;
	}

	dentry = debugfs_create_file("profile_en", 0644, root,
					NULL, &pmucal_dbg_profile_fops);
	if (!dentry) {
		pr_err("%s %s: could not create profile_en file\n",
				PMUCAL_PREFIX, __func__);
		return 0;
	}

	if (profile_en) {
		exynos_pmu_update(profile_en_offset,
				(1 << profile_en_bit),
				(1 << profile_en_bit));
	}

	/* CPU */
	while((node = of_find_node_by_type(node, "pmucal_dbg_cpu"))) {
		of_property_read_u32(node, "id", &i);
		of_property_read_string(node, "name", &buf);
		dentry = debugfs_create_dir(buf, root);
		debugfs_create_file("emul_en", 0644, dentry, &cpu_list[i], &pmucal_dbg_emul_fops);
	}

	/* Cluster */
	while((node = of_find_node_by_type(node, "pmucal_dbg_cluster"))) {
		of_property_read_u32(node, "id", &i);
		of_property_read_string(node, "name", &buf);
		dentry = debugfs_create_dir(buf, root);
		debugfs_create_file("emul_en", 0644, dentry, &cluster_list[i], &pmucal_dbg_emul_fops);
	}

	/* System power modes */
	while((node = of_find_node_by_type(node, "pmucal_dbg_system"))) {
		of_property_read_u32(node, "id", &i);
		of_property_read_string(node, "name", &buf);
		dentry = debugfs_create_dir(buf, root);
		debugfs_create_file("emul_en", 0644, dentry, &system_list[i], &pmucal_dbg_emul_fops);
	}

	/* Local power domains */
	while((node = of_find_node_by_type(node, "pmucal_dbg_local"))) {
		of_property_read_u32(node, "id", &i);
		of_property_read_string(node, "name", &buf);
		dentry = debugfs_create_dir(buf, root);
		debugfs_create_file("emul_en", 0644, dentry, &local_list[i], &pmucal_dbg_emul_fops);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(pmucal_dbg_debugfs_init);

MODULE_LICENSE("GPL");
