/* SPDX-License-Identifier: GPL-2.0-or-later
 * sound/soc/samsung/vts/vts_shared_info.h
 *
 * ALSA SoC - Samsung VTS driver
 *
 * Copyright (c) 2016 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef VTSS_SHARED_INFO_H_
#define VTSS_SHARED_INFO_H_

enum VTS_IPC_ID {
	EXT_IPC_RECEIVED,
	EXT_IPC_SYSTEM = 1,
	EXT_IPC_VTS_CONFIG = 0x10,
	EXT_IPC_ID_COUNT,
};

enum VTS_SYSTEM_MSG {
	VTS_SUSPEND = 1,
	VTS_REPORT_LOG = 0x10,
};

enum VTS_CONFIGMSG {
	SET_REC_RATE = 0x1,
	SET_TRI_RATE = 0x10,
};

struct VTS_IPC_SYSTEM_MSG {
	enum VTS_SYSTEM_MSG msgtype;
	int param1;
	int param2;
	int param3;
	union {
		int param_s32[0];
		unsigned long long param_u64[0];
		char param_bundle[32];
	} bundle;
};

struct VTS_IPC_CONFIG_MSG {
	enum VTS_CONFIGMSG msgtype;
	int param1;
	int param2;
	int param3;
};

struct vts_ext_ipc_msg {
	enum VTS_IPC_ID ipcid;
	int id;
	union VTS_IPC_MSG {
		struct VTS_IPC_SYSTEM_MSG system;
		struct VTS_IPC_CONFIG_MSG config;
	} msg;
};

struct vts_shared_config {
	/* status */
	unsigned int rec_mode;
	unsigned int rec_ch;
	unsigned int rec_period_size;
	unsigned int rec_period_count;

	unsigned int trigger_mode;
	unsigned int trigger_ch;
	unsigned int trigger_period_size;
	unsigned int trigger_period_count;

	unsigned int target_sys_clk;
	/* extended ipc -- TODO */
	struct vts_ext_ipc_msg ipc;
};

struct vts_shared_info {
	unsigned int log_pos_write;
	unsigned int log_pos_read;
	unsigned int kernel_sec;
	unsigned int kernel_msec;
	unsigned int vendor_data[10];
	struct vts_shared_config config_ap;
	struct vts_shared_config config_fw;
};

#endif

