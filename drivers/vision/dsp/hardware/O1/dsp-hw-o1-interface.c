// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-hw-o1-system.h"
#include "dsp-hw-o1-memory.h"
#include "dsp-hw-o1-ctrl.h"
#include "dsp-hw-o1-dump.h"
#include "dsp-hw-o1-interface.h"

#define DSP_ISR_TIMER_REPEAT_TIME	(1)

enum dsp_o1_irq_id {
	DSP_O1_IRQ_GIC_NS,
	DSP_O1_IRQ_SWI0_NS,
	DSP_O1_IRQ_SWI1_NS,
	DSP_O1_IRQ_SWI2_NS,
	DSP_O1_IRQ_MBOX_NS,
	DSP_O1_IRQ_WDT_NS,
	DSP_O1_IRQ_STRM_NS,
	DSP_O1_IRQ_SDMA_NS,
	DSP_O1_IRQ_GIC_S,
	DSP_O1_IRQ_SWI0_S,
	DSP_O1_IRQ_SWI1_S,
	DSP_O1_IRQ_SWI2_S,
	DSP_O1_IRQ_MBOX_S,
	DSP_O1_IRQ_WDT_S,
	DSP_O1_IRQ_STRM_S,
	DSP_O1_IRQ_SDMA_S,
	DSP_O1_IRQ_COUNT,
};

struct dsp_hw_o1_interface {
	int irq[DSP_O1_IRQ_COUNT];
};

static void __dsp_hw_o1_interface_isr(struct dsp_interface *itf);

static int dsp_hw_o1_interface_send_irq(struct dsp_interface *itf, int status)
{
	int ret;

	dsp_enter();
	if (status & BIT(DSP_TO_CC_INT_RESET)) {
		dsp_ctrl_writel(DSP_O1_SYSC_NS_INTR_SWI_SET_TO_CC, 0x1 << 0);
	} else if (status & BIT(DSP_TO_CC_INT_MAILBOX)) {
		dsp_ctrl_writel(DSP_O1_SYSC_NS_INTR_SWI_SET_TO_CC, 0x1 << 1);
	} else {
		dsp_err("wrong interrupt requested(%x)\n", status);
		ret = -EINVAL;
		goto p_err;
	}
	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_o1_interface_check_irq(struct dsp_interface *itf)
{
	unsigned int status;
	unsigned long flags;

	dsp_enter();
	spin_lock_irqsave(&itf->irq_lock, flags);
	status = dsp_ctrl_readl(DSP_O1_SYSC_NS_MBOX_FR_CC_TO_HOST_INTR);
	__dsp_hw_o1_interface_isr(itf);

	if (!status)
		goto p_end;

	dsp_ctrl_writel(DSP_O1_SYSC_NS_MBOX_FR_CC_TO_HOST_INTR, 0x0);
	dsp_leave();
p_end:
	spin_unlock_irqrestore(&itf->irq_lock, flags);
	return 0;
}

static void __dsp_hw_o1_interface_isr_boot_done(struct dsp_interface *itf)
{
	dsp_enter();
	if (!(itf->sys->system_flag & BIT(DSP_SYSTEM_BOOT))) {
		itf->sys->system_flag = BIT(DSP_SYSTEM_BOOT);
		wake_up(&itf->sys->system_wq);
	} else {
		dsp_warn("boot_done interrupt occurred incorrectly\n");
	}
	dsp_leave();
}

static void __dsp_hw_o1_interface_isr_reset_done(struct dsp_interface *itf)
{
	dsp_enter();
	if (!(itf->sys->system_flag & BIT(DSP_SYSTEM_RESET))) {
		itf->sys->system_flag = BIT(DSP_SYSTEM_RESET);
		wake_up(&itf->sys->system_wq);
	} else {
		dsp_warn("reset_done interrupt occurred incorrectly\n");
	}
	dsp_leave();
}

static void __dsp_hw_o1_interface_isr_reset_request(struct dsp_interface *itf)
{
	dsp_enter();
	dsp_info("reset request\n");
	dsp_leave();
}

static void __dsp_hw_o1_interface_isr_mailbox(struct dsp_interface *itf)
{
	dsp_enter();
	itf->sys->mailbox.ops->receive_task(&itf->sys->mailbox);
	dsp_leave();
}

static void __dsp_hw_o1_interface_isr(struct dsp_interface *itf)
{
	unsigned int status;

	dsp_enter();
	status = dsp_ctrl_dhcp_readl(
			DSP_O1_DHCP_IDX(DSP_O1_DHCP_TO_HOST_INT_STATUS));
	if (status & BIT(DSP_TO_HOST_INT_BOOT)) {
		__dsp_hw_o1_interface_isr_boot_done(itf);
	} else if (status & BIT(DSP_TO_HOST_INT_MAILBOX)) {
		__dsp_hw_o1_interface_isr_mailbox(itf);
	} else if (status & BIT(DSP_TO_HOST_INT_RESET_DONE)) {
		__dsp_hw_o1_interface_isr_reset_done(itf);
	} else if (status & BIT(DSP_TO_HOST_INT_RESET_REQUEST)) {
		__dsp_hw_o1_interface_isr_reset_request(itf);
	} else {
		dsp_dbg("interrupt status is invalid(%u)\n", status);
		return;
	}
	dsp_ctrl_dhcp_writel(
			DSP_O1_DHCP_IDX(DSP_O1_DHCP_TO_HOST_INT_STATUS), 0);
	dsp_leave();
}

static irqreturn_t dsp_hw_o1_interface_isr0(int irq, void *data)
{
	//struct dsp_interface *itf = (struct dsp_interface *)data;

	dsp_enter();
	dsp_warn("GIC (non-secure)\n");
	dsp_dump_ctrl();
	dsp_leave();
	return IRQ_HANDLED;
}

static irqreturn_t dsp_hw_o1_interface_isr1(int irq, void *data)
{
	//struct dsp_interface *itf = (struct dsp_interface *)data;

	dsp_enter();
	dsp_warn("SWI[0] (non-secure)\n");
	dsp_dump_ctrl();
	dsp_leave();
	return IRQ_HANDLED;
}

static irqreturn_t dsp_hw_o1_interface_isr2(int irq, void *data)
{
	//struct dsp_interface *itf = (struct dsp_interface *)data;

	dsp_enter();
	dsp_warn("SWI[1] (non-secure)\n");
	dsp_dump_ctrl();
	dsp_leave();
	return IRQ_HANDLED;
}

static irqreturn_t dsp_hw_o1_interface_isr3(int irq, void *data)
{
	//struct dsp_interface *itf = (struct dsp_interface *)data;

	dsp_enter();
	dsp_warn("SWI[2] (non-secure)\n");
	dsp_dump_ctrl();
	dsp_leave();
	return IRQ_HANDLED;
}

static irqreturn_t dsp_hw_o1_interface_isr4(int irq, void *data)
{
	struct dsp_interface *itf;
	unsigned int status;
	unsigned long flags;

	dsp_enter();
	itf = (struct dsp_interface *)data;

	spin_lock_irqsave(&itf->irq_lock, flags);
	status = dsp_ctrl_readl(DSP_O1_SYSC_NS_MBOX_FR_CC_TO_HOST_INTR);
	if (!status) {
		dsp_warn("interrupt status is unstable\n");
		goto p_end;
	}

	__dsp_hw_o1_interface_isr(itf);

	dsp_ctrl_writel(DSP_O1_SYSC_NS_MBOX_FR_CC_TO_HOST_INTR, 0x0);
	dsp_leave();
p_end:
	spin_unlock_irqrestore(&itf->irq_lock, flags);
	return IRQ_HANDLED;
}

static irqreturn_t dsp_hw_o1_interface_isr5(int irq, void *data)
{
	//struct dsp_interface *itf = (struct dsp_interface *)data;

	dsp_enter();
	dsp_warn("WDT (non-secure)\n");
	dsp_dump_ctrl();
	dsp_leave();
	return IRQ_HANDLED;
}

static irqreturn_t dsp_hw_o1_interface_isr6(int irq, void *data)
{
	//struct dsp_interface *itf = (struct dsp_interface *)data;

	dsp_enter();
	dsp_warn("STRM (non-secure)\n");
	dsp_dump_ctrl();
	dsp_leave();
	return IRQ_HANDLED;
}

static irqreturn_t dsp_hw_o1_interface_isr7(int irq, void *data)
{
	//struct dsp_interface *itf = (struct dsp_interface *)data;

	dsp_enter();
	dsp_warn("SDMA (non-secure)\n");
	dsp_dump_ctrl();
	dsp_leave();
	return IRQ_HANDLED;
}

static irqreturn_t dsp_hw_o1_interface_isr8(int irq, void *data)
{
	//struct dsp_interface *itf = (struct dsp_interface *)data;

	dsp_enter();
	dsp_warn("GIC (secure)\n");
	dsp_leave();
	return IRQ_HANDLED;
}

static irqreturn_t dsp_hw_o1_interface_isr9(int irq, void *data)
{
	//struct dsp_interface *itf = (struct dsp_interface *)data;

	dsp_enter();
	dsp_warn("SWI[0] (secure)\n");
	dsp_dump_ctrl();
	dsp_leave();
	return IRQ_HANDLED;
}

static irqreturn_t dsp_hw_o1_interface_isr10(int irq, void *data)
{
	//struct dsp_interface *itf = (struct dsp_interface *)data;

	dsp_enter();
	dsp_warn("SWI[1] (secure)\n");
	dsp_dump_ctrl();
	dsp_leave();
	return IRQ_HANDLED;
}

static irqreturn_t dsp_hw_o1_interface_isr11(int irq, void *data)
{
	//struct dsp_interface *itf = (struct dsp_interface *)data;

	dsp_enter();
	dsp_warn("SWI[2] (secure)\n");
	dsp_dump_ctrl();
	dsp_leave();
	return IRQ_HANDLED;
}

static irqreturn_t dsp_hw_o1_interface_isr12(int irq, void *data)
{
	//struct dsp_interface *itf = (struct dsp_interface *)data;

	dsp_enter();
	dsp_warn("MBOX (secure)\n");
	dsp_dump_ctrl();
	dsp_leave();
	return IRQ_HANDLED;
}

static irqreturn_t dsp_hw_o1_interface_isr13(int irq, void *data)
{
	//struct dsp_interface *itf = (struct dsp_interface *)data;

	dsp_enter();
	dsp_warn("WDT (secure)\n");
	dsp_dump_ctrl();
	dsp_leave();
	return IRQ_HANDLED;
}

static irqreturn_t dsp_hw_o1_interface_isr14(int irq, void *data)
{
	//struct dsp_interface *itf = (struct dsp_interface *)data;

	dsp_enter();
	dsp_warn("STRM (secure)\n");
	dsp_dump_ctrl();
	dsp_leave();
	return IRQ_HANDLED;
}

static irqreturn_t dsp_hw_o1_interface_isr15(int irq, void *data)
{
	//struct dsp_interface *itf = (struct dsp_interface *)data;

	dsp_enter();
	dsp_warn("SDMA (secure)\n");
	dsp_dump_ctrl();
	dsp_leave();
	return IRQ_HANDLED;
}

static irqreturn_t (*isr_list[DSP_O1_IRQ_COUNT])(int, void *) = {
	dsp_hw_o1_interface_isr0,
	dsp_hw_o1_interface_isr1,
	dsp_hw_o1_interface_isr2,
	dsp_hw_o1_interface_isr3,
	dsp_hw_o1_interface_isr4,
	dsp_hw_o1_interface_isr5,
	dsp_hw_o1_interface_isr6,
	dsp_hw_o1_interface_isr7,
	dsp_hw_o1_interface_isr8,
	dsp_hw_o1_interface_isr9,
	dsp_hw_o1_interface_isr10,
	dsp_hw_o1_interface_isr11,
	dsp_hw_o1_interface_isr12,
	dsp_hw_o1_interface_isr13,
	dsp_hw_o1_interface_isr14,
	dsp_hw_o1_interface_isr15
};

static int dsp_hw_o1_interface_start(struct dsp_interface *itf)
{
	struct dsp_hw_o1_interface *itf_sub;

	dsp_enter();
	itf_sub = itf->sub_data;

	dsp_ctrl_writel(DSP_O1_SYSC_NS_INTR_MBOX_ENABLE_FR_CC, 0x1);
	dsp_ctrl_writel(DSP_O1_SYSC_NS_MBOX_FR_CC_TO_HOST_INTR, 0x0);
	enable_irq(itf_sub->irq[DSP_O1_IRQ_MBOX_NS]);
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_interface_stop(struct dsp_interface *itf)
{
	struct dsp_hw_o1_interface *itf_sub;

	dsp_enter();
	itf_sub = itf->sub_data;

	dsp_ctrl_writel(DSP_O1_SYSC_NS_INTR_MBOX_ENABLE_FR_CC, 0x0);
	dsp_ctrl_writel(DSP_O1_SYSC_NS_MBOX_FR_CC_TO_HOST_INTR, 0x0);
	disable_irq(itf_sub->irq[DSP_O1_IRQ_MBOX_NS]);
	dsp_leave();
	return 0;
}

#ifdef ENABLE_DSP_VELOCE
static void dsp_interface_isr_timer(struct timer_list *t)
{
	struct dsp_interface *itf;
	unsigned int status;

	dsp_enter();
	itf = from_timer(itf, t, isr_timer);

	status = dsp_ctrl_readl(DSP_O1_SYSC_NS_MBOX_FR_CC_TO_HOST_INTR);
	if (status)
		dsp_hw_o1_interface_isr4(0, itf);

	mod_timer(&itf->isr_timer,
			jiffies + msecs_to_jiffies(DSP_ISR_TIMER_REPEAT_TIME));
	dsp_leave();
}
#endif

static int dsp_hw_o1_interface_open(struct dsp_interface *itf)
{
	dsp_enter();
#ifdef ENABLE_DSP_VELOCE
	timer_setup(&itf->isr_timer, dsp_interface_isr_timer, 0);
	mod_timer(&itf->isr_timer,
			jiffies + msecs_to_jiffies(DSP_ISR_TIMER_REPEAT_TIME));
#endif
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_interface_close(struct dsp_interface *itf)
{
	dsp_enter();
#ifdef ENABLE_DSP_VELOCE
	del_timer_sync(&itf->isr_timer);
#endif
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_interface_probe(struct dsp_system *sys)
{
	int ret;
	struct dsp_interface *itf;
	struct dsp_hw_o1_interface *itf_sub;
	struct platform_device *pdev;
	int idx;

	dsp_enter();
	itf = &sys->interface;
	itf->sys = sys;
	itf->sfr = sys->sfr;
	pdev = to_platform_device(sys->dev);

	itf_sub = kzalloc(sizeof(*itf_sub), GFP_KERNEL);
	if (!itf_sub) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc itf_sub\n");
		goto p_err_itf_sub;
	}
	itf->sub_data = itf_sub;

	for (idx = 0; idx < DSP_O1_IRQ_COUNT; ++idx) {
		ret = platform_get_irq(pdev, idx);
		if (ret < 0) {
			dsp_err("Failed to get irq%d(%d)\n", idx, ret);
			goto p_err_irq;
		}
		itf_sub->irq[idx] = ret;

		ret = devm_request_irq(itf->sys->dev, itf_sub->irq[idx],
				isr_list[idx], 0, dev_name(itf->sys->dev), itf);
		if (ret) {
			dsp_err("Failed to request irq%d(%d)\n", idx, ret);
			goto p_err_irq;
		}

		disable_irq(itf_sub->irq[idx]);
	}

	spin_lock_init(&itf->irq_lock);
	dsp_leave();
	return 0;
p_err_irq:
	for (idx -= 1; idx >= 0; --idx) {
		enable_irq(itf_sub->irq[idx]);
		devm_free_irq(itf->sys->dev, itf_sub->irq[idx], itf);
	}

	kfree(itf->sub_data);
p_err_itf_sub:
	return ret;
}

static void dsp_hw_o1_interface_remove(struct dsp_interface *itf)
{
	int idx;
	struct dsp_hw_o1_interface *itf_sub;

	dsp_enter();
	itf_sub = itf->sub_data;

	for (idx = 0; idx < DSP_O1_IRQ_COUNT; ++idx) {
		enable_irq(itf_sub->irq[idx]);
		devm_free_irq(itf->sys->dev, itf_sub->irq[idx], itf);
	}

	kfree(itf->sub_data);
	dsp_leave();
}

static const struct dsp_interface_ops o1_interface_ops = {
	.send_irq	= dsp_hw_o1_interface_send_irq,
	.check_irq	= dsp_hw_o1_interface_check_irq,

	.start		= dsp_hw_o1_interface_start,
	.stop		= dsp_hw_o1_interface_stop,
	.open		= dsp_hw_o1_interface_open,
	.close		= dsp_hw_o1_interface_close,
	.probe		= dsp_hw_o1_interface_probe,
	.remove		= dsp_hw_o1_interface_remove,
};

int dsp_hw_o1_interface_init(void)
{
	int ret;

	dsp_enter();
	ret = dsp_interface_register_ops(DSP_DEVICE_ID_O1, &o1_interface_ops);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
