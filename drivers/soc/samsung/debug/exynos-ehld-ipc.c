/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/cpu_pm.h>
#include <linux/suspend.h>

#include <soc/samsung/debug-snapshot.h>
#include <soc/samsung/exynos-adv-tracer-ipc.h>
#include <soc/samsung/exynos-ehld.h>

enum ehld_ipc_cmd {
	eEHLD_IPC_CMD_SET_ENABLE,
	eEHLD_IPC_CMD_GET_ENABLE,
	eEHLD_IPC_CMD_SET_INTERVAL,
	eEHLD_IPC_CMD_SET_LOCKUP_WARN_VAL,
	eEHLD_IPC_CMD_SET_LOCKUP_VAL,
	eEHLD_IPC_CMD_NOTI_CPU_ON,
	eEHLD_IPC_CMD_NOTI_CPU_OFF,
	eEHLD_IPC_CMD_LOCKUP_DETECT_WARN,
	eEHLD_IPC_CMD_LOCKUP_DETECT_SW,
	eEHLD_IPC_CMD_LOCKUP_DETECT_HW,
	eEHLD_IPC_CMD_SET_INIT_VAL,
};

struct plugin_ehld_info {
	struct adv_tracer_plugin *ehld_dev;
	unsigned int enable;
	unsigned int interval;
} plugin_ehld;

int adv_tracer_ehld_get_enable(void)
{
	struct adv_tracer_ipc_cmd cmd;
	int ret = 0;

	cmd.cmd_raw.cmd = eEHLD_IPC_CMD_GET_ENABLE;
	ret = adv_tracer_ipc_send_data(plugin_ehld.ehld_dev->id,
					(struct adv_tracer_ipc_cmd *)&cmd);
	if (ret < 0) {
		pr_info("ehld-ipc: cannot get enable\n");
		return ret;
	}
	plugin_ehld.enable = cmd.buffer[1];

	pr_info("ehld-ipc: EHLD %sabled\n",
			plugin_ehld.enable ? "en" : "dis");

	return plugin_ehld.enable;
}

int adv_tracer_ehld_set_enable(int en)
{
	struct adv_tracer_ipc_cmd cmd;
	int ret = 0;

	cmd.cmd_raw.cmd = eEHLD_IPC_CMD_SET_ENABLE;
	cmd.buffer[1] = en;
	ret = adv_tracer_ipc_send_data(plugin_ehld.ehld_dev->id, &cmd);
	if (ret < 0) {
		pr_err("ehld-ipc: cannot enable setting\n");
		return ret;
	}
	plugin_ehld.enable = en;
	pr_info("ehld-ipc: set EHLD to %sabled\n",
			plugin_ehld.enable ? "en" : "dis");
	return 0;
}

int adv_tracer_ehld_set_init_val(u32 interval, u32 count, u32 cpu_mask)
{
	struct adv_tracer_ipc_cmd cmd;
	int ret = 0;

	if (!interval)
		return -EINVAL;

	cmd.cmd_raw.cmd = eEHLD_IPC_CMD_SET_INIT_VAL;
	cmd.buffer[1] = interval;
	cmd.buffer[2] = count;
	cmd.buffer[3] = cpu_mask;

	ret = adv_tracer_ipc_send_data(plugin_ehld.ehld_dev->id, &cmd);
	if (ret < 0) {
		pr_err("ehld-ipc: failed to init value\n");
		return ret;
	}
	plugin_ehld.interval = interval;

	pr_info("ehld-ipc: set interval to %u ms\n", interval);

	return 0;
}

int adv_tracer_ehld_set_interval(u32 interval)
{
	struct adv_tracer_ipc_cmd cmd;
	int ret = 0;

	if (!interval)
		return -EINVAL;

	cmd.cmd_raw.cmd = eEHLD_IPC_CMD_SET_INTERVAL;
	cmd.buffer[1] = interval;
	ret = adv_tracer_ipc_send_data(plugin_ehld.ehld_dev->id, &cmd);
	if (ret < 0) {
		pr_err("ehld-ipc: can't set the interval\n");
		return ret;
	}
	plugin_ehld.interval = interval;
	pr_err("ehld-ipc: set interval to %u ms\n", interval);

	return 0;
}

int adv_tracer_ehld_set_warn_count(u32 count)
{
	struct adv_tracer_ipc_cmd cmd;
	int ret = 0;

	if (!count)
		return -EINVAL;

	cmd.cmd_raw.cmd = eEHLD_IPC_CMD_SET_LOCKUP_WARN_VAL;
	cmd.buffer[1] = count;
	ret = adv_tracer_ipc_send_data(plugin_ehld.ehld_dev->id, &cmd);
	if (ret < 0) {
		pr_err("ehld-ipc: can't set lockup warning count\n");
		return ret;
	}
	pr_info("ehld-ipc: set warning count for %u times\n", count);

	return 0;
}

int adv_tracer_ehld_set_lockup_count(u32 count)
{
	struct adv_tracer_ipc_cmd cmd;
	int ret = 0;

	if (!count)
		return 0;

	cmd.cmd_raw.cmd = eEHLD_IPC_CMD_SET_LOCKUP_VAL;
	cmd.buffer[1] = count;
	ret = adv_tracer_ipc_send_data(plugin_ehld.ehld_dev->id, &cmd);
	if (ret < 0) {
		pr_err("ehld-ipc: can't set lockup count\n");
		return ret;
	}

	pr_info("ehld-ipc: set lockup count for %u times\n", count);

	return 0;
}

u32 adv_tracer_ehld_get_interval(void)
{
	return plugin_ehld.interval;
}

int adv_tracer_ehld_noti_cpu_state(int cpu, int en)
{
	struct adv_tracer_ipc_cmd cmd;
	int ret = 0;

	if (!ret)
		return -EINVAL;

	if (en == true)
		cmd.cmd_raw.cmd = eEHLD_IPC_CMD_NOTI_CPU_ON;
	else
		cmd.cmd_raw.cmd = eEHLD_IPC_CMD_NOTI_CPU_OFF;

	cmd.buffer[1] = cpu;

	ret = adv_tracer_ipc_send_data_polling(plugin_ehld.ehld_dev->id, &cmd);
	if (ret < 0) {
		pr_err("ehld-ipc: cannot cmd state\n");
		return ret;
	}

	return 0;
}

static void adv_tracer_ehld_handler(struct adv_tracer_ipc_cmd *cmd,
				    unsigned int len)
{
	switch (cmd->cmd_raw.cmd) {
	case eEHLD_IPC_CMD_LOCKUP_DETECT_WARN:
	pr_err("ehld-ipc: CPU%x is Hardlockup Warning - cnt:0x%x\n",
		(unsigned int)cmd->buffer[1],
		(unsigned int)cmd->buffer[2]);
		ehld_do_action((int)cmd->buffer[1], (unsigned int)cmd->buffer[2]);
		break;
	case eEHLD_IPC_CMD_LOCKUP_DETECT_HW:
	pr_err("ehld-ipc: CPU%x is Hardlockup Detected - cnt:0x%x by HW\n",
		(unsigned int)cmd->buffer[1],
		(unsigned int)cmd->buffer[2]);
		ehld_do_action((int)cmd->buffer[1], (unsigned int)cmd->buffer[2]);
	break;
	case eEHLD_IPC_CMD_LOCKUP_DETECT_SW:
	pr_err("ehld-ipc: CPU%x is Hardlockup Detected - counter:0x%x by SW\n",
		(unsigned int)cmd->buffer[1],
		(unsigned int)cmd->buffer[2]);
		ehld_do_action((int)cmd->buffer[1], (unsigned int)cmd->buffer[2]);
	break;
	}
}

int adv_tracer_ehld_init(void *p)
{
	struct adv_tracer_plugin *ehld = NULL;
	struct device_node *np = (struct device_node *)p;
	int ret = 0;

	ehld = kzalloc(sizeof(struct adv_tracer_plugin), GFP_KERNEL);

	if (!ehld) {
		ret = -ENOMEM;
		return ret;
	}

	ret = adv_tracer_ipc_request_channel(np,
					(ipc_callback)adv_tracer_ehld_handler,
					&ehld->id, &ehld->len);
	if (ret < 0) {
		pr_err("ehld-ipc: request fail(%d)\n", ret);
		ret = -ENODEV;
		kfree(ehld);
		return ret;
	}

	plugin_ehld.ehld_dev = ehld;
	pr_info("ehld-ipc: %s successful - id:0x%x, len:0x%x\n",
			__func__, ehld->id, ehld->len);
	return ret;
}
