/*
 * Samsung EXYNOS Camera PostProcessing driver
 *
 *  Copyright (C) 2019 Samsung Electronics Co., Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#ifndef CAMERAPP_VOTF_ENUM_H
#define CAMERAPP_VOTF_ENUM_H

#define MODULE_NAME "camerapp-votf"
#define IP_MAX 16
#define ID_MAX 16
#define TWS_MAX 16
#define TRS_MAX 16
#define VOTF_INVALID 0xffff

enum votf_state {
	VS_DISCONNECTED,
	VS_READY,
	VS_CONNECTED,	/* ring connected */
};

enum reset_type {
	SW_CORE_RESET,
	SW_RESET
};

enum votf_irq {
	START_ON_BUSY,
	OVERFLOW,
	LOW_THRESH = 3,
	HIGH_THRESH,
	DONE
};

enum votf_service {
	TWS,
	TRS,
	SERVICE_CNT
};

enum votf_module_type {
	/* C2SERV */
	M0S4,
	M2S2,
	M3S1,
	M16S16,
	/* C2AGENT */
	M6S4,
	MODULE_TYPE_CNT
};

enum votf_module {
	C2SERV,
	C2AGENT,
	MODULE_CNT
};

enum votf_mode {
	VOTF_NORMAL,
	VOTF_FRS,
	VOTF_TRS_HEIGHT_X2,
	VOTF_MODE_CNT,
};

enum votf_debug_state {
	VOTF_IDLE = 0,
	VOTF_TRS_WAIT_CON = 1,
	VOTF_TWS_WAIT_CON = 2,
	VOTF_CONNECT = 3,
	VOTF_WAIT_TOKEN_ACK = 5,
	VOTF_WAIT_RESET_ACK = 7,
};

#endif /* CAMERAPP_VOTF_ENUM_H */
