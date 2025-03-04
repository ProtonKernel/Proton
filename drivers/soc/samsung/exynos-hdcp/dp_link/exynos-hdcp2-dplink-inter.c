/*
 * drivers/soc/samsung/exynos-hdcp/dp_link/exynos-hdcp2-dplink.c
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/smc.h>
#include <asm/cacheflush.h>
#include <soc/samsung/exynos-smc.h>
#include <linux/types.h>
#include <linux/delay.h>

#include "../exynos-hdcp2.h"
#include "../exynos-hdcp2-log.h"
#include "exynos-hdcp2-dplink.h"
#include "exynos-hdcp2-dplink-inter.h"
#include "exynos-hdcp2-dplink-if.h"
#include "exynos-hdcp2-dplink-auth.h"

#define DRM_WAIT_RETRY_COUNT	1000
/* current link data */
enum auth_state auth_proc_state;
EXPORT_SYMBOL_GPL(auth_proc_state);
enum dp_state dp_hdcp_state;
EXPORT_SYMBOL_GPL(dp_hdcp_state);

int hdcp_dplink_auth_check(enum auth_signal hdcp_signal)
{
	int ret = 0;

#if defined(CONFIG_HDCP2_FUNC_TEST_MODE)
	ret = exynos_smc(SMC_DRM_HDCP_FUNC_TEST, 1, 0, 0);
#endif
	switch (hdcp_signal) {
		case HDCP_DRM_OFF:
			ret = 0;
			break;
		case HDCP_DRM_ON:
			ret = exynos_smc(SMC_DRM_HDCP_AUTH_INFO, DP_HDCP22_DISABLE, 0, 0);
			dplink_clear_irqflag_all();
			ret = hdcp_dplink_authenticate();
			break;
		case HDCP_RP_READY:
			if (auth_proc_state == HDCP_AUTH_PROCESS_DONE) {
				ret = hdcp_dplink_authenticate();
				if (ret == 0)
					auth_proc_state = HDCP_AUTH_PROCESS_DONE;
			}
			break;
		default:
			ret = HDCP_ERROR_INVALID_STATE;

	}
	return ret;
}
EXPORT_SYMBOL_GPL(hdcp_dplink_auth_check);

int hdcp_dplink_get_rxstatus(uint8_t *status)
{
	int ret = 0;

	ret = hdcp_dplink_get_rxinfo(status);
	return ret;
}
EXPORT_SYMBOL_GPL(hdcp_dplink_get_rxstatus);

int hdcp_dplink_set_paring_available(void)
{
	hdcp_info("pairing avaible\n");
	return dplink_set_paring_available();
}
EXPORT_SYMBOL_GPL(hdcp_dplink_set_paring_available);

int hdcp_dplink_set_hprime_available(void)
{
	hdcp_info("h-prime avaible\n");
	return dplink_set_hprime_available();
}
EXPORT_SYMBOL_GPL(hdcp_dplink_set_hprime_available);

int hdcp_dplink_set_rp_ready(void)
{
	hdcp_info("ready avaible\n");
	return dplink_set_rp_ready();
}
EXPORT_SYMBOL_GPL(hdcp_dplink_set_rp_ready);

int hdcp_dplink_set_reauth(void)
{
	uint64_t ret = 0;

	hdcp_info("reauth requested.\n");
	ret = exynos_smc(SMC_DRM_HDCP_AUTH_INFO, DP_HDCP22_DISABLE, 0, 0);
	return dplink_set_reauth_req();
}
EXPORT_SYMBOL_GPL(hdcp_dplink_set_reauth);

int hdcp_dplink_set_integrity_fail(void)
{
	uint64_t ret = 0;

	hdcp_info("integrity check fail.\n");
	ret = exynos_smc(SMC_DRM_HDCP_AUTH_INFO, DP_HDCP22_DISABLE, 0, 0);
	return dplink_set_integrity_fail();
}
EXPORT_SYMBOL_GPL(hdcp_dplink_set_integrity_fail);

int hdcp_dplink_cancel_auth(void)
{
	uint64_t ret = 0;

	hdcp_info("Cancel authenticate.\n");
	ret = exynos_smc(SMC_DRM_HDCP_AUTH_INFO, DP_HPD_STATUS_ZERO, 0, 0);
	auth_proc_state = HDCP_AUTH_PROCESS_STOP;

	return dplink_set_integrity_fail();
}
EXPORT_SYMBOL_GPL(hdcp_dplink_cancel_auth);

void hdcp_dplink_clear_all(void)
{
	uint64_t ret = 0;

	hdcp_info("HDCP flag clear\n");
	ret = exynos_smc(SMC_DRM_HDCP_AUTH_INFO, DP_HDCP22_DISABLE, 0, 0);
	dplink_clear_irqflag_all();
}
EXPORT_SYMBOL_GPL(hdcp_dplink_clear_all);

void hdcp_dplink_connect_state(enum dp_state state)
{
	uint64_t ret = 0;

	dp_hdcp_state = state;
	hdcp_info("Displayport connect info (%d)\n", dp_hdcp_state);
	ret = exynos_smc(SMC_DRM_DP_CONNECT_INFO, dp_hdcp_state, 0, 0);
}
EXPORT_SYMBOL_GPL(hdcp_dplink_connect_state);

int hdcp_dplink_is_auth_state(void)
{
	return 0;
	/* todo: check link auth status */
	#if 0
	if (lk_data->state == LINK_ST_A5_AUTHENTICATED)
		return 1;
	else
		return 0;
	#endif
}

MODULE_LICENSE("GPL");
