/*
 * drivers/media/video/exynos/fimc-is-mc2/fimc-is-interface.h
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * The header file related to camera
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_INTERFACE_H
#define IS_INTERFACE_H
#include "is-metadata.h"
#include "is-framemgr.h"
#include "is-video.h"
#include "is-time.h"
#include "is-cmd.h"
#include "is-work.h"

#define MAX_NBLOCKING_COUNT	3

#define TRY_TIMEOUT_COUNT	2
#define SENSOR_TIMEOUT_COUNT	2
#define TRY_RECV_AWARE_COUNT	10000

#define LOWBIT_OF(num)	(num >= 32 ? 0 : (u32)1<<num)
#define HIGHBIT_OF(num)	(num >= 32 ? (u32)1<<(num-32) : 0)

enum is_interface_state {
	IS_IF_STATE_OPEN,
	IS_IF_STATE_READY,
	IS_IF_STATE_START,
	IS_IF_STATE_BUSY,
	IS_IF_STATE_LOGGING
};

enum streaming_state {
	IS_IF_STREAMING_INIT,
	IS_IF_STREAMING_OFF,
	IS_IF_STREAMING_ON
};

enum processing_state {
	IS_IF_PROCESSING_INIT,
	IS_IF_PROCESSING_OFF,
	IS_IF_PROCESSING_ON
};

enum pdown_ready_state {
	IS_IF_POWER_DOWN_READY,
	IS_IF_POWER_DOWN_NREADY
};

enum launch_state {
	IS_IF_LAUNCH_FIRST,
	IS_IF_LAUNCH_SUCCESS,
};

enum fw_boot_state {
	IS_IF_RESUME,
	IS_IF_SUSPEND,
};

enum is_fw_boot {
	FIRST_LAUNCHING,
	WARM_BOOT,
	COLD_BOOT,
};

struct is_interface {
	void __iomem			*regs;
	struct is_common_reg __iomem	*com_regs;
	unsigned long			state;
	spinlock_t			process_barrier;
	struct is_msg		process_msg;
	struct mutex			request_barrier;
	struct is_msg		request_msg;

	atomic_t			lock_pid;
	wait_queue_head_t		lock_wait_queue;
	wait_queue_head_t		init_wait_queue;
	wait_queue_head_t		idle_wait_queue;
	struct is_msg		reply;
#ifdef MEASURE_TIME
#ifdef INTERFACE_TIME
	struct is_interface_time	time[HIC_COMMAND_END];
#endif
#endif

	struct workqueue_struct		*workqueue;
	struct work_struct		work_wq[WORK_MAX_MAP];
	struct is_work_list	work_list[WORK_MAX_MAP];

	/* sensor streaming flag */
	enum streaming_state		streaming[IS_STREAM_COUNT];
	/* firmware processing flag */
	enum processing_state		processing[IS_STREAM_COUNT];
	/* frrmware power down ready flag */
	enum pdown_ready_state		pdown_ready;

	unsigned long			fw_boot;

	struct is_framemgr		*framemgr;

	struct is_work_list	nblk_cam_ctrl;

	/* shot timeout check */
	spinlock_t			shot_check_lock;
	atomic_t			shot_check[IS_STREAM_COUNT];
	atomic_t			shot_timeout[IS_STREAM_COUNT];
	/* sensor timeout check */
	atomic_t			sensor_check[IS_SENSOR_COUNT];
	atomic_t			sensor_timeout[IS_SENSOR_COUNT];
	struct timer_list		timer;

	/* callback func to handle error report for specific purpose */
	void				*err_report_data;
	int				(*err_report_vendor)(void *data, u32 err_report_type);

	struct camera2_uctl		isp_peri_ctl;
	/* check firsttime */
	unsigned long			launch_state;
	enum is_fw_boot		fw_boot_mode;
	ulong				itf_kvaddr;
	void				*core;
};

void is_itf_fwboot_init(struct is_interface *this);
int is_interface_probe(struct is_interface *this,
	struct is_minfo *minfo,
	ulong regs,
	u32 irq,
	void *core_data);
int is_interface_open(struct is_interface *this);
int is_interface_close(struct is_interface *this);
void is_interface_lock(struct is_interface *this);
void is_interface_unlock(struct is_interface *this);
void is_interface_reset(struct is_interface *this);

void is_storefirm(struct is_interface *this);
void is_restorefirm(struct is_interface *this);
int is_set_fwboot(struct is_interface *this, int val);

int is_hw_logdump(struct is_interface *this);
int is_hw_regdump(struct is_interface *this);
int is_hw_memdump(struct is_interface *this,
	ulong start,
	ulong end);
int is_hw_enum(struct is_interface *this);
int is_hw_fault(struct is_interface *this);
int is_hw_open(struct is_interface *this,
	u32 instance, u32 module, u32 info, u32 path, u32 flag,
	u32 *mwidth, u32 *mheight);
int is_hw_close(struct is_interface *this,
	u32 instance);
int is_hw_saddr(struct is_interface *interface,
	u32 instance, u32 *setfile_addr);
int is_hw_setfile(struct is_interface *interface,
	u32 instance);
int is_hw_process_on(struct is_interface *this,
	u32 instance, u32 group);
int is_hw_process_off(struct is_interface *this,
	u32 instance, u32 group, u32 mode);
int is_hw_stream_on(struct is_interface *interface,
	u32 instance);
int is_hw_stream_off(struct is_interface *interface,
	u32 instance);
int is_hw_s_param(struct is_interface *interface,
	u32 instance, u32 lindex, u32 hindex, u32 indexes);
int is_hw_a_param(struct is_interface *this,
	u32 instance, u32 group, u32 sub_mode);
int is_hw_g_capability(struct is_interface *this,
	u32 instance, u32 address);
int is_hw_map(struct is_interface *this,
	u32 instance, u32 group, u32 address, u32 size);
int is_hw_unmap(struct is_interface *this,
	u32 instance, u32 group);
int is_hw_power_down(struct is_interface *interface,
	u32 instance);
int is_hw_i2c_lock(struct is_interface *interface,
	u32 instance, int clk, bool lock);
int is_hw_sys_ctl(struct is_interface *this,
	u32 instance, int cmd, int val);
int is_hw_sensor_mode(struct is_interface *this,
	u32 instance, int cfg);

int is_hw_shot_nblk(struct is_interface *this,
	u32 instance, u32 group, u32 shot, u32 fcount, u32 rcount);
int is_hw_s_camctrl_nblk(struct is_interface *this,
	u32 instance, u32 address, u32 fcount);
int is_hw_msg_test(struct is_interface *this, u32 sync_id, u32 msg_test_id);

/* func to register error report callback */
int is_set_err_report_vendor(struct is_interface *itf,
		void *err_report_data,
		int (*err_report_vendor)(void *data, u32 err_report_type));

#endif
