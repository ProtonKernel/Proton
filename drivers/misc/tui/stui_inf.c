/* tui/stui_inf.c
 *
 * Samsung TUI HW Handler driver.
 *
 * Copyright (c) 2015 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#ifdef USE_TEE_CLIENT_API
#include <tee_client_api.h>
#endif /* USE_TEE_CLIENT_API */
#include "stui_inf.h"

#define TUI_REE_EXTERNAL_EVENT      42
#define SESSION_CANCEL_DELAY        150
#define MAX_WAIT_CNT                10

#define TUIHW_LOG_TAG "tuill_hw"

static int tui_mode = STUI_MODE_OFF;
static int stui_touch_type;
static DEFINE_SPINLOCK(tui_lock);

#ifdef USE_TEE_CLIENT_API
static TEEC_UUID uuid = {
	.timeLow = 0x0,
	.timeMid = 0x0,
	.timeHiAndVersion = 0x0,
	.clockSeqAndNode = {0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0, 0x81},
};
#endif

#ifdef CONFIG_SAMSUNG_TUI_LOWLEVEL
static DEFINE_SPINLOCK(iwdf_lock);

struct iwd_functions {
	int (*cancel_session)(void);
};

struct iwd_functions iwdf = {
	.cancel_session = NULL
};

void register_iwd_functions(struct iwd_functions *f)
{
	unsigned long fls;
	spin_lock_irqsave(&iwdf_lock, fls);
	iwdf.cancel_session = f->cancel_session;
	spin_unlock_irqrestore(&iwdf_lock, fls);
}
EXPORT_SYMBOL(register_iwd_functions);

void unregister_iwd_functions(void)
{
	unsigned long fls;
	spin_lock_irqsave(&iwdf_lock, fls);
	iwdf.cancel_session = NULL;
	spin_unlock_irqrestore(&iwdf_lock, fls);
}
EXPORT_SYMBOL(unregister_iwd_functions);
#endif /* CONFIG_SAMSUNG_TUI_LOWLEVEL */

int stui_get_mode(void)
{
	unsigned long fls;
	int ret_mode;

	spin_lock_irqsave(&tui_lock, fls);
	ret_mode = tui_mode;
	spin_unlock_irqrestore(&tui_lock, fls);
	pr_debug(TUIHW_LOG_TAG " %s << ret_mode=%#X\n", __func__, ret_mode);
	return ret_mode;
}
EXPORT_SYMBOL(stui_get_mode);

void stui_set_mode(int mode)
{
	unsigned long fls;

	pr_debug(TUIHW_LOG_TAG " %s >> mode=%#X\n", __func__, mode);
	spin_lock_irqsave(&tui_lock, fls);
	tui_mode = mode;
	spin_unlock_irqrestore(&tui_lock, fls);
}
EXPORT_SYMBOL(stui_set_mode);

int stui_set_mask(int mask)
{
	unsigned long fls;
	int ret_mode;

	pr_debug(TUIHW_LOG_TAG " %s >> mask=%#X\n", __func__, mask);
	spin_lock_irqsave(&tui_lock, fls);
	ret_mode = (tui_mode |= mask);
	spin_unlock_irqrestore(&tui_lock, fls);
	return ret_mode;
}
EXPORT_SYMBOL(stui_set_mask);

int stui_clear_mask(int mask)
{
	unsigned long fls;
	int ret_mode;

	pr_debug(TUIHW_LOG_TAG " %s >> mask=%#X\n", __func__, mask);
	spin_lock_irqsave(&tui_lock, fls);
	ret_mode = (tui_mode &= ~mask);
	spin_unlock_irqrestore(&tui_lock, fls);
	return ret_mode;
}
EXPORT_SYMBOL(stui_clear_mask);

void stui_set_touch_type(uint32_t type)
{
	stui_touch_type = type;
}
EXPORT_SYMBOL(stui_set_touch_type);

int stui_get_touch_type(void)
{
	return stui_touch_type;
}
EXPORT_SYMBOL(stui_get_touch_type);

#ifdef USE_TEE_CLIENT_API
static atomic_t canceling = ATOMIC_INIT(0);
int stui_cancel_session(void)
{
	TEEC_Context context;
	TEEC_Session session;
	int result = 0;
	TEEC_Operation operation;
	int ret = -1;
	int count = 0;
#ifdef CONFIG_SAMSUNG_TUI_LOWLEVEL
	unsigned long fls;
#endif /* CONFIG_SAMSUNG_TUI_LOWLEVEL */

	pr_debug(TUIHW_LOG_TAG " %s >>\n", __func__);

	if (!(STUI_MODE_ALL & stui_get_mode())) {
		pr_debug(TUIHW_LOG_TAG " session cancel is not needed\n");
		return 0;
	}

	if (atomic_cmpxchg(&canceling, 0, 1) != 0) {
		pr_debug(TUIHW_LOG_TAG " already canceling.\n");

		while ((STUI_MODE_ALL & stui_get_mode()) && (count < MAX_WAIT_CNT)) {
			msleep(SESSION_CANCEL_DELAY);
			count++;
		}

		if (STUI_MODE_ALL & stui_get_mode()) {
			pr_err(TUIHW_LOG_TAG " session was not cancelled yet\n");
		} else {
			pr_info(TUIHW_LOG_TAG " session was cancelled successfully\n");
			ret = 0;
		}

		return ret;
	}

#ifdef CONFIG_SAMSUNG_TUI_LOWLEVEL
	spin_lock_irqsave(&iwdf_lock, fls);
	if (iwdf.cancel_session != NULL) {
		result = iwdf.cancel_session();
	}
	spin_unlock_irqrestore(&iwdf_lock, fls);
	if (result != 0) {
		pr_err(TUIHW_LOG_TAG " iwd_cancel_session returned: 0x%x\n", result);
		goto out;
	}
#endif /* CONFIG_SAMSUNG_TUI_LOWLEVEL */

	result = TEEC_InitializeContext(NULL, &context);
	if (result != TEEC_SUCCESS) {
		pr_err(TUIHW_LOG_TAG " TEEC_InitializeContext returned: 0x%x\n", result);
		goto out;
	}

	result = TEEC_OpenSession(&context, &session, &uuid, TEEC_LOGIN_PUBLIC, NULL, NULL, NULL);
	if (result != TEEC_SUCCESS) {
		pr_err(TUIHW_LOG_TAG " TEEC_OpenSession returned: 0x%x\n", result);
		goto finalize_context;
	}

	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE, TEEC_NONE, TEEC_NONE);

	result = TEEC_InvokeCommand(&session, TUI_REE_EXTERNAL_EVENT, &operation, NULL);
	if (result != TEEC_SUCCESS) {
		pr_err(TUIHW_LOG_TAG " TEEC_InvokeCommand returned: 0x%x\n", result);
		goto close_session;
	} else {
		pr_debug(TUIHW_LOG_TAG " invoked cancel cmd\n");
	}

	TEEC_CloseSession(&session);
	TEEC_FinalizeContext(&context);

	while ((STUI_MODE_ALL & stui_get_mode()) && (count < MAX_WAIT_CNT)) {
		msleep(SESSION_CANCEL_DELAY);
		count++;
	}

	if (STUI_MODE_ALL & stui_get_mode()) {
		pr_err(TUIHW_LOG_TAG " session was not cancelled yet\n");
	} else {
		pr_debug(TUIHW_LOG_TAG " session was cancelled successfully\n");
		ret = 0;
	}

	atomic_set(&canceling, 0);
	return ret;

close_session:
	TEEC_CloseSession(&session);
finalize_context:
	TEEC_FinalizeContext(&context);
out:
	atomic_set(&canceling, 0);
	pr_err(TUIHW_LOG_TAG " %s << ret=%d, result=0x%x\n", __func__, ret, result);

	return ret;
}
#else /* USE_TEE_CLIENT_API */
int stui_cancel_session(void)
{
#ifdef CONFIG_SAMSUNG_TUI_LOWLEVEL
	int ret = -1;
	unsigned long fls;

	if (!(STUI_MODE_ALL & stui_get_mode())) {
		pr_debug(TUIHW_LOG_TAG " session cancel is not needed\n");
		return 0;
	}

	spin_lock_irqsave(&iwdf_lock, fls);
	if (iwdf.cancel_session != NULL) {
		ret = iwdf.cancel_session();
	}
	spin_unlock_irqrestore(&iwdf_lock, fls);
	return ret;
#else /* CONFIG_SAMSUNG_TUI_LOWLEVEL */
	pr_err(TUIHW_LOG_TAG " %s not supported\n", __func__);
	return -1;
#endif /* CONFIG_SAMSUNG_TUI_LOWLEVEL */
}
#endif /* USE_TEE_CLIENT_API */
EXPORT_SYMBOL(stui_cancel_session);

static int __init teegris_tui_inf_init(void)
{
	pr_info(TUIHW_LOG_TAG "=============== Running TEEgris TUI Inf ===============");
	return 0;
}

static void __exit teegris_tui_inf_exit(void)
{
	pr_info(TUIHW_LOG_TAG "Unloading teegris tui inf module.");
}

module_init(teegris_tui_inf_init);
module_exit(teegris_tui_inf_exit);

MODULE_AUTHOR("TUI Teegris");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("TEEGRIS TUI");
