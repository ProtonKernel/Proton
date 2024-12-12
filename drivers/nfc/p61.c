/*
 * Copyright (C) 2012-2020 NXP Semiconductors
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
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/spi/spi.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/regulator/consumer.h>
#include <linux/ioctl.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/spi/spidev.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>

#include "nfc_wakelock.h"
#include "p61.h"
#include "pn547.h"
#include "cold_reset.h"
#ifdef CONFIG_SEC_NFC_LOGGER
#include "./nfc_logger/nfc_logger.h"
#endif
#if defined(CONFIG_ESE_SECURE) && defined(CONFIG_ESE_USE_TZ_API)
#include "../misc/tzdev/include/tzdev/tee_client_api.h"
#endif

extern long  pn547_dev_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg);

/* size of maximum read/write buffer supported by driver */
#ifdef CONFIG_NFC_FEATURE_SN100U
#define MAX_BUFFER_SIZE   780U
#else
#define MAX_BUFFER_SIZE   258U
#endif

/* Different driver debug lever */
enum P61_DEBUG_LEVEL {
	P61_DEBUG_OFF,
	P61_FULL_DEBUG
};

/* Variable to store current debug level request by ioctl */
static unsigned char debug_level = P61_FULL_DEBUG;
static unsigned char pwr_req_on;
#define P61_DBG_MSG(msg...) {\
	switch (debug_level) {\
	case P61_DEBUG_OFF:\
		break;\
	case P61_FULL_DEBUG:\
		NFC_LOG_INFO("[NXP-P61] " msg);\
		break;\
		/*fallthrough*/\
	default:\
		NFC_LOG_ERR("[NXP-P61] Wrong debug level(%d)\n", debug_level);\
		break;\
	} \
}

#define P61_ERR_MSG(msg...) NFC_LOG_ERR("[NXP-P61] " msg)
#define P61_INFO_MSG(msg...) NFC_LOG_INFO("[NXP-P61] " msg)

#if defined(CONFIG_ESE_SECURE) && defined(CONFIG_ESE_USE_TZ_API)
static TEEC_UUID ese_drv_uuid = {
	0x00000000, 0x0000, 0x0000, {0x00, 0x00, 0x65, 0x73, 0x65, 0x44, 0x72, 0x76}
};

enum pm_mode {
	PM_SUSPEND,
	PM_RESUME,
	SECURE_CHECK,
};
#endif

char *g_pin_status[] = {"sleep", "default", "lpm", "ese_on", "ese_off"};
#define ESE_PIN_STATUS_CNT 5

/* Device specific macro and structure */
struct p61_device {
	wait_queue_head_t read_wq; /* wait queue for read interrupt */
	struct mutex read_mutex; /* read mutex */
	struct mutex write_mutex; /* write mutex */
	struct spi_device *spi;  /* spi device structure */
	struct miscdevice miscdev; /* char device as misc driver */
	unsigned int rst_gpio; /* SW Reset gpio */
	unsigned int irq_gpio; /* P61 will interrupt DH for any ntf */
	bool irq_enabled; /* flag to indicate irq is used */
	unsigned char enable_poll_mode; /* enable the poll mode */
	spinlock_t irq_enabled_lock; /*spin lock for read irq */

	bool tz_mode;
	spinlock_t ese_spi_lock;
	bool isGpio_cfgDone;
	struct nfc_wake_lock ese_lock;
	bool device_opened;

	struct pinctrl *pinctrl;
	struct clk *ese_spi_clk[MAX_SPI_CLK_CNT];
	struct pinctrl_state *pinctrl_state[ESE_PIN_STATUS_CNT];

#ifdef CONFIG_NFC_FEATURE_SN100U
	int spi_cs_gpio;
	int pid;
	bool pid_diff;
#if IS_ENABLED(CONFIG_SPI_MSM_GENI)
	struct delayed_work spi_release_work;
	struct nfc_wake_lock spi_release_wakelock;
#endif
#endif
#if defined(CONFIG_ESE_SECURE) && defined(CONFIG_ESE_USE_TZ_API)
	int ese_secure_check;
#endif
	enum ap_vendors ap_vendor;
	unsigned char *buf;
	struct device_node *spi_device_node;
};
static struct p61_device *p61_dev;

/* T==1 protocol specific global data */
const unsigned char SOF = 0xA5u;

#if defined(CONFIG_ESE_SECURE)
static int p61_clk_control(struct p61_device *p61_dev, bool onoff);
#if defined(CONFIG_ESE_USE_TZ_API)
static uint32_t tz_tee_ese_drv(enum pm_mode mode);
#endif
#endif

static int ese_get_pin_ctrl_count(void)
{
	/* return ARRAY_SIZE(g_pin_status); */
	return ESE_PIN_STATUS_CNT;
}

#if defined(CONFIG_NFC_FEATURE_SN100U) || !defined(CONFIG_ESE_SECURE)
static int ese_get_pin_ctrl_number(char *str)
{
	int i;
	int cnt = ese_get_pin_ctrl_count();
	int ret = -1;

	for (i = 0; i < cnt; i++) {
		if (!strncmp(g_pin_status[i], str, strlen(str))) {
			ret = i;
			break;
		}
	}

	return ret;
}

static int ese_set_spi_configuration(char *name)
{
	int ret = 0;
	int pin_num = ese_get_pin_ctrl_number(name);

	NFC_LOG_INFO("%s [%s:%d]\n", __func__, name, pin_num);

	if (pin_num < 0 || !p61_dev->pinctrl_state[pin_num]) {
		NFC_LOG_INFO("%s pinctrl skip!(%d)\n", __func__, pin_num);
		return -EINVAL;
	}

	ret = pinctrl_select_state(p61_dev->pinctrl, p61_dev->pinctrl_state[pin_num]);
	if (ret < 0)
		NFC_LOG_INFO("%s pinctrl_select_state[%s] failed\n", __func__, name);

	return ret;
}
#endif

#if IS_ENABLED(CONFIG_SPI_MSM_GENI) && defined(CONFIG_NFC_FEATURE_SN100U)
static void p61_spi_release_work(struct work_struct *work)
{
	if (p61_dev == NULL) {
		NFC_LOG_ERR("%s: spi probe is not called\n", __func__);
		return;
	}

	NFC_LOG_INFO("release ese spi\n");
	ese_set_spi_configuration("sleep");
}
#endif

int ese_spi_pinctrl(int enable)
{
	int ret = 0;

	NFC_LOG_INFO("[p61] %s (%d)\n", __func__, enable);
	if (!p61_dev) {
		NFC_LOG_INFO("[p61] ese not probed!\n");
		return -ENODEV;
	}
	if (p61_dev->ap_vendor == AP_VENDOR_SLSI) {
		switch (enable) {
		case 0:
#ifdef CONFIG_ESE_SECURE
			p61_clk_control(p61_dev, false);
#ifdef CONFIG_ESE_USE_TZ_API
			tz_tee_ese_drv(PM_SUSPEND);
#endif
#else
			ese_set_spi_configuration("ese_off");
#endif
			break;
		case 1:
#ifdef CONFIG_ESE_SECURE
			p61_clk_control(p61_dev, true);
#ifdef CONFIG_ESE_USE_TZ_API
			tz_tee_ese_drv(PM_RESUME);
#endif
#else
			ese_set_spi_configuration("ese_on");
#endif

			break;
		default:
			NFC_LOG_ERR("%s no matching!\n", __func__);
			ret = -EINVAL;
		}
	} else if (p61_dev->ap_vendor == AP_VENDOR_MTK) {
		switch (enable) {
		case 0:
			ese_set_spi_configuration("ese_off");
			break;
		case 1:
			ese_set_spi_configuration("ese_on");
			break;
		default:
			NFC_LOG_ERR("%s no matching!\n", __func__);
			ret = -EINVAL;
		}
	}

	return ret;
}
EXPORT_SYMBOL_GPL(ese_spi_pinctrl);

#ifdef CONFIG_ESE_SECURE
static int p61_clk_control(struct p61_device *p61_dev, bool onoff)
{
	static bool old_value;
	int i;

	if (p61_dev->ap_vendor != AP_VENDOR_SLSI)
		return 0;

	if (old_value == onoff) {
		NFC_LOG_INFO("%s: ALREADY %s\n", __func__,
			onoff ? "enabled" : "disabled");
		return 0;
	}

	if (onoff == true) {
		for (i = 0; i < MAX_SPI_CLK_CNT; i++) {
			if (p61_dev->ese_spi_clk[i])
				clk_prepare_enable(p61_dev->ese_spi_clk[i]);
		}

		/* There is a quarter-multiplier before the USI_v2 SPI */
		clk_set_rate(p61_dev->ese_spi_clk[SPI_SRC_CLK], p61_dev->spi->max_speed_hz * 4);
		usleep_range(5000, 5100);
		NFC_LOG_INFO("%s clock:%lu\n", __func__, clk_get_rate(p61_dev->ese_spi_clk[SPI_SRC_CLK]));
	} else {
		for (i = 0; i < MAX_SPI_CLK_CNT; i++) {
			if (p61_dev->ese_spi_clk[i])
				clk_disable_unprepare(p61_dev->ese_spi_clk[i]);
		}
	}
	old_value = onoff;

	NFC_LOG_INFO("%s: clock %s\n", __func__, onoff ? "enabled" : "disabled");
	return 0;
}

static int p61_clk_setup(struct device *dev, struct p61_device *p61_dev)
{
	struct platform_device *spi_pdev;
	int clk_cnt;
	int i;
	int ret;

	if (p61_dev->ap_vendor == AP_VENDOR_SLSI) {
		spi_pdev = of_find_device_by_node(p61_dev->spi_device_node);
		if (IS_ERR_OR_NULL(spi_pdev))
			return -EPERM;

		clk_cnt = of_property_count_strings(p61_dev->spi_device_node, "clock-names");
		if (clk_cnt != MAX_SPI_CLK_CNT)
			NFC_LOG_ERR("ese spi clk cnt : %d, need to be checked!\n", clk_cnt);

		for (i = 0; i < MAX_SPI_CLK_CNT; i++) {
			const char *clock_name;

			ret = of_property_read_string_index(p61_dev->spi_device_node, "clock-names", i,
					&(clock_name));
			if (ret < 0) {
				NFC_LOG_ERR("Can't get clock-names[%d]\n", i);
				goto err_clk_get;
			}

			p61_dev->ese_spi_clk[i] = clk_get(&spi_pdev->dev, clock_name);
			if (IS_ERR(p61_dev->ese_spi_clk[i])) {
				NFC_LOG_ERR("Can't get %s\n", clock_name);
				p61_dev->ese_spi_clk[i] = NULL;
				goto err_clk_get;
			}
			NFC_LOG_INFO("get clock(%s)\n", clock_name);
		}
	}

	return 0;
err_clk_get:
	for (i--; i >= 0; i--)
		clk_put(p61_dev->ese_spi_clk[i]);
	return -EPERM;
}
#endif

#if defined(CONFIG_ESE_SECURE) && defined(CONFIG_ESE_USE_TZ_API)
static uint32_t tz_tee_ese_drv(enum pm_mode mode)
{
	TEEC_Context context;
	TEEC_Session session;
	TEEC_Result result;
	uint32_t returnOrigin = TEEC_NONE;

	result = TEEC_InitializeContext(NULL, &context);
	if (result != TEEC_SUCCESS)
		goto out;

	result = TEEC_OpenSession(&context, &session, &ese_drv_uuid, TEEC_LOGIN_PUBLIC,
			NULL, NULL, &returnOrigin);
	if (result != TEEC_SUCCESS)
		goto finalize_context;

	/* test with valid cmd id, expected result : TEEC_SUCCESS */
	result = TEEC_InvokeCommand(&session, mode, NULL, &returnOrigin);
	if (result != TEEC_SUCCESS) {
		P61_ERR_MSG("%s with cmd %d : FAIL\n", __func__, mode);
		goto close_session;
	}

	P61_ERR_MSG("%s: return origin %d\n", __func__, returnOrigin);

close_session:
	TEEC_CloseSession(&session);
finalize_context:
	TEEC_FinalizeContext(&context);
out:
	P61_INFO_MSG("%s: cmd %d result=%#x origin=%#x\n", __func__, mode, result, returnOrigin);

	return result;
}

int tz_tee_ese_secure_check(void)
{
	return	tz_tee_ese_drv(SECURE_CHECK);
}
#endif

#if !defined(CONFIG_ESE_SECURE)
static int p61_xfer(struct p61_device *p61_dev,
			struct p61_ioctl_transfer *tr)
{
	int status = 0;
	struct spi_message m;
	struct spi_transfer t;
	int read_write = 0; /* 0: write, 1: read */
	/*For SDM845 & linux4.9: need to change spi buffer
	 * from stack to dynamic memory
	 */

	if (p61_dev == NULL || tr == NULL)
		return -EFAULT;

	if (tr->len > DEFAULT_BUFFER_SIZE || !tr->len)
		return -EMSGSIZE;

	spi_message_init(&m);
	memset(&t, 0, sizeof(t));

	memset(p61_dev->buf, 0, tr->len); /*memset 0 for read */
	if (tr->tx_buffer != NULL) { /*write */
		read_write = 0;
		if (copy_from_user(p61_dev->buf, tr->tx_buffer, tr->len) != 0)
			return -EFAULT;
	}

	t.rx_buf = p61_dev->buf;
	t.tx_buf = p61_dev->buf;
	t.len = tr->len;

	spi_message_add_tail(&t, &m);

	status = spi_sync(p61_dev->spi, &m);
	if (status == 0) {
		if (tr->rx_buffer != NULL) { /*read */
			unsigned long missing = 0;

			read_write = 1;
			missing = copy_to_user(tr->rx_buffer, p61_dev->buf, tr->len);
			if (missing != 0)
				tr->len = tr->len - (unsigned int)missing;
		}
	}
	NFC_LOG_REC("%s: %s(%d)\n", __func__, read_write ? "read":"write", tr->len);

	return status;
} /* vfsspi_xfer */

static int p61_rw_spi_message(struct p61_device *p61_dev,
				 unsigned long arg)
{
	struct p61_ioctl_transfer   *dup = NULL;
	int err = 0;

	dup = kmalloc(sizeof(struct p61_ioctl_transfer), GFP_KERNEL);
	if (dup == NULL)
		return -ENOMEM;

	if (copy_from_user(dup, (void *)arg,
			   sizeof(struct p61_ioctl_transfer)) != 0) {
		kfree(dup);
		return -EFAULT;
	}

	err = p61_xfer(p61_dev, dup);
	if (err != 0) {
		kfree(dup);
		NFC_LOG_ERR("%s: p61_xfer failed!\n", __func__);
		return err;
	}

	if (copy_to_user((void *)arg, dup,
			 sizeof(struct p61_ioctl_transfer)) != 0) {
		kfree(dup);
		return -EFAULT;
	}
	kfree(dup);
	return 0;
}

#ifdef CONFIG_COMPAT
static int p61_rw_spi_message_compat(struct p61_device *p61_dev,
				 unsigned long arg)
{
	struct p61_ioctl_transfer32 __user *argp = compat_ptr(arg);
	struct p61_ioctl_transfer32 it32;
	struct p61_ioctl_transfer   *dup = NULL;
	int err = 0;

	dup = kmalloc(sizeof(struct p61_ioctl_transfer), GFP_KERNEL);
	if (dup == NULL)
		return -ENOMEM;

	if (copy_from_user(&it32, argp, sizeof(it32))) {
		kfree(dup);
		return -EFAULT;
	}

	dup->rx_buffer = (__u8 *)(uintptr_t)it32.rx_buffer;
	dup->tx_buffer = (__u8 *)(uintptr_t)it32.tx_buffer;
	dup->len = it32.len;

	err = p61_xfer(p61_dev, dup);
	if (err != 0) {
		kfree(dup);
		NFC_LOG_ERR("%s: p61_xfer failed!\n", __func__);
		return err;
	}

	if (it32.rx_buffer) {
		if (__put_user(dup->len, &argp->len)) {
			kfree(dup);
			return -EFAULT;
		}
	}
	kfree(dup);
	return 0;
}
#endif	/*CONFIG_COMPAT */
#endif

/**
 * \ingroup spi_driver
 * \brief Called from SPI LibEse to initilaize the P61 device
 *
 * \param[in]       struct inode *
 * \param[in]       struct file *
 *
 * \retval 0 if ok.
 */
static int p61_dev_open(struct inode *inode, struct file *filp)
{
	struct spi_device *spidev = NULL;

#ifdef CONFIG_MAKE_NODE_USING_PLATFORM_DEVICE
	struct p61_device *p61_ese = p61_dev;

	if (p61_ese == NULL) {
		NFC_LOG_ERR("%s: spi probe is not called\n", __func__);
		return -EAGAIN;
	}
#else

	struct p61_device *p61_ese = container_of(filp->private_data,
				struct p61_device, miscdev);
#endif

	if (!get_nfcc_dev_data()) {
		NFC_LOG_ERR("%s:open fail,no nfc dev\n", __func__);
		return -EAGAIN;
	}

	spidev = spi_dev_get(p61_ese->spi);

	filp->private_data = p61_ese;

	if (p61_ese->device_opened) {
		NFC_LOG_ERR("%s: already opened!\n", __func__);
		return -EBUSY;
	}
#ifdef CONFIG_NFC_FEATURE_SN100U
	ese_spi_pinctrl(1);
#if IS_ENABLED(CONFIG_SPI_MSM_GENI)
	cancel_delayed_work_sync(&p61_dev->spi_release_work);
#endif
	if (p61_dev->ap_vendor != AP_VENDOR_QCT)
		msleep(60);
#endif
#if defined(CONFIG_ESE_SECURE) && defined(CONFIG_ESE_USE_TZ_API)
	if (p61_ese->ese_secure_check == NOT_CHECKED) {
		int ret = 0;

		ret = tz_tee_ese_secure_check();
		if (ret) {
			p61_ese->ese_secure_check = ESE_NOT_SECURED;
			P61_ERR_MSG("eSE spi is not Secured\n");
			return -EBUSY;
		}
		p61_ese->ese_secure_check = ESE_SECURED;
	} else if (p61_ese->ese_secure_check == ESE_NOT_SECURED) {
		P61_ERR_MSG("eSE spi is not Secured\n");
		return -EBUSY;
	}
#endif

	NFC_LOG_INFO("%s: Major No: %d, Minor No: %d\n", __func__,
			imajor(inode), iminor(inode));

	if (!wake_lock_active(&p61_ese->ese_lock)) {
		NFC_LOG_INFO("%s: [NFC-ESE] wake lock.\n", __func__);
		wake_lock(&p61_ese->ese_lock);
	}

	p61_ese->device_opened = true;
#ifdef CONFIG_NFC_FEATURE_SN100U
	p61_ese->pid = task_pid_nr(current);
	NFC_LOG_INFO("%s: pid:%d\n", __func__, p61_ese->pid);
#endif

	return 0;
}

/**
 * \ingroup spi_driver
 * \brief To configure the P61_SET_PWR/P61_SET_DBG/P61_SET_POLL
 * \n	P61_SET_PWR - hard reset (arg=2), soft reset (arg=1)
 * \n	P61_SET_DBG - Enable/Disable (based on arg value) the driver logs
 * \n	P61_SET_POLL - Configure the driver in poll (arg = 1),
 *							interrupt (arg = 0) based read operation
 * \param[in]       struct file *
 * \param[in]       unsigned int
 * \param[in]       unsigned long
 *
 * \retval 0 if ok.
 *
 */
static long p61_dev_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	int ret = 0;
	struct p61_device *p61_dev = NULL;

	if (_IOC_TYPE(cmd) != P61_MAGIC) {
		NFC_LOG_ERR("%s: invalid magic. cmd=0x%X Received=0x%X Expected=0x%X\n",
			__func__, cmd, _IOC_TYPE(cmd), P61_MAGIC);
		return -ENOTTY;
	}

	p61_dev = filp->private_data;

	switch (cmd) {
	case P61_SET_PWR:
		if (arg == 2)
			NFC_LOG_INFO("%s: P61_SET_PWR. No Action.\n", __func__);
		break;

	case P61_SET_DBG:
		debug_level = (unsigned char)arg;
		P61_DBG_MSG(KERN_INFO"[NXP-P61] -  Debug level %d",
			debug_level);
		break;
	case P61_SET_POLL:
		p61_dev->enable_poll_mode = (unsigned char)arg;
		if (p61_dev->enable_poll_mode == 0) {
			P61_DBG_MSG(KERN_INFO"[NXP-P61] - IRQ Mode is set\n");
		} else {
			P61_DBG_MSG(KERN_INFO"[NXP-P61] - Poll Mode is set\n");
			p61_dev->enable_poll_mode = 1;
		}
		break;

#if !defined(CONFIG_NFC_FEATURE_SN100U)
	case P61_SET_SPI_CONFIG:
		NFC_LOG_INFO("%s P61_SET_SPI_CONFIG. No Action.\n", __func__);
		break;
	case P61_ENABLE_SPI_CLK:
		NFC_LOG_INFO("%s P61_ENABLE_SPI_CLK. No Action.\n", __func__);
		break;
	case P61_DISABLE_SPI_CLK:
		NFC_LOG_INFO("%s P61_DISABLE_SPI_CLK. No Action.\n", __func__);
		break;
#endif

	case P61_RW_SPI_DATA:
#if !defined(CONFIG_ESE_SECURE)
		ret = p61_rw_spi_message(p61_dev, arg);
#endif
		break;

	case P61_SET_SPM_PWR:
		NFC_LOG_INFO("%s: P61_SET_SPM_PWR: enter\n", __func__);
		ret = pn547_dev_ioctl(filp, P61_SET_SPI_PWR, arg);
		if (arg == 0 || arg == 1 || arg == 3)
			pwr_req_on = arg;
		NFC_LOG_INFO("%s: P61_SET_SPM_PWR: exit\n", __func__);
		break;

	case P61_GET_SPM_STATUS:
		NFC_LOG_INFO("%s: P61_GET_SPM_STATUS: enter\n", __func__);
		ret = pn547_dev_ioctl(filp, P61_GET_PWR_STATUS, arg);
		NFC_LOG_INFO("%s: P61_GET_SPM_STATUS: exit\n", __func__);
		break;

	case P61_GET_ESE_ACCESS:
		/*P61_DBG_MSG(KERN_ALERT " P61_GET_ESE_ACCESS: enter");*/
		ret = pn547_dev_ioctl(filp, P547_GET_ESE_ACCESS, arg);
		NFC_LOG_INFO("%s: P61_GET_ESE_ACCESS ret: %d exit\n", __func__, ret);
		break;

	case P61_SET_DWNLD_STATUS:
		P61_DBG_MSG(KERN_ALERT "P61_SET_DWNLD_STATUS: enter\n");
		ret = pn547_dev_ioctl(filp, PN547_SET_DWNLD_STATUS, arg);
		NFC_LOG_INFO("%s: P61_SET_DWNLD_STATUS: =%lu exit\n", __func__, arg);
		break;

#ifdef CONFIG_NFC_FEATURE_SN100U
	case ESE_PERFORM_COLD_RESET:
		P61_DBG_MSG(KERN_ALERT " ESE_PERFORM_COLD_RESET: enter");
		ret = ese_cold_reset(ESE_COLD_RESET_SOURCE_SPI);
		P61_DBG_MSG(KERN_ALERT " P61_INHIBIT_PWR_CNTRL ret: %d exit", ret);
		break;

	case PERFORM_RESET_PROTECTION:
		P61_DBG_MSG(KERN_ALERT " PERFORM_RESET_PROTECTION: enter");
		ret = do_reset_protection((arg == 1 ? true : false));
		P61_DBG_MSG(KERN_ALERT " PERFORM_RESET_PROTECTION ret: %d exit", ret);
	break;
#endif

	default:
		NFC_LOG_INFO("%s: no matching ioctl!\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long p61_dev_compat_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	int ret = 0;
	struct p61_device *p61_dev = NULL;

	if (_IOC_TYPE(cmd) != P61_MAGIC)
		return -ENOTTY;

	p61_dev = filp->private_data;

	switch (cmd) {
	case P61_SET_PWR_COMPAT:
		if (arg == 2)
			NFC_LOG_INFO("%s: P61_SET_PWR. No Action.\n", __func__);
		break;

	case P61_SET_DBG_COMPAT:
		debug_level = (unsigned char)arg;
		P61_DBG_MSG(KERN_INFO"[NXP-P61] -  Debug level %d",
			debug_level);
		break;
	case P61_SET_POLL_COMPAT:
		p61_dev->enable_poll_mode = (unsigned char)arg;
		if (p61_dev->enable_poll_mode == 0) {
			P61_DBG_MSG(KERN_INFO"[NXP-P61] - IRQ Mode is set\n");
		} else {
			P61_DBG_MSG(KERN_INFO"[NXP-P61] - Poll Mode is set\n");
			p61_dev->enable_poll_mode = 1;
		}
		break;

#if !defined(CONFIG_NFC_FEATURE_SN100U)
	case P61_SET_SPI_CONFIG:
		NFC_LOG_INFO("%s P61_SET_SPI_CONFIG. No Action.\n", __func__);
		break;
	case P61_ENABLE_SPI_CLK:
		NFC_LOG_INFO("%s P61_ENABLE_SPI_CLK. No Action.\n", __func__);
		break;
	case P61_DISABLE_SPI_CLK:
		NFC_LOG_INFO("%s P61_DISABLE_SPI_CLK. No Action.\n", __func__);
		break;
#endif

	case P61_RW_SPI_DATA_COMPAT:
#if !defined(CONFIG_ESE_SECURE)
		ret = p61_rw_spi_message_compat(p61_dev, arg);
#endif
		break;

	case P61_SET_SPM_PWR_COMPAT:
		NFC_LOG_INFO("%s: P61_SET_SPM_PWR: enter\n", __func__);
		ret = pn547_dev_ioctl(filp, P61_SET_SPI_PWR, arg);
		if (arg == 0 || arg == 1 || arg == 3)
			pwr_req_on = arg;
		NFC_LOG_INFO("%s: P61_SET_SPM_PWR: exit\n", __func__);
		break;

	case P61_GET_SPM_STATUS_COMPAT:
		NFC_LOG_INFO("%s: P61_GET_SPM_STATUS: enter\n", __func__);
		ret = pn547_dev_ioctl(filp, P61_GET_PWR_STATUS, arg);
		NFC_LOG_INFO("%s: P61_GET_SPM_STATUS: exit\n", __func__);
		break;

	case P61_GET_ESE_ACCESS_COMPAT:
		/*P61_DBG_MSG(KERN_ALERT " P61_GET_ESE_ACCESS: enter");*/
		ret = pn547_dev_ioctl(filp, P547_GET_ESE_ACCESS, arg);
		NFC_LOG_INFO("%s: P61_GET_ESE_ACCESS ret: %d exit\n", __func__, ret);
		break;

	case P61_SET_DWNLD_STATUS_COMPAT:
		P61_DBG_MSG(KERN_ALERT "P61_SET_DWNLD_STATUS: enter\n");
		ret = pn547_dev_ioctl(filp, PN547_SET_DWNLD_STATUS, arg);
		NFC_LOG_INFO("%s: P61_SET_DWNLD_STATUS: =%lu exit\n", __func__, arg);
		break;

#ifdef CONFIG_NFC_FEATURE_SN100U
	case ESE_PERFORM_COLD_RESET_COMPAT:
		P61_DBG_MSG(KERN_ALERT " ESE_PERFORM_COLD_RESET: enter");
		ret = ese_cold_reset(ESE_COLD_RESET_SOURCE_SPI);
		P61_DBG_MSG(KERN_ALERT " P61_INHIBIT_PWR_CNTRL ret: %d exit", ret);
		break;

	case PERFORM_RESET_PROTECTION_COMPAT:
		P61_DBG_MSG(KERN_ALERT " PERFORM_RESET_PROTECTION: enter");
		ret = do_reset_protection((arg == 1 ? true : false));
		P61_DBG_MSG(KERN_ALERT " PERFORM_RESET_PROTECTION ret: %d exit", ret);
	break;
#endif

	default:
		NFC_LOG_INFO("%s: no matching ioctl!\n", __func__);
		ret = -EINVAL;
	}

	return ret;
//	return p61_dev_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif

#ifdef CONFIG_NFC_FEATURE_SN100U
/* this function is defined temporarily to fix build error. this function is used by uwb */
long p61_cold_reset(void)
{
	int ret;

	NFC_LOG_INFO("UWB ESE_COLD_RESET: enter");
	ret = ese_cold_reset(ESE_COLD_RESET_SOURCE_UWB);
	NFC_LOG_INFO("ret: %d exit", ret);

	return ret;
}
EXPORT_SYMBOL(p61_cold_reset);
#endif

/*
 * Called when a process closes the device file.
 */
static int p61_dev_release(struct inode *inode, struct file *file)
{
	struct p61_device *p61_dev = file->private_data;
#ifdef CONFIG_NFC_FEATURE_SN100U
	int pid;
#endif

	NFC_LOG_INFO("%s: ++\n", __func__);

#ifdef CONFIG_NFC_FEATURE_SN100U
	do_reset_protection(false);
	ese_spi_pinctrl(0);
	if (p61_dev->ap_vendor != AP_VENDOR_QCT)
		msleep(60);
#if IS_ENABLED(CONFIG_SPI_MSM_GENI)
	schedule_delayed_work(&p61_dev->spi_release_work,
				msecs_to_jiffies(2000));
	wake_lock_timeout(&p61_dev->spi_release_wakelock,
				msecs_to_jiffies(2100));
#endif
#endif

#ifdef CONFIG_NFC_FEATURE_SN100U
	pid = task_pid_nr(current);
	NFC_LOG_INFO("%s: open pid :%d, close pid :%d\n", __func__, p61_dev->pid, pid);
	if (pid != p61_dev->pid)
		p61_dev->pid_diff = true;
	else
		p61_dev->pid_diff = false;
#endif

	if (wake_lock_active(&p61_dev->ese_lock)) {
		NFC_LOG_INFO("%s: [NFC-ESE] wake unlock.\n", __func__);
		wake_unlock(&p61_dev->ese_lock);
	}

	if (pwr_req_on && (pwr_req_on != 5)) {
		NFC_LOG_INFO("%s: release spi session.\n", __func__);
		pwr_req_on = 0;
		pn547_dev_ioctl(file, P61_SET_SPI_PWR, 0);
		pn547_dev_ioctl(file, P61_SET_SPI_PWR, 5);
	}
	p61_dev->device_opened = false;

	return 0;
}

/**
 * \ingroup spi_driver
 * \brief Write data to P61 on SPI
 *
 * \param[in]       struct file *
 * \param[in]       const char *
 * \param[in]       size_t
 * \param[in]       loff_t *
 *
 * \retval data size
 *
 */
static ssize_t p61_dev_write(struct file *filp, const char *buf, size_t count,
	loff_t *offset)
{
	int ret = -1;
	struct p61_device *p61_dev;

	P61_DBG_MSG("%s: Enter count %zu\n", __func__, count);

#ifdef CONFIG_ESE_SECURE
	return 0;
#endif
	p61_dev = filp->private_data;

	mutex_lock(&p61_dev->write_mutex);
	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	memset(p61_dev->buf, 0, count);
	if (copy_from_user(p61_dev->buf, &buf[0], count)) {
		P61_ERR_MSG("%s: failed to copy from user space\n", __func__);
		mutex_unlock(&p61_dev->write_mutex);
		return -EFAULT;
	}
	/* Write data */
	ret = spi_write(p61_dev->spi, p61_dev->buf, count);
	if (ret < 0)
		ret = -EIO;
	else
		ret = count;

	mutex_unlock(&p61_dev->write_mutex);
	NFC_LOG_INFO("%s: -count %zu  %d- Exit\n", __func__, count, ret);

	return ret;
}

/**
 * \ingroup spi_driver
 * \brief Used to read data from P61 in Poll/interrupt mode configured using
 *  ioctl call
 *
 * \param[in]       struct file *
 * \param[in]       char *
 * \param[in]       size_t
 * \param[in]       loff_t *
 *
 * \retval read size
 *
 */
/* for p61 only */
static ssize_t p61_dev_read(struct file *filp, char *buf, size_t count,
	loff_t *offset)
{
	int ret = -EIO;
	struct p61_device *p61_dev = filp->private_data;
	unsigned char sof = 0x00;
	int total_count = 0;
	//unsigned char rx_buffer[MAX_BUFFER_SIZE];

	P61_DBG_MSG("%s: count %zu - Enter\n", __func__, count);

#ifdef CONFIG_ESE_SECURE
	return 0;
#endif
	if (count < 1) {
		P61_ERR_MSG("Invalid length (min : 258) [%zu]\n", count);
		return -EINVAL;
	}
	mutex_lock(&p61_dev->read_mutex);
	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	//memset(&rx_buffer[0], 0x00, sizeof(rx_buffer));
	memset(p61_dev->buf, 0x00, MAX_BUFFER_SIZE);

	P61_DBG_MSG(" %s Poll Mode Enabled\n", __func__);
	do {
		sof = 0x00;
		ret = spi_read(p61_dev->spi, (void *)&sof, 1);
		if (ret < 0) {
			P61_ERR_MSG("spi_read failed [SOF]\n");
			goto fail;
		}
		//P61_DBG_MSG(KERN_INFO"SPI_READ returned 0x%x\n", sof);
		/* if SOF not received, give some time to P61 */
		/* RC put the conditional delay only if SOF not received */
		if (sof != SOF)
			usleep_range(5000, 5100);
	} while (sof != SOF);
	P61_DBG_MSG("SPI_READ returned 0x%x...\n", sof);

	total_count = 1;
	//rx_buffer[0] = sof;
	*p61_dev->buf = sof;
	/* Read the HEADR of Two bytes*/
	ret = spi_read(p61_dev->spi, p61_dev->buf + 1, 2);
	if (ret < 0) {
		P61_ERR_MSG("spi_read fails after [PCB]\n");
		ret = -EIO;
		goto fail;
	}

	total_count += 2;
	/* Get the data length */
	//count = rx_buffer[2];
	count = *(p61_dev->buf + 2);
	NFC_LOG_REC("Data Length = %zu", count);
	/* Read the available data along with one byte LRC */
	ret = spi_read(p61_dev->spi, (void *)(p61_dev->buf + 3), (count+1));
	if (ret < 0) {
		NFC_LOG_ERR("%s: spi_read failed\n", __func__);
		ret = -EIO;
		goto fail;
	}
	total_count = (total_count + (count+1));
	P61_DBG_MSG(KERN_INFO"total_count = %d", total_count);

	if (copy_to_user(buf, p61_dev->buf, total_count)) {
		P61_ERR_MSG("%s : failed to copy to user space\n", __func__);
		ret = -EFAULT;
		goto fail;
	}
	ret = total_count;
	P61_DBG_MSG("%s: ret %d Exit\n", __func__, ret);

	mutex_unlock(&p61_dev->read_mutex);

	return ret;

fail:
	P61_ERR_MSG("%s: Error ret %d Exit\n", __func__, ret);
	NFC_LOG_INFO("%s: count %zu  %d- Exit\n", __func__, count, ret);

	mutex_unlock(&p61_dev->read_mutex);
	return ret;
}

/**
 * \ingroup spi_driver
 * \brief Set the P61 device specific context for future use.
 * \param[in]       struct spi_device *
 * \param[in]       void *
 * \retval void
 */
static inline void p61_set_data(struct spi_device *spi, void *data)
{
	dev_set_drvdata(&spi->dev, data);
}

/**
 * \ingroup spi_driver
 * \brief Get the P61 device specific context.
 * \param[in]       const struct spi_device *
 * \retval Device Parameters
 */
static inline void *p61_get_data(const struct spi_device *spi)
{
	return dev_get_drvdata(&spi->dev);
}

static const struct file_operations p61_dev_fops = {
	.owner = THIS_MODULE,
	.read = p61_dev_read,
	.write = p61_dev_write,
	.open = p61_dev_open,
	.unlocked_ioctl = p61_dev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = p61_dev_compat_ioctl,
#endif
	.release = p61_dev_release,
};

#ifdef CONFIG_MAKE_NODE_USING_PLATFORM_DEVICE
static struct miscdevice p61_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "p61",
	.fops = &p61_dev_fops,
};
#endif

static int p61_parse_dt(struct device *dev,
	struct p61_device *p61_dev)
{
	struct device_node *np = dev->of_node;
	struct device_node *spi_device_node;
	struct platform_device *spi_pdev;
	int ese_det_gpio;
	const char *ap_str;
	int ret;

	ese_det_gpio = of_get_named_gpio(np, "ese-det-gpio", 0);
	if (!gpio_is_valid(ese_det_gpio)) {
		NFC_LOG_INFO("%s: ese-det-gpio is not set\n", __func__);
	} else {
		ret = gpio_request(ese_det_gpio, "ese_det_gpio");
		if (ret < 0)
			NFC_LOG_ERR("%s failed to get gpio ese_det_gpio\n", __func__);

		gpio_direction_input(ese_det_gpio);
		if (!gpio_get_value(ese_det_gpio)) {
			NFC_LOG_INFO("%s: ese is not supported\n", __func__);
#ifdef CONFIG_MAKE_NODE_USING_PLATFORM_DEVICE
			misc_deregister(&p61_misc_device);
			NFC_LOG_ERR("Misc Deregister\n");
#endif
			return -ENODEV;
		}
		NFC_LOG_INFO("%s: ese is supported\n", __func__);
	}

	if (!of_property_read_string(np, "p61,ap_vendor", &ap_str)) {
		if (!strcmp(ap_str, "slsi"))
			p61_dev->ap_vendor = AP_VENDOR_SLSI;
		else if (!strcmp(ap_str, "qct") || !strcmp(ap_str, "qualcomm"))
			p61_dev->ap_vendor = AP_VENDOR_QCT;
		else if (!strcmp(ap_str, "mtk"))
			p61_dev->ap_vendor = AP_VENDOR_MTK;
		NFC_LOG_INFO("AP vendor is %d\n", p61_dev->ap_vendor);
	} else {
		NFC_LOG_INFO("AP vendor is not set\n");
	}

	spi_device_node = of_get_parent(np);
	if (!IS_ERR_OR_NULL(spi_device_node)) {
		int i;
		int pin_ctrl_cnt = ese_get_pin_ctrl_count();

		p61_dev->spi_device_node = spi_device_node;
		spi_pdev = of_find_device_by_node(spi_device_node);
		p61_dev->pinctrl = devm_pinctrl_get(&spi_pdev->dev);

		if (!IS_ERR(p61_dev->pinctrl)) {
			for (i = 0; i < pin_ctrl_cnt; i++) {
				p61_dev->pinctrl_state[i] = pinctrl_lookup_state(p61_dev->pinctrl, g_pin_status[i]);
				if (IS_ERR(p61_dev->pinctrl_state[i])) {
					NFC_LOG_INFO("p61_pinctrl[%s] not found(%ld)\n",
							g_pin_status[i], PTR_ERR(p61_dev->pinctrl_state[i]));
					p61_dev->pinctrl_state[i] = NULL;
				}
			}
		} else {
			NFC_LOG_INFO("%s: devm_pinctrl_get failed\n", __func__);
		}
	} else {
		NFC_LOG_INFO("target does not use spi pinctrl\n");
	}

#ifdef CONFIG_NFC_FEATURE_SN100U
	p61_dev->spi_cs_gpio = of_get_named_gpio(np, "ese-spi_cs-gpio", 0);
	if (!gpio_is_valid(p61_dev->spi_cs_gpio))
		NFC_LOG_INFO("%s: cs_gpio is not set\n", __func__);
	else
		NFC_LOG_INFO("%s: cs_gpio is %d\n", __func__, p61_dev->spi_cs_gpio);
#endif
	return 0;
}

#if defined(CONFIG_NFC_FEATURE_SN100U)
static void p61_shutdown(void)
{
	if (p61_dev && !p61_dev->device_opened && !p61_dev->pid_diff) {
		NFC_LOG_INFO("%s : pid_diff %s\n", __func__, p61_dev->pid_diff ? "true" : "false");
		p61_dev->device_opened = true;
		ese_set_spi_configuration("lpm");
		P61_DBG_MSG("%s\n", __func__);
	}
}
#endif

/**
 * \ingroup spi_driver
 * \brief To probe for P61 SPI interface. If found initialize the SPI clock,
 * bit rate & SPI mode. It will create the dev entry (P61) for user space.
 *
 * \param[in]       struct spi_device *
 *
 * \retval 0 if ok.
 *
 */
static int p61_probe(struct device *dev)
{
	int ret = -1;

	nfc_logger_init();

	p61_dev = kzalloc(sizeof(*p61_dev), GFP_KERNEL);
	if (p61_dev == NULL) {
		P61_ERR_MSG("failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	ret = p61_parse_dt(dev, p61_dev);
	if (ret) {
		NFC_LOG_ERR("%s: Failed to parse DT\n", __func__);
		goto p61_parse_dt_failed;
	}
	NFC_LOG_INFO("%s: tz_mode=%d, isGpio_cfgDone:%d\n", __func__,
			p61_dev->tz_mode, p61_dev->isGpio_cfgDone);

#if defined(CONFIG_ESE_SECURE)
#if defined(CONFIG_ESE_USE_TZ_API)
	p61_dev->ese_secure_check = NOT_CHECKED;
	NFC_LOG_INFO("%s: eSE Secured system\n", __func__);
#endif
	ret = p61_clk_setup(dev, p61_dev);
	if (ret)
		NFC_LOG_ERR("%s - Failed to do clk_setup\n", __func__);

#endif
#ifndef CONFIG_MAKE_NODE_USING_PLATFORM_DEVICE
	p61_dev->miscdev.minor = MISC_DYNAMIC_MINOR;
	p61_dev->miscdev.name = "p61";
	p61_dev->miscdev.fops = &p61_dev_fops;
	p61_dev->miscdev.parent = dev;
#endif
	dev_set_drvdata(dev, p61_dev);
#if defined(CONFIG_NFC_FEATURE_SN100U)
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG) && !defined(CONFIG_NFC_PVDD_LATE_ENABLE)
	if (lpcharge)
		ese_set_spi_configuration("lpm");
	else
#endif
	if (p61_dev->ap_vendor == AP_VENDOR_MTK)
		ese_set_spi_configuration("lpm");
	else
		ese_set_spi_configuration("sleep");
#else
#if !defined(CONFIG_ESE_SECURE)
	ese_set_spi_configuration("sleep");
#endif
#endif

	/* init mutex and queues */
	init_waitqueue_head(&p61_dev->read_wq);
	mutex_init(&p61_dev->read_mutex);
	mutex_init(&p61_dev->write_mutex);
	spin_lock_init(&p61_dev->ese_spi_lock);

	wake_lock_init(&p61_dev->ese_lock, WAKE_LOCK_SUSPEND, "ese_wake_lock");
#if IS_ENABLED(CONFIG_SPI_MSM_GENI) && defined(CONFIG_NFC_FEATURE_SN100U)
	INIT_DELAYED_WORK(&p61_dev->spi_release_work, p61_spi_release_work);
	wake_lock_init(&p61_dev->spi_release_wakelock, WAKE_LOCK_SUSPEND, "ese_spi_wake_lock");
#endif
	p61_dev->device_opened = false;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG) && !defined(CONFIG_NFC_PVDD_LATE_ENABLE)
	if (!lpcharge) {
#else
	{
#endif
#ifndef CONFIG_MAKE_NODE_USING_PLATFORM_DEVICE
		ret = misc_register(&p61_dev->miscdev);
		if (ret < 0) {
			P61_ERR_MSG("misc_register failed! %d\n", ret);
			goto err_exit0;
		}
#endif
	}

	p61_dev->enable_poll_mode = 1; /* No USE? */

	p61_dev->buf = kzalloc(sizeof(unsigned char) * MAX_BUFFER_SIZE, GFP_KERNEL);
	if (p61_dev->buf == NULL) {
		P61_ERR_MSG("failed to allocate for spi buffer\n");
		ret = -ENOMEM;
		goto err_exit0;
	}
#if defined(CONFIG_NFC_FEATURE_SN100U)
	pn547_register_ese_shutdown(p61_shutdown);
#endif

	NFC_LOG_INFO("%s: finished\n", __func__);
	return ret;

err_exit0:
	mutex_destroy(&p61_dev->read_mutex);
	mutex_destroy(&p61_dev->write_mutex);
	wake_lock_destroy(&p61_dev->ese_lock);

p61_parse_dt_failed:
	if (p61_dev != NULL)
		kfree(p61_dev);
err_exit:
	P61_DBG_MSG("ERROR: Exit : %s ret %d\n", __func__, ret);
	return ret;
}

static int p61_remove(struct device *dev)
{
	struct p61_device *p61_dev = dev_get_drvdata(dev);

	P61_DBG_MSG("Entry : %s\n", __func__);
	mutex_destroy(&p61_dev->read_mutex);
#ifndef CONFIG_MAKE_NODE_USING_PLATFORM_DEVICE
	misc_deregister(&p61_dev->miscdev);
#endif
	wake_lock_destroy(&p61_dev->ese_lock);
	kfree(p61_dev->buf);
	kfree(p61_dev);

	return 0;
}

static int p61_platform_probe(struct platform_device *pdev)
{
	int ret = -1;

	ret = p61_probe(&pdev->dev);
	if (ret)
		goto p61_platform_setup_failed;

	NFC_LOG_INFO("%s: finished...\n", __func__);
	return ret;

p61_platform_setup_failed:
	P61_DBG_MSG("ERROR: Exit : %s ret %d\n", __func__, ret);
	return ret;
}

static int p61_platform_remove(struct platform_device *pdev)
{
	P61_DBG_MSG("Entry : %s\n", __func__);
	p61_remove(&pdev->dev);
	P61_DBG_MSG("Exit : %s\n", __func__);
	return 0;
}

static const struct of_device_id p61_secure_match_table[] = {
	{ .compatible = "p61_secure",},
	{},
};

static struct platform_driver p61_platform_driver = {
	.driver = {
		.name = "p61",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = p61_secure_match_table,
#endif
	},
	.probe =  p61_platform_probe,
	.remove = p61_platform_remove,
};

static int p61_spi_probe(struct spi_device *spi)
{
	int ret = -1;

	NFC_LOG_INFO("chip select(%d), bus number(%d), speed(%u)\n",
		spi->chip_select, spi->master->bus_num, spi->max_speed_hz);


	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;

	ret = spi_setup(spi);
	if (ret < 0) {
		P61_ERR_MSG("failed to do spi_setup()\n");
		goto p61_spi_setup_failed;
	}

	ret = p61_probe(&spi->dev);
	if (ret)
		goto p61_spi_setup_failed;

	p61_dev->spi = spi;


	NFC_LOG_INFO("%s: finished\n", __func__);
	return ret;

p61_spi_setup_failed:
	P61_DBG_MSG("ERROR: Exit : %s ret %d\n", __func__, ret);
	return ret;
}

static int p61_spi_remove(struct spi_device *spi)
{
	P61_DBG_MSG("Entry : %s\n", __func__);
	p61_remove(&spi->dev);
	P61_DBG_MSG("Exit : %s\n", __func__);
	return 0;
}

static const struct of_device_id p61_match_table[] = {
	{ .compatible = "p61",},
	{},
};

static struct spi_driver p61_driver = {
	.driver = {
		.name = "p61",
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = p61_match_table,
#endif
	},
	.probe =  p61_spi_probe,
	.remove = p61_spi_remove,
};

#if IS_MODULE(CONFIG_SAMSUNG_NFC)
#ifdef CONFIG_MAKE_NODE_USING_PLATFORM_DEVICE
static int p61_ese_probe(struct platform_device *pdev)
{
	int ret = -1;

	ret = misc_register(&p61_misc_device);
	if (ret < 0)
		NFC_LOG_INFO("misc_register failed! %d\n", ret);

	NFC_LOG_INFO("%s: finished...\n", __func__);
	return 0;
}

static int p61_ese_remove(struct platform_device *pdev)
{
	NFC_LOG_INFO("Entry : %s\n", __func__);
	misc_deregister(&p61_misc_device);
	return 0;
}

static const struct of_device_id p61_ese_match_table[] = {
	{ .compatible = "p61_ese",},
	{},
};

static struct platform_driver p61_ese_driver = {
	.driver = {
		.name = "p61_ese",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = p61_ese_match_table,
#endif

	},
	.probe =  p61_ese_probe,
	.remove = p61_ese_remove,
};
#endif

int p61_dev_init(void)
{
	int ret;

	debug_level = P61_FULL_DEBUG;

	NFC_LOG_INFO("Entry\n");
	ret = platform_driver_register(&p61_platform_driver);
	NFC_LOG_INFO("eSE platform_driver_register, ret %d\n", ret);
#ifdef CONFIG_MAKE_NODE_USING_PLATFORM_DEVICE
	ret = platform_driver_register(&p61_ese_driver);
	NFC_LOG_INFO("eSE driver platform register, ret %d\n", ret);
#endif
	ret = spi_register_driver(&p61_driver);
	NFC_LOG_INFO("eSE spi driver register, ret %d\n", ret);

	return ret;
}
EXPORT_SYMBOL(p61_dev_init);
void p61_dev_exit(void)
{
#ifdef CONFIG_MAKE_NODE_USING_PLATFORM_DEVICE
	platform_driver_unregister(&p61_ese_driver);
#endif
#ifdef CONFIG_ESE_SECURE
	platform_driver_unregister(&p61_platform_driver);
#else
	spi_unregister_driver(&p61_driver);
#endif
	NFC_LOG_INFO("Exit\n");
}
EXPORT_SYMBOL(p61_dev_exit);
#else
/**
 * \ingroup ese_driver
 * \brief Module init interface
 *
 * \param[in]       void
 *
 * \retval handle
 *
 */
static int __init p61_dev_init(void)
{
	int ret;

	debug_level = P61_FULL_DEBUG;

	NFC_LOG_INFO("Entry\n");
	ret = platform_driver_register(&p61_platform_driver);
	NFC_LOG_INFO("eSE platform_driver_register, ret %d\n", ret);

	ret = spi_register_driver(&p61_driver);
	NFC_LOG_INFO("eSE spi driver register, ret %d\n", ret);

	return ret;
}

/**
 * \ingroup ese_driver
 * \brief Module exit interface
 *
 * \param[in]       void
 *
 * \retval void
 *
 */
static void __exit p61_dev_exit(void)
{
	NFC_LOG_INFO("Entry\n");
#ifdef CONFIG_ESE_SECURE
	platform_driver_unregister(&p61_platform_driver);
#else
	spi_unregister_driver(&p61_driver);
#endif
	NFC_LOG_INFO("Exit\n");
}

/* module_init(p61_dev_init); */
late_initcall(p61_dev_init);
module_exit(p61_dev_exit);

MODULE_AUTHOR("BHUPENDRA PAWAR");
MODULE_DESCRIPTION("NXP P61 SPI driver");
MODULE_LICENSE("GPL");
#endif
