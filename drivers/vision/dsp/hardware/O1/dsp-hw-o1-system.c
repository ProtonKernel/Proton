// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <soc/samsung/debug-snapshot.h>
#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-device.h"
#include "dsp-binary.h"
#include "dsp-hw-o1-bus.h"
#include "dsp-hw-o1-llc.h"
#include "dsp-hw-o1-clk.h"
#include "dsp-hw-o1-ctrl.h"
#include "dsp-hw-o1-debug.h"
#include "dsp-hw-o1-dump.h"
#include "dsp-hw-o1-interface.h"
#include "dsp-hw-o1-mailbox.h"
#include "dsp-hw-o1-memory.h"
#include "dsp-hw-o1-pm.h"
#include "dsp-hw-o1-governor.h"
#include "dsp-hw-o1-system.h"

#define DSP_WAIT_BOOT_TIME	(100)
#define DSP_WAIT_MAILBOX_TIME	(1500)
#define DSP_WAIT_RESET_TIME	(100)

#define DSP_STATIC_KERNEL	(1)
#define DSP_DYNAMIC_KERNEL	(2)

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
static struct memlog_obj *dsp_memlog_log_file_obj;
#endif

static int dsp_hw_o1_system_request_control(struct dsp_system *sys,
		unsigned int id, union dsp_control *cmd)
{
	int ret;

	dsp_enter();
	switch (id) {
	case DSP_CONTROL_ENABLE_DVFS:
		ret = sys->pm.ops->dvfs_enable(&sys->pm, cmd->dvfs.pm_qos);
		if (ret)
			goto p_err;
		break;
	case DSP_CONTROL_DISABLE_DVFS:
		ret = sys->pm.ops->dvfs_disable(&sys->pm, cmd->dvfs.pm_qos);
		if (ret)
			goto p_err;
		break;
	case DSP_CONTROL_ENABLE_BOOST:
		mutex_lock(&sys->boost_lock);
		if (!sys->boost) {
			ret = sys->pm.ops->boost_enable(&sys->pm);
			if (ret) {
				mutex_unlock(&sys->boost_lock);
				goto p_err;
			}

			ret = sys->bus.ops->mo_get_by_id(&sys->bus,
					DSP_O1_MO_MAX);
			if (ret) {
				sys->pm.ops->boost_disable(&sys->pm);
				mutex_unlock(&sys->boost_lock);
				goto p_err;
			}

			sys->boost = true;
		}
		mutex_unlock(&sys->boost_lock);
		break;
	case DSP_CONTROL_DISABLE_BOOST:
		mutex_lock(&sys->boost_lock);
		if (sys->boost) {
			sys->bus.ops->mo_put_by_id(&sys->bus, DSP_O1_MO_MAX);
			sys->pm.ops->boost_disable(&sys->pm);
			sys->boost = false;
		}
		mutex_unlock(&sys->boost_lock);
		break;
	case DSP_CONTROL_REQUEST_MO:
		ret = sys->bus.ops->mo_get_by_name(&sys->bus,
				cmd->mo.scenario_name);
		if (ret)
			goto p_err;
		break;
	case DSP_CONTROL_RELEASE_MO:
		sys->bus.ops->mo_put_by_name(&sys->bus, cmd->mo.scenario_name);
		break;
	case DSP_CONTROL_REQUEST_PRESET:
		sys->governor.ops->request(&sys->governor, &cmd->preset);
		break;
	default:
		ret = -EINVAL;
		dsp_err("control cmd id is invalid(%u)\n", id);
		goto p_err;
	}
	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_hw_o1_system_wait_task(struct dsp_system *sys,
		struct dsp_task *task)
{
	int ret;
	struct dsp_hw_o1_system *sys_sub;
	unsigned int wait_time;
	long timeout;

	dsp_enter();
	sys_sub = sys->sub_data;

	wait_time = sys->wait[DSP_SYSTEM_WAIT_MAILBOX];
#ifndef ENABLE_DSP_VELOCE
	dsp_dbg("wait mode : %u / task : %u/%u\n",
			sys_sub->wait_mode, task->id, task->message_id);
	if (sys_sub->wait_mode == DSP_HW_O1_WAIT_MODE_INTERRUPT) {
		timeout = wait_event_timeout(sys->task_manager.done_wq,
				task->state == DSP_TASK_STATE_COMPLETE,
				msecs_to_jiffies(wait_time));
		if (!timeout) {
			sys->interface.ops->check_irq(&sys->interface);
			if (task->state == DSP_TASK_STATE_COMPLETE) {
				timeout = 1;
				dsp_warn("interrupt was unstable\n");
			}
		}
	} else if (sys_sub->wait_mode == DSP_HW_O1_WAIT_MODE_BUSY_WAITING) {
		timeout = wait_time * 1000;

		while (timeout) {
			if (task->state == DSP_TASK_STATE_COMPLETE)
				break;

			sys->interface.ops->check_irq(&sys->interface);
			timeout -= 10;
			udelay(10);
		}
	} else {
		ret = -EINVAL;
		dsp_err("wait mode(%u) is invalid\n", sys_sub->wait_mode);
		goto p_err;
	}
#else
	while (1) {
		if (task->state == DSP_TASK_STATE_COMPLETE)
			break;

		dsp_ctrl_pc_dump();
		dsp_ctrl_userdefined_dump();
		mdelay(100);
	}
	timeout = 1;
#endif
	if (!timeout) {
		ret = -ETIMEDOUT;
		dsp_err("task wait time(%ums) is expired(%u/%u)\n",
				sys->wait[DSP_SYSTEM_WAIT_MAILBOX],
				task->id, task->message_id);
		if (sys->debug_mode & BIT(DSP_DEBUG_MODE_TASK_TIMEOUT_PANIC)) {
			dsp_dump_set_value(DSP_O1_DUMP_DEFAULT_VALUE |
					1 << DSP_O1_DUMP_VALUE_SDMA_ND);
			dsp_dump_ctrl();
			dbg_snapshot_expire_watchdog();
			BUG();
		} else {
			dsp_dump_ctrl();
		}
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_hw_o1_system_flush(struct dsp_system *sys)
{
	struct dsp_task_manager *tmgr;
	unsigned long flags;

	dsp_enter();
	tmgr = &sys->task_manager;

	spin_lock_irqsave(&tmgr->slock, flags);
	dsp_task_manager_set_block_mode(tmgr, true);
	dsp_task_manager_flush_process(tmgr, -ENOSTR);
	spin_unlock_irqrestore(&tmgr->slock, flags);

	sys->ops->reset(sys);
	dsp_leave();
}

static void __dsp_hw_o1_system_recovery(struct dsp_system *sys)
{
	int ret;
	struct dsp_task_manager *tmgr;
	unsigned long flags;

	dsp_enter();
	tmgr = &sys->task_manager;

	ret = sys->ops->boot(sys);
	if (ret) {
		dsp_err("Failed to recovery device\n");
		return;
	}

	dsp_graph_manager_recovery(&sys->dspdev->core.graph_manager);

	spin_lock_irqsave(&tmgr->slock, flags);
	dsp_task_manager_set_block_mode(tmgr, false);
	spin_unlock_irqrestore(&tmgr->slock, flags);
	dsp_leave();
}

static int dsp_hw_o1_system_execute_task(struct dsp_system *sys,
		struct dsp_task *task)
{
	int ret;

	dsp_enter();
	if (task->message_id == DSP_COMMON_EXECUTE_MSG)
		sys->governor.ops->check_done(&sys->governor);

	sys->pm.ops->update_devfreq_busy(&sys->pm, task->pool->pm_qos);
	ret = sys->mailbox.ops->send_task(&sys->mailbox, task);
	if (ret)
		goto p_err;

	dsp_dump_mailbox_pool_debug(task->pool);

	/* TODO Devfreq change criteria required if not waiting */
	if (task->wait) {
		ret = __dsp_hw_o1_system_wait_task(sys, task);
		if (ret) {
			if (task->recovery) {
				__dsp_hw_o1_system_flush(sys);
				__dsp_hw_o1_system_recovery(sys);
			}
			goto p_err;
		}

		if (task->result) {
			ret = task->result;
			dsp_err("task result is failure(%d/%u/%u)\n",
					ret, task->id, task->message_id);
			goto p_err;
		}
	}

	sys->pm.ops->update_devfreq_idle(&sys->pm, task->pool->pm_qos);
	task->owner->normal_count++;
	dsp_leave();
	return 0;
p_err:
	sys->pm.ops->update_devfreq_idle(&sys->pm, task->pool->pm_qos);
	task->owner->error_count++;
	dsp_dump_mailbox_pool_error(task->pool);
	dsp_dump_task_manager_count(task->owner);
	dsp_dump_kernel(&sys->dspdev->core.graph_manager.kernel_manager);
	return ret;
}

static void dsp_hw_o1_system_iovmm_fault_dump(struct dsp_system *sys)
{
	dsp_enter();
	dsp_dump_set_value(DSP_O1_DUMP_DEFAULT_VALUE |
			1 << DSP_O1_DUMP_VALUE_SDMA_ND);
	dsp_dump_ctrl();
	dsp_dump_task_manager_count(&sys->task_manager);
	dsp_dump_kernel(&sys->dspdev->core.graph_manager.kernel_manager);
	dbg_snapshot_expire_watchdog();
	dsp_leave();
}

static int __dsp_hw_o1_system_master_copy(struct dsp_system *sys)
{
	int ret;
	struct dsp_hw_o1_system *sys_sub;
	struct dsp_reserved_mem *rmem;

	dsp_enter();
	sys_sub = sys->sub_data;
	rmem = &sys->memory.reserved_mem[DSP_O1_RESERVED_MEM_FW_MASTER];

	if (!sys_sub->boot_bin_size) {
		dsp_warn("master bin is not loaded yet\n");
		ret = dsp_binary_alloc_load(DSP_O1_MASTER_FW_NAME, NULL,
				DSP_O1_FW_EXTENSION, &sys_sub->boot_bin,
				&sys_sub->boot_bin_size);
		if (ret)
			goto p_err;
	}

	if (rmem->size < sys_sub->boot_bin_size) {
		ret = -EFAULT;
		dsp_err("master bin size is over(%zu/%zu)\n",
				rmem->size, sys_sub->boot_bin_size);
		goto p_err;
	}

	memcpy(rmem->kvaddr, sys_sub->boot_bin, sys_sub->boot_bin_size);
	rmem->used_size = sys_sub->boot_bin_size;

#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
	sys_sub->loader_desc.skip_request_firmware = true;
	ret = imgloader_boot(&sys_sub->loader_desc);
	if (ret) {
		dsp_err("Failed to boot imgloader(%d/%llx/%zu/%zu)\n",
				ret, rmem->paddr, rmem->used_size,
				rmem->size);
		goto p_err;
	}
#endif

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_hw_o1_system_init(struct dsp_system *sys)
{
	struct dsp_priv_mem *pmem;
	struct dsp_hw_o1_system *sys_sub;
	unsigned int chip_id;
	unsigned int product_id;
	struct dsp_reserved_mem *rmem;

	dsp_enter();
	pmem = sys->memory.priv_mem;
	sys_sub = sys->sub_data;

	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_TO_CC_INT_STATUS), 0);
	dsp_ctrl_dhcp_writel(
			DSP_O1_DHCP_IDX(DSP_O1_DHCP_TO_HOST_INT_STATUS), 0);

	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_DEBUG_LAYER_START),
			sys->layer_start);
	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_DEBUG_LAYER_END),
			sys->layer_end);

	memcpy(pmem[DSP_O1_PRIV_MEM_FW].kvaddr,
			pmem[DSP_O1_PRIV_MEM_FW].bac_kvaddr,
			pmem[DSP_O1_PRIV_MEM_FW].used_size);

	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_FW_RESERVED_SIZE),
			pmem[DSP_O1_PRIV_MEM_FW].size);
	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_IVP_PM_IOVA),
			pmem[DSP_O1_PRIV_MEM_IVP_PM].iova);
	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_IVP_PM_SIZE),
			pmem[DSP_O1_PRIV_MEM_IVP_PM].used_size);
	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_IVP_DM_IOVA),
			pmem[DSP_O1_PRIV_MEM_IVP_DM].iova);
	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_IVP_DM_SIZE),
			pmem[DSP_O1_PRIV_MEM_IVP_DM].used_size);
	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_IAC_PM_IOVA),
			pmem[DSP_O1_PRIV_MEM_IAC_PM].iova);
	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_IAC_PM_SIZE),
			pmem[DSP_O1_PRIV_MEM_IAC_PM].used_size);
	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_IAC_DM_IOVA),
			pmem[DSP_O1_PRIV_MEM_IAC_DM].iova);
	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_IAC_DM_SIZE),
			pmem[DSP_O1_PRIV_MEM_IAC_DM].used_size);

	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_MAILBOX_VERSION), 0);
	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_MESSAGE_VERSION), 0);

	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_FW_LOG_MEMORY_IOVA),
			pmem[DSP_O1_PRIV_MEM_FW_LOG].iova);
	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_FW_LOG_MEMORY_SIZE),
			pmem[DSP_O1_PRIV_MEM_FW_LOG].size);
	dsp_ctrl_dhcp_writel(
			DSP_O1_DHCP_IDX(DSP_O1_DHCP_TO_CC_MBOX_MEMORY_IOVA),
			pmem[DSP_O1_PRIV_MEM_MBOX_MEMORY].iova);
	dsp_ctrl_dhcp_writel(
			DSP_O1_DHCP_IDX(DSP_O1_DHCP_TO_CC_MBOX_MEMORY_SIZE),
			pmem[DSP_O1_PRIV_MEM_MBOX_MEMORY].size);
	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_TO_CC_MBOX_POOL_IOVA),
			pmem[DSP_O1_PRIV_MEM_MBOX_POOL].iova);
	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_TO_CC_MBOX_POOL_SIZE),
			pmem[DSP_O1_PRIV_MEM_MBOX_POOL].size);

	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_KERNEL_MODE),
			DSP_DYNAMIC_KERNEL);
	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_DL_OUT_IOVA),
			pmem[DSP_O1_PRIV_MEM_DL_OUT].iova);
	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_DL_OUT_SIZE),
			pmem[DSP_O1_PRIV_MEM_DL_OUT].size);

#ifndef ENABLE_DSP_VELOCE
	chip_id = readl(sys_sub->chip_id);
#else
	chip_id = 0xdeadbeef;
#endif
	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_CHIPID_REV), chip_id);
	dsp_info("CHIPID : %#x\n", chip_id);

#ifndef ENABLE_DSP_VELOCE
	product_id = readl(sys_sub->product_id);
#else
	product_id = 0xdeadbeef;
#endif
	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_PRODUCT_ID),
			product_id);
	dsp_info("PRODUCT_ID : %#x\n", product_id);

	rmem = sys->memory.reserved_mem;
	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_PAYLOAD_SHMEM_IOVA),
			rmem[DSP_O1_RESERVED_MEM_SHMEM].iova);
	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_PAYLOAD_SHMEM_SIZE),
			rmem[DSP_O1_RESERVED_MEM_SHMEM].size);

	if (sys_sub->wait_mode == DSP_HW_O1_WAIT_MODE_INTERRUPT)
		dsp_ctrl_writel(DSP_O1_SYSC_NS_INTR_MBOX_ENABLE_FR_CC, 0x11);
	else if (sys_sub->wait_mode == DSP_HW_O1_WAIT_MODE_BUSY_WAITING)
		dsp_ctrl_writel(DSP_O1_SYSC_NS_INTR_MBOX_ENABLE_FR_CC, 0x0);

	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_INTERRUPT_MODE),
			sys_sub->wait_mode);
	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_DRIVER_VERSION),
			sys->dspdev->version);
	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_FIRMWARE_VERSION),
			0xdeadbeef);
	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_RESERVED0),
			0xdeadbeef);
	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_RESERVED1),
			0xdeadbeef);
	dsp_ctrl_dhcp_writel(DSP_O1_DHCP_IDX(DSP_O1_DHCP_RESERVED2),
			0xdeadbeef);

	dsp_leave();
}

static int __dsp_hw_o1_system_wait_boot(struct dsp_system *sys)
{
	int ret;
	struct dsp_hw_o1_system *sys_sub;
	unsigned int wait_time;
	long timeout;

	dsp_enter();
	sys_sub = sys->sub_data;

	wait_time = sys->wait[DSP_SYSTEM_WAIT_BOOT];
#ifndef ENABLE_DSP_VELOCE
	dsp_dbg("[boot] wait mode : %u\n", sys_sub->wait_mode);
	if (sys_sub->wait_mode == DSP_HW_O1_WAIT_MODE_INTERRUPT) {
		timeout = wait_event_timeout(sys->system_wq,
				sys->system_flag & BIT(DSP_SYSTEM_BOOT),
				msecs_to_jiffies(wait_time));
		if (!timeout) {
			sys->interface.ops->check_irq(&sys->interface);
			if (sys->system_flag & BIT(DSP_SYSTEM_BOOT)) {
				timeout = 1;
				dsp_warn("interrupt was unstable\n");
			}
		}
	} else if (sys_sub->wait_mode == DSP_HW_O1_WAIT_MODE_BUSY_WAITING) {
		timeout = wait_time * 1000;

		while (timeout) {
			if (sys->system_flag & BIT(DSP_SYSTEM_BOOT))
				break;

			sys->interface.ops->check_irq(&sys->interface);
			timeout -= 10;
			udelay(10);
		}
	} else {
		ret = -EINVAL;
		dsp_err("wait mode(%u) is invalid\n", sys_sub->wait_mode);
		goto p_err;
	}
#else
	while (1) {
		if (sys->system_flag & BIT(DSP_SYSTEM_BOOT))
			break;

		dsp_ctrl_pc_dump();
		dsp_ctrl_userdefined_dump();
		mdelay(100);
	}
	timeout = 1;
#endif
	if (!timeout) {
		ret = -ETIMEDOUT;
		dsp_err("Failed to boot DSP (wait time %ums)\n",
				sys->wait[DSP_SYSTEM_WAIT_BOOT]);
		if (sys->debug_mode & BIT(DSP_DEBUG_MODE_BOOT_TIMEOUT_PANIC)) {
			dsp_dump_set_value(DSP_O1_DUMP_DEFAULT_VALUE |
					1 << DSP_O1_DUMP_VALUE_SDMA_ND);
			dsp_dump_ctrl();
			dbg_snapshot_expire_watchdog();
			BUG();
		} else {
			dsp_dump_ctrl();
		}
		goto p_err;
	} else {
		dsp_info("Completed to boot DSP\n");
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_hw_o1_system_check_kernel_mode(struct dsp_system *sys)
{
	int kernel_mode;

	dsp_enter();
	kernel_mode = dsp_ctrl_dhcp_readl(
			DSP_O1_DHCP_IDX(DSP_O1_DHCP_KERNEL_MODE));
	if (kernel_mode != DSP_DYNAMIC_KERNEL) {
		dsp_err("static kernel is no longer available\n");
		return -EINVAL;
	}
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_system_boot(struct dsp_system *sys)
{
	int ret;
	struct dsp_hw_o1_system *sys_sub;

	dsp_enter();
	sys_sub = sys->sub_data;
	sys->system_flag = 0x0;

	sys->ctrl.ops->all_init(&sys->ctrl);
	ret = __dsp_hw_o1_system_master_copy(sys);
	if (ret)
		goto p_err;

	__dsp_hw_o1_system_init(sys);
	sys->debug.ops->log_start(&sys->debug);

	sys->pm.ops->update_devfreq_boot(&sys->pm);
	sys->ctrl.ops->start(&sys->ctrl);
	ret = __dsp_hw_o1_system_wait_boot(sys);
	sys->pm.ops->update_devfreq_min(&sys->pm);
	if (ret)
		goto p_err_reset;

	dsp_ctrl_writel(DSP_O1_SYSC_NS_CLOCK_GATE_EN, 0xffffffff);

	ret = __dsp_hw_o1_system_check_kernel_mode(sys);
	if (ret)
		goto p_err_reset;

	ret = sys->mailbox.ops->start(&sys->mailbox);
	if (ret)
		goto p_err_reset;

	sys->boot_init |= BIT(DSP_SYSTEM_DSP_INIT);
	dsp_leave();
	return 0;
p_err_reset:
	sys->ops->reset(sys);
p_err:
	return ret;
}

static int __dsp_hw_o1_system_wait_reset(struct dsp_system *sys)
{
	int ret;
	struct dsp_hw_o1_system *sys_sub;
	unsigned int wait_time;
	long timeout;

	dsp_enter();
	sys_sub = sys->sub_data;

	wait_time = sys->wait[DSP_SYSTEM_WAIT_RESET];
#ifndef ENABLE_DSP_VELOCE
	dsp_dbg("[reset] wait mode : %u\n", sys_sub->wait_mode);
	if (sys_sub->wait_mode == DSP_HW_O1_WAIT_MODE_INTERRUPT) {
		timeout = wait_event_timeout(sys->system_wq,
				sys->system_flag & BIT(DSP_SYSTEM_RESET),
				msecs_to_jiffies(wait_time));
		if (!timeout) {
			sys->interface.ops->check_irq(&sys->interface);
			if (sys->system_flag & BIT(DSP_SYSTEM_RESET)) {
				timeout = 1;
				dsp_warn("interrupt was unstable\n");
			}
		}
	} else if (sys_sub->wait_mode == DSP_HW_O1_WAIT_MODE_BUSY_WAITING) {
		timeout = wait_time * 1000;

		while (timeout) {
			if (sys->system_flag & BIT(DSP_SYSTEM_RESET))
				break;

			sys->interface.ops->check_irq(&sys->interface);
			timeout -= 10;
			udelay(10);
		}
	} else {
		ret = -EINVAL;
		dsp_err("wait mode(%u) is invalid\n", sys_sub->wait_mode);
		goto p_err;
	}
#else
	while (1) {
		if (sys->system_flag & BIT(DSP_SYSTEM_RESET))
			break;

		dsp_ctrl_pc_dump();
		dsp_ctrl_userdefined_dump();
		mdelay(100);
	}
	timeout = 1;
#endif
	if (!timeout) {
		ret = -ETIMEDOUT;
		dsp_err("Failed to reset DSP (wait time %ums)\n",
				sys->wait[DSP_SYSTEM_WAIT_RESET]);
		if (sys->debug_mode & BIT(DSP_DEBUG_MODE_RESET_TIMEOUT_PANIC)) {
			dsp_dump_set_value(DSP_O1_DUMP_DEFAULT_VALUE |
					1 << DSP_O1_DUMP_VALUE_SDMA_ND);
			dsp_dump_ctrl();
			dbg_snapshot_expire_watchdog();
			BUG();
		} else {
			dsp_dump_ctrl();
		}
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_o1_system_reset(struct dsp_system *sys)
{
	int ret;
	struct dsp_hw_o1_system *sys_sub;

	dsp_enter();
	sys_sub = sys->sub_data;

	/* clock gating of CPU_SS is disabled because of HW bug */
	dsp_ctrl_writel(DSP_O1_SYSC_NS_CLOCK_GATE_EN, 0xfffffffb);
	sys->system_flag = 0x0;
	sys->pm.ops->update_devfreq_boot(&sys->pm);
	sys->interface.ops->send_irq(&sys->interface, BIT(DSP_TO_CC_INT_RESET));
	ret = __dsp_hw_o1_system_wait_reset(sys);
	if (ret)
		sys->ctrl.ops->force_reset(&sys->ctrl);
	else
		sys->ctrl.ops->reset(&sys->ctrl);
	sys->pm.ops->update_devfreq_min(&sys->pm);

	sys->mailbox.ops->stop(&sys->mailbox);
	sys->debug.ops->log_stop(&sys->debug);
	sys->boot_init &= ~BIT(DSP_SYSTEM_DSP_INIT);
#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
	imgloader_shutdown(&sys_sub->loader_desc);
#endif
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_system_power_active(struct dsp_system *sys)
{
	dsp_check();
	return sys->pm.ops->devfreq_active(&sys->pm);
}

static int dsp_hw_o1_system_set_boot_qos(struct dsp_system *sys, int val)
{
	int ret;

	dsp_enter();
	ret = sys->pm.ops->set_boot_qos(&sys->pm, val);
	if (ret)
		goto p_err;
	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_o1_system_runtime_resume(struct dsp_system *sys)
{
	int ret;
	struct dsp_hw_o1_system *sys_sub;

	dsp_enter();
	sys_sub = sys->sub_data;

	ret = sys->pm.ops->enable(&sys->pm);
	if (ret)
		goto p_err_pm;

	ret = sys->clk.ops->enable(&sys->clk);
	if (ret)
		goto p_err_clk;

#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
	if (sys_sub->sysevent_dev) {
		void *retval;

		retval = sysevent_get(sys_sub->sysevent_desc.name);
		if (!retval) {
			dsp_err("Failed to get sysevent\n");
			sys_sub->sysevent_get = false;
		} else {
			sys_sub->sysevent_get = true;
		}
	}
#endif

	dsp_leave();
	return 0;
p_err_clk:
	sys->pm.ops->disable(&sys->pm);
p_err_pm:
	return ret;
}

static int dsp_hw_o1_system_runtime_suspend(struct dsp_system *sys)
{
	struct dsp_hw_o1_system *sys_sub;

	dsp_enter();
	sys_sub = sys->sub_data;
#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
	if (sys_sub->sysevent_dev && sys_sub->sysevent_get) {
		sysevent_put(sys_sub->sysevent_dev);
		sys_sub->sysevent_get = false;
	}
#endif

	sys->governor.ops->flush_work(&sys->governor);
	sys->clk.ops->disable(&sys->clk);
	sys->pm.ops->disable(&sys->pm);
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_system_resume(struct dsp_system *sys)
{
	dsp_enter();
	__dsp_hw_o1_system_recovery(sys);
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_system_suspend(struct dsp_system *sys)
{
	dsp_enter();
	__dsp_hw_o1_system_flush(sys);
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_system_npu_start(struct dsp_system *sys, bool boot,
		dma_addr_t fw_iova)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int __dsp_hw_o1_system_binary_load(struct dsp_system *sys)
{
	int ret;
	struct dsp_memory *mem;
	struct dsp_priv_mem *pmem;

	dsp_enter();
	mem = &sys->memory;

	pmem = &mem->priv_mem[DSP_O1_PRIV_MEM_FW];
	ret = dsp_binary_load(DSP_O1_FW_NAME, sys->fw_postfix,
			DSP_O1_FW_EXTENSION, pmem->bac_kvaddr, pmem->size,
			&pmem->used_size);
	if (ret)
		goto p_err_load;

	pmem = &mem->priv_mem[DSP_O1_PRIV_MEM_IVP_PM];
	ret = dsp_binary_load(DSP_O1_IVP_PM_NAME, sys->fw_postfix,
			DSP_O1_FW_EXTENSION, pmem->kvaddr, pmem->size,
			&pmem->used_size);
	if (ret)
		goto p_err_load;

	pmem = &mem->priv_mem[DSP_O1_PRIV_MEM_IVP_DM];
	ret = dsp_binary_load(DSP_O1_IVP_DM_NAME, sys->fw_postfix,
			DSP_O1_FW_EXTENSION, pmem->kvaddr, pmem->size,
			&pmem->used_size);
	if (ret)
		goto p_err_load;

	pmem = &mem->priv_mem[DSP_O1_PRIV_MEM_IAC_PM];
	ret = dsp_binary_load(DSP_O1_IAC_PM_NAME, sys->fw_postfix,
			DSP_O1_FW_EXTENSION, pmem->kvaddr, pmem->size,
			&pmem->used_size);
	if (ret)
		goto p_err_load;

	pmem = &mem->priv_mem[DSP_O1_PRIV_MEM_IAC_DM];
	ret = dsp_binary_load(DSP_O1_IAC_DM_NAME, sys->fw_postfix,
			DSP_O1_FW_EXTENSION, pmem->kvaddr, pmem->size,
			&pmem->used_size);
	if (ret)
		goto p_err_load;

	dsp_leave();
	return 0;
p_err_load:
	return ret;
}

static int dsp_hw_o1_system_start(struct dsp_system *sys)
{
	int ret;
	struct dsp_task_manager *tmgr;
	unsigned long flags;

	dsp_enter();
	tmgr = &sys->task_manager;

	ret = sys->interface.ops->start(&sys->interface);
	if (ret)
		goto p_err_interface;

	ret = __dsp_hw_o1_system_binary_load(sys);
	if (ret)
		goto p_err_load;

	spin_lock_irqsave(&tmgr->slock, flags);
	dsp_task_manager_set_block_mode(tmgr, false);
	spin_unlock_irqrestore(&tmgr->slock, flags);

	dsp_leave();
	return 0;
p_err_load:
	sys->interface.ops->stop(&sys->interface);
p_err_interface:
	return ret;
}

static int dsp_hw_o1_system_stop(struct dsp_system *sys)
{
	dsp_enter();
	sys->interface.ops->stop(&sys->interface);
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_system_open(struct dsp_system *sys)
{
	int ret;

	dsp_enter();
	ret = sys->pm.ops->open(&sys->pm);
	if (ret)
		goto p_err_pm;

	ret = sys->clk.ops->open(&sys->clk);
	if (ret)
		goto p_err_clk;

	ret = sys->bus.ops->open(&sys->bus);
	if (ret)
		goto p_err_bus;

	ret = sys->llc.ops->open(&sys->llc);
	if (ret)
		goto p_err_llc;

	ret = sys->memory.ops->open(&sys->memory);
	if (ret)
		goto p_err_memory;

	ret = sys->interface.ops->open(&sys->interface);
	if (ret)
		goto p_err_interface;

	ret = sys->ctrl.ops->open(&sys->ctrl);
	if (ret)
		goto p_err_ctrl;

	ret = sys->mailbox.ops->open(&sys->mailbox);
	if (ret)
		goto p_err_mbox;

	ret = sys->governor.ops->open(&sys->governor);
	if (ret)
		goto p_err_governor;

	ret = sys->debug.ops->open(&sys->debug);
	if (ret)
		goto p_err_hw_debug;

	ret = sys->dump.ops->open(&sys->dump);
	if (ret)
		goto p_err_dump;

	dsp_leave();
	return 0;
p_err_dump:
	sys->debug.ops->close(&sys->debug);
p_err_hw_debug:
	sys->governor.ops->close(&sys->governor);
p_err_governor:
	sys->mailbox.ops->close(&sys->mailbox);
p_err_mbox:
	sys->ctrl.ops->close(&sys->ctrl);
p_err_ctrl:
	sys->interface.ops->close(&sys->interface);
p_err_interface:
	sys->memory.ops->close(&sys->memory);
p_err_memory:
	sys->llc.ops->close(&sys->llc);
p_err_llc:
	sys->bus.ops->close(&sys->bus);
p_err_bus:
	sys->clk.ops->close(&sys->clk);
p_err_clk:
	sys->pm.ops->close(&sys->pm);
p_err_pm:
	return ret;
}

static int dsp_hw_o1_system_close(struct dsp_system *sys)
{
	dsp_enter();
	sys->dump.ops->close(&sys->dump);
	sys->debug.ops->close(&sys->debug);
	sys->governor.ops->close(&sys->governor);
	sys->mailbox.ops->close(&sys->mailbox);
	sys->ctrl.ops->close(&sys->ctrl);
	sys->interface.ops->close(&sys->interface);
	sys->memory.ops->close(&sys->memory);
	sys->llc.ops->close(&sys->llc);
	sys->bus.ops->close(&sys->bus);
	sys->clk.ops->close(&sys->clk);
	sys->pm.ops->close(&sys->pm);
	dsp_leave();
	return 0;
}

static void __dsp_hw_o1_system_master_load_async(const struct firmware *fw,
		void *context)
{
	int ret, idx, retry = 10;
	struct dsp_system *sys;
	struct dsp_hw_o1_system *sys_sub;
	char full_name[DSP_BINARY_NAME_SIZE];

	dsp_enter();
	sys = context;
	sys_sub = sys->sub_data;

	snprintf(full_name, DSP_BINARY_NAME_SIZE, "%s.%s",
			DSP_O1_MASTER_FW_NAME, DSP_O1_FW_EXTENSION);

	if (!fw) {
		for (idx = 0; idx < retry; ++idx) {
#if KERNEL_VERSION(4, 18, 0) > LINUX_VERSION_CODE
			ret = request_firmware_direct(&fw, full_name, sys->dev);
#else
			ret = firmware_request_nowarn(&fw, full_name, sys->dev);
#endif
			if (ret >= 0)
				break;
			/* Wait for the file system to be mounted at boot time*/
			msleep(500);
		}
		if (ret < 0) {
			dsp_err("Failed to request binary[%s]\n", full_name);
			return;
		}
	}

	sys_sub->boot_bin = vmalloc(fw->size);
	if (!sys_sub->boot_bin) {
		release_firmware(fw);
		dsp_err("Failed to malloc memory for master bin\n");
		return;
	}

	memcpy(sys_sub->boot_bin, fw->data, fw->size);
	sys_sub->boot_bin_size = fw->size;
	release_firmware(fw);
	dsp_info("binary[%s/%zu] is loaded\n",
			full_name, sys_sub->boot_bin_size);
	dsp_leave();
}

#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
static int dsp_hw_o1_imgloader_mem_setup(struct imgloader_desc *desc,
		const unsigned char *metadata, size_t size,
		phys_addr_t *fw_phys_base, size_t *fw_bin_size,
		size_t *fw_mem_size)
{
	struct dsp_system *sys;
	struct dsp_reserved_mem *rmem;

	dsp_enter();
	sys = desc->data;
	rmem = &sys->memory.reserved_mem[DSP_O1_RESERVED_MEM_FW_MASTER];

	*fw_phys_base = rmem->paddr;
	*fw_bin_size = rmem->used_size;
	*fw_mem_size = rmem->size;

	dsp_leave();
	return 0;
}

static int dsp_hw_o1_imgloader_verify_fw(struct imgloader_desc *desc,
		phys_addr_t fw_phys_base, size_t fw_bin_size,
		size_t fw_mem_size)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_imgloader_blk_pwron(struct imgloader_desc *desc)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_imgloader_init_image(struct imgloader_desc *desc)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_imgloader_deinit_image(struct imgloader_desc *desc)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_imgloader_shutdown(struct imgloader_desc *desc)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static struct imgloader_ops hw_o1_imgloader_ops = {
	.mem_setup	= dsp_hw_o1_imgloader_mem_setup,
	.verify_fw	= dsp_hw_o1_imgloader_verify_fw,
	.blk_pwron	= dsp_hw_o1_imgloader_blk_pwron,
	.init_image	= dsp_hw_o1_imgloader_init_image,
	.deinit_image	= dsp_hw_o1_imgloader_deinit_image,
	.shutdown	= dsp_hw_o1_imgloader_shutdown,
};

static int __dsp_hw_o1_system_imgloader_init(struct dsp_system *sys)
{
	int ret;
	struct dsp_hw_o1_system *sys_sub;
	struct imgloader_desc *desc;

	dsp_enter();
	sys_sub = sys->sub_data;
	desc = &sys_sub->loader_desc;

	desc->dev = sys->dev;
	desc->owner = THIS_MODULE;
	desc->ops = &hw_o1_imgloader_ops;
	desc->data = (void *)sys;
	desc->name = "VPC";
	desc->fw_id = 0;

	ret = imgloader_desc_init(desc);
	if (ret) {
		dsp_err("Failed to init imgloader_desc(%d)\n", ret);
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_hw_o1_system_imgloader_deinit(struct dsp_system *sys)
{
	struct dsp_hw_o1_system *sys_sub;

	dsp_enter();
	sys_sub = sys->sub_data;
	imgloader_desc_release(&sys_sub->loader_desc);
	dsp_leave();
}
#else // CONFIG_EXYNOS_IMGLOADER
static int __dsp_hw_o1_system_imgloader_init(struct dsp_system *sys)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static void __dsp_hw_o1_system_imgloader_deinit(struct dsp_system *sys)
{
	dsp_enter();
	dsp_leave();
}
#endif

#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
static int dsp_hw_o1_system_sysevent_shutdown(const struct sysevent_desc *desc,
		bool force_stop)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_system_sysevent_powerup(const struct sysevent_desc *desc)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static void dsp_hw_o1_system_sysevent_crash_shutdown(
		const struct sysevent_desc *desc)
{
	dsp_enter();
	dsp_leave();
}

static int dsp_hw_o1_system_sysevent_ramdump(int enable,
		const struct sysevent_desc *desc)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_system_sysevent_notifier_cb(struct notifier_block *this,
		unsigned long code, void *cmd)
{
	struct notif_data *notifdata;

	notifdata = (struct notif_data *)cmd;
	switch (code) {
	case SYSTEM_EVENT_BEFORE_SHUTDOWN:
	case SYSTEM_EVENT_AFTER_SHUTDOWN:
	case SYSTEM_EVENT_BEFORE_POWERUP:
	case SYSTEM_EVENT_AFTER_POWERUP:
	case SYSTEM_EVENT_RAMDUMP_NOTIFICATION:
	case SYSTEM_EVENT_POWERUP_FAILURE:
	case SYSTEM_EVENT_SOC_RESET:
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block sysevent_nb = {
	.notifier_call = dsp_hw_o1_system_sysevent_notifier_cb,
};

static int __dsp_hw_o1_system_sysevent_init(struct dsp_system *sys)
{
	int ret;
	struct dsp_hw_o1_system *sys_sub;
	struct sysevent_desc *desc;
	struct platform_device *pdev;

	dsp_enter();
	sys_sub = sys->sub_data;
	desc = &sys_sub->sysevent_desc;
	pdev = to_platform_device(sys->dev);

	desc->name = pdev->name;
	desc->dev = sys->dev;
	desc->owner = THIS_MODULE;
	desc->shutdown = dsp_hw_o1_system_sysevent_shutdown;
	desc->powerup = dsp_hw_o1_system_sysevent_powerup;
	desc->crash_shutdown = dsp_hw_o1_system_sysevent_crash_shutdown;
	desc->ramdump = dsp_hw_o1_system_sysevent_ramdump;

	sys_sub->sysevent_dev = sysevent_register(desc);
	if (IS_ERR(sys_sub->sysevent_dev)) {
		ret = PTR_ERR(sys_sub->sysevent_dev);
		dsp_err("Failed to register sysevent(%d)\n", ret);
		goto p_err;
	}

	sysevent_notif_register_notifier(desc->name, &sysevent_nb);

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_hw_o1_system_sysevent_deinit(struct dsp_system *sys)
{
	struct dsp_hw_o1_system *sys_sub;

	dsp_enter();
	sys_sub = sys->sub_data;
	sysevent_unregister(sys_sub->sysevent_dev);
	dsp_leave();
}
#else // CONFIG_EXYNOS_SYSTEM_EVENT
static int __dsp_hw_o1_system_sysevent_init(struct dsp_system *sys)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static void __dsp_hw_o1_system_sysevent_deinit(struct dsp_system *sys)
{
	dsp_enter();
	dsp_leave();
}
#endif

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
static int dsp_hw_o1_system_log_status_notify(struct memlog_obj *obj,
		unsigned int flags)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_system_log_level_notify(struct memlog_obj *obj,
		unsigned int flags)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_system_log_enable_notify(struct memlog_obj *obj,
		unsigned int flags)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_system_file_ops_completed(struct memlog_obj *obj,
		unsigned int flags)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int __dsp_hw_o1_system_memlogger_init(struct dsp_system *sys)
{
	int ret;
	struct dsp_hw_o1_system *sys_sub;
	struct platform_device *pdev;
	struct memlog *desc;
	struct memlog_obj *file_obj, *obj;

	dsp_enter();
	sys_sub = sys->sub_data;
	pdev = to_platform_device(sys->dev);

	ret = memlog_register("DSP_HW", sys->dev, &desc);
	if (ret) {
		dsp_err("Failed to register memlogger(%d)\n", ret);
		goto p_err;
	}

	file_obj = memlog_alloc_file(desc, "log-fil", SZ_1M, SZ_1M, 100, 1);
	if (!file_obj) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc memlog log-file\n");
		goto p_err_alloc;
	}

	obj = memlog_alloc_printf(desc, SZ_1M, file_obj, "log-mem",
			MEMLOG_UFLAG_CACHEABLE);
	if (!obj) {
		memlog_free(file_obj);
		ret = -ENOMEM;
		dsp_err("Failed to alloc memlog log-mem\n");
		goto p_err_alloc;
	}

	sys_sub->memlog_desc = desc;
	dsp_memlog_log_file_obj = file_obj;
	dsp_memlog_log_obj = obj;

	desc->ops.log_status_notify = dsp_hw_o1_system_log_status_notify;
	desc->ops.log_level_notify = dsp_hw_o1_system_log_level_notify;
	desc->ops.log_enable_notify = dsp_hw_o1_system_log_enable_notify;
	desc->ops.file_ops_completed = dsp_hw_o1_system_file_ops_completed;

	dsp_leave();
	return 0;
p_err_alloc:
	memlog_unregister(desc);
p_err:
	return ret;
}

static void __dsp_hw_o1_system_memlogger_deinit(struct dsp_system *sys)
{
	struct dsp_hw_o1_system *sys_sub;

	dsp_enter();
	sys_sub = sys->sub_data;

	if (dsp_memlog_log_obj)
		memlog_free(dsp_memlog_log_obj);

	if (dsp_memlog_log_file_obj)
		memlog_free(dsp_memlog_log_file_obj);

	if (sys_sub->memlog_desc)
		memlog_unregister(sys_sub->memlog_desc);
	dsp_leave();
}
#else // CONFIG_EXYNOS_MEMORY_LOGGER
static int __dsp_hw_o1_system_memlogger_init(struct dsp_system *sys)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static void __dsp_hw_o1_system_memlogger_deinit(struct dsp_system *sys)
{
	dsp_enter();
	dsp_leave();
}
#endif

static int dsp_hw_o1_system_probe(struct dsp_device *dspdev)
{
	int ret;
	struct dsp_system *sys;
	struct platform_device *pdev;
	struct resource *res;
	void __iomem *regs;
	struct dsp_hw_o1_system *sys_sub;

	dsp_enter();
	sys = &dspdev->system;
	sys->dspdev = dspdev;
	sys->dev = dspdev->dev;
	pdev = to_platform_device(sys->dev);

	sys_sub = kzalloc(sizeof(*sys_sub), GFP_KERNEL);
	if (!sys_sub) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc sys_sub\n");
		goto p_err_sys_sub;
	}
	sys->sub_data = sys_sub;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -EINVAL;
		dsp_err("Failed to get resource0\n");
		goto p_err_resource0;
	}

	regs = devm_ioremap_resource(sys->dev, res);
	if (IS_ERR(regs)) {
		ret = PTR_ERR(regs);
		dsp_err("Failed to remap resource0(%d)\n", ret);
		goto p_err_ioremap0;
	}

	sys->sfr_pa = res->start;
	sys->sfr = regs;
	sys->sfr_size = resource_size(res);

	regs = devm_ioremap(sys->dev, 0x10000000, 0x4);
	if (IS_ERR(regs)) {
		ret = PTR_ERR(regs);
		dsp_err("Failed to remap product_id(%d)\n", ret);
		goto p_err_product_id;
	}
	sys_sub->product_id = regs;

	/*
	 * CHIPID_REV[31:24] Reserved
	 * CHIPID_REV[23:20] Main revision
	 * CHIPID_REV[19:16] Sub revision
	 * CHIPID_REV[15:0]  Reserved
	 */
	regs = devm_ioremap(sys->dev, 0x10000010, 0x4);
	if (IS_ERR(regs)) {
		ret = PTR_ERR(regs);
		dsp_err("Failed to remap chip_id(%d)\n", ret);
		goto p_err_chipid;
	}
	sys_sub->chip_id = regs;

	init_waitqueue_head(&sys->system_wq);

	sys->wait[DSP_SYSTEM_WAIT_BOOT] = DSP_WAIT_BOOT_TIME;
	sys->wait[DSP_SYSTEM_WAIT_MAILBOX] = DSP_WAIT_MAILBOX_TIME;
	sys->wait[DSP_SYSTEM_WAIT_RESET] = DSP_WAIT_RESET_TIME;
	sys_sub->wait_mode = DSP_HW_O1_WAIT_MODE_INTERRUPT;
	sys->boost = false;
	mutex_init(&sys->boost_lock);

	sys->layer_start = DSP_SET_DEFAULT_LAYER;
	sys->layer_end = DSP_SET_DEFAULT_LAYER;

	ret = sys->pm.ops->probe(sys);
	if (ret)
		goto p_err_pm;

	ret = sys->clk.ops->probe(sys);
	if (ret)
		goto p_err_clk;

	ret = sys->bus.ops->probe(sys);
	if (ret)
		goto p_err_bus;

	ret = sys->llc.ops->probe(sys);
	if (ret)
		goto p_err_llc;

	ret = sys->memory.ops->probe(sys);
	if (ret)
		goto p_err_memory;

	ret = sys->interface.ops->probe(sys);
	if (ret)
		goto p_err_interface;

	ret = sys->ctrl.ops->probe(sys);
	if (ret)
		goto p_err_ctrl;

	ret = dsp_task_manager_probe(sys);
	if (ret)
		goto p_err_task;

	ret = sys->mailbox.ops->probe(sys);
	if (ret)
		goto p_err_mbox;

	ret = sys->governor.ops->probe(sys);
	if (ret)
		goto p_err_governor;

	ret = sys->debug.ops->probe(dspdev);
	if (ret)
		goto p_err_hw_debug;

	ret = sys->dump.ops->probe(sys);
	if (ret)
		goto p_err_dump;

	ret = dsp_binary_load_async(DSP_O1_MASTER_FW_NAME, NULL,
			DSP_O1_FW_EXTENSION, sys,
			__dsp_hw_o1_system_master_load_async);
	if (ret < 0)
		goto p_err_bin_load;

	ret = __dsp_hw_o1_system_imgloader_init(sys);
	if (ret)
		goto p_err_imgloader;

	ret = __dsp_hw_o1_system_sysevent_init(sys);
	if (ret)
		goto p_err_sysevent;

	__dsp_hw_o1_system_memlogger_init(sys);

	dsp_leave();
	return 0;
p_err_sysevent:
	__dsp_hw_o1_system_imgloader_deinit(sys);
p_err_imgloader:
p_err_bin_load:
	sys->dump.ops->remove(&sys->dump);
p_err_dump:
	sys->debug.ops->remove(&sys->debug);
p_err_hw_debug:
	sys->governor.ops->remove(&sys->governor);
p_err_governor:
	sys->mailbox.ops->remove(&sys->mailbox);
p_err_mbox:
	dsp_task_manager_remove(&sys->task_manager);
p_err_task:
	sys->ctrl.ops->remove(&sys->ctrl);
p_err_ctrl:
	sys->interface.ops->remove(&sys->interface);
p_err_interface:
	sys->memory.ops->remove(&sys->memory);
p_err_memory:
	sys->llc.ops->remove(&sys->llc);
p_err_llc:
	sys->bus.ops->remove(&sys->bus);
p_err_bus:
	sys->clk.ops->remove(&sys->clk);
p_err_clk:
	sys->pm.ops->remove(&sys->pm);
p_err_pm:
	devm_iounmap(sys->dev, sys_sub->chip_id);
p_err_chipid:
	devm_iounmap(sys->dev, sys_sub->product_id);
p_err_product_id:
	devm_iounmap(sys->dev, sys->sfr);
p_err_ioremap0:
p_err_resource0:
	kfree(sys->sub_data);
p_err_sys_sub:
	return ret;
}

static void dsp_hw_o1_system_remove(struct dsp_system *sys)
{
	struct dsp_hw_o1_system *sys_sub;

	dsp_enter();
	sys_sub = sys->sub_data;

	__dsp_hw_o1_system_memlogger_deinit(sys);
	__dsp_hw_o1_system_sysevent_deinit(sys);
	__dsp_hw_o1_system_imgloader_deinit(sys);
	vfree(sys_sub->boot_bin);
	sys->dump.ops->remove(&sys->dump);
	sys->debug.ops->remove(&sys->debug);
	sys->governor.ops->remove(&sys->governor);
	sys->mailbox.ops->remove(&sys->mailbox);
	dsp_task_manager_remove(&sys->task_manager);
	sys->ctrl.ops->remove(&sys->ctrl);
	sys->interface.ops->remove(&sys->interface);
	sys->memory.ops->remove(&sys->memory);
	sys->llc.ops->remove(&sys->llc);
	sys->bus.ops->remove(&sys->bus);
	sys->clk.ops->remove(&sys->clk);
	sys->pm.ops->remove(&sys->pm);
	devm_iounmap(sys->dev, sys_sub->chip_id);
	devm_iounmap(sys->dev, sys_sub->product_id);
	devm_iounmap(sys->dev, sys->sfr);
	kfree(sys->sub_data);
	dsp_leave();
}

static const struct dsp_system_ops o1_system_ops = {
	.request_control	= dsp_hw_o1_system_request_control,
	.execute_task		= dsp_hw_o1_system_execute_task,
	.iovmm_fault_dump	= dsp_hw_o1_system_iovmm_fault_dump,
	.boot			= dsp_hw_o1_system_boot,
	.reset			= dsp_hw_o1_system_reset,
	.power_active		= dsp_hw_o1_system_power_active,
	.set_boot_qos		= dsp_hw_o1_system_set_boot_qos,
	.runtime_resume		= dsp_hw_o1_system_runtime_resume,
	.runtime_suspend	= dsp_hw_o1_system_runtime_suspend,
	.resume			= dsp_hw_o1_system_resume,
	.suspend		= dsp_hw_o1_system_suspend,

	.npu_start		= dsp_hw_o1_system_npu_start,
	.start			= dsp_hw_o1_system_start,
	.stop			= dsp_hw_o1_system_stop,

	.open			= dsp_hw_o1_system_open,
	.close			= dsp_hw_o1_system_close,
	.probe			= dsp_hw_o1_system_probe,
	.remove			= dsp_hw_o1_system_remove,
};

int dsp_hw_o1_system_init(void)
{
	int ret;

	dsp_enter();
	ret = dsp_system_register_ops(DSP_DEVICE_ID_O1, &o1_system_ops);
	if (ret)
		goto p_err;

	ret = dsp_hw_o1_pm_init();
	if (ret)
		goto p_err;

	ret = dsp_hw_o1_clk_init();
	if (ret)
		goto p_err;

	ret = dsp_hw_o1_bus_init();
	if (ret)
		goto p_err;

	ret = dsp_hw_o1_llc_init();
	if (ret)
		goto p_err;

	ret = dsp_hw_o1_memory_init();
	if (ret)
		goto p_err;

	ret = dsp_hw_o1_interface_init();
	if (ret)
		goto p_err;

	ret = dsp_hw_o1_ctrl_init();
	if (ret)
		goto p_err;

	ret = dsp_hw_o1_mailbox_init();
	if (ret)
		goto p_err;

	ret = dsp_hw_o1_governor_init();
	if (ret)
		goto p_err;

	ret = dsp_hw_o1_debug_init();
	if (ret)
		goto p_err;

	ret = dsp_hw_o1_dump_init();
	if (ret)
		goto p_err;

	dsp_info("hw_o1(%u) was initilized\n", DSP_DEVICE_ID_O1);
	dsp_leave();
	return 0;
p_err:
	return ret;
}
