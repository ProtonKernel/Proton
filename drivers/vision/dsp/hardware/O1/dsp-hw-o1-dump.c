// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-kernel.h"
#include "dsp-hw-o1-system.h"
#include "dsp-hw-o1-ctrl.h"
#include "dsp-hw-o1-mailbox.h"
#include "dsp-hw-o1-dump.h"

static const char *const o1_dump_value_name[] = {
	"DSP_O1_DUMP_VALUE_MBOX_ERROR",
	"DSP_O1_DUMP_VALUE_MBOX_DEBUG",
	"DSP_O1_DUMP_VALUE_DL_LOG_ON",
	"DSP_O1_DUMP_VALUE_DL_LOG_RESERVED",
	"DSP_O1_DUMP_VALUE_SYSCTRL_S",
	"DSP_O1_DUMP_VALUE_AAREG_S",
	"DSP_O1_DUMP_VALUE_SYSCTRL_NS",
	"DSP_O1_DUMP_VALUE_AAREG_NS",
	"DSP_O1_DUMP_VALUE_WDT",
	"DSP_O1_DUMP_VALUE_TIMER0",
	"DSP_O1_DUMP_VALUE_TIMER1",
	"DSP_O1_DUMP_VALUE_TIMER2",
	"DSP_O1_DUMP_VALUE_C2AGENT0",
	"DSP_O1_DUMP_VALUE_CPU_SS",
	"DSP_O1_DUMP_VALUE_GIC",
	"DSP_O1_DUMP_VALUE_VPD0_CTRL",
	"DSP_O1_DUMP_VALUE_VPD1_CTRL",
	"DSP_O1_DUMP_VALUE_SDMA_CM",
	"DSP_O1_DUMP_VALUE_STRM_CM",
	"DSP_O1_DUMP_VALUE_SDMA_ND",
	"DSP_O1_DUMP_VALUE_DHCP",
	"DSP_O1_DUMP_VALUE_USERDEFINED",
	"DSP_O1_DUMP_VALUE_FW_INFO",
	"DSP_O1_DUMP_VALUE_PC",
	"DSP_O1_DUMP_VALUE_TASK_COUNT",
};

static void dsp_hw_o1_dump_set_value(struct dsp_dump *dump,
		unsigned int dump_value)
{
	dsp_enter();
	dump->dump_value = dump_value;
	dsp_leave();
}

static void dsp_hw_o1_dump_print_value(struct dsp_dump *dump)
{
	dsp_enter();
	dsp_info("current dump_value : 0x%x\n", dump->dump_value);
	dsp_leave();
}

static void dsp_hw_o1_dump_print_status_user(struct dsp_dump *dump,
		struct seq_file *file)
{
	unsigned int idx;
	unsigned int dump_value = dump->dump_value;

	dsp_enter();
	seq_printf(file, "current dump_value : 0x%x / default : 0x%x\n",
			dump_value, DSP_O1_DUMP_DEFAULT_VALUE);

	for (idx = 0; idx < DSP_O1_DUMP_VALUE_END; idx++) {
		seq_printf(file, "[%2d][%s] - %s\n", idx,
				dump_value & BIT(idx) ? "*" : " ",
				o1_dump_value_name[idx]);
	}

	dsp_leave();
}

static void dsp_hw_o1_dump_ctrl(struct dsp_dump *dump)
{
	int idx;
	int start, end;
	unsigned int dump_value = dump->dump_value;

	dsp_enter();
	dsp_dump_print_value();
	if (dump_value & BIT(DSP_O1_DUMP_VALUE_SYSCTRL_NS)) {
		start = DSP_O1_SYSC_NS_VERSION;
		end = DSP_O1_SYSC_NS_MBOX_FR_IVP3_1_TO_CC$;
		dsp_notice("SYSCTRL_NS register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_O1_DUMP_VALUE_AAREG_NS)) {
		start = DSP_O1_AAREG_NS_SEMAPHORE_REG$;
		end = DSP_O1_AAREG_NS_SEMAPHORE_REG$;
		dsp_notice("AAREG_NS register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_O1_DUMP_VALUE_WDT)) {
		start = DSP_O1_WDT_DSPC_WTCON;
		end = DSP_O1_WDT_DSPC_WTMINCNT;
		dsp_notice("WDT register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_O1_DUMP_VALUE_TIMER0)) {
		start = DSP_O1_TIMER0_PWM_CONFIG0;
		end = DSP_O1_TIMER0_TINT_CSTAT;
		dsp_notice("TIMER0 register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_O1_DUMP_VALUE_CPU_SS)) {
		start = DSP_O1_CPU_SS_DSPC_CPU_REMAPS0_NS;
		end = DSP_O1_CPU_SS_CPU_L1RSTDISABLE;
		dsp_notice("CPU_SS register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_O1_DUMP_VALUE_GIC)) {
		start = DSP_O1_GICD_CTLR;
		end = DSP_O1_GICC_DIR;
		dsp_notice("GIC register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_O1_DUMP_VALUE_VPD0_CTRL)) {
		start = DSP_O1_VPD0_MCGEN;
		end = DSP_O1_VPD0_IVP1_MSG_TH1_$;
		dsp_notice("VPD0_CTRL register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_O1_DUMP_VALUE_VPD1_CTRL)) {
		start = DSP_O1_VPD1_MCGEN;
		end = DSP_O1_VPD1_IVP1_MSG_TH1_$;
		dsp_notice("VPD1_CTRL register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_O1_DUMP_VALUE_SDMA_CM)) {
		start = DSP_O1_SDMA_CM_SDMA_VERSION;
		end = DSP_O1_SDMA_CM_DEBUG_REG0;
		dsp_notice("SDMA_CM register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_O1_DUMP_VALUE_STRM_CM)) {
		start = DSP_O1_STRM_CM_INTR_MASK_BLK_DONE_S$;
		end = DSP_O1_STRM_CM_NODE_MTM1_LOCATION$;
		dsp_notice("STRM_CM register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_O1_DUMP_VALUE_SDMA_ND)) {
		start = DSP_O1_SDMA_ND_ENABLE_ND$;
		end = DSP_O1_SDMA_ND_PADDING_DATA_INFO_ND$;
		dsp_notice("SDMA_ND register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_O1_DUMP_VALUE_DHCP))
		dsp_ctrl_dhcp_dump();

	if (dump_value & BIT(DSP_O1_DUMP_VALUE_USERDEFINED))
		dsp_ctrl_userdefined_dump();

	if (dump_value & BIT(DSP_O1_DUMP_VALUE_FW_INFO))
		dsp_ctrl_fw_info_dump();

	if (dump_value & BIT(DSP_O1_DUMP_VALUE_PC))
		dsp_ctrl_pc_dump();

	dsp_leave();
}

static void dsp_hw_o1_dump_ctrl_user(struct dsp_dump *dump,
		struct seq_file *file)
{
	int idx;
	int start, end;
	unsigned int dump_value = dump->dump_value;

	dsp_enter();
	if (dump_value & BIT(DSP_O1_DUMP_VALUE_SYSCTRL_NS)) {
		start = DSP_O1_SYSC_NS_VERSION;
		end = DSP_O1_SYSC_NS_MBOX_FR_IVP3_1_TO_CC$;
		seq_printf(file, "SYSCTRL_NS register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_O1_DUMP_VALUE_AAREG_NS)) {
		start = DSP_O1_AAREG_NS_SEMAPHORE_REG$;
		end = DSP_O1_AAREG_NS_SEMAPHORE_REG$;
		seq_printf(file, "AAREG_NS register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_O1_DUMP_VALUE_WDT)) {
		start = DSP_O1_WDT_DSPC_WTCON;
		end = DSP_O1_WDT_DSPC_WTMINCNT;
		seq_printf(file, "WDT register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_O1_DUMP_VALUE_TIMER0)) {
		start = DSP_O1_TIMER0_PWM_CONFIG0;
		end = DSP_O1_TIMER0_TINT_CSTAT;
		seq_printf(file, "TIMER0 register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_O1_DUMP_VALUE_CPU_SS)) {
		start = DSP_O1_CPU_SS_DSPC_CPU_REMAPS0_NS;
		end = DSP_O1_CPU_SS_CPU_L1RSTDISABLE;
		seq_printf(file, "CPU_SS register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_O1_DUMP_VALUE_GIC)) {
		start = DSP_O1_GICD_CTLR;
		end = DSP_O1_GICC_DIR;
		seq_printf(file, "GIC register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_O1_DUMP_VALUE_VPD0_CTRL)) {
		start = DSP_O1_VPD0_MCGEN;
		end = DSP_O1_VPD0_IVP1_MSG_TH1_$;
		seq_printf(file, "VPD0_CTRL register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_O1_DUMP_VALUE_VPD1_CTRL)) {
		start = DSP_O1_VPD1_MCGEN;
		end = DSP_O1_VPD1_IVP1_MSG_TH1_$;
		seq_printf(file, "VPD1_CTRL register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_O1_DUMP_VALUE_SDMA_CM)) {
		start = DSP_O1_SDMA_CM_SDMA_VERSION;
		end = DSP_O1_SDMA_CM_DEBUG_REG0;
		seq_printf(file, "SDMA_CM register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_O1_DUMP_VALUE_STRM_CM)) {
		start = DSP_O1_STRM_CM_INTR_MASK_BLK_DONE_S$;
		end = DSP_O1_STRM_CM_NODE_MTM1_LOCATION$;
		seq_printf(file, "STRM_CM register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_O1_DUMP_VALUE_SDMA_ND)) {
		start = DSP_O1_SDMA_ND_ENABLE_ND$;
		end = DSP_O1_SDMA_ND_PADDING_DATA_INFO_ND$;
		seq_printf(file, "SDMA_ND register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_O1_DUMP_VALUE_DHCP))
		dsp_ctrl_user_dhcp_dump(file);

	if (dump_value & BIT(DSP_O1_DUMP_VALUE_USERDEFINED))
		dsp_ctrl_user_userdefined_dump(file);

	if (dump_value & BIT(DSP_O1_DUMP_VALUE_FW_INFO))
		dsp_ctrl_user_fw_info_dump(file);

	if (dump_value & BIT(DSP_O1_DUMP_VALUE_PC))
		dsp_ctrl_user_pc_dump(file);

	dsp_leave();
}

static void dsp_hw_o1_dump_mailbox_pool_error(struct dsp_dump *dump,
		struct dsp_mailbox_pool *pool)
{
	dsp_enter();
	if (dump->dump_value & BIT(DSP_O1_DUMP_VALUE_MBOX_ERROR))
		dsp_mailbox_dump_pool(pool);
	dsp_leave();
}

static void dsp_hw_o1_dump_mailbox_pool_debug(struct dsp_dump *dump,
		struct dsp_mailbox_pool *pool)
{
	dsp_enter();
	if (dump->dump_value & BIT(DSP_O1_DUMP_VALUE_MBOX_DEBUG))
		dsp_mailbox_dump_pool(pool);
	dsp_leave();
}

static void dsp_hw_o1_dump_task_manager_count(struct dsp_dump *dump,
		struct dsp_task_manager *tmgr)
{
	dsp_enter();
	if (dump->dump_value & BIT(DSP_O1_DUMP_VALUE_TASK_COUNT))
		dsp_task_manager_dump_count(tmgr);
	dsp_leave();
}

static void dsp_hw_o1_dump_kernel(struct dsp_dump *dump,
		struct dsp_kernel_manager *kmgr)
{
	dsp_enter();
	if (dump->dump_value & BIT(DSP_O1_DUMP_VALUE_DL_LOG_ON))
		dsp_kernel_dump(kmgr);
	dsp_leave();
}

static int dsp_hw_o1_dump_open(struct dsp_dump *dump)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_dump_close(struct dsp_dump *dump)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_dump_probe(struct dsp_system *sys)
{
	struct dsp_dump *dump;

	dsp_enter();
	dump = &sys->dump;
	dump->dump_value = DSP_O1_DUMP_DEFAULT_VALUE;
	dsp_leave();
	return 0;
}

static void dsp_hw_o1_dump_remove(struct dsp_dump *dump)
{
	dsp_enter();
	dsp_leave();
}

static const struct dsp_dump_ops o1_dump_ops = {
	.set_value		= dsp_hw_o1_dump_set_value,
	.print_value		= dsp_hw_o1_dump_print_value,
	.print_status_user	= dsp_hw_o1_dump_print_status_user,
	.ctrl			= dsp_hw_o1_dump_ctrl,
	.ctrl_user		= dsp_hw_o1_dump_ctrl_user,

	.mailbox_pool_error	= dsp_hw_o1_dump_mailbox_pool_error,
	.mailbox_pool_debug	= dsp_hw_o1_dump_mailbox_pool_debug,
	.task_manager_count	= dsp_hw_o1_dump_task_manager_count,
	.kernel			= dsp_hw_o1_dump_kernel,

	.open			= dsp_hw_o1_dump_open,
	.close			= dsp_hw_o1_dump_close,
	.probe			= dsp_hw_o1_dump_probe,
	.remove			= dsp_hw_o1_dump_remove,
};

int dsp_hw_o1_dump_init(void)
{
	int ret;

	dsp_enter();
	ret = dsp_dump_register_ops(DSP_DEVICE_ID_O1, &o1_dump_ops);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
