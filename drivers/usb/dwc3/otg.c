/**
 * otg.c - DesignWare USB3 DRD Controller OTG
 *
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Authors: Ido Shayevitz <idos@codeaurora.org>
 *	    Anton Tikhomirov <av.tikhomirov@samsung.com>
 *	    Minho Lee <minho55.lee@samsung.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/workqueue.h>
#include <linux/usb/samsung_usb.h>
//#include <linux/mfd/samsung/s2mps18-private.h>
#include <linux/suspend.h>
//#include <soc/samsung/exynos-pm.h>
#if defined(CONFIG_OTG_DEFAULT)
#include <linux/usb/typec.h>
#endif
#include <linux/battery/sec_battery_common.h>

#include "core.h"
#include "core_exynos.h"
#include "otg.h"
#include "io.h"
#ifdef CONFIG_OF
#include <linux/of_device.h>
#endif
#if defined(CONFIG_USB_PORT_POWER_OPTIMIZATION)
#include "usb_power_notify.h"
#endif
#include "../../base/base.h"
#ifdef CONFIG_USB_EXYNOS_TPMON
#include "exynos_usb_tpmon.h"
#endif
#ifdef CONFIG_DWC3_RETRY_CONNECTION
#include <linux/usb/composite.h>
#endif
#include <linux/reboot.h>

#define OTG_NO_CONNECT		0
#define OTG_CONNECT_ONLY	1
#define OTG_DEVICE_CONNECT	2
#define LINK_DEBUG_L		(0x0C)
#define LINK_DEBUG_H		(0x10)
#define BUS_ACTIVITY_CHECK	(0x3F << 16)
#define READ_TRANS_OFFSET	10

struct dwc3 *g_dwc;
extern int exynos_usbdrd_pipe3_enable(struct phy *phy);
extern int exynos_usbdrd_pipe3_disable(struct phy *phy);

/* -------------------------------------------------------------------------- */
#if defined(CONFIG_OTG_DEFAULT)
struct intf_typec {
	/* struct mutex lock; */ /* device lock */
	struct device *dev;
	struct typec_port *port;
	struct typec_capability cap;
	struct typec_partner *partner;
};
#endif

static int usb_reboot_noti(struct notifier_block *nb, unsigned long event, void *buf);
static struct notifier_block usb_reboot_notifier = {
	.notifier_call = usb_reboot_noti,
};

int otg_connection;
EXPORT_SYMBOL_GPL(otg_connection);
int usb_audio_connection;
EXPORT_SYMBOL_GPL(usb_audio_connection);
int phy_status = 1;
EXPORT_SYMBOL_GPL(phy_status);
static int dwc3_otg_statemachine(struct otg_fsm *fsm)
{
	struct usb_otg *otg = fsm->otg;
	enum usb_otg_state prev_state = otg->state;
	int ret = 0;

	if (fsm->reset) {
		if (otg->state == OTG_STATE_A_HOST) {
			otg_drv_vbus(fsm, 0);
			otg_start_host(fsm, 0);
		} else if (otg->state == OTG_STATE_B_PERIPHERAL) {
			otg_start_gadget(fsm, 0);
		}

		otg->state = OTG_STATE_UNDEFINED;
		goto exit;
	}

	switch (otg->state) {
	case OTG_STATE_UNDEFINED:
		if (fsm->id)
			otg->state = OTG_STATE_B_IDLE;
		else
			otg->state = OTG_STATE_A_IDLE;
		break;
	case OTG_STATE_B_IDLE:
		if (!fsm->id) {
			otg->state = OTG_STATE_A_IDLE;
		} else if (fsm->b_sess_vld) {
			ret = otg_start_gadget(fsm, 1);
			if (!ret)
				otg->state = OTG_STATE_B_PERIPHERAL;
			else
				pr_err("OTG SM: cannot start gadget\n");
		}
		break;
	case OTG_STATE_B_PERIPHERAL:
		if (!fsm->id || !fsm->b_sess_vld) {
			ret = otg_start_gadget(fsm, 0);
			if (!ret)
				otg->state = OTG_STATE_B_IDLE;
			else
				pr_err("OTG SM: cannot stop gadget\n");
		}
		break;
	case OTG_STATE_A_IDLE:
		if (fsm->id) {
			otg->state = OTG_STATE_B_IDLE;
		} else {
			ret = otg_start_host(fsm, 1);
			if (!ret) {
				otg_drv_vbus(fsm, 1);
				otg->state = OTG_STATE_A_HOST;
			} else {
				pr_err("OTG SM: cannot start host\n");
			}
		}
		break;
	case OTG_STATE_A_HOST:
		if (fsm->id) {
			otg_drv_vbus(fsm, 0);
			ret = otg_start_host(fsm, 0);
			if (!ret)
				otg->state = OTG_STATE_A_IDLE;
			else
				pr_err("OTG SM: cannot stop host\n");
		}
		break;
	default:
		pr_err("OTG SM: invalid state\n");
	}

exit:
	if (!ret)
		ret = (otg->state != prev_state);

	pr_debug("OTG SM: %s => %s\n", usb_otg_state_string(prev_state),
		(ret > 0) ? usb_otg_state_string(otg->state) : "(no change)");

	return ret;
}

/* -------------------------------------------------------------------------- */

static struct dwc3_ext_otg_ops *dwc3_otg_exynos_rsw_probe(struct dwc3 *dwc)
{
	struct dwc3_ext_otg_ops *ops;
	bool ext_otg;

	//ext_otg = dwc3_exynos_rsw_available(dwc->dev->parent); // two module
	ext_otg = dwc3_exynos_rsw_available(dwc->dev); // one module
	if (!ext_otg) {
		dev_err(dwc->dev, "failed to get ext_otg\n");
		return NULL;
	}

	/* Allocate and init otg instance */
	ops = devm_kzalloc(dwc->dev, sizeof(struct dwc3_ext_otg_ops),
			GFP_KERNEL);
	if (!ops) {
		 dev_err(dwc->dev, "unable to allocate dwc3_ext_otg_ops\n");
		 return NULL;
	}

	ops->setup = dwc3_exynos_rsw_setup;
	ops->exit = dwc3_exynos_rsw_exit;
	ops->start = dwc3_exynos_rsw_start;
	ops->stop = dwc3_exynos_rsw_stop;

	dev_err(dwc->dev, "dwc3_otg_exynos_rsw_probe done\n");

	return ops;
}

static void dwc3_otg_set_host_mode(struct dwc3_otg *dotg)
{
	struct dwc3 *dwc = dotg->dwc;
	u32 reg;

	if (dotg->regs) {
		reg = dwc3_readl(dotg->regs, DWC3_OCTL);
		reg &= ~DWC3_OTG_OCTL_PERIMODE;
		dwc3_writel(dotg->regs, DWC3_OCTL, reg);
	} else {
		dwc3_set_mode(dwc, DWC3_GCTL_PRTCAP_HOST);
	}
}

static void dwc3_otg_set_peripheral_mode(struct dwc3_otg *dotg)
{
	struct dwc3 *dwc = dotg->dwc;
	u32 reg;

	if (dotg->regs) {
		reg = dwc3_readl(dotg->regs, DWC3_OCTL);
		reg |= DWC3_OTG_OCTL_PERIMODE;
		dwc3_writel(dotg->regs, DWC3_OCTL, reg);
	} else {
		dwc3_set_mode(dwc, DWC3_GCTL_PRTCAP_DEVICE);
	}
}

static void dwc3_otg_drv_vbus(struct otg_fsm *fsm, int on)
{
	struct dwc3_otg	*dotg = container_of(fsm, struct dwc3_otg, fsm);
	int ret;

	if (IS_ERR(dotg->vbus_reg)) {
		dev_err(dotg->dwc->dev, "vbus regulator is not available\n");
		return;
	}

	if (on)
		ret = regulator_enable(dotg->vbus_reg);
	else
		ret = regulator_disable(dotg->vbus_reg);

	if (ret)
		dev_err(dotg->dwc->dev, "failed to turn Vbus %s\n",
						on ? "on" : "off");
}

void dwc3_otg_check_bus_act(struct dwc3 *dwc)
{
	u32 reg;
	u32 xm_wtran, xm_rtran, xm_ch_status;
	int retries = 100;

	reg = dwc3_readl(dwc->regs, DWC3_GDBGLSPMUX_HST);
	reg |= BUS_ACTIVITY_CHECK;
	dwc3_writel(dwc->regs, DWC3_GDBGLSPMUX_HST, reg);

	do {
		reg = readl(phycon_base_addr + LINK_DEBUG_L);
		xm_ch_status = reg & 0x3FF;
		xm_rtran = (reg >> READ_TRANS_OFFSET) & 0x3FFFFF;
		reg = readl(phycon_base_addr + LINK_DEBUG_H);
		xm_wtran = reg & 0x3FFFFF;

		if (!xm_rtran && !xm_wtran)
			break;
		mdelay(1);
	} while (--retries);

	pr_info("%s %s: retries = %d\n", __func__,
		retries ? "clear" : "timeout", retries);
}

int exynos_usbdrd_inform_dp_use(int use, int lane_cnt)
{
	int ret = 0;

	pr_info("[%s] dp use = %d, lane_cnt = %d\n",
			__func__, use, lane_cnt);

	if ((use == 1) && (lane_cnt == 4)) {
#if defined(CONFIG_USB_PORT_POWER_OPTIMIZATION)
		ret = xhci_portsc_set(0);
#endif
		udelay(1);
	}

	return ret;
}
EXPORT_SYMBOL(exynos_usbdrd_inform_dp_use);

void exynos_usbdrd_request_phy_isol(void)
{

}

static void dwc3_otg_qos_lock_delayed_work(struct work_struct *wk)
{
	struct delayed_work *delay_work =
		container_of(wk, struct delayed_work, work);
	struct dwc3 *dwc = container_of(delay_work, struct dwc3, usb_qos_lock_delayed_work);
	struct dwc3_otg	*dotg = dwc->dotg;
	struct device *dev = dotg->dwc->dev;

	dev_info(dev, "%s\n", __func__);
	dwc3_exynos_set_bus_clock(dwc->dev, -1);
}

void dwc3_otg_pm_ctrl(struct dwc3 *dwc, int onoff)
{
	u32 reg;

	if (onoff == 0) {
		pr_info("Disable USB U1/U2 for performance.\n");
		/* Disable U1U2 */
		reg = dwc3_readl(dwc->regs, DWC3_DCTL);
		reg &= ~(DWC3_DCTL_INITU1ENA | DWC3_DCTL_ACCEPTU1ENA |
				DWC3_DCTL_INITU2ENA | DWC3_DCTL_ACCEPTU2ENA);
		dwc3_writel(dwc->regs, DWC3_DCTL, reg);

	} else {
		/* Enable U1U2 */
		reg = dwc3_readl(dwc->regs, DWC3_DCTL);
		reg |= (DWC3_DCTL_INITU1ENA | DWC3_DCTL_ACCEPTU1ENA |
				DWC3_DCTL_INITU2ENA | DWC3_DCTL_ACCEPTU2ENA);
		dwc3_writel(dwc->regs, DWC3_DCTL, reg);
	}
}

/*
    owner 0 - USB
    owner 1 - DP
 */
static void usb3_phy_control(struct dwc3_otg	*dotg,
		int owner, int on)
{
	struct dwc3	*dwc = dotg->dwc;
	struct device	*dev = dwc->dev;

	dev_info(dev, "%s, USB3.0 PHY %s\n", __func__, on ? "on" : "off");

	if (on) {
		dwc3_core_susphy_set(dwc, 0);
		exynos_usbdrd_pipe3_enable(dwc->usb3_generic_phy);
		dwc3_core_susphy_set(dwc, 1);
	} else {
		dwc3_core_susphy_set(dwc, 0);
		exynos_usbdrd_pipe3_disable(dwc->usb3_generic_phy);
		dwc3_core_susphy_set(dwc, 1);
	}
}

void usb_power_notify_control(int owner, int on)
{
	struct dwc3_otg	*dotg;
	struct device	*dev;
	u8 owner_bit = 0;

	if (g_dwc && g_dwc->dotg && g_dwc->dev) {
		dotg = g_dwc->dotg;
		dev = g_dwc->dev;
	} else {
		pr_err("%s g_dwc or dotg or dev NULL\n", __func__);
		goto err;
	}

	dev_info(dev, "%s, combo phy = %d, owner= %d, on = %d\n",
			__func__, dotg->combo_phy_control, owner, on);

	mutex_lock(&dotg->lock);

	owner_bit = (1 << owner);

	if (on) {
		if (dotg->combo_phy_control) {
			dotg->combo_phy_control |= owner_bit;
			goto out;
		} else {
			usb3_phy_control(dotg, owner, on);
			dotg->combo_phy_control |= owner_bit;
		}
	} else {
		dotg->combo_phy_control &= ~(owner_bit);

		if (dotg->combo_phy_control == 0)
			usb3_phy_control(dotg, owner, on);
	}
out:
	mutex_unlock(&dotg->lock);
err:
	return;
}
EXPORT_SYMBOL(usb_power_notify_control);

int dwc3_otg_phy_enable(struct otg_fsm *fsm, int owner, bool on)
{
	struct usb_otg	*otg = fsm->otg;
	struct dwc3_otg	*dotg = container_of(otg, struct dwc3_otg, otg);
	struct dwc3	*dwc = dotg->dwc;
	struct device	*dev = dotg->dwc->dev;
	int ret = 0;
	u8 owner_bit = 0;

	mutex_lock(&dotg->lock);

	dev_info(dev, "%s combo phy=%d usb2 phy=%d owner=%d (usb:0 dp:1) on=%d\n",
			__func__, dotg->combo_phy_control, dotg->usb2_phy_control,
				owner, on);

	if (owner > 1)
		goto out;

	owner_bit = (1 << owner);

	if (on) {
		phy_status = 1;
		if (dotg->combo_phy_control && dotg->usb2_phy_control) {
			dotg->combo_phy_control |= owner_bit;
			dotg->usb2_phy_control |= owner_bit;
			goto out;
		} else if (dotg->usb2_phy_control == 0) {
			/*
			 * dwc3_otg_phy_enable function enables
			 * both combo phy and usb2 phy
			 */
			exynos_usbdrd_phy_conn(dwc->usb2_generic_phy, 1);

			pm_runtime_get_sync(dev);
			dwc3_exynos_set_sclk_clock(dwc->dev);
			dwc3_exynos_set_bus_clock(dwc->dev, 1);
			ret = dwc3_core_init(dwc);
			if (ret) {
				dev_err(dwc->dev, "%s: failed to reinitialize core\n",
						__func__);
				goto err;
			}

			ret = dwc3_exynos_core_init(dwc);
			if (ret) {
				dev_err(dev, "failed to reinitialize exynos core\n");
				goto err;
			}

			dotg->combo_phy_control |= owner_bit;
			dotg->usb2_phy_control |= owner_bit;
		} else { /* dotg->combo_phy_control == 0 */
			usb3_phy_control(dotg, owner, on);
			dotg->combo_phy_control |= owner_bit;
		}
	} else {
		dotg->combo_phy_control &= ~(owner_bit);
		dotg->usb2_phy_control &= ~(owner_bit);

		if (dotg->usb2_phy_control == 0
				&& dotg->combo_phy_control == 0) {

			phy_status = 0;
			/*
			 * Need to check why phy init_count has mismatch.
			 * Phy_init is called from usb_phy_roothub_init() in
			 * usb_add_hcd function(usb/core/hcd.c)
			 */
			dwc->usb2_generic_phy->init_count = 1;
			dwc->usb3_generic_phy->init_count = 1;
			dwc->usb2_generic_phy->power_count = 1;
			dwc->usb3_generic_phy->power_count = 1;

			dwc3_core_exit(dwc);
err:
			dwc3_exynos_set_bus_clock(dwc->dev, -1);
			dwc3_otg_check_bus_act(dwc);
			pm_runtime_put_sync_suspend(dev);
			exynos_usbdrd_phy_conn(dwc->usb2_generic_phy, 0);
		}
	}
out:
	mutex_unlock(&dotg->lock);
	return ret;
}
EXPORT_SYMBOL_GPL(dwc3_otg_phy_enable);

int list_clear = 0;
static int dwc3_otg_start_host(struct otg_fsm *fsm, int on)
{
	struct usb_otg	*otg = fsm->otg;
	struct dwc3_otg	*dotg = container_of(otg, struct dwc3_otg, otg);
	struct dwc3	*dwc = dotg->dwc;
	struct device	*dev = dotg->dwc->dev;
	int ret = 0, i;
	int ret1 = -1;

	if (!dotg->dwc->xhci) {
		dev_err(dev, "%s: does not have any xhci\n", __func__);
		return -EINVAL;
	}

	dev_info(dev, "Turn %s host\n", on ? "on" : "off");

	if (on) {
		otg_connection = 1;
		ret = dwc3_otg_phy_enable(fsm, 0, on);
		if (ret) {
			dev_err(dwc->dev, "%s: failed to reinitialize core\n",
					__func__);
			goto err1;
		}

		dwc3_otg_set_host_mode(dotg);

		if (list_clear == 1)
			INIT_LIST_HEAD(&dwc->xhci->dev.links.needs_suppliers);
		ret = platform_device_add(dwc->xhci);
		if (ret) {
			dev_err(dev, "%s: cannot add xhci\n", __func__);
			goto err2;
		}

#if defined(CONFIG_USB_PORT_POWER_OPTIMIZATION)
		register_usb_power_notify();
#endif
#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
		usbpd_set_host_on(dotg->man, on);
#endif
	} else {
		otg_connection = 0;
#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
		usbpd_set_host_on(dotg->man, on);
#endif
#if defined(CONFIG_USB_PORT_POWER_OPTIMIZATION)
		xhci_port_power_set(1, 3);
		unregister_usb_power_notify();
#endif
		if (dotg->dwc3_suspended) {
			pr_info("%s: wait resume completion\n", __func__);
			ret1 = wait_for_completion_timeout(&dotg->resume_cmpl,
							msecs_to_jiffies(5000));
		}

#ifdef CONFIG_SND_EXYNOS_USB_AUDIO
		/* USB audio disconnect progressing */
		for (i = 0; i < 1000; i += 100) {
			if (usb_audio_connection) {
				usleep_range(100000, 110000);
				pr_info("%s: wait audio disconnect\n", __func__);
			} else {
				pr_info("%s: audio disconnect DONE!!\n", __func__);
				break;
			}
		}
#endif
		platform_device_del(dwc->xhci);
		dwc->xhci->dev.p->dead = 0;
		list_clear = 1;

err2:
		ret = dwc3_otg_phy_enable(fsm, 0, on);
	}
err1:
	return ret;
}

#ifdef CONFIG_DWC3_RETRY_CONNECTION
int exynos_usb_recovery_reconn(void);

static void retry_configuration(struct timer_list *t)
{
	struct dwc3 *dwc = from_timer(dwc, t, usb_connect_timer);
	struct usb_gadget *gadget = &dwc->gadget;
	struct usb_composite_dev *cdev = get_gadget_data(gadget);

	pr_info("%s: +++\n", __func__);

	if (cdev == NULL || dwc->retry_cnt == REMOVED_RETRY_CNT) {
		pr_err("Stop retry configuration(cdev is NULL) or Removed\n");
		return;
	}

	if (!cdev->config) {
		if (dwc->retry_cnt > MAX_RETRY_CNT) {
			pr_err("%s: Re-try 5 times, But usb enumeration fail\n",
					__func__);
			return;
		}

		pr_info("%s: retry USB enumeration\n", __func__);

		exynos_usb_recovery_reconn();

		dwc->retry_cnt += 1;
	} else {
		dwc->retry_cnt = 0;
		pr_info("%s: already configuration done!!\n", __func__);
		return;
	}

	pr_info("%s: ---\n", __func__);
}
#endif

static int dwc3_otg_start_gadget(struct otg_fsm *fsm, int on)
{
	struct usb_otg	*otg = fsm->otg;
	struct dwc3_otg	*dotg = container_of(otg, struct dwc3_otg, otg);
	struct dwc3	*dwc = dotg->dwc;
	struct device	*dev = dotg->dwc->dev;
	int ret = 0;

	if (!otg->gadget) {
		dev_err(dev, "%s does not have any gadget\n", __func__);
		return -EINVAL;
	}

	dev_info(dev, "Turn %s gadget %s\n",
			on ? "on" : "off", otg->gadget->name);

	if (on) {
		__pm_stay_awake(dotg->wakelock);
		dwc->vbus_state = true;
		ret = dwc3_otg_phy_enable(fsm, 0, on);
		if (ret)
			dev_err(dwc->dev, "%s: failed to reinitialize core\n",
					__func__);

		dwc3_otg_set_peripheral_mode(dotg);
		ret = usb_gadget_vbus_connect(otg->gadget);

		if (ret) {
			dev_err(dwc->dev, "%s: vbus connect failed\n",
					__func__);
			/* Reinitialize LDO and PHY */
			dotg->combo_phy_control = 0;
			dwc3_gadget_disconnect_proc(dwc);
			usb_gadget_vbus_disconnect(otg->gadget);

			mdelay(200);
			ret = dwc3_otg_phy_enable(fsm, 0, 0);
			mdelay(200);
			ret = dwc3_otg_phy_enable(fsm, 0, 1);
#ifdef CONFIG_USB_EXYNOS_TPMON
			usb_tpmon_open();
#endif
			dwc3_otg_set_peripheral_mode(dotg);
			ret = usb_gadget_vbus_connect(otg->gadget);
			if (ret)
				dev_err(dwc->dev, "%s: vbus connect failed again\n",
					__func__);
		}

#ifdef CONFIG_DWC3_RETRY_CONNECTION
		pr_info("%s: start check usb configuration timer\n", __func__);
		timer_setup(&dwc->usb_connect_timer, retry_configuration, 0);
		mod_timer(&dwc->usb_connect_timer,
				jiffies + CHG_CONNECTED_DELAY_TIME);
#endif
	} else {
		dwc->vbus_state = false;
#ifdef CONFIG_DWC3_RETRY_CONNECTION
		dwc->retry_cnt = REMOVED_RETRY_CNT;
		del_timer_sync(&dwc->usb_connect_timer);
#endif
#ifdef CONFIG_USB_EXYNOS_TPMON
		usb_tpmon_close();
#endif
		ret = usb_gadget_vbus_disconnect(otg->gadget);
		if (ret)
			dev_err(dwc->dev, "%s: vbus disconnect failed\n",
					__func__);

		dwc3_gadget_disconnect_proc(dwc);
		/* avoid missing disconnect interrupt */
		ret = wait_for_completion_timeout(&dwc->disconnect,
				msecs_to_jiffies(200));
		if (!ret) {
			dev_err(dwc->dev, "%s: disconnect completion timeout\n",
					__func__);
			return ret;
		}
		ret = dwc3_otg_phy_enable(fsm, 0, on);
		__pm_relax(dotg->wakelock);
	}

	return 0;
}

static struct otg_fsm_ops dwc3_otg_fsm_ops = {
	.drv_vbus	= dwc3_otg_drv_vbus,
	.start_host	= dwc3_otg_start_host,
	.start_gadget	= dwc3_otg_start_gadget,
};

/* -------------------------------------------------------------------------- */

void dwc3_otg_run_sm(struct otg_fsm *fsm)
{
	struct dwc3_otg	*dotg = container_of(fsm, struct dwc3_otg, fsm);
	int		state_changed;

	/* Prevent running SM on early system resume */
	if (!dotg->ready)
		return;

	mutex_lock(&fsm->lock);

	do {
		state_changed = dwc3_otg_statemachine(fsm);
	} while (state_changed > 0);

	mutex_unlock(&fsm->lock);
}
EXPORT_SYMBOL_GPL(dwc3_otg_run_sm);

/* Bind/Unbind the peripheral controller driver */
static int dwc3_otg_set_peripheral(struct usb_otg *otg,
				struct usb_gadget *gadget)
{
	struct dwc3_otg	*dotg = container_of(otg, struct dwc3_otg, otg);
	struct otg_fsm	*fsm = &dotg->fsm;
	struct device	*dev = dotg->dwc->dev;

	if (gadget) {
		dev_info(dev, "Binding gadget %s\n", gadget->name);

		otg->gadget = gadget;
	} else {
		dev_info(dev, "Unbinding gadget\n");

		mutex_lock(&fsm->lock);

		if (otg->state == OTG_STATE_B_PERIPHERAL) {
			/* Reset OTG Statemachine */
			fsm->reset = 1;
			dwc3_otg_statemachine(fsm);
			fsm->reset = 0;
		}
		otg->gadget = NULL;

		mutex_unlock(&fsm->lock);

		dwc3_otg_run_sm(fsm);
	}

	return 0;
}

static int dwc3_otg_get_id_state(struct dwc3_otg *dotg)
{
	u32 reg = dwc3_readl(dotg->regs, DWC3_OSTS);

	return !!(reg & DWC3_OTG_OSTS_CONIDSTS);
}

static int dwc3_otg_get_b_sess_state(struct dwc3_otg *dotg)
{
	u32 reg = dwc3_readl(dotg->regs, DWC3_OSTS);

	return !!(reg & DWC3_OTG_OSTS_BSESVALID);
}

static irqreturn_t dwc3_otg_interrupt(int irq, void *_dotg)
{
	struct dwc3_otg	*dotg = (struct dwc3_otg *)_dotg;
	struct otg_fsm	*fsm = &dotg->fsm;
	u32 oevt, handled_events = 0;
	irqreturn_t ret = IRQ_NONE;

	oevt = dwc3_readl(dotg->regs, DWC3_OEVT);

	/* ID */
	if (oevt & DWC3_OEVTEN_OTGCONIDSTSCHNGEVNT) {
		fsm->id = dwc3_otg_get_id_state(dotg);
		handled_events |= DWC3_OEVTEN_OTGCONIDSTSCHNGEVNT;
	}

	/* VBus */
	if (oevt & DWC3_OEVTEN_OTGBDEVVBUSCHNGEVNT) {
		fsm->b_sess_vld = dwc3_otg_get_b_sess_state(dotg);
		handled_events |= DWC3_OEVTEN_OTGBDEVVBUSCHNGEVNT;
	}

	if (handled_events) {
		dwc3_writel(dotg->regs, DWC3_OEVT, handled_events);
		ret = IRQ_WAKE_THREAD;
	}

	return ret;
}

static irqreturn_t dwc3_otg_thread_interrupt(int irq, void *_dotg)
{
	struct dwc3_otg	*dotg = (struct dwc3_otg *)_dotg;

	dwc3_otg_run_sm(&dotg->fsm);

	return IRQ_HANDLED;
}

static void dwc3_otg_enable_irq(struct dwc3_otg *dotg)
{
	/* Enable only connector ID status & VBUS change events */
	dwc3_writel(dotg->regs, DWC3_OEVTEN,
			DWC3_OEVTEN_OTGCONIDSTSCHNGEVNT |
			DWC3_OEVTEN_OTGBDEVVBUSCHNGEVNT);
}

static void dwc3_otg_disable_irq(struct dwc3_otg *dotg)
{
	dwc3_writel(dotg->regs, DWC3_OEVTEN, 0x0);
}

static void dwc3_otg_reset(struct dwc3_otg *dotg)
{
	/*
	 * OCFG[2] - OTG-Version = 0
	 * OCFG[1] - HNPCap = 0
	 * OCFG[0] - SRPCap = 0
	 */
	dwc3_writel(dotg->regs, DWC3_OCFG, 0x0);

	/*
	 * OCTL[6] - PeriMode = 1
	 * OCTL[5] - PrtPwrCtl = 0
	 * OCTL[4] - HNPReq = 0
	 * OCTL[3] - SesReq = 0
	 * OCTL[2] - TermSelDLPulse = 0
	 * OCTL[1] - DevSetHNPEn = 0
	 * OCTL[0] - HstSetHNPEn = 0
	 */
	dwc3_writel(dotg->regs, DWC3_OCTL, DWC3_OTG_OCTL_PERIMODE);

	/* Clear all otg events (interrupts) indications  */
	dwc3_writel(dotg->regs, DWC3_OEVT, (u32)DWC3_OEVT_CLEAR_ALL);
}

/* -------------------------------------------------------------------------- */

static ssize_t
dwc3_otg_show_state(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dwc3_exynos	*exynos = dev_get_drvdata(dev);
	struct dwc3		*dwc = exynos->dwc;
	struct usb_otg		*otg = &dwc->dotg->otg;

	return snprintf(buf, PAGE_SIZE, "%s\n",
			usb_otg_state_string(otg->state));
}

static DEVICE_ATTR(state, S_IRUSR | S_IRGRP,
	dwc3_otg_show_state, NULL);

static ssize_t
dwc3_otg_show_b_sess(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dwc3_exynos	*exynos = dev_get_drvdata(dev);
	struct dwc3		*dwc = exynos->dwc;
	struct otg_fsm	*fsm = &dwc->dotg->fsm;

	return snprintf(buf, PAGE_SIZE, "%d\n", fsm->b_sess_vld);
}

static ssize_t
dwc3_otg_store_b_sess(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t n)
{
	struct dwc3_exynos	*exynos = dev_get_drvdata(dev);
	struct dwc3		*dwc = exynos->dwc;
	struct otg_fsm	*fsm = &dwc->dotg->fsm;
	int		b_sess_vld;

	if (sscanf(buf, "%d", &b_sess_vld) != 1)
		return -EINVAL;

	fsm->b_sess_vld = !!b_sess_vld;

	dwc3_otg_run_sm(fsm);

	return n;
}

static DEVICE_ATTR(b_sess, S_IWUSR | S_IRUSR | S_IRGRP,
	dwc3_otg_show_b_sess, dwc3_otg_store_b_sess);

static ssize_t
dwc3_otg_show_id(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dwc3_exynos	*exynos = dev_get_drvdata(dev);
	struct dwc3		*dwc = exynos->dwc;
	struct otg_fsm	*fsm = &dwc->dotg->fsm;

	return snprintf(buf, PAGE_SIZE, "%d\n", fsm->id);
}

static ssize_t
dwc3_otg_store_id(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t n)
{
	struct dwc3_exynos	*exynos = dev_get_drvdata(dev);
	struct dwc3		*dwc = exynos->dwc;
	struct otg_fsm	*fsm = &dwc->dotg->fsm;
	int id;

	if (sscanf(buf, "%d", &id) != 1)
		return -EINVAL;

	fsm->id = !!id;

	dwc3_otg_run_sm(fsm);

	return n;
}

static DEVICE_ATTR(id, S_IWUSR | S_IRUSR | S_IRGRP,
	dwc3_otg_show_id, dwc3_otg_store_id);

static struct attribute *dwc3_otg_attributes[] = {
	&dev_attr_id.attr,
	&dev_attr_b_sess.attr,
	&dev_attr_state.attr,
	NULL
};

static const struct attribute_group dwc3_otg_attr_group = {
	.attrs = dwc3_otg_attributes,
};

/**
 * dwc3_otg_start
 * @dwc: pointer to our controller context structure
 */
int dwc3_otg_start(struct dwc3 *dwc)
{
	struct dwc3_otg	*dotg = dwc->dotg;
	struct otg_fsm	*fsm = &dotg->fsm;
	int		ret;

	if (dotg->ext_otg_ops) {
		ret = dwc3_ext_otg_start(dotg);
		if (ret) {
			dev_err(dwc->dev, "failed to start external OTG\n");
			return ret;
		}
	} else {
		dotg->regs = dwc->regs;

		dwc3_otg_reset(dotg);

		dotg->fsm.id = dwc3_otg_get_id_state(dotg);
		dotg->fsm.b_sess_vld = dwc3_otg_get_b_sess_state(dotg);

		dotg->irq = platform_get_irq(to_platform_device(dwc->dev), 0);
		ret = devm_request_threaded_irq(dwc->dev, dotg->irq,
				dwc3_otg_interrupt,
				dwc3_otg_thread_interrupt,
				IRQF_SHARED, "dwc3-otg", dotg);
		if (ret) {
			dev_err(dwc->dev, "failed to request irq #%d --> %d\n",
					dotg->irq, ret);
			return ret;
		}

		dwc3_otg_enable_irq(dotg);
	}

	dotg->ready = 1;

	dwc3_otg_run_sm(fsm);

	return 0;
}

/**
 * dwc3_otg_stop
 * @dwc: pointer to our controller context structure
 */
void dwc3_otg_stop(struct dwc3 *dwc)
{
	struct dwc3_otg         *dotg = dwc->dotg;

	if (dotg->ext_otg_ops) {
		dwc3_ext_otg_stop(dotg);
	} else {
		dwc3_otg_disable_irq(dotg);
		free_irq(dotg->irq, dotg);
	}

	dotg->ready = 0;
}

/* -------------------------------------------------------------------------- */
u32 otg_is_connect(void)
{
	if (!otg_connection)
		return OTG_NO_CONNECT;
#if defined(CONFIG_USB_PORT_POWER_OPTIMIZATION)
	else if (is_otg_only)
		return OTG_CONNECT_ONLY;
#endif
	else
		return OTG_DEVICE_CONNECT;
}
EXPORT_SYMBOL_GPL(otg_is_connect);

u32 get_speed_and_disu1u2(void)
{
	u32 reg;
	struct dwc3 *dwc = g_dwc;

	/* Disable U1/U2 */
	reg = dwc3_readl(dwc->regs, DWC3_DCTL);
	reg &= ~(DWC3_DCTL_INITU1ENA | DWC3_DCTL_ACCEPTU1ENA |
			DWC3_DCTL_INITU2ENA | DWC3_DCTL_ACCEPTU2ENA);
	dwc3_writel(dwc->regs, DWC3_DCTL, reg);

	return dwc->level_val;
}
EXPORT_SYMBOL_GPL(get_speed_and_disu1u2);

int get_idle_ip_index(void)
{
	if (g_dwc == NULL) {
		pr_info("Idle ip index will be set next time.(g_dwc==NULL)\n");
		return -ENODEV;
	}
	return dwc3_exynos_get_idle_ip_index(g_dwc->dev);

}
EXPORT_SYMBOL_GPL(get_idle_ip_index);

#ifdef CONFIG_DWC3_RETRY_CONNECTION
struct work_struct recovery_reconn_work;

static void dwc3_recovery_reconnection(struct work_struct *w)
{
	struct dwc3_otg *dotg = g_dwc->dotg;
	struct otg_fsm	*fsm = &dotg->fsm;
	int ret = 0;

	/* Lock to avoid real cable insert/remove operation. */
	mutex_lock(&fsm->lock);

	/* To ignore PHY disable */
	dwc3_otg_phy_enable(fsm, DWC3_PHY_OWNER_EMEG, 1);

	if (otg_connection == 1) {
		pr_err("Recovery Host Reconnection\n");
		ret = dwc3_otg_start_host(fsm, 0);
		if (ret < 0) {
			pr_err("Cable was already disconnected!!\n");
			goto emeg_out;
		}
	} else {
		pr_err("Recovery Gadget Reconnection\n");
		if (g_dwc->vbus_state == false) {
			pr_err("Cable was already disconnected!!\n");
			goto emeg_out;
		}
		dwc3_otg_start_gadget(fsm, 0);
	}

	msleep(50);
	if (otg_connection == 1)
		dwc3_otg_start_host(fsm, 1);
	else
		dwc3_otg_start_gadget(fsm, 1);

emeg_out:
	dwc3_otg_phy_enable(fsm, DWC3_PHY_OWNER_EMEG, 0);

	mutex_unlock(&fsm->lock);
}

int exynos_usb_recovery_reconn(void)
{
	if (g_dwc == NULL) {
		pr_err("WARNING : g_dwc is NULL\n");
		return -ENODEV;
	}

	schedule_work(&recovery_reconn_work);

	return 0;
}
EXPORT_SYMBOL_GPL(exynos_usb_recovery_reconn);
#endif

static int dwc3_otg_pm_notifier(struct notifier_block *nb,
		unsigned long action, void *nb_data)
{
	struct dwc3_otg *dotg
		= container_of(nb, struct dwc3_otg, pm_nb);

	switch (action) {
	case PM_SUSPEND_PREPARE:
		pr_info("%s suspend prepare\n", __func__);
		dotg->dwc3_suspended = 1;
		reinit_completion(&dotg->resume_cmpl);
		break;
	case PM_POST_SUSPEND:
		pr_info("%s post suspend\n", __func__);
		dotg->dwc3_suspended = 0;
		complete(&dotg->resume_cmpl);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

#if defined(CONFIG_OTG_DEFAULT)
static void typec_work_func(struct work_struct *work)
{
#if defined(CONFIG_TYPEC)
	struct dwc3_otg		*dotg = container_of(work, struct dwc3_otg,
							typec_work.work);
	struct dwc3		*dwc = dotg->dwc;
	struct intf_typec	*typec = dotg->typec;
	struct typec_partner_desc partner;

	pr_info("%s\n", __func__);

	typec->cap.type = TYPEC_PORT_DRP;
	typec->cap.revision = USB_TYPEC_REV_1_2;
	typec->cap.pd_revision = 0x312;
	typec->cap.prefer_role = TYPEC_NO_PREFERRED_ROLE;

	typec->port = typec_register_port(dwc->dev, &typec->cap);
	if (!typec->port) {
		dev_err(dwc->dev, "failed register port\n");
		return;
	}

	typec_set_data_role(typec->port, TYPEC_DEVICE);
	typec_set_pwr_role(typec->port, TYPEC_SINK);
	typec_set_pwr_opmode(typec->port, TYPEC_PWR_MODE_USB);

	memset(&partner, 0, sizeof(struct typec_partner_desc));

	typec->partner = typec_register_partner(typec->port, &partner);
	if (!dotg->typec->partner)
		dev_err(dwc->dev, "failed register partner\n");
#endif
}
#endif

static int usb_reboot_noti(struct notifier_block *nb, unsigned long event, void *buf)
{
	struct dwc3_otg *dotg = g_dwc->dotg;
	struct dwc3	*dwc = dotg->dwc;
	struct otg_fsm  *fsm = &dotg->fsm;

	switch (event) {
	case SYS_RESTART:
		if (otg_connection == 1) {
			dev_info(dwc->dev, "Host enabled. Turn off host\n");
			fsm->id = 1;
			dwc3_otg_run_sm(fsm);
		}
		break;
	}
	return 0;
}

#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
void dwc3_usb_set_vbus_current(void *data, int state)
{
	struct dwc3_otg *dotg;
	struct dwc3 *dwc;

	dotg = (struct dwc3_otg *)data;

	dwc = dotg->dwc;
	if (!dwc) {
		pr_err("%s dwc is null\n", __func__);
		goto err;
	}

	dev_info(dwc->dev, "%s : set current %d to %d\n",
		__func__, dwc->vbus_current, state);

	dwc->vbus_current = state;
	schedule_work(&dwc->set_vbus_current_work);
err:
	return;
}

struct usb_ops ops_usb = {
	.usb_set_vbus_current = dwc3_usb_set_vbus_current,
};
#endif

int dwc3_otg_init(struct dwc3 *dwc)
{
	struct dwc3_otg *dotg;
	struct dwc3_ext_otg_ops *ops = NULL;
	int ret = 0;
#if defined(CONFIG_OTG_DEFAULT)
	struct intf_typec	*typec;
#endif

	dev_info(dwc->dev, "%s\n", __func__);

	/* EXYNOS SoCs don't have HW OTG, but support SW OTG. */
	ops = dwc3_otg_exynos_rsw_probe(dwc);
	if (!ops)
		return 0;

	g_dwc = dwc;
#ifdef CONFIG_DWC3_RETRY_CONNECTION
	INIT_WORK(&recovery_reconn_work, dwc3_recovery_reconnection);
#endif

	/* Allocate and init otg instance */
	dotg = devm_kzalloc(dwc->dev, sizeof(struct dwc3_otg), GFP_KERNEL);
	if (!dotg) {
		dev_err(dwc->dev, "unable to allocate dwc3_otg\n");
		return -ENOMEM;
	}


	/* This reference is used by dwc3 modules for checking otg existance */
	dwc->dotg = dotg;
	dotg->dwc = dwc;
	dev_info(dwc->dev, "%s, dotg = %8x\n", __func__, dwc->dotg);

	ret = of_property_read_u32(dwc->dev->of_node,"ldos", &dotg->ldos);
	if (ret < 0) {
		dev_err(dwc->dev, "can't get ldo information\n");
		return -EINVAL;
	}
#if 0
	ret = of_property_read_u32(dwc->dev->of_node,
				"usb-pm-qos-int", &dotg->pm_qos_int_val);
	if (ret < 0) {
		dev_err(dwc->dev, "couldn't read usb-pm-qos-int %s node, error = %d\n",
					dwc->dev->of_node->name, ret);
		dotg->pm_qos_int_val = 0;
	} else {
		pm_qos_add_request(&dotg->pm_qos_int_req,
					PM_QOS_DEVICE_THROUGHPUT, 0);
	}
#endif
	dotg->ext_otg_ops = ops;

	dotg->otg.set_peripheral = dwc3_otg_set_peripheral;
	dotg->otg.set_host = NULL;

	dotg->otg.state = OTG_STATE_UNDEFINED;

	mutex_init(&dotg->fsm.lock);
	dotg->fsm.ops = &dwc3_otg_fsm_ops;
	dotg->fsm.otg = &dotg->otg;

	dotg->vbus_reg = devm_regulator_get(dwc->dev, "dwc3-vbus");
	if (IS_ERR(dotg->vbus_reg))
		dev_err(dwc->dev, "failed to obtain vbus regulator\n");

	if (dotg->ext_otg_ops) {
		dev_info(dwc->dev, "%s, dwc3_ext_otg_setup call\n", __func__);
		ret = dwc3_ext_otg_setup(dotg);
		if (ret) {
			dev_err(dwc->dev, "failed to setup OTG\n");
			return ret;
		}
	}

	dotg->wakelock = wakeup_source_register(dwc->dev,
		"dwc3-otg");
	mutex_init(&dotg->lock);

	ret = sysfs_create_group(&dwc->dev->kobj, &dwc3_otg_attr_group);
	if (ret)
		dev_err(dwc->dev, "failed to create dwc3 otg attributes\n");

	/* Enable LDO initially */
	exynos_usbdrd_phy_conn(dwc->usb2_generic_phy, 1);

	init_completion(&dotg->resume_cmpl);
	dotg->dwc3_suspended = 0;
	dotg->pm_nb.notifier_call = dwc3_otg_pm_notifier;
	register_pm_notifier(&dotg->pm_nb);
	/*register_usb_is_connect(otg_is_connect);*/

#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
	dotg->usb_d.ops = &ops_usb;
	dotg->usb_d.data = (void *)dotg;
	dotg->man = register_usb(&dotg->usb_d);
#endif

	INIT_DELAYED_WORK(&dwc->usb_qos_lock_delayed_work,
					dwc3_otg_qos_lock_delayed_work);

#if defined(CONFIG_OTG_DEFAULT)
	INIT_DELAYED_WORK(&dotg->typec_work, typec_work_func);

	typec = devm_kzalloc(dwc->dev, sizeof(*typec), GFP_KERNEL);
	if (!typec)
		return -ENOMEM;

	/* mutex_init(&md05->lock); */
	typec->dev = dwc->dev;
	dotg->typec = typec;

	schedule_delayed_work(&dotg->typec_work,
			      msecs_to_jiffies(2000));
#endif

	exynos_usbdrd_phy_conn(dwc->usb2_generic_phy, 0);
	dev_info(dwc->dev, "%s done\n", __func__);
#ifdef CONFIG_USB_EXYNOS_TPMON
	usb_tpmon_init();
#endif
	ret = register_reboot_notifier(&usb_reboot_notifier);
	if (ret)
		dev_err(dwc->dev, "failed register reboot notifier\n");

	return 0;
}

void dwc3_otg_exit(struct dwc3 *dwc)
{
	struct dwc3_otg *dotg = dwc->dotg;

	if (!dotg->ext_otg_ops)
		return;

#if defined(CONFIG_OTG_DEFAULT)
	typec_unregister_partner(dotg->typec->partner);
	typec_unregister_port(dotg->typec->port);
#endif

	unregister_pm_notifier(&dotg->pm_nb);

	dwc3_ext_otg_exit(dotg);

	sysfs_remove_group(&dwc->dev->kobj, &dwc3_otg_attr_group);
	wakeup_source_unregister(dotg->wakelock);
	free_irq(dotg->irq, dotg);
	dotg->otg.state = OTG_STATE_UNDEFINED;
	dwc->dotg = NULL;
#ifdef CONFIG_USB_EXYNOS_TPMON
	usb_tpmon_exit();
#endif
}
