/*
 * s2mps24-notifier.c - Interrupt controller support for S2MPS24
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
#include <linux/mfd/samsung/s2mps24.h>
#include <linux/mfd/samsung/s2mps24-regulator.h>
#include <linux/notifier.h>
#include <linux/irq.h>
#include <linux/regulator/pmic_class.h>

static struct notifier_block slave_pmic_notifier;
static struct s2mps24_dev *s2mps24_global;
static u8 irq_reg[S2MPS24_IRQ_GROUP_NR] = {0};

static int s2mps24_buck_oi_cnt[S2MPS24_BUCK_MAX]; /* BUCK 1~8 */
static int s2mps24_buck_ocp_cnt[S2MPS24_BUCK_MAX]; /* BUCK 1~8 */
static int s2mps24_bb_oi_cnt; /* BUCK-BOOST */
static int s2mps24_bb_ocp_cnt; /* BUCK-BOOST */
static int s2mps24_temp_cnt[S2MPS24_TEMP_MAX]; /* Temp */

/* add IRQ handling func here */
static void s2mps24_temp_irq(struct s2mps24_dev *s2mps24, int degree)
{
	int temp = 0;
	s2mps24_temp_cnt[degree]++;

	if (degree == 0)
		temp = 120;
	else
		temp = 140;

	pr_info("%s: S2MPS24 thermal %dC IRQ: %d\n",
		__func__, temp, s2mps24_temp_cnt[degree]);
}

static void s2mps24_bb_ocp_irq(struct s2mps24_dev *s2mps24)
{
	s2mps24_bb_ocp_cnt++;

	pr_info("%s: S2MPS24 BUCKBOOST OCP IRQ: %d\n",
		__func__, s2mps24_bb_ocp_cnt);
}

static void s2mps24_buck_ocp_irq(struct s2mps24_dev *s2mps24, int buck)
{
	s2mps24_buck_ocp_cnt[buck]++;

	pr_info("%s: S2MPS24 BUCK[%d] OCP IRQ: %d\n",
		__func__, buck + 1, s2mps24_buck_ocp_cnt[buck]);
}

static void s2mps24_bb_oi_irq(struct s2mps24_dev *s2mps24)
{
	s2mps24_bb_oi_cnt++;

	pr_info("%s: S2MPS24 BUCKBOOST OI IRQ: %d\n",
		__func__, s2mps24_bb_oi_cnt);
}

static void s2mps24_buck_oi_irq(struct s2mps24_dev *s2mps24, int buck)
{
	s2mps24_buck_oi_cnt[buck]++;

	pr_info("%s: S2MPS24 BUCK[%d] OI IRQ: %d\n",
		__func__, buck + 1, s2mps24_buck_oi_cnt[buck]);
}

static const u8 s2mps24_get_irq_mask[] = {
	/* OI */
	[S2MPS24_IRQ_OI_B1S] = S2MPS24_PMIC_IRQ_OI_B1_MASK,
	[S2MPS24_IRQ_OI_B2S] = S2MPS24_PMIC_IRQ_OI_B2_MASK,
	[S2MPS24_IRQ_OI_B3S] = S2MPS24_PMIC_IRQ_OI_B3_MASK,
	[S2MPS24_IRQ_OI_B4S] = S2MPS24_PMIC_IRQ_OI_B4_MASK,
	[S2MPS24_IRQ_OI_B5S] = S2MPS24_PMIC_IRQ_OI_B5_MASK,
	[S2MPS24_IRQ_OI_B6S] = S2MPS24_PMIC_IRQ_OI_B6_MASK,
	[S2MPS24_IRQ_OI_B7S] = S2MPS24_PMIC_IRQ_OI_B7_MASK,
	[S2MPS24_IRQ_OI_B8S] = S2MPS24_PMIC_IRQ_OI_B8_MASK,
	[S2MPS24_IRQ_OI_BBS] = S2MPS24_PMIC_IRQ_OI_BB_MASK,
	/* OCP */
	[S2MPS24_IRQ_OCP_B1S] = S2MPS24_PMIC_IRQ_OCP_B1_MASK,
	[S2MPS24_IRQ_OCP_B2S] = S2MPS24_PMIC_IRQ_OCP_B2_MASK,
	[S2MPS24_IRQ_OCP_B3S] = S2MPS24_PMIC_IRQ_OCP_B3_MASK,
	[S2MPS24_IRQ_OCP_B4S] = S2MPS24_PMIC_IRQ_OCP_B4_MASK,
	[S2MPS24_IRQ_OCP_B5S] = S2MPS24_PMIC_IRQ_OCP_B5_MASK,
	[S2MPS24_IRQ_OCP_B6S] = S2MPS24_PMIC_IRQ_OCP_B6_MASK,
	[S2MPS24_IRQ_OCP_B7S] = S2MPS24_PMIC_IRQ_OCP_B7_MASK,
	[S2MPS24_IRQ_OCP_B8S] = S2MPS24_PMIC_IRQ_OCP_B8_MASK,
	[S2MPS24_IRQ_OCP_BBS] = S2MPS24_PMIC_IRQ_OCP_BB_MASK,
	/* Temp */
	[S2MPS24_IRQ_INT120C] = S2MPS24_IRQ_INT120C_MASK,
	[S2MPS24_IRQ_INT140C] = S2MPS24_IRQ_INT140C_MASK,
};

static void s2mps24_call_interrupt(struct s2mps24_dev *s2mps24,
				   u8 int1, u8 int2, u8 int3, u8 int4, u8 int5)
{
	size_t i;
	u8 reg = 0;

	/* OI interrupt */
	for (i = 0; i < S2MPS24_BUCK_MAX; i++) {
		reg = S2MPS24_IRQ_OI_B1S + i;

		if (int4 & s2mps24_get_irq_mask[reg])
			s2mps24_buck_oi_irq(s2mps24, i);
	}

	/* OCP interrupt */
	for (i = 0; i < S2MPS24_BUCK_MAX; i++) {
		reg = S2MPS24_IRQ_OCP_B1S + i;

		if (int2 & s2mps24_get_irq_mask[reg])
			s2mps24_buck_ocp_irq(s2mps24, i);
	}

	/* BUCK-BOOST OI interrupt */
	if (int5 & s2mps24_get_irq_mask[S2MPS24_IRQ_OI_BBS])
		s2mps24_bb_oi_irq(s2mps24);

	/* BUCK-BOOST OCP interrupt */
	if (int3 & s2mps24_get_irq_mask[S2MPS24_IRQ_OCP_BBS])
		s2mps24_bb_ocp_irq(s2mps24);

	/* Thermal interrupt */
	for (i = 0; i < S2MPS24_TEMP_MAX; i++) {
		reg = S2MPS24_IRQ_INT120C + i;

		if (int1 & s2mps24_get_irq_mask[reg])
			s2mps24_temp_irq(s2mps24, i);
	}
}

static void s2mps24_irq_work_func(struct work_struct *work)
{
	pr_info("%s: slave pmic interrupt(0x%02hhx, 0x%02hhx, 0x%02hhx, 0x%02hhx, 0x%02hhx)\n",
		__func__, irq_reg[S2MPS24_PMIC_INT1],
		irq_reg[S2MPS24_PMIC_INT2], irq_reg[S2MPS24_PMIC_INT3],
		irq_reg[S2MPS24_PMIC_INT4], irq_reg[S2MPS24_PMIC_INT5]);
}

static int s2mps24_notifier_handler(struct notifier_block *nb,
				    unsigned long action,
				    void *data)
{
	int ret;
	struct s2mps24_dev *s2mps24 = data;

	if (!s2mps24) {
		pr_err("%s: fail to load dev.\n", __func__);
		return IRQ_HANDLED;
	}

	mutex_lock(&s2mps24->irq_lock);

	/* Read interrupt */
	ret = s2mps24_bulk_read(s2mps24->pmic, S2MPS24_REG_INT1,
				S2MPS24_NUM_IRQ_PMIC_REGS,
				&irq_reg[S2MPS24_PMIC_INT1]);
	if (ret) {
		pr_err("%s: fail to read INT sources\n", __func__);
		mutex_unlock(&s2mps24->irq_lock);

		return IRQ_HANDLED;
	}

	queue_delayed_work(s2mps24->irq_wqueue, &s2mps24->irq_work, 0);

	/* Call interrupt */
	s2mps24_call_interrupt(s2mps24, irq_reg[S2MPS24_PMIC_INT1],
			       irq_reg[S2MPS24_PMIC_INT2],
			       irq_reg[S2MPS24_PMIC_INT3],
			       irq_reg[S2MPS24_PMIC_INT4],
			       irq_reg[S2MPS24_PMIC_INT5]);

	mutex_unlock(&s2mps24->irq_lock);

	return IRQ_HANDLED;
}

static BLOCKING_NOTIFIER_HEAD(s2mps24_notifier);

static int s2mps24_register_notifier(struct notifier_block *nb,
			      struct s2mps24_dev *s2mps24)
{
	int ret;

	ret = blocking_notifier_chain_register(&s2mps24_notifier, nb);
	if (ret < 0)
		pr_err("%s: fail to register notifier\n", __func__);

	return ret;
}

void s2mps24_call_notifier(void)
{
	blocking_notifier_call_chain(&s2mps24_notifier, 0, s2mps24_global);
}
EXPORT_SYMBOL(s2mps24_call_notifier);

static const u8 s2mps24_mask_reg[] = {
	[S2MPS24_PMIC_INT1] = S2MPS24_REG_INT1M,
	[S2MPS24_PMIC_INT2] = S2MPS24_REG_INT2M,
	[S2MPS24_PMIC_INT3] = S2MPS24_REG_INT3M,
	[S2MPS24_PMIC_INT4] = S2MPS24_REG_INT4M,
	[S2MPS24_PMIC_INT5] = S2MPS24_REG_INT5M,
};

static int s2mps24_unmask_interrupt(struct s2mps24_dev *s2mps24)
{
	int ret;
	/* unmask IRQM interrupt */
	ret = s2mps24_update_reg(s2mps24->i2c, S2MPS24_PMIC_REG_IRQM,
				 0x00, S2MPS24_PMIC_REG_IRQM_MASK);
	if (ret)
		goto err;

	/* unmask OI interrupt */
	ret = s2mps24_write_reg(s2mps24->pmic, S2MPS24_REG_INT4M, 0x00);
	if (ret)
		goto err;
	ret = s2mps24_update_reg(s2mps24->pmic, S2MPS24_REG_INT5M, 0x00, 0x01);
	if (ret)
		goto err;

	/* unmask OCP interrupt */
	ret = s2mps24_write_reg(s2mps24->pmic, S2MPS24_REG_INT2M, 0x00);
	if (ret)
		goto err;
	ret = s2mps24_update_reg(s2mps24->pmic, S2MPS24_REG_INT3M, 0x00, 0x01);
	if (ret)
		goto err;

	/* unmask Temp interrupt */
	ret = s2mps24_update_reg(s2mps24->pmic, S2MPS24_REG_INT1M, 0x00, 0x0C);
	if (ret)
		goto err;

	return 0;
err:
	return -1;
}

static int s2mps24_set_interrupt(struct s2mps24_dev *s2mps24)
{
	u8 i, val;
	int ret;

	/* Mask all the interrupt sources */
	for (i = 0; i < S2MPS24_IRQ_GROUP_NR; i++) {
		ret = s2mps24_write_reg(s2mps24->pmic, s2mps24_mask_reg[i], 0xFF);
		if (ret)
			goto err;
	}
	ret = s2mps24_update_reg(s2mps24->i2c, S2MPS24_PMIC_REG_IRQM,
				 S2MPS24_PMIC_REG_IRQM_MASK, S2MPS24_PMIC_REG_IRQM_MASK);
	if (ret)
		goto err;

	/* Unmask interrupt sources */
	ret = s2mps24_unmask_interrupt(s2mps24);
	if (ret < 0) {
		pr_err("%s: s2mps24_unmask_interrupt fail\n", __func__);
		goto err;
	}

	/* Check unmask interrupt register */
	for (i = 0; i < S2MPS24_IRQ_GROUP_NR; i++) {
		ret = s2mps24_read_reg(s2mps24->pmic, s2mps24_mask_reg[i], &val);
		if (ret)
			goto err;
		pr_info("%s: INT%dM = 0x%02hhx\n", __func__, i + 1, val);
	}

	return 0;
err:
	return -1;
}

static int s2mps24_set_wqueue(struct s2mps24_dev *s2mps24)
{
	s2mps24->irq_wqueue = create_singlethread_workqueue("s2mps24-wqueue");
	if (!s2mps24->irq_wqueue) {
		pr_err("%s: fail to create workqueue\n", __func__);
		return -1;
	}

	INIT_DELAYED_WORK(&s2mps24->irq_work, s2mps24_irq_work_func);

	return 0;
}

static void s2mps24_set_notifier(struct s2mps24_dev *s2mps24)
{
	slave_pmic_notifier.notifier_call = s2mps24_notifier_handler;
	s2mps24_register_notifier(&slave_pmic_notifier, s2mps24);
}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
static ssize_t irq_read_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	int i, cnt = 0;

	cnt += snprintf(buf + cnt, PAGE_SIZE, "------ INTERRUPTS (/pmic/%s) ------\n",
			dev_driver_string(s2mps24_global->dev));

	for (i = 0; i < S2MPS24_BUCK_MAX; i++)
		cnt += snprintf(buf + cnt, PAGE_SIZE, "BUCK%d_OCP_IRQ:\t\t%5d\n",
				i + 1, s2mps24_buck_ocp_cnt[i]);

	cnt += snprintf(buf + cnt, PAGE_SIZE, "BUCKBOOST_OCP_IRQ:\t%5d\n",
			s2mps24_bb_ocp_cnt);

	for (i = 0; i < S2MPS24_BUCK_MAX; i++)
		cnt += snprintf(buf + cnt, PAGE_SIZE, "BUCK%d_OI_IRQ:\t\t%5d\n",
				i + 1, s2mps24_buck_oi_cnt[i]);

	cnt += snprintf(buf + cnt, PAGE_SIZE, "BUCKBOOST_OI_IRQ:\t%5d\n",
			s2mps24_bb_oi_cnt);

	for (i = 0; i < S2MPS24_TEMP_MAX; i++)
		cnt += snprintf(buf + cnt, PAGE_SIZE, "TEMP_%d_IRQ:\t\t%5d\n",
				i ? 140 : 120, s2mps24_temp_cnt[i]);

	return cnt;
}

static struct pmic_device_attribute irq_attr[] = {
	PMIC_ATTR(irq_read_all, S_IRUGO, irq_read_show, NULL),
};

static int s2mps24_create_irq_sysfs(struct s2mps24_dev *s2mps24)
{
	struct device *s2mps24_irq_dev;
	struct device *dev = s2mps24->dev;
	char device_name[32] = {0, };
	int err = -ENODEV, i = 0;

	pr_info("%s()\n", __func__);

	/* Dynamic allocation for device name */
	snprintf(device_name, sizeof(device_name) - 1, "%s-irq@%s",
		 dev_driver_string(dev), dev_name(dev));

	s2mps24_irq_dev = pmic_device_create(s2mps24, device_name);
	s2mps24->irq_dev = s2mps24_irq_dev;

	/* Create sysfs entries */
	for (i = 0; i < ARRAY_SIZE(irq_attr); i++) {
		err = device_create_file(s2mps24_irq_dev, &irq_attr[i].dev_attr);
		if (err)
			goto remove_pmic_device;
	}

	return 0;

remove_pmic_device:
	for (i--; i >= 0; i--)
		device_remove_file(s2mps24_irq_dev, &irq_attr[i].dev_attr);
	pmic_device_destroy(s2mps24_irq_dev->devt);

	return -ENODEV;
}
#endif

int s2mps24_notifier_init(struct s2mps24_dev *s2mps24)
{
	s2mps24_global = s2mps24;
	mutex_init(&s2mps24->irq_lock);

	/* Register notifier */
	s2mps24_set_notifier(s2mps24);

	/* Set workqueue */
	if (s2mps24_set_wqueue(s2mps24) < 0) {
		pr_err("%s: s2mps24_set_wqueue fail\n", __func__);
		goto err;
	}

	/* Set interrupt */
	if (s2mps24_set_interrupt(s2mps24) < 0) {
		pr_err("%s: s2mps24_set_interrupt fail\n", __func__);
		goto err;
	}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	if (s2mps24_create_irq_sysfs(s2mps24) < 0) {
		pr_info("%s: failed to create sysfs\n", __func__);
		goto err;
	}
#endif
	return 0;
err:
	return -1;
}
EXPORT_SYMBOL_GPL(s2mps24_notifier_init);

void s2mps24_notifier_deinit(struct s2mps24_dev *s2mps24)
{
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	struct device *s2mps24_irq_dev = s2mps24->irq_dev;
	int i = 0;

	/* Remove sysfs entries */
	for (i = 0; i < ARRAY_SIZE(irq_attr); i++)
		device_remove_file(s2mps24_irq_dev, &irq_attr[i].dev_attr);
	pmic_device_destroy(s2mps24_irq_dev->devt);
#endif
}
EXPORT_SYMBOL(s2mps24_notifier_deinit);
