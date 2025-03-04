// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/syscalls.h>
#include <linux/wait.h>
#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-hw-o1-system.h"
#include "dsp-device.h"
#include "dsp-binary.h"
#include "dsp-debug.h"
#include "dsp-hw-o1-ctrl.h"
#include "dsp-hw-o1-dump.h"
#include "dsp-hw-o1-bus.h"
#include "dsp-hw-o1-llc.h"
#include "dsp-hw-o1-memory.h"
#include "dsp-hw-o1-pm.h"
#include "dsp-hw-o1-mailbox.h"
#include "hardware/common/dsp-hw-common-debug.h"
#include "dsp-hw-o1-debug.h"

#define DSP_HW_DEBUG_LOG_LINE_SIZE	(128)
#define DSP_HW_DEBUG_LOG_TIME		(1)

struct dsp_hw_o1_debug {
	struct dentry	*llc;
};

static int dsp_hw_debug_power_show(struct seq_file *file, void *unused)
{
	struct dsp_hw_debug *debug;
	struct dsp_device *dspdev;

	dsp_enter();
	debug = file->private;
	dspdev = debug->dspdev;

	mutex_lock(&dspdev->lock);
	if (dsp_device_power_active(dspdev))
		seq_puts(file, "DSP power on\n");
	else
		seq_puts(file, "DSP power off\n");

	seq_printf(file, "open(%u)/start(%u)/sec_start(%u)\n",
			dspdev->open_count, dspdev->start_count,
			dspdev->sec_start_count);
	mutex_unlock(&dspdev->lock);

	dsp_leave();
	return 0;
}

static int dsp_hw_debug_power_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, dsp_hw_debug_power_show, inode->i_private);
}

static ssize_t dsp_hw_debug_power_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	struct seq_file *file;
	struct dsp_hw_debug *debug;
	char command[10];
	ssize_t size;

	dsp_enter();
	file = filp->private_data;
	debug = file->private;

	size = simple_write_to_buffer(command, sizeof(command), ppos,
			user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		dsp_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}

	command[size - 1] = '\0';
	if (sysfs_streq(command, "open")) {
		ret = dsp_device_open(debug->dspdev);
		if (ret)
			goto p_err;
	} else if (sysfs_streq(command, "close")) {
		dsp_device_close(debug->dspdev);
	} else if (sysfs_streq(command, "start")) {
		ret = dsp_device_start(debug->dspdev, 0);
		if (ret)
			goto p_err;
	} else if (sysfs_streq(command, "stop")) {
		dsp_device_stop(debug->dspdev, 1);
	} else if (sysfs_streq(command, "sec_start")) {
		ret = dsp_device_start_secure(debug->dspdev, 0, 0);
		if (ret)
			goto p_err;
	} else if (sysfs_streq(command, "sec_stop")) {
		dsp_device_stop_secure(debug->dspdev, 1);
	} else {
		ret = -EINVAL;
		dsp_err("power command[%s] of debugfs is invalid\n", command);
		goto p_err;
	}

	dsp_leave();
	return count;
p_err:
	return ret;
}

static const struct file_operations dsp_hw_debug_power_fops = {
	.open		= dsp_hw_debug_power_open,
	.read		= seq_read,
	.write		= dsp_hw_debug_power_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int dsp_hw_debug_clk_show(struct seq_file *file, void *unused)
{
	struct dsp_hw_debug *debug;
	struct dsp_device *dspdev;

	dsp_enter();
	debug = file->private;
	dspdev = debug->dspdev;

	mutex_lock(&dspdev->lock);
	if (dsp_device_power_active(dspdev))
		dspdev->system.clk.ops->user_dump(&dspdev->system.clk, file);
	else
		seq_puts(file, "power off\n");
	mutex_unlock(&dspdev->lock);

	dsp_leave();
	return 0;
}

static int dsp_hw_debug_clk_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, dsp_hw_debug_clk_show, inode->i_private);
}

static const struct file_operations dsp_hw_debug_clk_fops = {
	.open		= dsp_hw_debug_clk_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release
};

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
static int dsp_hw_debug_devfreq_show(struct seq_file *file, void *unused)
{
	struct dsp_hw_debug *debug;
	struct dsp_pm *pm;
	struct dsp_pm_devfreq *devfreq;
	int count, idx;

	dsp_enter();
	debug = file->private;
	pm = &debug->dspdev->system.pm;

	mutex_lock(&pm->lock);

	for (count = 0; count < DSP_O1_DEVFREQ_COUNT; ++count) {
		devfreq = &pm->devfreq[count];
		seq_printf(file, "[%s] id : %d\n", devfreq->name, count);
		seq_printf(file, "[%s] available level count [0 - %u]\n",
				devfreq->name, devfreq->count - 1);
		for (idx = 0; idx < devfreq->count; ++idx)
			seq_printf(file, "[%s] [L%02u] %u\n",
					devfreq->name, idx,
					devfreq->table[idx]);

		seq_printf(file, "[%s] boot    : L%02u\n",
				devfreq->name, devfreq->boot_qos);
		seq_printf(file, "[%s] dynamic : L%02u\n",
				devfreq->name, devfreq->dynamic_qos);
		seq_printf(file, "[%s] static  : L%02u\n",
				devfreq->name, devfreq->static_qos);
		seq_printf(file, "[%s] current : L%02u\n",
				devfreq->name, devfreq->current_qos);
		seq_printf(file, "[%s] min     : L%02u\n",
				devfreq->name, devfreq->min_qos);
		if (devfreq->force_qos < 0)
			seq_printf(file, "[%s] force   : none\n",
					devfreq->name);
		else
			seq_printf(file, "[%s] force   : L%02u\n",
					devfreq->name, devfreq->force_qos);
		seq_printf(file, "[%s] dynamic total count : %u\n",
				devfreq->name, devfreq->dynamic_total_count);
		seq_printf(file, "[%s] dynamic count : ", devfreq->name);
		for (idx = 0; idx < DSP_DEVFREQ_RESERVED_NUM; ++idx)
			seq_printf(file, "%3u ", devfreq->dynamic_count[idx]);
		seq_puts(file, "\n");
		seq_printf(file, "[%s] static total count : %u\n",
				devfreq->name, devfreq->static_total_count);
		seq_printf(file, "[%s] static count : ", devfreq->name);
		for (idx = 0; idx < DSP_DEVFREQ_RESERVED_NUM; ++idx)
			seq_printf(file, "%3u ", devfreq->static_count[idx]);
		seq_puts(file, "\n");
	}

	seq_printf(file, "[pm] dvfs mode : %d\n", pm->dvfs);
	seq_printf(file, "[pm] dvfs disable count : %u\n",
			pm->dvfs_disable_count);
	seq_printf(file, "[pm] dvfs mode lock : %d\n", pm->dvfs_lock);

	for (count = 0; count < DSP_O1_EXTRA_DEVFREQ_COUNT; ++count) {
		devfreq = &pm->extra_devfreq[count];
		seq_printf(file, "[%s] id : %d\n", devfreq->name, count);
		seq_printf(file, "[%s] available level count [0 - %u]\n",
				devfreq->name, devfreq->count - 1);
		for (idx = 0; idx < devfreq->count; ++idx)
			seq_printf(file, "[%s] [L%02u] %u\n",
					devfreq->name, idx,
					devfreq->table[idx]);

		seq_printf(file, "[%s] min     : L%02u\n",
				devfreq->name, devfreq->min_qos);
		seq_printf(file, "[%s] current : L%02u\n",
				devfreq->name, devfreq->current_qos);
	}

	seq_puts(file, "Command to change devfreq setting\n");
	seq_puts(file, "id/level information can be checked with ");
	seq_puts(file, "'cat /d/dsp/hardware/devfreq'\n");
	seq_puts(file, " [mode 0] add dynamic qos\n");
	seq_puts(file, "  echo 0 {level} > /d/dsp/hardware/devfreq\n");
	seq_puts(file, " [mode 1] del dynamic qos\n");
	seq_puts(file, "  echo 1 {level} > /d/dsp/hardware/devfreq\n");
	seq_puts(file, " [mode 2] add static qos\n");
	seq_puts(file, "  echo 2 {level} > /d/dsp/hardware/devfreq\n");
	seq_puts(file, " [mode 3] del static qos\n");
	seq_puts(file, "  echo 3 {level} > /d/dsp/hardware/devfreq\n");
	seq_puts(file, " [mode 4] enable force qos\n");
	seq_puts(file, "  echo 4 {id} {level} > /d/dsp/hardware/devfreq\n");
	seq_puts(file, " [mode 5] disable force qos\n");
	seq_puts(file, "  echo 5 {id} > /d/dsp/hardware/devfreq\n");
	seq_puts(file, " [mode 6] lock dvfs mode\n");
	seq_puts(file, "  echo 6 {0 or others} > /d/dsp/hardware/devfreq\n");

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
}

static int dsp_hw_debug_devfreq_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, dsp_hw_debug_devfreq_show, inode->i_private);
}

static ssize_t dsp_hw_debug_devfreq_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	struct seq_file *file;
	struct dsp_hw_debug *debug;
	struct dsp_pm *pm;
	char buf[30];
	ssize_t size;
	int mode, num1, num2;

	dsp_enter();
	file = filp->private_data;
	debug = file->private;
	pm = &debug->dspdev->system.pm;

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		dsp_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = sscanf(buf, "%d %d %d", &mode, &num1, &num2);
	if ((ret != 2) && (ret != 3)) {
		dsp_err("Failed to get devfreq parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	switch (mode) {
	case 0:
		pm->ops->update_devfreq_busy(pm, num1);
		break;
	case 1:
		pm->ops->update_devfreq_idle(pm, num1);
		break;
	case 2:
		pm->ops->dvfs_disable(pm, num1);
		break;
	case 3:
		pm->ops->dvfs_enable(pm, num1);
		break;
	case 4:
		pm->ops->set_force_qos(pm, num1, num2);
		break;
	case 5:
		pm->ops->set_force_qos(pm, num1, -1);
		break;
	case 6:
		pm->dvfs_lock = num1;
		break;
	default:
		ret = -EINVAL;
		dsp_err("mode for devfreq setting is invalid(%d)\n", mode);
		goto p_err;
	}

	dsp_leave();
	return count;
p_err:
	return ret;
}

static const struct file_operations dsp_hw_debug_devfreq_fops = {
	.open		= dsp_hw_debug_devfreq_open,
	.read		= seq_read,
	.write		= dsp_hw_debug_devfreq_write,
	.llseek		= seq_lseek,
	.release	= single_release
};
#endif

static int dsp_hw_debug_sfr_show(struct seq_file *file, void *unused)
{
	int ret;
	struct dsp_hw_debug *debug;

	dsp_enter();
	debug = file->private;

	ret = dsp_device_open(debug->dspdev);
	if (ret)
		goto p_err_open;

	ret = dsp_device_start(debug->dspdev, 0);
	if (ret)
		goto p_err_start;

	dsp_dump_ctrl_user(file);
	dsp_device_stop(debug->dspdev, 1);
	dsp_device_close(debug->dspdev);

	dsp_leave();
	return 0;
p_err_start:
	dsp_device_close(debug->dspdev);
p_err_open:
	return 0;
}

static int dsp_hw_debug_sfr_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, dsp_hw_debug_sfr_show, inode->i_private);
}

static const struct file_operations dsp_hw_debug_sfr_fops = {
	.open		= dsp_hw_debug_sfr_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int dsp_hw_debug_mem_show(struct seq_file *file, void *unused)
{
	struct dsp_hw_debug *debug;
	struct dsp_memory *mem;
	int idx;

	dsp_enter();
	debug = file->private;
	mem = &debug->dspdev->system.memory;

	for (idx = 0; idx < DSP_O1_PRIV_MEM_COUNT; ++idx)
		seq_printf(file, "[id:%d][%15s] : %zu KB (%zu KB ~ %zu KB)\n",
				idx, mem->priv_mem[idx].name,
				mem->priv_mem[idx].size / SZ_1K,
				mem->priv_mem[idx].min_size / SZ_1K,
				mem->priv_mem[idx].max_size / SZ_1K);

	seq_puts(file, "Command to change allocated memory size\n");
	seq_puts(file, " echo {mem_id} {size} > /d/dsp/hardware/mem\n");
	seq_puts(file, " (Size must be in KB\n");
	seq_puts(file, "  Command only works when power is off)\n");

	dsp_leave();
	return 0;
}

static int dsp_hw_debug_mem_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, dsp_hw_debug_mem_show, inode->i_private);
}

static ssize_t dsp_hw_debug_mem_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	struct seq_file *file;
	struct dsp_hw_debug *debug;
	struct dsp_device *dspdev;
	struct dsp_memory *mem;
	char buf[128];
	ssize_t len;
	unsigned int id, size;
	struct dsp_priv_mem *pmem;

	dsp_enter();
	file = filp->private_data;
	debug = file->private;
	dspdev = debug->dspdev;
	mem = &dspdev->system.memory;

	if (count > sizeof(buf)) {
		dsp_err("writing size(%zd) is larger than buffer\n", count);
		goto out;
	}

	len = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (len <= 0) {
		dsp_err("Failed to get user parameter(%zd)\n", len);
		goto out;
	}
	buf[len - 1] = '\0';

	ret = sscanf(buf, "%u %u\n", &id, &size);
	if (ret != 2) {
		dsp_err("Failed to get command changing memory size(%d)\n",
				ret);
		goto out;
	}

	if (id >= DSP_O1_PRIV_MEM_COUNT) {
		dsp_err("memory id(%u) of command is invalid(0 ~ %u)\n",
				id, DSP_O1_PRIV_MEM_COUNT - 1);
		goto out;
	}

	mutex_lock(&dspdev->lock);
	if (dspdev->open_count) {
		dsp_err("device was already running(%u)\n", dspdev->open_count);
		mutex_unlock(&dspdev->lock);
		goto out;
	}

	pmem = &mem->priv_mem[id];
	size = PAGE_ALIGN(size * SZ_1K);
	if (size >= pmem->min_size && size <= pmem->max_size) {
		dsp_info("size of %s is changed(%zu KB -> %u KB)\n",
				pmem->name, pmem->size / SZ_1K, size / SZ_1K);
		pmem->size = size;
	} else {
		dsp_warn("invalid size %u KB (%s, %zu KB ~ %zu KB)\n",
				size / SZ_1K, pmem->name,
				pmem->min_size / SZ_1K,
				pmem->max_size / SZ_1K);
	}

	mutex_unlock(&dspdev->lock);

	dsp_leave();
out:
	return count;
}

static const struct file_operations dsp_hw_debug_mem_fops = {
	.open		= dsp_hw_debug_mem_open,
	.read		= seq_read,
	.write		= dsp_hw_debug_mem_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int dsp_hw_debug_fw_log_show(struct seq_file *file, void *unused)
{
	struct dsp_hw_debug *debug;
	struct dsp_hw_debug_log *log;
	struct dsp_device *dspdev;

	dsp_enter();
	debug = file->private;
	log = debug->log;
	dspdev = debug->dspdev;

	seq_puts(file, "< Information about fw log of DSP >\n");

	mutex_lock(&dspdev->lock);
	if (dspdev->start_count) {
		seq_printf(file, "front : %d / rear : %d\n",
				readl(&log->queue->front),
				readl(&log->queue->rear));
		seq_printf(file, "data_size : %d / data_count : %d\n",
				readl(&log->queue->data_size),
				readl(&log->queue->data_count));
	} else {
		seq_puts(file, "log structure is not initilized\n");
	}
	mutex_unlock(&dspdev->lock);

	dsp_leave();
	return 0;
}

static int dsp_hw_debug_fw_log_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, dsp_hw_debug_fw_log_show, inode->i_private);
}

static const struct file_operations dsp_hw_debug_fw_log_fops = {
	.open		= dsp_hw_debug_fw_log_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int dsp_hw_debug_wait_ctrl_show(struct seq_file *file, void *unused)
{
	struct dsp_hw_debug *debug;
	struct dsp_system *sys;
	struct dsp_hw_o1_system *sys_sub;

	dsp_enter();
	debug = file->private;
	sys = &debug->dspdev->system;
	sys_sub = sys->sub_data;

	seq_printf(file, "[id:%u] boot wait time %ums\n",
			DSP_SYSTEM_WAIT_BOOT, sys->wait[DSP_SYSTEM_WAIT_BOOT]);
	seq_printf(file, "[id:%u] mailbox wait time %ums\n",
			DSP_SYSTEM_WAIT_MAILBOX,
			sys->wait[DSP_SYSTEM_WAIT_MAILBOX]);
	seq_printf(file, "[id:%u] reset wait time %ums\n",
			DSP_SYSTEM_WAIT_RESET,
			sys->wait[DSP_SYSTEM_WAIT_RESET]);

	seq_printf(file, "[id:%u] wait mode %u (irq:%u/busy-waiting:%u)\n",
			DSP_SYSTEM_WAIT_NUM, sys_sub->wait_mode,
			DSP_HW_O1_WAIT_MODE_INTERRUPT,
			DSP_HW_O1_WAIT_MODE_BUSY_WAITING);

	seq_puts(file, "Command to change wait time\n");
	seq_puts(file, " echo {id} {time/mode} > /d/dsp/hardware/wait_ctrl\n");
	dsp_leave();
	return 0;
}

static int dsp_hw_debug_wait_ctrl_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, dsp_hw_debug_wait_ctrl_show, inode->i_private);
}

static ssize_t dsp_hw_debug_wait_ctrl_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	struct seq_file *file;
	struct dsp_hw_debug *debug;
	struct dsp_device *dspdev;
	struct dsp_hw_o1_system *sys_sub;
	char buf[30];
	ssize_t size;
	unsigned int id, ctrl;

	dsp_enter();
	file = filp->private_data;
	debug = file->private;
	dspdev = debug->dspdev;
	sys_sub = dspdev->system.sub_data;

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		dsp_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = sscanf(buf, "%u %u", &id, &ctrl);
	if (ret != 2) {
		dsp_err("Failed to get wait_ctrl parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	if (id > DSP_SYSTEM_WAIT_NUM) {
		ret = -EINVAL;
		dsp_err("wait_ctrl id(%u) of command is invalid(0 ~ %u)\n",
				id, DSP_SYSTEM_WAIT_NUM);
		goto p_err;
	}

	mutex_lock(&dspdev->lock);
	if (dspdev->open_count) {
		dsp_err("device already opened (%u/%u)\n",
				dspdev->open_count, dspdev->start_count);
	} else if (id < DSP_SYSTEM_WAIT_NUM) {
		dsp_info("wait_time of id[%u] is changed from %ums to %ums\n",
				id, dspdev->system.wait[id], ctrl);
		dspdev->system.wait[id] = ctrl;
	} else if (id == DSP_SYSTEM_WAIT_NUM) {
		if (ctrl >= DSP_HW_O1_WAIT_MODE_NUM) {
			mutex_unlock(&dspdev->lock);
			ret = -EINVAL;
			dsp_err("wait mode(%u) is invalid\n", ctrl);
			goto p_err;
		}

		dsp_info("wait_mode is changed from %u to %u\n",
				sys_sub->wait_mode, ctrl);
		sys_sub->wait_mode = ctrl;
	}

	mutex_unlock(&dspdev->lock);

	dsp_leave();
	return count;
p_err:
	return ret;
}

static const struct file_operations dsp_hw_debug_wait_ctrl_fops = {
	.open		= dsp_hw_debug_wait_ctrl_open,
	.read		= seq_read,
	.write		= dsp_hw_debug_wait_ctrl_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int dsp_hw_debug_layer_range_show(struct seq_file *file, void *unused)
{
	struct dsp_hw_debug *debug;
	struct dsp_system *sys;

	dsp_enter();
	debug = file->private;
	sys = &debug->dspdev->system;

	seq_printf(file, "layer_range start:%#x / end:%#x\n",
			sys->layer_start, sys->layer_end);
	seq_puts(file, "Command to set layer_range to run layer by layer\n");
	seq_printf(file, "%#x is default value and it is ignored\n",
			DSP_SET_DEFAULT_LAYER);
	seq_puts(file, " echo ${start} ${end} > /d/dsp/hardware/layer_range\n");
	seq_puts(file, "Please use a decimal number\n");

	dsp_leave();
	return 0;
}

static int dsp_hw_debug_layer_range_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, dsp_hw_debug_layer_range_show,
			inode->i_private);
}

static ssize_t dsp_hw_debug_layer_range_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	struct seq_file *file;
	struct dsp_hw_debug *debug;
	struct dsp_device *dspdev;
	char buf[30];
	ssize_t size;
	unsigned int start_layer;
	unsigned int end_layer;

	dsp_enter();
	file = filp->private_data;
	debug = file->private;
	dspdev = debug->dspdev;

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		dsp_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = sscanf(buf, "%u %u", &start_layer, &end_layer);
	if (ret != 2) {
		dsp_err("Failed to get layer_range parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	mutex_lock(&dspdev->lock);
	if (dspdev->open_count) {
		dsp_err("device already opened (%u/%u)\n",
				dspdev->open_count, dspdev->start_count);
	} else {
		dspdev->system.layer_start = start_layer;
		dspdev->system.layer_end = end_layer;
		dsp_info("layer_range is set (%d ~ %d)\n",
				start_layer, end_layer);
	}
	mutex_unlock(&dspdev->lock);

	dsp_leave();
	return count;
p_err:
	return ret;
}

static const struct file_operations dsp_hw_debug_layer_range_fops = {
	.open		= dsp_hw_debug_layer_range_open,
	.read		= seq_read,
	.write		= dsp_hw_debug_layer_range_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int dsp_hw_debug_mailbox_show(struct seq_file *file, void *unused)
{
	dsp_enter();
	seq_printf(file, "mailbox supportes version : v%u ~ v%u\n",
			DSP_MAILBOX_VERSION_START + 1,
			DSP_MAILBOX_VERSION_END - 1);
	seq_printf(file, "message supportes version : v%u ~ v%u\n",
			DSP_MESSAGE_VERSION_START + 1,
			DSP_MESSAGE_VERSION_END - 1);

	seq_puts(file, "Command to send mail as count value\n");
	seq_puts(file, " echo {count} > /d/dsp/hardware/mailbox\n");
	dsp_leave();
	return 0;
}

static int dsp_hw_debug_mailbox_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, dsp_hw_debug_mailbox_show, inode->i_private);
}

static void __dsp_hw_debug_mailbox_test(struct dsp_device *dspdev,
		unsigned int count)
{
	int ret;
	struct dsp_system *sys;
	int idx = -1, fail;
	struct dsp_mailbox_pool **pool;
	struct dsp_task **task;

	dsp_enter();
	sys = &dspdev->system;

	pool = vmalloc(sizeof(*pool) * count);
	if (!pool)
		goto p_err_pool;

	task = vmalloc(sizeof(*task) * count);
	if (!task)
		goto p_err_task;

	for (idx = 0; idx < count; ++idx) {
		pool[idx] = dsp_mailbox_alloc_pool(&sys->mailbox,
				SZ_1K * (idx % 2 + 1));
		if (IS_ERR(pool[idx])) {
			for (fail = 0; fail < idx; ++fail)
				dsp_mailbox_free_pool(pool[fail]);
			goto p_err_alloc;
		}
	}

	for (idx = 0; idx < count; ++idx) {
		task[idx] = dsp_task_create(&sys->task_manager, false);
		if (IS_ERR(task[idx])) {
			for (fail = 0; fail < idx; ++fail)
				dsp_task_destroy(task[fail]);
			goto p_err_create;
		}

		task[idx]->message_version = DSP_MESSAGE_V3;
		task[idx]->message_id = idx % DSP_COMMON_MESSAGE_NUM;
		task[idx]->pool = pool[idx];
		task[idx]->wait = true;
	}

	for (idx = 0; idx < count; ++idx) {
		ret = sys->ops->execute_task(sys, task[idx]);
		if (ret)
			goto p_err_execute;
	}

	for (idx = 0; idx < count; ++idx) {
		dsp_task_destroy(task[idx]);
		dsp_mailbox_free_pool(pool[idx]);
	}

	vfree(task);
	vfree(pool);
	dsp_leave();
	return;
p_err_execute:
	for (fail = 0; fail < count; ++fail)
		dsp_task_destroy(task[fail]);
p_err_create:
	for (fail = 0; fail < count; ++fail)
		dsp_mailbox_free_pool(pool[fail]);
p_err_alloc:
	vfree(task);
p_err_task:
	vfree(pool);
p_err_pool:
	dsp_err("mailbox test is fail(%u/%d)\n", count, idx);
}

static ssize_t dsp_hw_debug_mailbox_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	struct seq_file *file;
	struct dsp_hw_debug *debug;
	unsigned int mail_count;

	dsp_enter();
	file = filp->private_data;
	debug = file->private;

	ret = kstrtouint_from_user(user_buf, count, 0, &mail_count);
	if (ret) {
		dsp_err("Failed to get user parameter(%d)\n", ret);
		goto p_err;
	}

	if (!mail_count) {
		ret = -EINVAL;
		dsp_err("mail count must be bigger than zero\n");
		goto p_err;
	}

	ret = dsp_device_open(debug->dspdev);
	if (ret)
		goto p_err_open;

	ret = dsp_device_start(debug->dspdev, 0);
	if (ret)
		goto p_err_start;

	__dsp_hw_debug_mailbox_test(debug->dspdev, mail_count);

	dsp_device_stop(debug->dspdev, 1);
	dsp_device_close(debug->dspdev);

	dsp_leave();
	return count;
p_err_start:
	dsp_device_close(debug->dspdev);
p_err_open:
p_err:
	return ret;
}

static const struct file_operations dsp_hw_debug_mailbox_fops = {
	.open		= dsp_hw_debug_mailbox_open,
	.read		= seq_read,
	.write		= dsp_hw_debug_mailbox_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int dsp_hw_debug_userdefined_show(struct seq_file *file, void *unused)
{
	struct dsp_hw_debug *debug;
	struct dsp_system *sys;

	dsp_enter();
	debug = file->private;
	sys = &debug->dspdev->system;

	seq_printf(file, "USERDEFINED : base_addr(%#x/%#x)/size(%u words)\n",
			(int)(sys->sfr_pa + DSP_O1_SM_USERDEFINED_BASE),
			DSP_O1_HW_BASE_ADDR + DSP_O1_SM_USERDEFINED_BASE,
			DSP_O1_SM_USERDEFINED_COUNT);

	seq_puts(file, "Command to change userdefined value\n");
	seq_puts(file, " echo {num} {0x(val)} > /d/dsp/hardware/userdefined\n");
	dsp_leave();
	return 0;
}

static int dsp_hw_debug_userdefined_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, dsp_hw_debug_userdefined_show,
			inode->i_private);
}

static ssize_t dsp_hw_debug_userdefined_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	struct seq_file *file;
	struct dsp_hw_debug *debug;
	struct dsp_device *dspdev;
	char buf[30];
	ssize_t size;
	unsigned int num, val;

	dsp_enter();
	file = filp->private_data;
	debug = file->private;
	dspdev = debug->dspdev;

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		dsp_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = sscanf(buf, "%u 0x%x", &num, &val);
	if (ret != 2) {
		dsp_err("Failed to get userdefined parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	if (num >= DSP_O1_SM_USERDEFINED_COUNT) {
		ret = -EINVAL;
		dsp_err("num(%u) of command is invalid(0 ~ %u)\n",
				num, DSP_O1_SM_USERDEFINED_COUNT - 1);
		goto p_err;
	}

	mutex_lock(&dspdev->lock);
	if (dsp_device_power_active(dspdev)) {
		dsp_info("USERDEFINED(%u) is changed from %#x to %#x\n",
				num,
				dsp_ctrl_sm_readl(DSP_O1_SM_USERDEFINED(num)),
				val);
		dsp_ctrl_sm_writel(DSP_O1_SM_USERDEFINED(num), val);
	} else {
		dsp_err("Failed to change userdefined as power is off\n");
	}
	mutex_unlock(&dspdev->lock);

	dsp_leave();
	return count;
p_err:
	return ret;
}

static const struct file_operations dsp_hw_debug_userdefined_fops = {
	.open		= dsp_hw_debug_userdefined_open,
	.read		= seq_read,
	.write		= dsp_hw_debug_userdefined_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int dsp_hw_debug_dump_value_show(struct seq_file *file, void *unused)
{
	struct dsp_hw_debug *debug;
	struct dsp_system *sys;

	dsp_enter();
	debug = file->private;
	sys = &debug->dspdev->system;

	seq_puts(file, "How to change dump_value\n");
	seq_puts(file, "- $echo ${dump_value} > /d/dsp/hardware/dump_value\n");
	seq_puts(file, "- ${dump_value} should be HEX like 0x0000\n");

	dsp_dump_print_status_user(file);

	dsp_leave();
	return 0;
}

static int dsp_hw_debug_dump_value_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, dsp_hw_debug_dump_value_show,
			inode->i_private);
}

static ssize_t dsp_hw_debug_dump_value_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	struct seq_file *file;
	struct dsp_hw_debug *debug;
	struct dsp_device *dspdev;
	char buf[30];
	ssize_t size;
	unsigned int dump_value;

	dsp_enter();
	file = filp->private_data;
	debug = file->private;
	dspdev = debug->dspdev;

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		dsp_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = sscanf(buf, "0x%x", &dump_value);
	if (ret != 1) {
		dsp_err("Failed to get dump_value parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	mutex_lock(&dspdev->lock);
	if (dspdev->open_count) {
		dsp_err("device already opened (%u/%u)\n",
				dspdev->open_count, dspdev->start_count);
	} else {
		dsp_dump_set_value(dump_value);
		dsp_dump_print_value();
	}
	mutex_unlock(&dspdev->lock);

	dsp_leave();
	return count;
p_err:
	return ret;
}

static const struct file_operations dsp_hw_debug_dump_value_fops = {
	.open		= dsp_hw_debug_dump_value_open,
	.read		= seq_read,
	.write		= dsp_hw_debug_dump_value_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int dsp_hw_debug_firmware_mode_show(struct seq_file *file, void *unused)
{
	struct dsp_hw_debug *debug;
	struct dsp_system *sys;

	dsp_enter();
	debug = file->private;
	sys = &debug->dspdev->system;

	if (sys->fw_postfix[0] == '\0')
		seq_puts(file, "Firmware mode : normal (none postfix)\n");
	else
		seq_printf(file, "Firmware mode : %s\n", sys->fw_postfix);

	seq_puts(file, "Command to control firmware mode\n");
	seq_puts(file, " echo {postfix} > /d/dsp/hardware/firmware_mode\n");
	seq_puts(file, "  : load [FW]_{postfix}.bin instead of [FW].bin\n");
	seq_puts(file, " echo > /d/dsp/hardware/firwmare_mode\n");
	seq_puts(file, "  : revert to loading [FW].bin\n");

	dsp_leave();
	return 0;
}

static int dsp_hw_debug_firmware_mode_open(struct inode *inode,
		struct file *filp)
{
	return single_open(filp, dsp_hw_debug_firmware_mode_show,
			inode->i_private);
}

static ssize_t dsp_hw_debug_firmware_mode_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	struct seq_file *file;
	struct dsp_hw_debug *debug;
	struct dsp_system *sys;
	char command[32];
	ssize_t size;

	dsp_enter();
	file = filp->private_data;
	debug = file->private;
	sys = &debug->dspdev->system;

	size = simple_write_to_buffer(command, sizeof(command), ppos,
			user_buf, count);
	if (size <= 0) {
		ret = size;
		dsp_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}

	if (size > sizeof(sys->fw_postfix)) {
		ret = -EINVAL;
		dsp_err("command size is invalid(%zu > %zu)\n",
				size + 1, sizeof(sys->fw_postfix));
		goto p_err;
	}

	command[size - 1] = '\0';
	if (sys->fw_postfix[0] == '\0')
		dsp_info("Firmware mode is changed [normal] -> [%s]\n",
				command);
	else
		dsp_info("Firmware mode is changed [%s] -> [%s]\n",
				sys->fw_postfix, command);

	memcpy(sys->fw_postfix, command, size);
	dsp_leave();
	return count;
p_err:
	return ret;
}

static const struct file_operations dsp_hw_debug_firmware_mode_fops = {
	.open		= dsp_hw_debug_firmware_mode_open,
	.read		= seq_read,
	.write		= dsp_hw_debug_firmware_mode_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int dsp_hw_debug_bus_show(struct seq_file *file, void *unused)
{
	struct dsp_hw_debug *debug;
	struct dsp_bus *bus;
	int idx;

	dsp_enter();
	debug = file->private;
	bus = &debug->dspdev->system.bus;

	seq_printf(file, "DSP mo scenario count[%u]\n",
			DSP_O1_MO_SCENARIO_COUNT);
	for (idx = 0; idx < DSP_O1_MO_SCENARIO_COUNT; ++idx)
		seq_printf(file, "[%d] [%32s] bts idx:%u, status:%d\n",
				idx, bus->scen[idx].name,
				bus->scen[idx].bts_scen_idx,
				bus->scen[idx].enabled);

	seq_puts(file, "Command to control DSP bus setting\n");
	seq_puts(file, " [mode 0] set mo setting\n");
	seq_puts(file, "  echo 0 {scen_name} > /d/dsp/hardware/bus\n");
	seq_puts(file, " [mode 1] unset mo setting\n");
	seq_puts(file, "  echo 1 {scen_name} > /d/dsp/hardware/bus\n");

	dsp_leave();
	return 0;
}

static int dsp_hw_debug_bus_open(struct inode *inode,
		struct file *filp)
{
	return single_open(filp, dsp_hw_debug_bus_show, inode->i_private);
}

static ssize_t dsp_hw_debug_bus_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	struct seq_file *file;
	struct dsp_hw_debug *debug;
	struct dsp_bus *bus;
	char buf[30];
	ssize_t size;
	int mode;
	char bus_scen[DSP_BUS_SCENARIO_NAME_LEN];

	dsp_enter();
	file = filp->private_data;
	debug = file->private;
	bus = &debug->dspdev->system.bus;

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		dsp_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = sscanf(buf, "%d %s", &mode, bus_scen);
	if (ret != 2) {
		dsp_err("Failed to get bus parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	switch (mode) {
	case 0:
		bus->ops->mo_get_by_name(bus, bus_scen);
		break;
	case 1:
		bus->ops->mo_put_by_name(bus, bus_scen);
		break;
	default:
		ret = -EINVAL;
		dsp_err("mode for bus setting is invalid(%d)\n", mode);
		goto p_err;
	}

	dsp_leave();
	return count;
p_err:
	return ret;
}

static const struct file_operations dsp_hw_debug_bus_fops = {
	.open		= dsp_hw_debug_bus_open,
	.read		= seq_read,
	.write		= dsp_hw_debug_bus_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static struct dsp_control_preset test_req;

static int dsp_hw_debug_governor_show(struct seq_file *file, void *unused)
{
	dsp_enter();
	seq_printf(file, "[test_req] [0][%24s] : %d\n",
			"async", test_req.async);
	seq_printf(file, "[test_req] [1][%24s] : %d\n",
			"big_core_level", test_req.big_core_level);
	seq_printf(file, "[test_req] [2][%24s] : %d\n",
			"middle_core_level", test_req.middle_core_level);
	seq_printf(file, "[test_req] [3][%24s] : %d\n",
			"little_core_level", test_req.little_core_level);
	seq_printf(file, "[test_req] [4][%24s] : %d\n",
			"mif_level", test_req.mif_level);
	seq_printf(file, "[test_req] [5][%24s] : %d\n",
			"int_level", test_req.int_level);
	seq_printf(file, "[test_req] [6][%24s] : %d\n",
			"mo_scenario_id", test_req.mo_scenario_id);
	seq_printf(file, "[test_req] [7][%24s] : %d\n",
			"llc_scenario_id", test_req.llc_scenario_id);
	seq_printf(file, "[test_req] [8][%24s] : %d\n",
			"dvfs_ctrl", test_req.dvfs_ctrl);
	dsp_leave();
	return 0;
}

static int dsp_hw_debug_governor_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, dsp_hw_debug_governor_show, inode->i_private);
}

static int dsp_hw_debug_governor_test(struct seq_file *file)
{
	int ret;
	struct dsp_hw_debug *debug;
	struct dsp_governor *governor;

	dsp_enter();
	debug = file->private;
	governor = &debug->dspdev->system.governor;

	ret = dsp_device_open(debug->dspdev);
	if (ret)
		goto p_err_open;

	ret = dsp_device_start(debug->dspdev, 0);
	if (ret)
		goto p_err_start;

	governor->ops->request(governor, &test_req);
	dsp_device_stop(debug->dspdev, 1);
	dsp_device_close(debug->dspdev);

	dsp_leave();
	return 0;
p_err_start:
	dsp_device_close(debug->dspdev);
p_err_open:
	return 0;
}

static ssize_t dsp_hw_debug_governor_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	struct seq_file *file;
	char buf[30];
	ssize_t size;
	int mode, num1;

	dsp_enter();
	file = filp->private_data;

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		dsp_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = sscanf(buf, "%d %d", &mode, &num1);
	if (ret != 2) {
		dsp_err("Failed to get governor parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	switch (mode) {
	case 0:
		test_req.async = num1;
		break;
	case 1:
		test_req.big_core_level = num1;
		break;
	case 2:
		test_req.middle_core_level = num1;
		break;
	case 3:
		test_req.little_core_level = num1;
		break;
	case 4:
		test_req.mif_level = num1;
		break;
	case 5:
		test_req.int_level = num1;
		break;
	case 6:
		test_req.mo_scenario_id = num1;
		break;
	case 7:
		test_req.llc_scenario_id = num1;
		break;
	case 8:
		test_req.dvfs_ctrl = num1;
		break;
	case 9:
		dsp_hw_debug_governor_test(file);
		break;
	default:
		ret = -EINVAL;
		dsp_err("mode for governor req setting is invalid(%d)\n", mode);
		goto p_err;
	}

	dsp_leave();
	return count;
p_err:
	return ret;
}

static const struct file_operations dsp_hw_debug_governor_fops = {
	.open		= dsp_hw_debug_governor_open,
	.read		= seq_read,
	.write		= dsp_hw_debug_governor_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int dsp_hw_debug_llc_show(struct seq_file *file, void *unused)
{
	struct dsp_hw_debug *debug;
	struct dsp_llc *llc;
	struct dsp_llc_region *region;
	int idx, jdx;

	dsp_enter();
	debug = file->private;
	llc = &debug->dspdev->system.llc;

	seq_printf(file, "DSP llc scenario count[%u]\n",
			DSP_O1_LLC_SCENARIO_COUNT);
	for (idx = 0; idx < DSP_O1_LLC_SCENARIO_COUNT; ++idx) {
		seq_printf(file, "[%d] [%15s] : status:%d, region_count:%u\n",
				idx, llc->scen[idx].name,
				llc->scen[idx].enabled,
				llc->scen[idx].region_count);
		for (jdx = 0; jdx < llc->scen[idx].region_count; ++jdx) {
			region = &llc->scen[idx].region_list[jdx];
			if (region->idx)
				seq_printf(file, "[%d] region[%d] : %s/%u/%u\n",
						idx, jdx,
						region->name,
						region->idx,
						region->way);
			else
				seq_printf(file, "region(%u) idx is not set\n",
						jdx);
		}
	}

	seq_puts(file, "Command to control DSP llc setting\n");
	seq_puts(file, " [mode 0] set llc setting\n");
	seq_puts(file, "  echo 0 {scen_name} > /d/dsp/hardware/llc\n");
	seq_puts(file, " [mode 1] unset llc setting\n");
	seq_puts(file, "  echo 1 {scen_name} > /d/dsp/hardware/llc\n");

	dsp_leave();
	return 0;
}

static int dsp_hw_debug_llc_open(struct inode *inode,
		struct file *filp)
{
	return single_open(filp, dsp_hw_debug_llc_show, inode->i_private);
}

static ssize_t dsp_hw_debug_llc_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	struct seq_file *file;
	struct dsp_hw_debug *debug;
	struct dsp_llc *llc;
	char buf[30];
	ssize_t size;
	int mode;
	char llc_scen[DSP_LLC_SCENARIO_NAME_LEN];

	dsp_enter();
	file = filp->private_data;
	debug = file->private;
	llc = &debug->dspdev->system.llc;

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		dsp_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = sscanf(buf, "%d %s", &mode, llc_scen);
	if (ret != 2) {
		dsp_err("Failed to get llc parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	switch (mode) {
	case 0:
		llc->ops->llc_get_by_name(llc, llc_scen);
		break;
	case 1:
		llc->ops->llc_put_by_name(llc, llc_scen);
		break;
	default:
		ret = -EINVAL;
		dsp_err("mode for llc setting is invalid(%d)\n", mode);
		goto p_err;
	}

	dsp_leave();
	return count;
p_err:
	return ret;
}

static const struct file_operations dsp_hw_debug_llc_fops = {
	.open		= dsp_hw_debug_llc_open,
	.read		= seq_read,
	.write		= dsp_hw_debug_llc_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static void dsp_hw_debug_log_print(struct timer_list *t)
{
	int ret;
	struct dsp_hw_debug_log *log;
	char line[DSP_HW_DEBUG_LOG_LINE_SIZE];
	int count = 0;

	log = from_timer(log, t, timer);

	while (true) {
		/* Restricted from working too long */
		if (count == dsp_util_queue_read_count(log->queue))
			break;

		if (dsp_util_queue_check_empty(log->queue))
			break;

		ret = dsp_util_queue_dequeue(log->queue, line,
				DSP_HW_DEBUG_LOG_LINE_SIZE);
		if (ret)
			break;

		line[DSP_HW_DEBUG_LOG_LINE_SIZE - 2] = '\n';
		line[DSP_HW_DEBUG_LOG_LINE_SIZE - 1] = '\0';

		dsp_info("[timer(%4d)] %s", readl(&log->queue->front), line);
		count++;
	}

	mod_timer(&log->timer,
			jiffies + msecs_to_jiffies(DSP_HW_DEBUG_LOG_TIME));
}

static void dsp_hw_o1_debug_log_flush(struct dsp_hw_debug *debug)
{
	int ret;
	struct dsp_hw_debug_log *log;
	char line[DSP_HW_DEBUG_LOG_LINE_SIZE];

	dsp_enter();
	log = debug->log;

	if (!log->queue)
		return;

	while (true) {
		if (dsp_util_queue_check_empty(log->queue))
			break;

		ret = dsp_util_queue_dequeue(log->queue, line,
				DSP_HW_DEBUG_LOG_LINE_SIZE);
		if (ret)
			break;

		line[DSP_HW_DEBUG_LOG_LINE_SIZE - 2] = '\n';
		line[DSP_HW_DEBUG_LOG_LINE_SIZE - 1] = '\0';

		dsp_info("[flush(%4d)] %s", readl(&log->queue->front), line);
	}
	dsp_leave();
}

static int dsp_hw_o1_debug_log_start(struct dsp_hw_debug *debug)
{
	struct dsp_hw_debug_log *log;
	struct dsp_system *sys;
	struct dsp_priv_mem *pmem;

	dsp_enter();
	log = debug->log;
	sys = &debug->dspdev->system;

	pmem = &sys->memory.priv_mem[DSP_O1_PRIV_MEM_DHCP];
	log->queue = pmem->kvaddr + DSP_O1_DHCP_IDX(DSP_O1_DHCP_LOG_QUEUE);

	pmem = &sys->memory.priv_mem[DSP_O1_PRIV_MEM_FW_LOG];
	dsp_util_queue_init(log->queue, DSP_HW_DEBUG_LOG_LINE_SIZE,
			pmem->size, (unsigned int)pmem->iova,
			(unsigned long long)pmem->kvaddr);

	mod_timer(&log->timer,
			jiffies + msecs_to_jiffies(DSP_HW_DEBUG_LOG_TIME));
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_debug_log_stop(struct dsp_hw_debug *debug)
{
	dsp_enter();
	del_timer_sync(&debug->log->timer);
	debug->ops->log_flush(debug);
	debug->log->queue = NULL;
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_debug_open(struct dsp_hw_debug *debug)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_debug_close(struct dsp_hw_debug *debug)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static void __dsp_hw_debug_log_init(struct dsp_hw_debug *debug)
{
	struct dsp_hw_debug_log *log;

	dsp_enter();
	log = debug->log;

	timer_setup(&log->timer, dsp_hw_debug_log_print, 0);
	dsp_leave();
}

static int dsp_hw_o1_debug_probe(struct dsp_device *dspdev)
{
	int ret;
	struct dsp_hw_debug *debug;
	struct dsp_hw_o1_debug *debug_sub;

	dsp_enter();
	ret = dsp_hw_common_debug_probe(dspdev);
	if (ret)
		goto p_err;

	debug = &dspdev->system.debug;

	debug_sub = kzalloc(sizeof(*debug_sub), GFP_KERNEL);
	if (!debug_sub) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc debug_sub\n");
		goto p_err_debug_sub;
	}
	debug->sub_data = debug_sub;

	debug->power = debugfs_create_file("power", 0640, debug->root, debug,
			&dsp_hw_debug_power_fops);
	if (!debug->power)
		dsp_warn("Failed to create power debugfs file\n");

	debug->clk = debugfs_create_file("clk", 0640, debug->root, debug,
			&dsp_hw_debug_clk_fops);
	if (!debug->clk)
		dsp_warn("Failed to create clk debugfs file\n");

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	debug->devfreq = debugfs_create_file("devfreq", 0640, debug->root,
			debug, &dsp_hw_debug_devfreq_fops);
	if (!debug->devfreq)
		dsp_warn("Failed to create devfreq debugfs file\n");
#endif

	debug->sfr = debugfs_create_file("sfr", 0640, debug->root, debug,
			&dsp_hw_debug_sfr_fops);
	if (!debug->sfr)
		dsp_warn("Failed to create sfr debugfs file\n");

	debug->mem = debugfs_create_file("mem", 0640, debug->root, debug,
			&dsp_hw_debug_mem_fops);
	if (!debug->mem)
		dsp_warn("Failed to create mem debugfs file\n");

	debug->fw_log = debugfs_create_file("fw_log", 0640, debug->root, debug,
			&dsp_hw_debug_fw_log_fops);
	if (!debug->fw_log)
		dsp_warn("Failed to create fw_log debugfs file\n");

	debug->wait_ctrl = debugfs_create_file("wait_ctrl", 0640, debug->root,
			debug, &dsp_hw_debug_wait_ctrl_fops);
	if (!debug->wait_ctrl)
		dsp_warn("Failed to create wait_ctrl debugfs file\n");

	debug->layer_range = debugfs_create_file("layer_range", 0640,
			debug->root, debug, &dsp_hw_debug_layer_range_fops);
	if (!debug->layer_range)
		dsp_warn("Failed to create layer_range debugfs file\n");

	debug->mailbox = debugfs_create_file("mailbox", 0640, debug->root,
			debug, &dsp_hw_debug_mailbox_fops);
	if (!debug->mailbox)
		dsp_warn("Failed to create mailbox debugfs file\n");

	debug->userdefined = debugfs_create_file("userdefined", 0640,
			debug->root, debug, &dsp_hw_debug_userdefined_fops);
	if (!debug->userdefined)
		dsp_warn("Failed to create userdefined debugfs file\n");

	debug->dump_value = debugfs_create_file("dump_value", 0640,
			debug->root, debug, &dsp_hw_debug_dump_value_fops);
	if (!debug->dump_value)
		dsp_warn("Failed to create dump_value debugfs file\n");

	debug->firmware_mode = debugfs_create_file("firmware_mode", 0640,
			debug->root, debug, &dsp_hw_debug_firmware_mode_fops);
	if (!debug->firmware_mode)
		dsp_warn("Failed to create firmware_mode debugfs file\n");

	debug->bus = debugfs_create_file("bus", 0640, debug->root, debug,
			&dsp_hw_debug_bus_fops);
	if (!debug->bus)
		dsp_warn("Failed to create bus debugfs file\n");

	debug->governor = debugfs_create_file("governor", 0640, debug->root,
			debug, &dsp_hw_debug_governor_fops);
	if (!debug->governor)
		dsp_warn("Failed to create governor debugfs file\n");

	debug_sub->llc = debugfs_create_file("llc", 0640, debug->root, debug,
			&dsp_hw_debug_llc_fops);
	if (!debug_sub->llc)
		dsp_warn("Failed to create llc debugfs file\n");

	debug->log = kzalloc(sizeof(*debug->log), GFP_KERNEL);
	if (debug->log)
		__dsp_hw_debug_log_init(debug);
	else
		dsp_warn("Failed to alloc dsp_hw_debug_log\n");

	dsp_leave();
	return 0;
p_err_debug_sub:
	dsp_hw_common_debug_remove(debug);
p_err:
	return ret;
}

static void dsp_hw_o1_debug_remove(struct dsp_hw_debug *debug)
{
	dsp_enter();
	kfree(debug->log);
	kfree(debug->sub_data);
	dsp_hw_common_debug_remove(debug);
	dsp_leave();
}

static const struct dsp_hw_debug_ops o1_hw_debug_ops = {
	.log_flush	= dsp_hw_o1_debug_log_flush,
	.log_start	= dsp_hw_o1_debug_log_start,
	.log_stop	= dsp_hw_o1_debug_log_stop,

	.open		= dsp_hw_o1_debug_open,
	.close		= dsp_hw_o1_debug_close,
	.probe		= dsp_hw_o1_debug_probe,
	.remove		= dsp_hw_o1_debug_remove,
};

int dsp_hw_o1_debug_init(void)
{
	int ret;

	dsp_enter();
	ret = dsp_hw_debug_register_ops(DSP_DEVICE_ID_O1, &o1_hw_debug_ops);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
