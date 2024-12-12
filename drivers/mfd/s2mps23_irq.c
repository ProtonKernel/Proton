/*
 * s2mps23-irq.c - Interrupt controller support for S2MPS23
 *
 * Copyright (C) 2020 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
*/

#include <linux/err.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/mfd/samsung/s2mps23.h>
#include <linux/mfd/samsung/s2mps23-regulator.h>

#define S2MPS23_IBI_CNT		4

static u8 irq_reg[S2MPS23_IRQ_GROUP_NR] = {0};

static const u8 s2mps23_mask_reg[] = {
	/* TODO: Need to check other INTMASK */
	[S2MPS23_PMIC_INT1] = S2MPS23_PMIC_REG_INT1M,
	[S2MPS23_PMIC_INT2] = S2MPS23_PMIC_REG_INT2M,
	[S2MPS23_PMIC_INT3] = S2MPS23_PMIC_REG_INT3M,
	[S2MPS23_PMIC_INT4] = S2MPS23_PMIC_REG_INT4M,
	[S2MPS23_PMIC_INT5] = S2MPS23_PMIC_REG_INT5M,
	[S2MPS23_PMIC_INT6] = S2MPS23_PMIC_REG_INT6M,
	[S2MPS23_PMIC_INT7] = S2MPS23_PMIC_REG_INT7M,
};

static struct i2c_client *get_i2c(struct s2mps23_dev *s2mps23,
				enum s2mps23_irq_source src)
{
	switch (src) {
	case S2MPS23_PMIC_INT1 ... S2MPS23_PMIC_INT7:
		return s2mps23->pmic;
	default:
		return ERR_PTR(-EINVAL);
	}
}

struct s2mps23_irq_data {
	int mask;
	enum s2mps23_irq_source group;
};

#define DECLARE_IRQ(idx, _group, _mask)		\
	[(idx)] = { .group = (_group), .mask = (_mask) }

static const struct s2mps23_irq_data s2mps23_irqs[] = {
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_PWRONR_INT1,		S2MPS23_PMIC_INT1, 1 << 1),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_PWRONF_INT1,		S2MPS23_PMIC_INT1, 1 << 0),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_JIGONBF_INT1,		S2MPS23_PMIC_INT1, 1 << 2),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_JIGONBR_INT1,		S2MPS23_PMIC_INT1, 1 << 3),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_ACOKBF_INT1,		S2MPS23_PMIC_INT1, 1 << 4),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_ACOKBR_INT1,		S2MPS23_PMIC_INT1, 1 << 5),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_PWRON1S_INT1,		S2MPS23_PMIC_INT1, 1 << 6),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_MRB_INT1,			S2MPS23_PMIC_INT1, 1 << 7),

	DECLARE_IRQ(S2MPS23_PMIC_IRQ_RTC60S_INT2,		S2MPS23_PMIC_INT2, 1 << 0),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_RTCA1_INT2,		S2MPS23_PMIC_INT2, 1 << 1),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_RTCA0_INT2,		S2MPS23_PMIC_INT2, 1 << 2),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_SMPL_INT2,			S2MPS23_PMIC_INT2, 1 << 3),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_RTC1S_INT2,		S2MPS23_PMIC_INT2, 1 << 4),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_WTSR_INT2,			S2MPS23_PMIC_INT2, 1 << 5),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_WRSTB_INT2,		S2MPS23_PMIC_INT2, 1 << 7),

	DECLARE_IRQ(S2MPS23_PMIC_IRQ_120C_INT3,			S2MPS23_PMIC_INT3, 1 << 0),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_140C_INT3,			S2MPS23_PMIC_INT3, 1 << 1),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_TSD_INT3,			S2MPS23_PMIC_INT3, 1 << 2),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_OVP_INT3,			S2MPS23_PMIC_INT3, 1 << 3),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_TIMEOUT2_INT3,		S2MPS23_PMIC_INT3, 1 << 5),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_TIMEOUT3_INT3,		S2MPS23_PMIC_INT3, 1 << 6),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_SUB_SMPL_INT3,		S2MPS23_PMIC_INT3, 1 << 7),

	DECLARE_IRQ(S2MPS23_PMIC_IRQ_OCP_B1M_INT4,		S2MPS23_PMIC_INT4, 1 << 0),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_OCP_B2M_INT4,		S2MPS23_PMIC_INT4, 1 << 1),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_OCP_B3M_INT4,		S2MPS23_PMIC_INT4, 1 << 2),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_OCP_B4M_INT4,		S2MPS23_PMIC_INT4, 1 << 3),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_OCP_B5M_INT4,		S2MPS23_PMIC_INT4, 1 << 4),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_OCP_B6M_INT4,		S2MPS23_PMIC_INT4, 1 << 5),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_OCP_B7M_INT4,		S2MPS23_PMIC_INT4, 1 << 6),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_OCP_B8M_INT4,		S2MPS23_PMIC_INT4, 1 << 7),

	DECLARE_IRQ(S2MPS23_PMIC_IRQ_OCP_B9M_INT5,		S2MPS23_PMIC_INT5, 1 << 0),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_PARITY_ERR0_INT5,		S2MPS23_PMIC_INT5, 1 << 1),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_PARITY_ERR1_INT5,		S2MPS23_PMIC_INT5, 1 << 2),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_PARITY_ERR2_INT5,		S2MPS23_PMIC_INT5, 1 << 3),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_LDO28_SCP_INT5,		S2MPS23_PMIC_INT5, 1 << 7),

	DECLARE_IRQ(S2MPS23_PMIC_IRQ_OI_B1M_INT6,		S2MPS23_PMIC_INT6, 1 << 0),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_OI_B2M_INT6,		S2MPS23_PMIC_INT6, 1 << 1),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_OI_B3M_INT6,		S2MPS23_PMIC_INT6, 1 << 2),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_OI_B4M_INT6,		S2MPS23_PMIC_INT6, 1 << 3),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_OI_B5M_INT6,		S2MPS23_PMIC_INT6, 1 << 4),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_OI_B6M_INT6,		S2MPS23_PMIC_INT6, 1 << 5),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_OI_B7M_INT6,		S2MPS23_PMIC_INT6, 1 << 6),
	DECLARE_IRQ(S2MPS23_PMIC_IRQ_OI_B8M_INT6,		S2MPS23_PMIC_INT6, 1 << 7),

	DECLARE_IRQ(S2MPS23_PMIC_IRQ_OI_B9M_INT7,		S2MPS23_PMIC_INT7, 1 << 0),
};

static void s2mps23_irq_lock(struct irq_data *data)
{
	struct s2mps23_dev *s2mps23 = irq_get_chip_data(data->irq);

	mutex_lock(&s2mps23->irqlock);
}

static void s2mps23_irq_sync_unlock(struct irq_data *data)
{
	struct s2mps23_dev *s2mps23 = irq_get_chip_data(data->irq);
	int i;

	for (i = 0; i < S2MPS23_IRQ_GROUP_NR; i++) {
		u8 mask_reg = s2mps23_mask_reg[i];
		struct i2c_client *i2c = get_i2c(s2mps23, i);

		if (mask_reg == S2MPS23_REG_INVALID ||
				IS_ERR_OR_NULL(i2c))
			continue;
		s2mps23->irq_masks_cache[i] = s2mps23->irq_masks_cur[i];

		s2mps23_write_reg(i2c, s2mps23_mask_reg[i],
				s2mps23->irq_masks_cur[i]);
	}

	mutex_unlock(&s2mps23->irqlock);
}

static const inline struct s2mps23_irq_data *
irq_to_s2mps23_irq(struct s2mps23_dev *s2mps23, int irq)
{
	return &s2mps23_irqs[irq - s2mps23->irq_base];
}

static void s2mps23_irq_mask(struct irq_data *data)
{
	struct s2mps23_dev *s2mps23 = irq_get_chip_data(data->irq);
	const struct s2mps23_irq_data *irq_data =
	    irq_to_s2mps23_irq(s2mps23, data->irq);

	if (irq_data->group >= S2MPS23_IRQ_GROUP_NR)
		return;

	s2mps23->irq_masks_cur[irq_data->group] |= irq_data->mask;
}

static void s2mps23_irq_unmask(struct irq_data *data)
{
	struct s2mps23_dev *s2mps23 = irq_get_chip_data(data->irq);
	const struct s2mps23_irq_data *irq_data =
	    irq_to_s2mps23_irq(s2mps23, data->irq);

	if (irq_data->group >= S2MPS23_IRQ_GROUP_NR)
		return;

	s2mps23->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
}

static struct irq_chip s2mps23_irq_chip = {
	.name			= MFD_DEV_NAME,
	.irq_bus_lock		= s2mps23_irq_lock,
	.irq_bus_sync_unlock	= s2mps23_irq_sync_unlock,
	.irq_mask		= s2mps23_irq_mask,
	.irq_unmask		= s2mps23_irq_unmask,
};

static void s2mps23_report_irq(struct s2mps23_dev *s2mps23)
{
	int i;

	/* Apply masking */
	for (i = 0; i < S2MPS23_IRQ_GROUP_NR; i++)
		irq_reg[i] &= ~s2mps23->irq_masks_cur[i];

	/* Report */
	for (i = 0; i < S2MPS23_IRQ_NR; i++)
		if (irq_reg[s2mps23_irqs[i].group] & s2mps23_irqs[i].mask)
			handle_nested_irq(s2mps23->irq_base + i);
}

static int s2mps23_power_key_detection(struct s2mps23_dev *s2mps23)
{
	int ret, i;
	u8 val;

	/* Determine falling/rising edge for PWR_ON key */
	if ((irq_reg[S2MPS23_PMIC_INT1] & 0x3) == 0x3) {
		ret = s2mps23_read_reg(s2mps23->i2c,
				       S2MPS23_PMIC_REG_STATUS1, &val);
		if (ret) {
			pr_err("%s: fail to read register\n", __func__);
			goto power_key_err;
		}
		irq_reg[S2MPS23_PMIC_INT1] &= 0xFC;

		if (val & S2MPS23_STATUS1_PWRON) {
			irq_reg[S2MPS23_PMIC_INT1] = S2MPS23_FALLING_EDGE;
			s2mps23_report_irq(s2mps23);

			/* clear irq */
			for (i = 0; i < S2MPS23_IRQ_GROUP_NR; i++)
				irq_reg[i] &= 0x00;

			irq_reg[S2MPS23_PMIC_INT1] = S2MPS23_RISING_EDGE;
		} else {
			irq_reg[S2MPS23_PMIC_INT1] = S2MPS23_RISING_EDGE;
			s2mps23_report_irq(s2mps23);

			/* clear irq */
			for (i = 0; i < S2MPS23_IRQ_GROUP_NR; i++)
				irq_reg[i] &= 0x00;

			irq_reg[S2MPS23_PMIC_INT1] = S2MPS23_FALLING_EDGE;
		}
	}
	return 0;

power_key_err:
	return 1;
}

static void s2mps23_irq_work_func(struct work_struct *work)
{
	pr_info("%s: master pmic interrupt(0x%02hhx, 0x%02hhx, 0x%02hhx, 0x%02hhx, 0x%02hhx, 0x%02hhx, 0x%02hhx)\n",
		 __func__, irq_reg[S2MPS23_PMIC_INT1], irq_reg[S2MPS23_PMIC_INT2], irq_reg[S2MPS23_PMIC_INT3],
		 irq_reg[S2MPS23_PMIC_INT4], irq_reg[S2MPS23_PMIC_INT5], irq_reg[S2MPS23_PMIC_INT6],
		 irq_reg[S2MPS23_PMIC_INT7]);
}

static void s2mps23_pending_clear(struct s2mps23_dev *s2mps23)
{
	u32 val = 0;

	val = readl(s2mps23->sysreg_pending);
	writel(val, s2mps23->sysreg_pending);
}

static irqreturn_t s2mps23_irq_thread(int irq, void *data)
{
	struct s2mps23_dev *s2mps23 = data;
	u8 ibi_src[S2MPS23_IBI_CNT] = {0};
	u32 val;
	int i, ret;

	/* Clear interrupt pending bit */
	s2mps23_pending_clear(s2mps23);

	/* Read VGPIO_RX_MONITOR */
	val = readl(s2mps23->mem_base);

	for (i = 0; i < S2MPS23_IBI_CNT; i++) {
		ibi_src[i] = val & 0xFF;
		val = (val >> 8);
	}

	/* notify Master PMIC */
	if (ibi_src[0] & S2MPS23_IBI0_PMIC_M) {
		ret = s2mps23_bulk_read(s2mps23->pmic, S2MPS23_PMIC_REG_INT1,
				S2MPS23_NUM_IRQ_PMIC_REGS, &irq_reg[S2MPS23_PMIC_INT1]);
		if (ret) {
			pr_err("%s:%s Failed to read pmic interrupt: %d\n",
				MFD_DEV_NAME, __func__, ret);
			return IRQ_HANDLED;
		}

		queue_delayed_work(s2mps23->irq_wqueue, &s2mps23->irq_work, 0);

		/* Power-key W/A */
		ret = s2mps23_power_key_detection(s2mps23);
		if (ret)
			pr_err("%s: POWER-KEY detection error\n", __func__);

		/* Report IRQ */
		s2mps23_report_irq(s2mps23);
	}
	/* notify Slave PMIC */
	if (ibi_src[0] & S2MPS23_IBI0_PMIC_S) {
		pr_info("%s: IBI from slave pmic\n", __func__);
		s2mps24_call_notifier();
	}

	return IRQ_HANDLED;
}

static int s2mps23_set_wqueue(struct s2mps23_dev *s2mps23)
{
	s2mps23->irq_wqueue = create_singlethread_workqueue("s2mps23-wqueue");
	if (!s2mps23->irq_wqueue) {
		pr_err("%s: fail to create workqueue\n", __func__);
		return 1;
	}

	INIT_DELAYED_WORK(&s2mps23->irq_work, s2mps23_irq_work_func);

	return 0;
}

static void s2mps23_set_vgpio_monitor(struct s2mps23_dev *s2mps23)
{
	s2mps23->mem_base = ioremap(VGPIO_I3C_BASE + VGPIO_MONITOR_ADDR, SZ_32);
	if (s2mps23->mem_base == NULL)
		pr_info("%s: fail to allocate memory\n", __func__);

	s2mps23->sysreg_pending = ioremap(SYSREG_VGPIO2AP + INTC0_IPRIO_PEND, SZ_32);
	if (s2mps23->sysreg_pending == NULL)
		pr_err("%s: fail to allocate memory\n", __func__);
}

int s2mps23_irq_init(struct s2mps23_dev *s2mps23)
{
	int i;
	int ret;
	u8 i2c_data;
	int cur_irq;

	if (!s2mps23->irq_base) {
		dev_err(s2mps23->dev, "No interrupt base specified.\n");
		return 0;
	}

	mutex_init(&s2mps23->irqlock);

	/* Set VGPIO monitor */
	s2mps23_set_vgpio_monitor(s2mps23);

	/* Set workqueue */
	s2mps23_set_wqueue(s2mps23);

	/* Mask individual interrupt sources */
	for (i = 0; i < S2MPS23_IRQ_GROUP_NR; i++) {
		struct i2c_client *i2c;

		s2mps23->irq_masks_cur[i] = 0xff;
		s2mps23->irq_masks_cache[i] = 0xff;

		i2c = get_i2c(s2mps23, i);

		if (IS_ERR_OR_NULL(i2c))
			continue;
		if (s2mps23_mask_reg[i] == S2MPS23_REG_INVALID)
			continue;

		s2mps23_write_reg(i2c, s2mps23_mask_reg[i], 0xff);
	}

	/* Register with genirq */
	for (i = 0; i < S2MPS23_IRQ_NR; i++) {
		cur_irq = i + s2mps23->irq_base;
		irq_set_chip_data(cur_irq, s2mps23);
		irq_set_chip_and_handler(cur_irq, &s2mps23_irq_chip,
					 handle_level_irq);
		irq_set_nested_thread(cur_irq, 1);
#if IS_ENABLED(CONFIG_ARM)
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		irq_set_noprobe(cur_irq);
#endif
	}

	s2mps23_write_reg(s2mps23->i2c, S2MPS23_PMIC_REG_IRQM, 0xff);
	/* Unmask s2mps23 interrupt */
	ret = s2mps23_read_reg(s2mps23->i2c, S2MPS23_PMIC_REG_IRQM,
			  &i2c_data);
	if (ret) {
		pr_err("%s:%s fail to read intsrc mask reg\n",
					 MFD_DEV_NAME, __func__);
		return ret;
	}

	i2c_data &= ~(S2MPS23_PMIC_PM_IRQM);	/* Unmask pmic interrupt */
	s2mps23_write_reg(s2mps23->i2c, S2MPS23_PMIC_REG_IRQM, i2c_data);

	pr_info("%s:%s S2MPS23_PMIC_REG_IRQM=0x%02hhx\n",
		MFD_DEV_NAME, __func__, i2c_data);

	s2mps23->irq = irq_of_parse_and_map(s2mps23->dev->of_node, 0);
	ret = request_threaded_irq(s2mps23->irq, NULL, s2mps23_irq_thread,
				   IRQF_ONESHOT, "s2mps23-irq", s2mps23);

	if (ret) {
		dev_err(s2mps23->dev, "Failed to request IRQ %d: %d\n",
			s2mps23->irq, ret);
		destroy_workqueue(s2mps23->irq_wqueue);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(s2mps23_irq_init);

void s2mps23_irq_exit(struct s2mps23_dev *s2mps23)
{
	if (s2mps23->irq)
		free_irq(s2mps23->irq, s2mps23);

	iounmap(s2mps23->mem_base);
	iounmap(s2mps23->sysreg_pending);

	cancel_delayed_work_sync(&s2mps23->irq_work);
	destroy_workqueue(s2mps23->irq_wqueue);
}
EXPORT_SYMBOL_GPL(s2mps23_irq_exit);
