/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/of_reserved_mem.h>
#include <linux/io.h>
#include <linux/devfreq.h>
#include <soc/samsung/debug-snapshot.h>
#include <linux/sched/clock.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>

#include <soc/samsung/acpm_ipc_ctrl.h>
#include <soc/samsung/exynos-sci.h>
#if defined(CONFIG_EXYNOS_SCI_DBG) || defined(CONFIG_EXYNOS_SCI_DBG_MODULE)
#include <soc/samsung/exynos-sci_dbg.h>
#endif
static struct exynos_llc_dump_addr llc_reserved;
static struct exynos_sci_data *sci_data;
static int exynos_llc_enable;
static ktime_t llc_run_time;

#ifdef CONFIG_LOCKUP_DETECTOR_OTHER_CPU
static struct atomic_notifier_head hardlockup_notifier_list;
#endif

struct sci_handler {
	unsigned int	irq;
	char		name[16];
	void		*handler;
};

static void print_sci_data(struct exynos_sci_data *data)
{
	SCI_DBG("IPC Channel Number: %u\n", data->ipc_ch_num);
	SCI_DBG("IPC Channel Size: %u\n", data->ipc_ch_size);
	SCI_DBG("Use Initial LLC Region: %s\n",
			data->use_init_llc_region ? "True" : "False");
	SCI_DBG("Initial LLC Region: %s (%u)\n",
		data->region_name[data->initial_llc_region],
		data->initial_llc_region);
	SCI_DBG("LLC Enable: %s\n",
			data->llc_enable ? "True" : "False");
	SCI_DBG("CPU minimum region: %u\n", data->cpu_min_region);
}

static void exynos_ecc_int_en(void);

static irqreturn_t exynos_sci_handler(int irq, void *data)
{
	llc_ecc_logging();

	pr_err("SCI uncorrectable error (irqnum: %d)\n", irq);
	disable_irq_nosync(irq);
	dbg_snapshot_expire_watchdog();

	return IRQ_HANDLED;
}

static void set_llc_gov_en(int enable)
{
	sci_data->gov_data->llc_gov_en = enable;
}

#ifdef CONFIG_OF
static int exynos_sci_parse_dt(struct device_node *np,
				struct exynos_sci_data *data)
{
	struct sci_handler *sci_h;
	int ret, i;
	int size;
	unsigned int priority;

	if (!np)
		return -ENODEV;

	/* ECC irq request */
	sci_h = kzalloc(sizeof(struct sci_handler), GFP_KERNEL);

	sci_h->irq = irq_of_parse_and_map(np, 0);
	sci_h->handler = (void *)exynos_sci_handler;
	strcpy(sci_h->name, "sci_h");

	devm_request_irq(sci_data->dev, sci_h->irq, sci_h->handler,
#ifdef MULTI_IRQ_SUPPORT_ITMON
			IRQF_NOBALANCING | IRQF_GIC_MULTI_TARGET,
#else
			IRQF_NOBALANCING,
#endif
			sci_h->name, NULL);

	ret = of_property_read_u32(np, "use_init_llc_region",
					&data->use_init_llc_region);
	if (ret) {
		SCI_ERR("%s: Failed get initial_llc_region\n", __func__);
		return ret;
	}

	if (data->use_init_llc_region) {
		ret = of_property_read_u32(np, "initial_llc_region",
					&data->initial_llc_region);
		if (ret) {
			SCI_ERR("%s: Failed get initial_llc_region\n", __func__);
			return ret;
		}
	}

	ret = of_property_read_u32(np, "llc_enable",
					&data->llc_enable);
	if (ret) {
		SCI_ERR("%s: Failed get llc_enable\n", __func__);
		return ret;
	}

	/* retention */
	ret = of_property_read_u32(np, "ret_enable",
					&data->ret_enable);
	if (ret) {
		SCI_ERR("%s: Failed get ret_enable\n", __func__);
		return ret;
	}

	size = of_property_count_strings(np, "region_name");
	if (size < 0) {
		SCI_ERR("%s: Failed get number of region_name\n", __func__);
		return size;
	}

	size = of_property_read_string_array(np, "region_name", data->region_name, size);
	if (size < 0) {
		SCI_ERR("%s: Failed get region_name\n", __func__);
		return size;
	}

	size = of_property_count_u32_elems(np, "region_priority");
	if (size < 0) {
		SCI_ERR("%s: Failed get number of region_priority\n", __func__);
		return size;
	}

	for (i = 0; i < size; i++){
		ret = of_property_read_u32_index(np, "region_priority", i, &priority);
		if (ret) {
			SCI_ERR("%s: Failed get region_priority(index:%d)\n",
					__func__, i);
			return ret;
		}
		data->region_priority[i] = priority;
	}

	ret = of_property_read_u32(np, "cpu_min_region",
					&data->cpu_min_region);
	if (ret) {
		SCI_ERR("%s: Failed get cpu_min_region\n", __func__);
		return ret;
	}

	ret = of_property_read_u32(np, "llc_gov_en",
					&data->gov_data->llc_gov_en);
	if (ret) {
		SCI_ERR("%s: Failed get llc_gov_en\n", __func__);
		return ret;
	}

	ret = of_property_read_u32(np, "hfreq_rate",
					&data->gov_data->hfreq_rate);
	if (ret) {
		SCI_ERR("%s: Failed get hfreq_rate\n", __func__);
		return ret;
	}

	ret = of_property_read_u32(np, "on_time_th",
					&data->gov_data->on_time_th);
	if (ret) {
		SCI_ERR("%s: Failed get on_time_th\n", __func__);
		return ret;
	}

	ret = of_property_read_u32(np, "off_time_th",
					&data->gov_data->off_time_th);
	if (ret) {
		SCI_ERR("%s: Failed get off_time_th\n", __func__);
		return ret;
	}

	ret = of_property_read_u32(np, "freq_th",
					&data->gov_data->freq_th);
	if (ret) {
		SCI_ERR("%s: Failed get cpu_min_region\n", __func__);
		return ret;
	}

	return 0;
}
#else
static inline
int exynos_sci_parse_dt(struct device_node *np,
				struct exynos_sci_data *data)
{
	return -ENODEV;
}
#endif

static enum exynos_sci_err_code exynos_sci_ipc_err_handle(unsigned int cmd)
{
	enum exynos_sci_err_code err_code;

	err_code = SCI_CMD_GET(cmd, SCI_ERR_MASK, SCI_ERR_SHIFT);
	if (err_code)
		SCI_ERR("%s: SCI IPC error return(%u)\n", __func__, err_code);

	return err_code;
}

static int __exynos_sci_ipc_send_data(enum exynos_sci_cmd_index cmd_index,
				struct exynos_sci_data *data,
				unsigned int *cmd)
{
#if defined(CONFIG_EXYNOS_ACPM) || defined(CONFIG_EXYNOS_ACPM_MODULE)
	struct ipc_config config;
	unsigned int *sci_cmd;
#endif
	int ret = 0;

	if (cmd_index >= SCI_CMD_MAX) {
		SCI_ERR("%s: Invalid CMD Index: %u\n", __func__, cmd_index);
		ret = -EINVAL;
		goto out;
	}

#if defined(CONFIG_EXYNOS_ACPM) || defined(CONFIG_EXYNOS_ACPM_MODULE)
	sci_cmd = cmd;
	config.cmd = sci_cmd;
	config.response = true;
	config.indirection = false;

	ret = acpm_ipc_send_data(data->ipc_ch_num, &config);
	if (ret) {
		SCI_ERR("%s: Failed to send IPC(%d:%u) data\n",
			__func__, cmd_index, data->ipc_ch_num);
		goto out;
	}
#endif

out:
	return ret;
}

static int exynos_sci_ipc_send_data(enum exynos_sci_cmd_index cmd_index,
				struct exynos_sci_data *data,
				unsigned int *cmd)
{
	int ret;

	ret = __exynos_sci_ipc_send_data(cmd_index, data, cmd);

	return ret;
}

static void exynos_sci_base_cmd(struct exynos_sci_cmd_info *cmd_info,
					unsigned int *cmd)
{
	cmd[0] |= SCI_CMD_SET(cmd_info->cmd_index,
				SCI_CMD_IDX_MASK, SCI_CMD_IDX_SHIFT);
	cmd[0] |= SCI_CMD_SET(cmd_info->direction,
				SCI_ONE_BIT_MASK, SCI_IPC_DIR_SHIFT);
	cmd[0] |= SCI_CMD_SET(cmd_info->data, SCI_DATA_MASK, SCI_DATA_SHIFT);
}

static int exynos_sci_llc_invalidate(struct exynos_sci_data *data)
{
	struct exynos_sci_cmd_info cmd_info;
	unsigned int cmd[4] = {0, 0, 0, 0};
	int ret = 0;
	int tmp_reg;
	enum exynos_sci_err_code ipc_err;

	if (data->llc_region_prio[LLC_REGION_DISABLE])
		goto out;

	cmd_info.cmd_index = SCI_LLC_INVAL;
	cmd_info.direction = 0;
	cmd_info.data = 0;
	cmd[2] = data->invway;

	exynos_sci_base_cmd(&cmd_info, cmd);

	/* send command for SCI */
	ret = exynos_sci_ipc_send_data(cmd_info.cmd_index, data, cmd);
	if (ret) {
		SCI_ERR("%s: Failed send data\n", __func__);
		goto out;
	}

	ipc_err = exynos_sci_ipc_err_handle(cmd[1]);
	if (ipc_err) {
		ret = -EBADMSG;
		goto out;
	}

	/* llc_invalidate_wait */
	do {
		/* 0x1A000A0C */
		tmp_reg = __raw_readl(data->sci_base + SCI_SB_LLCSTATUS);
	} while (tmp_reg & (0x1 << 0));

out:
	return ret;
}

static int exynos_sci_llc_flush(struct exynos_sci_data *data,
		unsigned int region_index)
{
	struct exynos_sci_cmd_info cmd_info;
	unsigned int cmd[4] = {0, 0, 0, 0};
	int ret = 0;
	int tmp_reg = 0;
	enum exynos_sci_err_code ipc_err;

	if (data->llc_region_prio[LLC_REGION_DISABLE])
		goto out;

	cmd_info.cmd_index = SCI_LLC_FLUSH_PRE;
	cmd_info.direction = 0;
	cmd_info.data = region_index;
	/* cmd[2] only use sysfs(when region index is SYSFS_FLUSH_REGION_INDEX) */
	cmd[2] = data->invway;

	exynos_sci_base_cmd(&cmd_info, cmd);

	/* send command for SCI */
	ret = exynos_sci_ipc_send_data(cmd_info.cmd_index, data, cmd);
	if (ret) {
		SCI_ERR("%s: Failed send data\n", __func__);
		goto out;
	}

	ipc_err = exynos_sci_ipc_err_handle(cmd[1]);
	if (ipc_err) {
		ret = -EBADMSG;
		goto out;
	}

	/* llc_invalidate_wait */
	do {
		/* 0x1A000A0C */
		tmp_reg = __raw_readl(data->sci_base + SCI_SB_LLCSTATUS);
	} while (tmp_reg & (0x1 << 0));

	cmd_info.cmd_index = SCI_LLC_FLUSH_POST;
	cmd_info.direction = 0;
	cmd_info.data = 0;

	exynos_sci_base_cmd(&cmd_info, cmd);

	ret = exynos_sci_ipc_send_data(cmd_info.cmd_index, data, cmd);
	if (ret) {
		SCI_ERR("%s: Failed send data\n", __func__);
		goto out;
	}

	ipc_err = exynos_sci_ipc_err_handle(cmd[1]);
	if (ipc_err) {
		ret = -EBADMSG;
		goto out;
	}

	SCI_INFO("%s done[%d]\n", __func__, region_index);
out:
	return ret;
}

static int exynos_sci_llc_get_region_info(struct exynos_sci_data *data,
		unsigned int region_index, unsigned int *way)
{
	struct exynos_sci_cmd_info cmd_info;
	unsigned int cmd[4] = {0, 0, 0, 0};
	int ret = 0;
	enum exynos_sci_err_code ipc_err;

	if (data->llc_region_prio[LLC_REGION_DISABLE])
		goto out;

	cmd_info.cmd_index = SCI_LLC_GET_REGION_INFO;
	cmd_info.direction = SCI_IPC_GET;
	cmd_info.data = region_index;

	exynos_sci_base_cmd(&cmd_info, cmd);

	/* send command for SCI */
	ret = exynos_sci_ipc_send_data(cmd_info.cmd_index, data, cmd);
	if (ret) {
		SCI_ERR("%s: Failed send data\n", __func__);
		goto out;
	}

	ipc_err = exynos_sci_ipc_err_handle(cmd[1]);
	if (ipc_err) {
		ret = -EBADMSG;
		goto out;
	}

	*way = SCI_CMD_GET(cmd[1], SCI_DATA_MASK, SCI_DATA_SHIFT);

out:
	return ret;
}

static int exynos_sci_llc_region_alloc(struct exynos_sci_data *data,
					enum exynos_sci_ipc_dir direction,
					unsigned int *region_index, bool on,
					unsigned int way)
{
	struct exynos_sci_cmd_info cmd_info;
	unsigned int cmd[4] = {0, 0, 0, 0};
	int ret = 0;
	int index = 0;
	enum exynos_sci_err_code ipc_err;

	if (direction == SCI_IPC_SET) {
		if (*region_index >= LLC_REGION_MAX) {
			SCI_ERR("%s: Invalid Region Index: %u\n", __func__, *region_index);
			ret = -EINVAL;
			goto out;
		}

		if (*region_index > LLC_REGION_DISABLE) {
			if (on) {
				data->llc_region_prio[*region_index] = 1;
				index = SCI_LLC_REGION_ALLOC;
			} else {
				data->llc_region_prio[*region_index] = 0;
				index = SCI_LLC_REGION_DEALLOC;
			}
		}
	} else {
		index = SCI_LLC_REGION_ALLOC;
	}

	if (data->llc_region_prio[LLC_REGION_DISABLE])
		goto out;

	cmd_info.cmd_index = index;
	cmd_info.direction = direction;
	cmd_info.data = *region_index;
	cmd[2] = way;

	exynos_sci_base_cmd(&cmd_info, cmd);

	/* send command for SCI */
	ret = exynos_sci_ipc_send_data(cmd_info.cmd_index, data, cmd);
	if (ret) {
		SCI_ERR("%s: Failed send data\n", __func__);
		goto out;
	}

	ipc_err = exynos_sci_ipc_err_handle(cmd[1]);
	if (ipc_err) {
		ret = -EBADMSG;
		goto out;
	}

	if (direction == SCI_IPC_GET)
		*region_index = SCI_CMD_GET(cmd[1], SCI_DATA_MASK, SCI_DATA_SHIFT);

out:
	return ret;
}

static int exynos_sci_llc_region_priority(struct exynos_sci_data *data,
					enum exynos_sci_ipc_dir direction,
					unsigned int region_index,
					unsigned int *priority)
{
	struct exynos_sci_cmd_info cmd_info;
	unsigned int cmd[4] = {0, 0, 0, 0};
	int ret = 0;
	enum exynos_sci_err_code ipc_err;

	if (direction == SCI_IPC_SET) {
		if (region_index >= LLC_REGION_MAX) {
			SCI_ERR("%s: Invalid Region Index: %u\n", __func__, region_index);
			ret = -EINVAL;
			goto out;
		}
	}

	cmd_info.cmd_index = SCI_LLC_REGION_PRIORITY;
	cmd_info.direction = direction;
	cmd_info.data = region_index;
	cmd[2] = *priority;

	exynos_sci_base_cmd(&cmd_info, cmd);

	/* send command for SCI */
	ret = exynos_sci_ipc_send_data(cmd_info.cmd_index, data, cmd);
	if (ret) {
		SCI_ERR("%s: Failed send data\n", __func__);
		goto out;
	}

	ipc_err = exynos_sci_ipc_err_handle(cmd[1]);
	if (ipc_err) {
		ret = -EBADMSG;
		goto out;
	}

	if (direction == SCI_IPC_GET)
		*priority = SCI_CMD_GET(cmd[1], SCI_DATA_MASK, SCI_DATA_SHIFT);

out:
	return ret;
}

static int exynos_sci_ret_enable(struct exynos_sci_data *data,
					enum exynos_sci_ipc_dir direction,
					unsigned int *enable)
{
	struct exynos_sci_cmd_info cmd_info;
	unsigned int cmd[4] = {0, 0, 0, 0};
	int ret = 0;
	enum exynos_sci_err_code ipc_err;

	if (direction == SCI_IPC_SET) {
		if (*enable > 1) {
			SCI_ERR("%s: Invalid Control Index: %u\n", __func__, *enable);
			ret = -EINVAL;
			goto out;
		}
	}

	cmd_info.cmd_index = SCI_RET_EN;
	cmd_info.direction = direction;
	cmd_info.data = *enable;

	exynos_sci_base_cmd(&cmd_info, cmd);

	/* send command for SCI */
	ret = exynos_sci_ipc_send_data(cmd_info.cmd_index, data, cmd);
	if (ret) {
		SCI_ERR("%s: Failed send data\n", __func__);
		goto out;
	}

	ipc_err = exynos_sci_ipc_err_handle(cmd[1]);
	if (ipc_err) {
		ret = -EBADMSG;
		goto out;
	}

	if (direction == SCI_IPC_GET)
		*enable = SCI_CMD_GET(cmd[1], SCI_DATA_MASK, SCI_DATA_SHIFT);

out:
	return ret;
}

static int exynos_sci_cpu_min_region(struct exynos_sci_data *data,
					enum exynos_sci_ipc_dir direction,
					unsigned int *cpu_min_region)
{
	struct exynos_sci_cmd_info cmd_info;
	unsigned int cmd[4] = {0, 0, 0, 0};
	int ret = 0;
	enum exynos_sci_err_code ipc_err;

	cmd_info.cmd_index = SCI_LLC_CPU_MIN_REGION;
	cmd_info.direction = direction;
	cmd_info.data = *cpu_min_region;

	exynos_sci_base_cmd(&cmd_info, cmd);

	/* send command for SCI */
	ret = exynos_sci_ipc_send_data(cmd_info.cmd_index, data, cmd);
	if (ret) {
		SCI_ERR("%s: Failed send data\n", __func__);
		goto out;
	}

	ipc_err = exynos_sci_ipc_err_handle(cmd[1]);
	if (ipc_err) {
		ret = -EBADMSG;
		goto out;
	}

	if (direction == SCI_IPC_GET)
		*cpu_min_region = SCI_CMD_GET(cmd[1], SCI_DATA_MASK, SCI_DATA_SHIFT);

out:
	return ret;
}

static int exynos_sci_llc_enable(struct exynos_sci_data *data,
					enum exynos_sci_ipc_dir direction,
					unsigned int *enable)
{
	struct exynos_sci_cmd_info cmd_info;
	unsigned int cmd[4] = {0, 0, 0, 0};
	int ret = 0;
	enum exynos_sci_err_code ipc_err;

	if (direction == SCI_IPC_SET) {
		if (*enable) {
			sci_data->gov_data->en_cnt++;
		} else if (!*enable && sci_data->gov_data->en_cnt) {
			sci_data->gov_data->en_cnt--;
		} else {
			return 0;
		}

		/* en_cnt == 0(disable) or first enable */
		if (sci_data->gov_data->en_cnt > 1 ||
				(sci_data->gov_data->en_cnt == 1 && !*enable)) {
			return 0;
		}

		if (!sci_data->gov_data->en_cnt && sci_data->llc_on_flag) {
			sci_data->gov_data->en_cnt = 1;
			SCI_INFO("%s: cali en_cnt\n", __func__);
			return 0;
		}

		if (*enable > 1) {
			SCI_ERR("%s: Invalid Control Index: %u\n", __func__, *enable);
			ret = -EINVAL;
			goto out;
		}

		if (*enable)
			data->llc_region_prio[LLC_REGION_DISABLE] = 0;
		else
			data->llc_region_prio[LLC_REGION_DISABLE] = 1;
	}

	cmd_info.cmd_index = SCI_LLC_EN;
	cmd_info.direction = direction;
	cmd_info.data = *enable;

	exynos_sci_base_cmd(&cmd_info, cmd);

	/* send command for SCI */
	ret = exynos_sci_ipc_send_data(cmd_info.cmd_index, data, cmd);
	if (ret) {
		SCI_ERR("%s: Failed send data\n", __func__);
		goto out;
	}

	ipc_err = exynos_sci_ipc_err_handle(cmd[1]);
	if (ipc_err) {
		ret = -EBADMSG;
		goto out;
	}

	if (direction == SCI_IPC_GET)
		*enable = SCI_CMD_GET(cmd[1], SCI_DATA_MASK, SCI_DATA_SHIFT);


	if (direction == SCI_IPC_SET) {
		exynos_llc_enable = *enable;

		if (*enable) {
			llc_run_time = ktime_get();
		} else {
			llc_run_time = ktime_sub(ktime_get(), llc_run_time);
			sci_data->gov_data->enabled_time += llc_run_time;
		}

		SCI_INFO("%s: LLC is %s\n", __func__,
				exynos_llc_enable? "enabled" : "disabled");
	}
out:
	return ret;
}

/* Export Functions */
int llc_get_en(void)
{
	return exynos_llc_enable;
}
EXPORT_SYMBOL(llc_get_en);

int cam_llc_enable(bool on)
{
	int ret;
	unsigned int enable = on;

	sci_data->llc_on_flag = on;

	if (on) {
		set_exynos_sci_llc_debug_mode(0);
		set_llc_gov_en(0);
	} else {
		set_llc_gov_en(1);
	}

	ret = exynos_sci_llc_enable(sci_data, SCI_IPC_SET, &enable);
	if (ret) {
		SCI_ERR("%s: Failed llc enable control\n", __func__);
		return ret;
	}


	return 0;
}
EXPORT_SYMBOL(cam_llc_enable);

int llc_off_disable(bool off)
{
	unsigned long flags;
	int enable = !off;
	int ret;

	if (sci_data->llc_on_flag)
		return 0;

	set_exynos_sci_llc_debug_mode(off);
	set_llc_gov_en(!off);

	spin_lock_irqsave(&sci_data->lock, flags);
	if (off && exynos_llc_enable) {
		sci_data->gov_data->en_cnt = off;

		ret = exynos_sci_llc_enable(sci_data, SCI_IPC_SET, &enable);
		if (ret) {
			SCI_ERR("%s: Failed llc enable control\n", __func__);
			spin_unlock_irqrestore(&sci_data->lock, flags);
			return ret;
		}
	}
	spin_unlock_irqrestore(&sci_data->lock, flags);

	SCI_INFO("%s: off: %d\n", __func__, off);
	return 0;
}
EXPORT_SYMBOL(llc_off_disable);

int llc_enable(bool on)
{
	int ret;
	unsigned int enable = on;

#if defined(CONFIG_EXYNOS_SCI_DBG) || defined(CONFIG_EXYNOS_SCI_DBG_MODULE)
	bool debug_mode = get_exynos_sci_llc_debug_mode();
	if (debug_mode)
		return 0;
#endif

	ret = exynos_sci_llc_enable(sci_data, SCI_IPC_SET, &enable);
	if (ret) {
		SCI_ERR("%s: Failed llc enable control\n", __func__);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(llc_enable);

void llc_invalidate(unsigned int invway)
{
	int ret;

	if (!exynos_llc_enable)
		return;

	sci_data->invway = invway;
	ret = exynos_sci_llc_invalidate(sci_data);
	if (ret)
		SCI_ERR("%s: Failed llc invalidate\n", __func__);

	return;
}
EXPORT_SYMBOL(llc_invalidate);

void llc_flush(unsigned int region)
{
	int ret;

	if (!exynos_llc_enable)
		return;

	if (region >= LLC_REGION_MAX || !sci_data->llc_region_prio[region])
		return;

	ret = exynos_sci_llc_flush(sci_data, region);
	if (ret)
		SCI_ERR("%s: Failed llc flush\n", __func__);

	return;
}
EXPORT_SYMBOL(llc_flush);

unsigned int llc_get_region_info(unsigned int region_index)
{
	int ret;
	unsigned int way;

	if (!exynos_llc_enable)
		return 0;

	if (region_index >= LLC_REGION_MAX || !sci_data->llc_region_prio[region_index])
		return 0;

	ret = exynos_sci_llc_get_region_info(sci_data, region_index, &way);
	if (ret)
		SCI_ERR("%s: Failed get llc region info\n", __func__);

	return way;
}
EXPORT_SYMBOL(llc_get_region_info);

unsigned int llc_region_alloc(unsigned int region_index, bool on, unsigned int way)
{
	int ret;
	int enable = on;
	unsigned long flags;

#if defined(CONFIG_EXYNOS_SCI_DBG) || defined(CONFIG_EXYNOS_SCI_DBG_MODULE)
	bool debug_mode = get_exynos_sci_llc_debug_mode();
	if (debug_mode)
		return 0;
#endif
	if (region_index == LLC_REGION_DPU)
		return 0;

	spin_lock_irqsave(&sci_data->lock, flags);
	if (enable)
		exynos_sci_llc_enable(sci_data, SCI_IPC_SET, &enable);

	if (!exynos_llc_enable) {
		spin_unlock_irqrestore(&sci_data->lock, flags);
		return 0;
	}

	if (region_index > LLC_REGION_CPU &&
			way + sci_data->cpu_min_region > FULL_WAY_NUM) {
		SCI_INFO("%s: Available num way is %u\n", __func__,
				FULL_WAY_NUM - sci_data->cpu_min_region);
		spin_unlock_irqrestore(&sci_data->lock, flags);
		return way + sci_data->cpu_min_region - FULL_WAY_NUM;
	}

	ret = exynos_sci_llc_region_alloc(sci_data, SCI_IPC_SET,
			&region_index, on, way);
	if (ret)
		SCI_ERR("%s: Failed llc region allocate\n", __func__);

	if (!enable)
		exynos_sci_llc_enable(sci_data, SCI_IPC_SET, &enable);

	spin_unlock_irqrestore(&sci_data->lock, flags);
	SCI_INFO("%s: region[%d]: %s\n", __func__, region_index, on ? "on" : "off");

	return 0;
}
EXPORT_SYMBOL(llc_region_alloc);

static void print_register(int offset, int ecc, int shift)
{
	SCI_ERR("offset: 0x%x, value: 0x%x\n", offset, ecc);
}

static void get_llcecc_info(int offset, int shift)
{
	int reg;

	reg = __raw_readl(sci_data->sci_base + offset);
	print_register(offset, reg, shift);
}

void llc_ecc_logging(void)
{
	unsigned long flags;

	spin_lock_irqsave(&sci_data->lock, flags);

	get_llcecc_info(UcErrMiscInfo, UEMI_SHIFT);
	get_llcecc_info(UcErrOverrunMiscInfo, UEOMI_SHIFT);
	get_llcecc_info(CorrErrSource0, 0);
	get_llcecc_info(CorrErrSource1, 0);
	get_llcecc_info(CorrErrMiscInfo, 0);
	get_llcecc_info(CorrErrAddrLow, 0);
	get_llcecc_info(CorrErrAddrHigh, 0);
	get_llcecc_info(CorrErrOverrunSource0, 0);
	get_llcecc_info(CorrErrOverrunSource1, 0);
	get_llcecc_info(CorrErrOverrunMiscInfo, 0);

	spin_unlock_irqrestore(&sci_data->lock, flags);
}
EXPORT_SYMBOL(llc_ecc_logging);

/* LLC governor */
static int sci_freq_get_handler(struct notifier_block *nb, unsigned long event,
		void *buf)
{
	struct devfreq_freqs *freqs = (struct devfreq_freqs *)buf;
	unsigned int freq_new = freqs->new;
	unsigned int freq_old = freqs->old;
	unsigned int freq_th = sci_data->gov_data->freq_th;
	unsigned int hfreq_rate = sci_data->gov_data->hfreq_rate;
	unsigned int on_time_th = sci_data->gov_data->on_time_th;
	unsigned int off_time_th = sci_data->gov_data->off_time_th;
	u64 remain_time, active_rate;
	u64 now;

	if (!sci_data->gov_data->llc_gov_en)
		goto done;

	now = sched_clock();
	if (event == DEVFREQ_POSTCHANGE) {
		if (sci_data->gov_data->start_time) {
			if (freq_old >= freq_th && sci_data->gov_data->last_time)
				sci_data->gov_data->high_time
					+= now - sci_data->gov_data->last_time;
		}

		/* calc time */
		if (freq_new >= freq_th) {
			if (!sci_data->gov_data->start_time)
				sci_data->gov_data->start_time = now;

			sci_data->gov_data->last_time = now;
		} else {
			sci_data->gov_data->last_time = 0;
		}

		remain_time = now - sci_data->gov_data->start_time;
		active_rate = sci_data->gov_data->high_time * 100 / remain_time;

		if (!sci_data->gov_data->start_time)
			goto done;

		if (sci_data->gov_data->llc_req_flag &&
				active_rate > hfreq_rate) {
			sci_data->gov_data->start_time = now;
			sci_data->gov_data->high_time = 0;
			goto done;
		}

		if (remain_time > on_time_th &&	!sci_data->gov_data->llc_req_flag) {
			if (active_rate > hfreq_rate) {
				llc_region_alloc(LLC_REGION_CPU, 1, FULL_WAY_NUM);
				sci_data->gov_data->llc_req_flag = 1;
			}
			sci_data->gov_data->start_time = now;
			sci_data->gov_data->high_time = 0;
		} else if (remain_time > off_time_th &&	sci_data->gov_data->llc_req_flag) {
			if (active_rate <= hfreq_rate) {
				llc_region_alloc(LLC_REGION_CPU, 0, 0);
				sci_data->gov_data->llc_req_flag = 0;
			}

			sci_data->gov_data->start_time = now;
			sci_data->gov_data->high_time = 0;
		}
	}
done:
	return 0;
}

static struct notifier_block nb_sci_freq_get = {
	.notifier_call = sci_freq_get_handler,
	.priority = INT_MAX,
};

static void exynos_sci_get_noti(struct work_struct *work)
{
	struct devfreq *devfreq;
	int ret;

	devfreq = devfreq_get_devfreq_by_phandle(sci_data->dev, 0);
	if (IS_ERR(devfreq)) {
		ret = -EPROBE_DEFER;
		SCI_INFO("%s: failed to get phandle!!\n", __func__);
		schedule_delayed_work(&sci_data->gov_data->get_noti_work,
			msecs_to_jiffies(10000));
	} else {
		ret = devm_devfreq_register_notifier(sci_data->dev,
				devfreq, &nb_sci_freq_get,
				DEVFREQ_TRANSITION_NOTIFIER);
		SCI_INFO("%s: success get phandle!!\n", __func__);
	}
}

/* SYSFS Interface */
static ssize_t show_sci_data(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	int i;

	count += snprintf(buf + count, PAGE_SIZE, "IPC Channel Number: %u\n",
				data->ipc_ch_num);
	count += snprintf(buf + count, PAGE_SIZE, "IPC Channel Size: %u\n",
				data->ipc_ch_size);
	count += snprintf(buf + count, PAGE_SIZE, "Use Initial LLC Region: %s\n",
				data->use_init_llc_region ? "True" : "False");
	count += snprintf(buf + count, PAGE_SIZE, "Initial LLC Region: %s (%u)\n",
				data->region_name[data->initial_llc_region],
				data->initial_llc_region);
	count += snprintf(buf + count, PAGE_SIZE, "LLC Enable: %s\n",
				data->llc_enable ? "True" : "False");
	count += snprintf(buf + count, PAGE_SIZE, "Plugin Initial LLC Region: %s (%u)\n",
				data->region_name[data->plugin_init_llc_region],
				data->plugin_init_llc_region);
	count += snprintf(buf + count, PAGE_SIZE, "CPU minimum region: %u\n",
				data->cpu_min_region);
	count += snprintf(buf + count, PAGE_SIZE, "LLC Region Priority:\n");
	count += snprintf(buf + count, PAGE_SIZE, "prio   region                  on\n");
	for (i = 0; i < LLC_REGION_MAX; i++)
		count += snprintf(buf + count, PAGE_SIZE, "%2d     %s  %u\n",
				i, data->region_name[i], data->llc_region_prio[i]);

	return count;
}

static ssize_t store_llc_invalidate(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	unsigned int invalidate, invway;
	int ret;

	if (!exynos_llc_enable) {
		SCI_INFO("%s: LLC is disabled\n", __func__);
		return count;
	}

	ret = sscanf(buf, "%u %x", &invalidate, &invway);
	if (ret != 2)
		return -EINVAL;

	if (invalidate != 1) {
		SCI_ERR("%s: Invalid parameter: %u, should be set 1\n",
				__func__, invalidate);
		return -EINVAL;
	}

	data->invway = invway;

	ret = exynos_sci_llc_invalidate(data);
	if (ret) {
		SCI_ERR("%s: Failed llc invalidate\n", __func__);
		return ret;
	}

	return count;
}

static ssize_t store_llc_flush(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	unsigned int flush, invway;
	int ret;

	if (!exynos_llc_enable) {
		SCI_INFO("%s: LLC is disabled\n", __func__);
		return count;
	}

	ret = sscanf(buf, "%u %x", &flush, &invway);
	if (ret != 2)
		return -EINVAL;

	if (flush != 1) {
		SCI_ERR("%s: Invalid parameter: %u, should be set 1\n",
				__func__, flush);
		return -EINVAL;
	}

	data->invway = invway;

	ret = exynos_sci_llc_flush(data, SYSFS_FLUSH_REGION_INDEX);
	if (ret) {
		SCI_ERR("%s: Failed llc flush\n", __func__);
		return ret;
	}

	return count;
}

static ssize_t show_llc_get_region_info(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	unsigned int region_index, way;
	int ret;

	if (!exynos_llc_enable) {
		count += snprintf(buf + count, PAGE_SIZE, "LLC is disabled\n");
		return count;
	}

	for (region_index = LLC_REGION_DISABLE + 1; region_index < LLC_REGION_MAX; region_index++) {
		ret = exynos_sci_llc_get_region_info(data, region_index, &way);
		if (ret) {
			count += snprintf(buf + count, PAGE_SIZE,
					"Failed get llc region info\n");
			return count;
		}

		count += snprintf(buf + count, PAGE_SIZE, "LLC Region: %s (%u) : %u\n",
				data->region_name[region_index], region_index, way);
	}

	return count;
}

static ssize_t show_llc_region_alloc(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	unsigned int region_index;
	int ret;

	if (!exynos_llc_enable) {
		count += snprintf(buf + count, PAGE_SIZE, "LLC is disabled\n");
		return count;
	}

	ret = exynos_sci_llc_region_alloc(data, SCI_IPC_GET, &region_index, 0, 0);
	if (ret) {
		count += snprintf(buf + count, PAGE_SIZE,
				"Failed llc region allocate state\n");
		return count;
	}

	if (region_index < LLC_REGION_MAX) {
		count += snprintf(buf + count, PAGE_SIZE, "LLC Region: %s (%u)\n",
				data->region_name[region_index]);
	}

	return count;
}

static ssize_t store_llc_region_alloc(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	unsigned int region_index, on, way;
	int ret;

	if (!exynos_llc_enable) {
		SCI_INFO("%s: LLC is disabled\n", __func__);
		return count;
	}

	ret = sscanf(buf, "%u %u %u", &region_index, &on, &way);
	if (ret != 3) {
		SCI_ERR("%s: usage: echo [region_index] [on] [way] > llc_region_alloc\n",
				__func__);
		return -EINVAL;
	}

	ret = exynos_sci_llc_region_alloc(data, SCI_IPC_SET, &region_index, (bool)on, way);
	if (ret) {
		SCI_ERR("%s: Failed llc region allocate\n", __func__);
		return ret;
	}

	return count;
}

static ssize_t show_llc_enable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	unsigned int enable;
	int ret;

	ret = exynos_sci_llc_enable(data, SCI_IPC_GET, &enable);
	if (ret) {
		count += snprintf(buf + count, PAGE_SIZE,
				"Failed llc enable state\n");
		return count;
	}

	count += snprintf(buf + count, PAGE_SIZE, "LLC Enable: %s (%d)\n",
			enable ? "enable" : "disable", enable);

	return count;
}

static ssize_t store_llc_enable(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	unsigned int enable;
	int ret;

	ret = sscanf(buf, "%u",	&enable);
	if (ret != 1)
		return -EINVAL;

	ret = exynos_sci_llc_enable(data, SCI_IPC_SET, &enable);
	if (ret) {
		SCI_ERR("%s: Failed llc enable control\n", __func__);
		return ret;
	}

	exynos_llc_enable = enable;

	return count;
}

static ssize_t store_llc_dump_test(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned int dump;
	int ret;

	ret = sscanf(buf, "%u",	&dump);
	if (ret != 1)
		return -EINVAL;

	if (dump == 0)
		return -EINVAL;

	panic("LLC Dump Test!!!!\n");

	return count;
}

static ssize_t show_llc_dump_addr_info(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;

	count += snprintf(buf + count, PAGE_SIZE, "\n= LLC dump address info =\n");
	count += snprintf(buf + count, PAGE_SIZE,
			"physical address = 0x%08x\n", data->llc_dump_addr.p_addr);
	count += snprintf(buf + count, PAGE_SIZE,
			"dump region size = 0x%08x\n", data->llc_dump_addr.p_size);
	count += snprintf(buf + count, PAGE_SIZE,
			"dump buffer size = 0x%08x\n", data->llc_dump_addr.buff_size);

	return count;
}

static ssize_t show_llc_retention(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	unsigned int enable = 0;
	int ret;

	if (!exynos_llc_enable) {
		count += snprintf(buf + count, PAGE_SIZE, "LLC is disabled\n");
		return count;
	}

	ret = exynos_sci_ret_enable(data, SCI_IPC_GET, &enable);
	if (ret) {
		count += snprintf(buf + count, PAGE_SIZE,
				"Failed llc retention state\n");
		return count;
	}

	count += snprintf(buf + count, PAGE_SIZE, "LLC Retention: %s (%d)\n",
			enable ? "enable":"disable", enable);

	return count;
}

static ssize_t store_llc_retention(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	unsigned int enable;
	int ret;

	if (!exynos_llc_enable) {
		SCI_INFO("%s: LLC is disabled\n", __func__);
		return count;
	}

	ret = sscanf(buf, "%u", &enable);
	if (ret != 1)
		return -EINVAL;

	ret = exynos_sci_ret_enable(data, SCI_IPC_SET, &enable);
	if (ret) {
		SCI_ERR("%s: Failed llc retention control\n", __func__);
		return ret;
	}

	return count;
}

static ssize_t show_llc_region_priority(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	unsigned int priority = 0;
	int ret, i;

	if (!exynos_llc_enable) {
		count += snprintf(buf + count, PAGE_SIZE, "LLC is disabled\n");
		return count;
	}

	for (i = 1; i < LLC_REGION_MAX; i++) {
		ret = exynos_sci_llc_region_priority(data, SCI_IPC_GET, i, &priority);
		if (ret) {
			count += snprintf(buf + count, PAGE_SIZE,
					"Failed get llc region priority\n");
			return count;
		}
		count += snprintf(buf + count, PAGE_SIZE, "[%d] %s : priority %d\n",
				i, data->region_name[i], priority);
	}

	return count;
}

static ssize_t store_llc_region_priority(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	unsigned int llc_region_priority[LLC_REGION_MAX], priority;
	int ret, i, n;

	if (!exynos_llc_enable) {
		SCI_INFO("%s: LLC is disabled\n", __func__);
		return count;
	}

	llc_region_priority[LLC_REGION_DISABLE] = 0;
	for (i = 1; i < LLC_REGION_MAX; i++) {
		ret = sscanf(buf, "%u%n", &priority, &n);
		if (!ret)
			break;
		llc_region_priority[i] = priority;
		buf += n;
	}

	if (i != LLC_REGION_MAX)
		return -EINVAL;

	for (i = 0; i < LLC_REGION_MAX; i++) {
		ret = exynos_sci_llc_region_priority(data, SCI_IPC_SET,
				i, &llc_region_priority[i]);
		if (ret) {
			SCI_ERR("%s: Failed set llc region priority\n", __func__);
			return ret;
		}
	}

	return count;
}

static ssize_t llc_off_disable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	bool debug_mode = get_exynos_sci_llc_debug_mode();

	count += snprintf(buf + count, PAGE_SIZE, "llc_gov_en: %d\n",
			data->gov_data->llc_gov_en);

	count += snprintf(buf + count, PAGE_SIZE, "debug_mode: %d\n",
			debug_mode);

	count += snprintf(buf + count, PAGE_SIZE, "llc_en: %d\n",
			exynos_llc_enable);

	return count;
}

static ssize_t llc_off_disable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned int off;
	int ret;

	ret = kstrtou32(buf, 0, &off);
	if (ret)
		return -EINVAL;

	llc_off_disable(off);

	return count;
}

static ssize_t show_llc_gov_en(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;

	count += snprintf(buf + count, PAGE_SIZE, "llc_gov_en: %d\n",
			data->gov_data->llc_gov_en);

	return count;
}

static ssize_t store_llc_gov_en(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	unsigned int llc_gov_en;
	int ret;

	ret = sscanf(buf, "%u",	&llc_gov_en);
	if (ret != 1)
		return -EINVAL;

	data->gov_data->llc_gov_en = llc_gov_en;

	return count;
}

static ssize_t show_hfreq_rate(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;

	count += snprintf(buf + count, PAGE_SIZE, "hfreq_rate: %d %\n",
			data->gov_data->hfreq_rate);

	return count;
}

static ssize_t store_hfreq_rate(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	unsigned int hfreq_rate;
	int ret;

	ret = sscanf(buf, "%u",	&hfreq_rate);
	if (ret != 1)
		return -EINVAL;

	data->gov_data->hfreq_rate = hfreq_rate;

	return count;
}

static ssize_t show_on_time_th(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;

	count += snprintf(buf + count, PAGE_SIZE, "on_time_th: %d nsec\n",
			data->gov_data->on_time_th);

	return count;
}

static ssize_t store_on_time_th(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	unsigned int on_time_th;
	int ret;

	ret = sscanf(buf, "%u",	&on_time_th);
	if (ret != 1)
		return -EINVAL;

	data->gov_data->on_time_th = on_time_th;

	return count;
}

static ssize_t show_off_time_th(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;

	count += snprintf(buf + count, PAGE_SIZE, "off_time_th = %d nsec\n",
			data->gov_data->off_time_th);

	return count;
}

static ssize_t store_off_time_th(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	unsigned int off_time_th;
	int ret;

	ret = sscanf(buf, "%u",	&off_time_th);
	if (ret != 1)
		return -EINVAL;

	data->gov_data->off_time_th = off_time_th;

	return count;
}

static ssize_t show_freq_th(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;

	count += snprintf(buf + count, PAGE_SIZE, "freq_th = %d KHz\n",
			data->gov_data->freq_th);

	return count;
}

static ssize_t store_freq_th(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	unsigned int freq_th;
	int ret;

	ret = sscanf(buf, "%u",	&freq_th);
	if (ret != 1)
		return -EINVAL;

	data->gov_data->freq_th = freq_th;

	return count;
}

static ssize_t show_enabled_time(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;

	count += snprintf(buf + count, PAGE_SIZE, "enabled_time = %lu\n",
			data->gov_data->enabled_time);

	return count;
}

static ssize_t store_enabled_time(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	unsigned int enabled_time;
	int ret;

	ret = sscanf(buf, "%u",	&enabled_time);
	if (ret != 1)
		return -EINVAL;

	data->gov_data->enabled_time = 0;

	return count;
}

static DEVICE_ATTR_RW(llc_off_disable);
static DEVICE_ATTR(sci_data, 0440, show_sci_data, NULL);
static DEVICE_ATTR(llc_invalidate, 0640, NULL, store_llc_invalidate);
static DEVICE_ATTR(llc_flush, 0640, NULL, store_llc_flush);
static DEVICE_ATTR(llc_get_region_info, 0440, show_llc_get_region_info, NULL);
static DEVICE_ATTR(llc_region_alloc, 0640, show_llc_region_alloc, store_llc_region_alloc);
static DEVICE_ATTR(llc_enable, 0640, show_llc_enable, store_llc_enable);
static DEVICE_ATTR(llc_dump_test, 0640, NULL, store_llc_dump_test);
static DEVICE_ATTR(llc_dump_addr_info, 0440, show_llc_dump_addr_info, NULL);
static DEVICE_ATTR(llc_retention, 0640, show_llc_retention, store_llc_retention);
static DEVICE_ATTR(llc_region_priority, 0640, show_llc_region_priority, store_llc_region_priority);
static DEVICE_ATTR(llc_gov_en, 0640, show_llc_gov_en, store_llc_gov_en);
static DEVICE_ATTR(hfreq_rate, 0640, show_hfreq_rate, store_hfreq_rate);
static DEVICE_ATTR(on_time_th, 0640, show_on_time_th, store_on_time_th);
static DEVICE_ATTR(off_time_th, 0640, show_off_time_th, store_off_time_th);
static DEVICE_ATTR(freq_th, 0640, show_freq_th, store_freq_th);
static DEVICE_ATTR(enabled_time, 0640, show_enabled_time, store_enabled_time);

static struct attribute *exynos_sci_sysfs_entries[] = {
	&dev_attr_sci_data.attr,
	&dev_attr_llc_invalidate.attr,
	&dev_attr_llc_flush.attr,
	&dev_attr_llc_get_region_info.attr,
	&dev_attr_llc_region_alloc.attr,
	&dev_attr_llc_enable.attr,
	&dev_attr_llc_dump_test.attr,
	&dev_attr_llc_dump_addr_info.attr,
	&dev_attr_llc_retention.attr,
	&dev_attr_llc_region_priority.attr,
	&dev_attr_llc_gov_en.attr,
	&dev_attr_llc_off_disable.attr,
	&dev_attr_hfreq_rate.attr,
	&dev_attr_on_time_th.attr,
	&dev_attr_off_time_th.attr,
	&dev_attr_freq_th.attr,
	&dev_attr_enabled_time.attr,
	NULL,
};

static struct attribute_group exynos_sci_attr_group = {
	.name	= "sci_attr",
	.attrs	= exynos_sci_sysfs_entries,
};

static int exynos_sci_pm_suspend(struct device *dev)
{
	if (sci_data->gov_data->llc_req_flag) {
		llc_region_alloc(LLC_REGION_CPU, 0, 0);
		sci_data->gov_data->llc_req_flag = 0;
		sci_data->gov_data->high_time = 0;
		sci_data->gov_data->start_time = 0;
		sci_data->gov_data->last_time = 0;
	}

	return 0;
}

static void exynos_ecc_int_en(void)
{
	int reg;

	reg = __raw_readl(sci_data->sci_base + ECC_INT_EN);
	reg &= ~(0x1 << 10);
	__raw_writel(reg, sci_data->sci_base + ECC_INT_EN);
}

static int exynos_sci_pm_resume(struct device *dev)
{
	exynos_ecc_int_en();
	return 0;
}

static struct dev_pm_ops exynos_sci_pm_ops = {
	.suspend	= exynos_sci_pm_suspend,
	.resume		= exynos_sci_pm_resume,
};

#ifdef CONFIG_LOCKUP_DETECTOR_OTHER_CPU
static int exynos_sci_lockup_handler(struct notifier_block *nb,
		unsigned long l, void *core)
{
	llc_ecc_logging();

	return 0;
}
static struct notifier_block exynos_sci_lockup_nb = {
	.notifier_call = exynos_sci_lockup_handler,
};
#endif

#if IS_ENABLED(CONFIG_USE_LLC_LOG)
static void exynos_sci_llc_dump_config(struct exynos_sci_data *data)
{
	data->llc_dump_addr.p_addr = llc_reserved.p_addr;
	data->llc_dump_addr.p_size = llc_reserved.p_size;

	data->llc_dump_addr.buff_size = data->llc_dump_addr.p_size;

	dbg_snapshot_add_bl_item_info(LLC_DSS_NAME,
			data->llc_dump_addr.p_addr, data->llc_dump_addr.p_size);
}
#endif

static int sci_panic_handler(struct notifier_block *nb, unsigned long l,
		void *buf)
{
	int ret, enable;

	llc_ecc_logging();

	enable = 0;
	ret = exynos_sci_llc_enable(sci_data, SCI_IPC_GET, &enable);
	if (ret)
		SCI_ERR("%s: Failed get llc enable\n", __func__);

	SCI_INFO("%s: LLC enable status: %s (%d)\n", __func__,
			enable ? "enable" : "disable", enable);

	enable = 0;
	if (sci_data->gov_data->en_cnt > 1)
		sci_data->gov_data->en_cnt = 1;

	ret = exynos_sci_llc_enable(sci_data, SCI_IPC_SET, &enable);
	if (ret)
		SCI_ERR("%s: Failed llc disable\n", __func__);

	SCI_INFO("%s: LLC Disabled!\n", __func__);

	return 0;
}

static struct notifier_block nb_sci_panic = {
	.notifier_call = sci_panic_handler,
	.priority = INT_MAX,
};

static int exynos_sci_probe(struct platform_device *pdev)
{
	int ret = 0, i;
	struct reserved_mem *rmem;
	struct device_node *rmem_np;
	struct exynos_sci_data *data;

	rmem_np = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	rmem = of_reserved_mem_lookup(rmem_np);
	if (!rmem) {
		dev_err(&pdev->dev, "failed to acquire memory region\n");
		return 0;
	}

	llc_reserved.p_addr = rmem->base;
	llc_reserved.p_size = rmem->size;

	data = kzalloc(sizeof(struct exynos_sci_data), GFP_KERNEL);
	if (data == NULL) {
		SCI_ERR("%s: failed to allocate SCI device\n", __func__);
		ret = -ENOMEM;
		goto err_data;
	}

	data->gov_data = kzalloc(sizeof(struct exynos_sci_gov_data), GFP_KERNEL);
	if (data->gov_data == NULL) {
		SCI_ERR("%s: failed to allocate SCI gov data\n", __func__);
		ret = -ENOMEM;
		goto err_gov_data;
	}

	sci_data = data;
	data->dev = &pdev->dev;

	spin_lock_init(&data->lock);

#if defined(CONFIG_EXYNOS_ACPM) || defined(CONFIG_EXYNOS_ACPM_MODULE)
	/* acpm_ipc_request_channel */
	ret = acpm_ipc_request_channel(data->dev->of_node, NULL,
				&data->ipc_ch_num, &data->ipc_ch_size);
	if (ret) {
		SCI_ERR("%s: acpm request channel is failed, ipc_ch: %u, size: %u\n",
				__func__, data->ipc_ch_num, data->ipc_ch_size);
		goto err_acpm;
	}
#endif
	data->sci_base = ioremap(SCI_BASE, SZ_4K);
	if (IS_ERR(data->sci_base)) {
		SCI_ERR("%s: Failed SCI base remap\n", __func__);
		goto err_ioremap;
	}

	/* parsing dts data for SCI */
	ret = exynos_sci_parse_dt(data->dev->of_node, data);
	if (ret) {
		SCI_ERR("%s: failed to parse private data\n", __func__);
		goto err_parse_dt;
	}

	if (data->ret_enable) {
		ret = exynos_sci_ret_enable(data, SCI_IPC_SET, &data->ret_enable);
		if (ret) {
			SCI_ERR("%s: Failed ret enable control\n", __func__);
			goto err_ret_disable;
		}
	}

	ret = exynos_sci_llc_region_alloc(data, SCI_IPC_GET,
					&data->plugin_init_llc_region, 0, 0);
	if (ret) {
		SCI_ERR("%s: Falied get plugin initial llc region\n", __func__);
		goto err_plug_llc_region;
	}

	data->llc_region_prio[data->plugin_init_llc_region] = 1;

	if (data->use_init_llc_region) {
		ret = exynos_sci_llc_region_alloc(data, SCI_IPC_SET,
						&data->initial_llc_region,
						true, FULL_WAY_NUM);
		if (ret) {
			SCI_ERR("%s: Failed llc region allocate\n", __func__);
			goto err_llc_region;
		}
	}

	for (i = 0; i < LLC_REGION_MAX; i++) {
		ret = exynos_sci_llc_region_priority(data, SCI_IPC_SET,
				i, &data->region_priority[i]);
		if (ret) {
			SCI_ERR("%s: Failed set llc region priority\n", __func__);
			goto err_region_priority;
		}
	}

	if (data->cpu_min_region) {
		ret = exynos_sci_cpu_min_region(data, SCI_IPC_SET, &data->cpu_min_region);
		if (ret) {
			SCI_ERR("%s: Failed set cpu min region\n", __func__);
			goto err_cpu_min_region;
		}
	}

	if (data->llc_enable) {
		ret = exynos_sci_llc_enable(data, SCI_IPC_SET, &data->llc_enable);
		if (ret) {
			SCI_ERR("%s: Failed llc enable control\n", __func__);
			goto err_llc_disable;
		}
		exynos_llc_enable = data->llc_enable;
		data->gov_data->en_cnt = 1;
		data->gov_data->llc_req_flag = 1;
	} else {
		data->gov_data->en_cnt = 0;
		data->gov_data->llc_req_flag = 0;
	}

#ifdef CONFIG_LOCKUP_DETECTOR_OTHER_CPU
	atomic_notifier_chain_register(&hardlockup_notifier_list,
			&exynos_sci_lockup_nb);
#endif

#if IS_ENABLED(CONFIG_USE_LLC_LOG)
	exynos_sci_llc_dump_config(data);
#else
	carveout_reserved_mem(rmem);
	SCI_INFO("%s: No use llc log mem\n", __func__);
#endif

	atomic_notifier_chain_register(&panic_notifier_list, &nb_sci_panic);
	platform_set_drvdata(pdev, data);

	ret = sysfs_create_group(&data->dev->kobj, &exynos_sci_attr_group);
	if (ret)
		SCI_ERR("%s: failed creat sysfs for Exynos SCI\n", __func__);

	INIT_DELAYED_WORK(&data->gov_data->get_noti_work, exynos_sci_get_noti);

	exynos_ecc_int_en();
	sci_data->llc_ecc_flag = false;
	print_sci_data(data);

	schedule_delayed_work(&data->gov_data->get_noti_work,
			msecs_to_jiffies(10000));

	sci_data->llc_on_flag = false;
	sci_data->gov_data->high_time = 0;
	sci_data->gov_data->start_time = 0;
	sci_data->gov_data->last_time = 0;
	sci_data->gov_data->enabled_time = 0;

	SCI_INFO("%s: exynos sci is initialized!!\n", __func__);

	return 0;

err_llc_disable:
err_cpu_min_region:
err_region_priority:
err_llc_region:
err_plug_llc_region:
err_ret_disable:
err_parse_dt:
err_ioremap:
#if defined(CONFIG_EXYNOS_ACPM) || defined(CONFIG_EXYNOS_ACPM_MODULE)
	acpm_ipc_release_channel(data->dev->of_node, data->ipc_ch_num);
err_acpm:
#endif
err_gov_data:
	kfree(data);

err_data:
	return ret;
}

static int exynos_sci_remove(struct platform_device *pdev)
{
	struct exynos_sci_data *data = platform_get_drvdata(pdev);

	sysfs_remove_group(&data->dev->kobj, &exynos_sci_attr_group);
	platform_set_drvdata(pdev, NULL);
	iounmap(data->sci_base);
#if defined(CONFIG_EXYNOS_ACPM) || defined(CONFIG_EXYNOS_ACPM_MODULE)
	acpm_ipc_release_channel(data->dev->of_node, data->ipc_ch_num);
#endif
	kfree(data);

	SCI_INFO("%s: exynos sci is removed!!\n", __func__);

	return 0;
}

static struct platform_device_id exynos_sci_driver_ids[] = {
	{ .name = EXYNOS_SCI_MODULE_NAME, },
	{},
};
MODULE_DEVICE_TABLE(platform, exynos_sci_driver_ids);

static const struct of_device_id exynos_sci_match[] = {
	{ .compatible = "samsung,exynos-sci", },
	{},
};

static struct platform_driver exynos_sci_driver = {
	.remove = exynos_sci_remove,
	.id_table = exynos_sci_driver_ids,
	.driver = {
		.name = EXYNOS_SCI_MODULE_NAME,
		.owner = THIS_MODULE,
		.pm = &exynos_sci_pm_ops,
		.of_match_table = exynos_sci_match,
	},
	.probe = exynos_sci_probe,
};

static int exynos_sci_init(void)
{
	int ret;

	ret = platform_driver_register(&exynos_sci_driver);
	if (ret) {
		SCI_INFO("Error registering platform driver");
		return ret;
	}
#if defined(CONFIG_EXYNOS_SCI_DBG) || defined(CONFIG_EXYNOS_SCI_DBG_MODULE)
	ret = platform_driver_register(&exynos_sci_dbg_driver);
#endif
	return ret;
}
arch_initcall(exynos_sci_init);

MODULE_AUTHOR("Taekki Kim <taekki.kim@samsung.com>");
MODULE_DESCRIPTION("Samsung SCI Interface driver");
MODULE_LICENSE("GPL");
