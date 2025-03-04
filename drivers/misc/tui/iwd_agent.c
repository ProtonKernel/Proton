/*
 * Samsung TUI HW Handler driver.
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "iwd_agent.h"
#include "stui_core.h"
#include "stui_hal.h"
#include "stui_inf.h"
#include "stui_ioctl.h"
#include "tuill_defs.h"

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/reboot.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <core/iwsock.h>
#include <core/notifier.h>

static void unregister_tzdev_in_tuihw(void);

struct iwd_functions {
	int (*cancel_session)(void);
};

extern void register_iwd_functions(struct iwd_functions *f);
extern void unregister_iwd_functions(void);

struct tzdev_functions {
	struct sock_desc *(*socket)(unsigned int is_kern, unsigned int is_interruptible);
	int (*connect)(struct sock_desc *sd, const char *name, int flags);
	ssize_t (*write)(struct sock_desc *sd, void *buf, size_t count, int flags);
	ssize_t (*read)(struct sock_desc *sd, void *buf, size_t count, int flags);
	void (*release)(struct sock_desc *sd);
	int (*blnotreg)(enum tzdev_blocking_notifier_type type, struct notifier_block *nb);
	int (*blnotunreg)(enum tzdev_blocking_notifier_type type, struct notifier_block *nb);
};

static struct tzdev_functions tzdf = {
	.socket  = NULL,
	.connect = NULL,
	.write   = NULL,
	.read    = NULL,
	.release = NULL,
	.blnotreg = NULL,
	.blnotunreg = NULL
};

static bool reboot_flag = false;
static struct sock_desc *sd = NULL;
static struct task_struct *iwd_kthread = NULL;
static bool thread_flag = false;
static struct completion finished;
static int reboot_notif_registered = 0;
static int tzdev_notif_registered = 0;

//TEE_EVENT_TUI_REE_TYPE values, copied from tee_tui_low_api.h
enum {
	TEE_EVENT_TUI_REE_DISPLAYSTOPPED = 0x0000,  // Return control to REE
	TEE_EVENT_TUI_REE_CANCEL         = 0x0001,  // Power event
	TEE_EVENT_TUI_REE_ROT90          = 0x0002,  // Rotation 90
	TEE_EVENT_TUI_REE_ROT180         = 0x0003,  // Rotation 180
	TEE_EVENT_TUI_REE_ROT270         = 0x0004,  // Rotation 270
	TEE_EVENT_TUI_REE_ILLEGAL_VALUE  = 0x7fff,  // Illegal value
};

static int notifier_callback(struct notifier_block *self, unsigned long event, void *data);

static struct notifier_block tzdev_notif = {
	.notifier_call = notifier_callback
};

static struct notifier_block reboot_notif = {
	.notifier_call = notifier_callback
};

static int notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	(void) data;
	pr_debug(TUIHW_LOG_TAG " %s >> event=%lu\n", __func__, event);
	if (reboot_flag) {
		pr_info(TUIHW_LOG_TAG " reboot_flag is true\n");
		return NOTIFY_OK;
	}
	stui_cancel_session();
	if (self == &tzdev_notif && event == TZDEV_FINI_NOTIFIER) {
		pr_debug(TUIHW_LOG_TAG " %s TZDEV_FINI_NOTIFIER\n", __func__);
		unregister_tzdev_in_tuihw();
	}
	pr_debug(TUIHW_LOG_TAG " %s <<\n", __func__);
	return NOTIFY_OK;
}

static void iwd_register_callbacks(void)
{
	int ret = 0;
	pr_debug(TUIHW_LOG_TAG " %s >>\n", __func__);
	if (reboot_notif_registered == 0) {
		ret = register_reboot_notifier(&reboot_notif);
		if (ret != 0) {
			pr_err(TUIHW_LOG_TAG " Can't register reboot notifier\n");
			return;
		}

		reboot_notif_registered = 1;
	}
	pr_debug(TUIHW_LOG_TAG " %s <<\n", __func__);
}

static void iwd_unregister_callbacks(void)
{
	if (reboot_notif_registered == 1) {
		unregister_reboot_notifier(&reboot_notif);
		reboot_notif_registered = 0;
	}

	if (tzdev_notif_registered == 1) {
		if (tzdf.blnotunreg) {
			tzdf.blnotunreg(TZDEV_FINI_NOTIFIER, &tzdev_notif);
			tzdev_notif_registered = 0;
		}
	}
}

static int get_display_info(GetDisplayInfo_cmd_t *cmd, GetDisplayInfo_rsp_t *rsp)
{
	struct tui_hw_buffer buffer;
	(void)cmd;
	pr_debug(TUIHW_LOG_TAG " %s >>\n", __func__);
	memset(&buffer, 0, sizeof(struct tui_hw_buffer));
	if (stui_get_resolution(&buffer)) {
		pr_err(TUIHW_LOG_TAG " stui_get_resolution failed\n");
		return -1;
	}
	rsp->physical_width  = 0; //unknown
	rsp->physical_height = 0; //unknown
	rsp->pixel_width  = buffer.width;
	rsp->pixel_height = buffer.height;
	rsp->bit_depth    = 32;
	rsp->flags        = 0;
	rsp->num_periph   = 0;
	pr_debug(TUIHW_LOG_TAG " %s <<\n", __func__);
	return 0;
}

static int open_driver(OpenPeripheral_cmd_t *cmd, OpenPeripheral_rsp_t *rsp)
{
	//OpenPeripheral_cmd_t contains an array of drivers to open
	int ret;
	int i;
	int j;
	struct tui_hw_buffer buffer;
	pr_debug(TUIHW_LOG_TAG " %s >>  cmd->flags=%X, cmd->num=%d size=%zu\n",
			__func__, cmd->flags, cmd->num, sizeof(OpenPeripheral_cmd_t));
	for (i = 0; i < cmd->num; i++)
		pr_debug(TUIHW_LOG_TAG " cmd->peripheral_id[i]=%d\n", cmd->peripheral_id[i]);

	for (i = 0; i < cmd->num; i++) {
		switch(cmd->peripheral_id[i]) {
		case TUILL_TOUCH_DRV:
			pr_debug(TUIHW_LOG_TAG " opening TUILL_TOUCH_DRV\n");
			ret = stui_open_touch();
			if (ret < 0) {
				pr_err(TUIHW_LOG_TAG " ret=%X\n", ret);
				goto lbl_rollback;
			}
			break;
		case TUILL_DISPLAY_DRV:
			pr_debug(TUIHW_LOG_TAG " opening TUILL_DISPLAY_DRV\n");
			ret = stui_open_display(&buffer);
			if (ret < 0) {
				pr_err(TUIHW_LOG_TAG " ret=%X\n", ret);
				goto lbl_rollback;
			}
			rsp->FB.width         = buffer.width;
			rsp->FB.height        = buffer.height;
			rsp->FB.fb_physical   = buffer.fb_physical;
			rsp->FB.fb_size       = buffer.fb_size;
			rsp->FB.wb_physical   = buffer.wb_physical;
			rsp->FB.wb_size       = buffer.wb_size;
			rsp->FB.disp_physical = buffer.disp_physical;
			rsp->FB.disp_size     = buffer.disp_size;
			rsp->FB.touch_type    = 0; //stui_get_touch_type();
			break;
		}
	}
	pr_debug(TUIHW_LOG_TAG " %s <<\n", __func__);
	return 0;
lbl_rollback:
	for (j = 0; j < i; j++) {
		switch(cmd->peripheral_id[j]) {
		case TUILL_TOUCH_DRV:
			pr_debug(TUIHW_LOG_TAG " closing TUILL_TOUCH_DRV\n");
			stui_close_touch();
			break;
		case TUILL_DISPLAY_DRV:
			pr_debug(TUIHW_LOG_TAG " closing TUILL_DISPLAY_DRV\n");
			stui_close_display();
			break;
		}
	}
	pr_err(TUIHW_LOG_TAG " %s <<\n", __func__);
	return -1;
}

static int close_driver(ClosePeripheral_cmd_t *cmd)
{
	int i = 0;
	pr_debug(TUIHW_LOG_TAG " %s >>\n", __func__);
	for (i = 0; i < cmd->num; i++) {
		switch(cmd->peripheral_id[i]) {
		case TUILL_TOUCH_DRV:
			pr_debug(TUIHW_LOG_TAG " closing TUILL_TOUCH_DRV\n");
			stui_close_touch();
			break;
		case TUILL_DISPLAY_DRV:
			pr_debug(TUIHW_LOG_TAG " closing TUILL_DISPLAY_DRV\n");
			stui_close_display();
			break;
		}
	}
	pr_debug(TUIHW_LOG_TAG " %s <<\n", __func__);
	return 0;
}

static void reboot_phone()
{
	/**
	 *	kernel_restart - reboot the system
	 *	@cmd: pointer to buffer containing command to execute for restart or %NULL
	 */
	pr_debug(TUIHW_LOG_TAG " %s >>\n", __func__);
	reboot_flag = true;
	kernel_restart(NULL); // -> To restart the system
	reboot_flag = false;
	pr_debug(TUIHW_LOG_TAG " %s <<\n", __func__);
}

static int connecting_thread(void *data)
{
	int ret = 0;
	tuill_internal_command_t cmd;
	tuill_internal_command_t rsp;
	char serv_name[100];
	sprintf(serv_name, TUILL_SERVER_TEMPLATE, OS_IWD_SOCKET_NAME);
	pr_debug(TUIHW_LOG_TAG " %s >>\n", __func__);
	while (thread_flag) {
		/**
		 *	EPOLLHUP signal propagation to peripheral drivers when OS service panicked
		 *	takes 20-50 ms. Also we need to wait some time until peripheral drivers clear HAL.
		 *	500 ms timeout was chosen to be sure that SWd periferal was released before
		 *	NWd peripheral released.
		 */
		usleep_range(500000, 500001);//0.5 s

		sd = tzdf.socket(1, TZ_NON_INTERRUPTIBLE);

		if (IS_ERR(sd)) {
			pr_err(TUIHW_LOG_TAG " tz_iwsock_socket failed\n");
			continue;
		}

		pr_info(TUIHW_LOG_TAG " connecting to %s\n", serv_name);
		ret = tzdf.connect(sd, serv_name, 0);
		if (ret != 0) {
			tzdf.release(sd);
			sd = NULL;
			continue;
		}
		pr_info(TUIHW_LOG_TAG " connected to %s\n", serv_name);

		//sending handshake
		cmd.cmd        = TUILL_ICMD_SET_DRV_STATE;
		cmd.task_state = TASK_STATE_VOID;
		cmd.task_id    = -1;
		cmd.SetDrvInfo_cmd.drv_tui_mode = stui_get_mode();
		cmd.SetDrvInfo_cmd.index        = TUILL_TUIHW;
		ret = tzdf.write(sd, &cmd, sizeof(cmd), 0);

		while (ret >= 0) {
			if (!thread_flag) {
				pr_debug(TUIHW_LOG_TAG " thread was stopped\n");
				break;
			}
			ret = tzdf.read(sd, &cmd, sizeof(cmd), 0);
			if (ret > 0 && ret != sizeof(cmd)) {
				pr_err(TUIHW_LOG_TAG " tz_iwsock_read returned %d\n", ret);
				continue;
			} else if (ret == 0) {
				pr_err(TUIHW_LOG_TAG " connection was reset by peer\n");
				break;
			} else if (ret < 0) {
				pr_err(TUIHW_LOG_TAG " tz_iwsock_read returned %d\n", ret);
				break;
			}
			rsp.cmd        = cmd.cmd + RESPONSE_FLAG;
			rsp.task_state = cmd.task_state;
			rsp.task_id    = cmd.task_id;
			pr_debug(TUIHW_LOG_TAG " cmd.ret_code=0x%X\n", cmd.ret_code);
			if (cmd.ret_code & INJECT_ERR_FLAG) {
				pr_debug(TUIHW_LOG_TAG " error injection\n");
				rsp.ret_code = -1;
				ret = tzdf.write(sd, &rsp, sizeof(rsp), 0);
				continue;
			}
			if (cmd.ret_code & MAKE_TIMEOUT_FLAG) {
				pr_debug(TUIHW_LOG_TAG " timeout injection\n");
				continue;
			}
			switch(cmd.cmd) {
			case TUILL_ICMD_GET_DISPLAY_INFO:
				pr_debug(TUIHW_LOG_TAG " received TUILL_ICMD_GET_DISPLAY_INFO\n");
				rsp.ret_code = get_display_info(&cmd.GetDisplayInfo_cmd, &rsp.GetDisplayInfo_rsp);
				break;
			case TUILL_ICMD_OPEN_DRIVER:
				pr_debug(TUIHW_LOG_TAG " received TUILL_ICMD_OPEN_DRIVER\n");
				rsp.ret_code = open_driver(&cmd.OpenPeripheral_cmd, &rsp.OpenPeripheral_rsp);
				break;
			case TUILL_ICMD_CLOSE_DRIVER:
				pr_debug(TUIHW_LOG_TAG " received TUILL_ICMD_CLOSE_DRIVER\n");
				rsp.ret_code = close_driver(&cmd.ClosePeripheral_cmd);
				break;
			case TUILL_ICMD_REBOOT_PHONE:
				pr_debug(TUIHW_LOG_TAG " received TUILL_ICMD_REBOOT_PHONE\n");
				reboot_phone();
				break;
			}
			ret = tzdf.write(sd, &rsp, sizeof(rsp), 0);
		}
		if (sd) {
			tzdf.release(sd);
			sd = NULL;
		}
	}
	pr_debug(TUIHW_LOG_TAG " %s << ret=%d\n", __func__, ret);
	complete(&finished);
	return 0;
}

int iwd_cancel_session(void)
{
	//TODO: check atomicity of tz_iwsock_write
	tuill_internal_command_t cmd;
	int ret = 0;
	pr_debug(TUIHW_LOG_TAG " %s >>\n", __func__);
	cmd.cmd = TUILL_ICMD_CANCEL_TUI;
	cmd.version = TUILL_API_VERSION;
	cmd.CancelTUI_cmd.event = TEE_EVENT_TUI_REE_CANCEL;
	if (tzdf.write) {
		ret = tzdf.write(sd, &cmd, sizeof(cmd), 0);
	}
	pr_debug(TUIHW_LOG_TAG " tz_iwsock_write returned %d\n", ret);
	if (ret != sizeof(cmd)) {
		pr_err(TUIHW_LOG_TAG " %s <<\n", __func__);
		return -1;
	}
	pr_debug(TUIHW_LOG_TAG " %s <<\n", __func__);
	return 0;
}

int init_iwd_agent(void)
{
	struct iwd_functions iwdf;
	pr_debug(TUIHW_LOG_TAG " %s >>\n", __func__);
	init_completion(&finished);
	iwd_register_callbacks();
	iwdf.cancel_session = iwd_cancel_session;
	register_iwd_functions(&iwdf);
	pr_debug(TUIHW_LOG_TAG " %s <<\n", __func__);
	return 0;
}

void uninit_iwd_agent(void)
{
	pr_debug(TUIHW_LOG_TAG " %s >>\n", __func__);
	unregister_iwd_functions();
	iwd_unregister_callbacks();
}

static int __init_iwd_agent(void)
{
	int ret = 0;
	pr_debug(TUIHW_LOG_TAG " %s >>\n", __func__);

	ret = tzdf.blnotreg(TZDEV_FINI_NOTIFIER, &tzdev_notif);

	if (ret) {
		pr_err(TUIHW_LOG_TAG " Can't register tzdev notifier\n");
		return -EFAULT;
	}

	tzdev_notif_registered = 1;
	thread_flag = true;
	reinit_completion(&finished);
	iwd_kthread = kthread_run(connecting_thread, NULL, "connecting_thread");
	if (IS_ERR(iwd_kthread)) {
		pr_err(TUIHW_LOG_TAG " kthread_run failedn");
		thread_flag = false;
		return -EFAULT;
	}

	pr_debug(TUIHW_LOG_TAG " %s <<\n", __func__);
	return 0;
}

static void __uninit_iwd_agent(void)
{
	pr_debug(TUIHW_LOG_TAG " %s >>\n", __func__);
	thread_flag = false;
	if (tzdf.release && sd) {
		tzdf.release(sd);//to breake reading
		sd = NULL;
	}
	wait_for_completion(&finished);
}

int register_tzdev_in_tuihw(struct tzdev_functions *f)
{
	tzdf.socket     = f->socket;
	tzdf.connect    = f->connect;
	tzdf.write      = f->write;
	tzdf.read       = f->read;
	tzdf.release    = f->release;
	tzdf.blnotreg   = f->blnotreg;
	tzdf.blnotunreg = f->blnotunreg;
	return __init_iwd_agent();
}
EXPORT_SYMBOL(register_tzdev_in_tuihw);

static void unregister_tzdev_in_tuihw(void)
{
	__uninit_iwd_agent();
	tzdf.socket     = NULL;
	tzdf.connect    = NULL;
	tzdf.write      = NULL;
	tzdf.read       = NULL;
	tzdf.release    = NULL;
	tzdf.blnotreg   = NULL;
	tzdf.blnotunreg = NULL;
}

bool get_tui_reboot_flag(void)
{
	return reboot_flag;
}
EXPORT_SYMBOL(get_tui_reboot_flag);

