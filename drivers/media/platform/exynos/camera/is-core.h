/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CORE_H
#define IS_CORE_H

#include <linux/version.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <linux/sched/rt.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <soc/samsung/exynos-smc.h>
#else
#include <linux/smc.h>
#endif
#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
#include <soc/samsung/sysevent.h>
#endif

#include <exynos-is.h>
#include "is-interface.h"
#include "is-framemgr.h"
#include "is-resourcemgr.h"
#include "is-devicemgr.h"
#include "is-device-sensor.h"
#include "is-device-ischain.h"
#include "hardware/is-hw-control.h"
#include "interface/is-interface-ischain.h"
#include "is-spi.h"
#include "is-video.h"
#include "is-mem.h"
#include "is-vender.h"
#include "exynos-is-module.h"
#include "is-irq.h"

#define IS_DRV_NAME			"exynos-is"
#define IS_COMMAND_TIMEOUT			(30*HZ)
#define IS_STARTUP_TIMEOUT			(3*HZ)
#define IS_SHUTDOWN_TIMEOUT		(10*HZ)
#define IS_FLITE_STOP_TIMEOUT		(3*HZ)

#define IS_SENSOR_MAX_ENTITIES		(1)
#define IS_SENSOR_PAD_SOURCE_FRONT		(0)
#define IS_SENSOR_PADS_NUM			(1)

#define IS_FRONT_MAX_ENTITIES		(1)
#define IS_FRONT_PAD_SINK			(0)
#define IS_FRONT_PAD_SOURCE_BACK		(1)
#define IS_FRONT_PAD_SOURCE_BAYER		(2)
#define IS_FRONT_PAD_SOURCE_SCALERC	(3)
#define IS_FRONT_PADS_NUM			(4)

#define IS_BACK_MAX_ENTITIES		(1)
#define IS_BACK_PAD_SINK			(0)
#define IS_BACK_PAD_SOURCE_3DNR		(1)
#define IS_BACK_PAD_SOURCE_SCALERP		(2)
#define IS_BACK_PADS_NUM			(3)

#define IS_MAX_SENSOR_NAME_LEN		(16)

#define MAX_ODC_INTERNAL_BUF_WIDTH	(2560)  /* 4808 in HW */
#define MAX_ODC_INTERNAL_BUF_HEIGHT	(1920)  /* 3356 in HW */
#define SIZE_ODC_INTERNAL_BUF \
	(MAX_ODC_INTERNAL_BUF_WIDTH * MAX_ODC_INTERNAL_BUF_HEIGHT * 3)

#define MAX_DIS_INTERNAL_BUF_WIDTH	(2400)
#define MAX_DIS_INTERNAL_BUF_HEIGHT	(1360)
#define SIZE_DIS_INTERNAL_BUF \
	(MAX_DIS_INTERNAL_BUF_WIDTH * MAX_DIS_INTERNAL_BUF_HEIGHT * 2)

#define MAX_3DNR_INTERNAL_BUF_WIDTH	(4032)
#define MAX_3DNR_INTERNAL_BUF_HEIGHT	(2268)
#ifdef TPU_COMPRESSOR
#define SIZE_DNR_INTERNAL_BUF \
	ALIGN((MAX_3DNR_INTERNAL_BUF_WIDTH * MAX_3DNR_INTERNAL_BUF_HEIGHT * 22 / 2 / 8), 16)
#else
#define SIZE_DNR_INTERNAL_BUF \
	ALIGN((MAX_3DNR_INTERNAL_BUF_WIDTH * MAX_3DNR_INTERNAL_BUF_HEIGHT * 18 / 8), 16)
#endif

#define MAX_TNR_INTERNAL_BUF_WIDTH	(4032)
#define MAX_TNR_INTERNAL_BUF_HEIGHT	(3024)
#define SIZE_TNR_IMAGE_BUF \
	(MAX_TNR_INTERNAL_BUF_WIDTH * MAX_TNR_INTERNAL_BUF_HEIGHT * 14 / 8) /* 14 bittage */
#define SIZE_TNR_WEIGHT_BUF \
	(MAX_TNR_INTERNAL_BUF_WIDTH * MAX_TNR_INTERNAL_BUF_HEIGHT / 2 / 2) /* 8 bittage */


#define NUM_LHFD_INTERNAL_BUF		(3)
#define MAX_LHFD_INTERNEL_BUF_WIDTH	(640)
#define MAX_LHFD_INTERNEL_BUF_HEIGHT	(480)
#define MAX_LHFD_INTERNEL_BUF_SIZE \
	(MAX_LHFD_INTERNEL_BUF_WIDTH * MAX_LHFD_INTERNEL_BUF_HEIGHT)
#define SIZE_LHFD_INTERNEL_BUF \
	((MAX_LHFD_INTERNEL_BUF_SIZE * 45 / 4) + (4096 + 1024))
/*
 * FD one buffer size: 3.4 MB
 * FD_ map_data_1: MAX_FD_INTERNEL_BUF_SIZE * 3 / 2) byte
 * FD_ map_data_2: MAX_FD_INTERNEL_BUF_SIZE * 4) byte
 * FD_ map_data_3: MAX_FD_INTERNEL_BUF_SIZE * 4) byte
 * FD_ map_data_4: MAX_FD_INTERNEL_BUF_SIZE / 4) byte
 * FD_ map_data_5: MAX_FD_INTERNEL_BUF_SIZE) byte
 * FD_ map_data_6: 1024 byte
 * FD_ map_data_7: 256 byte
 */

#define SIZE_LHFD_SHOT_BUF		sizeof(struct camera2_shot)

#define MAX_LHFD_SHOT_BUF		(2)

#define NUM_VRA_INTERNAL_BUF		(1)
#define SIZE_VRA_INTERNEL_BUF		(0x00650000)

#define NUM_ODC_INTERNAL_BUF		(2)
#define NUM_DIS_INTERNAL_BUF		(1)
#define NUM_DNR_INTERNAL_BUF		(2)

#define GATE_IP_ISP			(0)
#define GATE_IP_DRC			(1)
#define GATE_IP_FD			(2)
#define GATE_IP_SCC			(3)
#define GATE_IP_SCP			(4)
#define GATE_IP_ODC			(0)
#define GATE_IP_DIS			(1)
#define GATE_IP_DNR			(2)

/* use sysfs for actuator */
#define INIT_MAX_SETTING					5

enum is_debug_device {
	IS_DEBUG_MAIN = 0,
	IS_DEBUG_EC,
	IS_DEBUG_SENSOR,
	IS_DEBUG_ISP,
	IS_DEBUG_DRC,
	IS_DEBUG_FD,
	IS_DEBUG_SDK,
	IS_DEBUG_SCALERC,
	IS_DEBUG_ODC,
	IS_DEBUG_DIS,
	IS_DEBUG_TDNR,
	IS_DEBUG_SCALERP
};

enum is_debug_target {
	IS_DEBUG_UART = 0,
	IS_DEBUG_MEMORY,
	IS_DEBUG_DCC3
};

enum is_secure_camera_type {
	IS_SECURE_CAMERA_IRIS = 1,
	IS_SECURE_CAMERA_FACE = 2,
};

enum is_front_input_entity {
	IS_FRONT_INPUT_NONE = 0,
	IS_FRONT_INPUT_SENSOR,
};

enum is_front_output_entity {
	IS_FRONT_OUTPUT_NONE = 0,
	IS_FRONT_OUTPUT_BACK,
	IS_FRONT_OUTPUT_BAYER,
	IS_FRONT_OUTPUT_SCALERC,
};

enum is_back_input_entity {
	IS_BACK_INPUT_NONE = 0,
	IS_BACK_INPUT_FRONT,
};

enum is_back_output_entity {
	IS_BACK_OUTPUT_NONE = 0,
	IS_BACK_OUTPUT_3DNR,
	IS_BACK_OUTPUT_SCALERP,
};

enum is_front_state {
	IS_FRONT_ST_POWERED = 0,
	IS_FRONT_ST_STREAMING,
	IS_FRONT_ST_SUSPENDED,
};

enum is_clck_gate_mode {
	CLOCK_GATE_MODE_HOST = 0,
	CLOCK_GATE_MODE_FW,
};

#if IS_ENABLED(CONFIG_SECURE_CAMERA_USE)
enum is_secure_state {
	IS_STATE_UNSECURE,
	IS_STATE_SECURING,
	IS_STATE_SECURED,
};
#endif

enum is_dual_mode {
	IS_DUAL_MODE_NOTHING,
	IS_DUAL_MODE_BYPASS,
	IS_DUAL_MODE_SYNC,
	IS_DUAL_MODE_OVERRUN,
	IS_DUAL_MODE_SWITCH,
};

enum is_hal_debug_mode {
	IS_HAL_DEBUG_SUDDEN_DEAD_DETECT,
	IS_HAL_DEBUG_PILE_REQ_BUF,
	IS_HAL_DEBUG_NDONE_REPROCESSING,
};

struct is_sysfs_debug {
	unsigned int en_dvfs;
	unsigned int pattern_en;
	unsigned int pattern_fps;
	unsigned long hal_debug_mode;
	unsigned int hal_debug_delay;
};

struct is_sysfs_actuator {
	unsigned int init_step;
	int init_positions[INIT_MAX_SETTING];
	int init_delays[INIT_MAX_SETTING];
	bool enable_fixed;
	int fixed_position;
};

struct is_dual_info {
	int pre_mode;
	int mode;
	int max_fps[SENSOR_POSITION_MAX];
	int tick_count;
	int max_bds_width;
	int max_bds_height;
};

#ifdef FIXED_SENSOR_DEBUG
struct is_sysfs_sensor {
	bool		is_en;
	bool		is_fps_en;
	unsigned int	frame_duration;
	unsigned int	long_exposure_time;
	unsigned int	short_exposure_time;
	unsigned int	long_analog_gain;
	unsigned int	short_analog_gain;
	unsigned int	long_digital_gain;
	unsigned int	short_digital_gain;
	unsigned int	set_fps;
	int		max_fps;
};
#endif

struct is_core {
	struct platform_device			*pdev;
	void __iomem				*regs;
	int					irq;
	u32					current_position;
	atomic_t				rsccount;
	unsigned long				state;
	bool					shutdown;
	struct is_sysfs_debug sysfs_debug;
	struct work_struct			wq_data_print_clk;

	/* depended on isp */
	struct exynos_platform_is		*pdata;

	struct is_resourcemgr		resourcemgr;
	struct is_groupmgr			groupmgr;
	struct is_devicemgr		devicemgr;
	struct is_interface		interface;

	struct mutex				sensor_lock;
	struct is_device_sensor		sensor[IS_SENSOR_COUNT];
	u32					chain_config;
	struct is_device_ischain		ischain[IS_STREAM_COUNT];

	struct is_hardware			hardware;
	struct is_interface_ischain	interface_ischain;

	struct v4l2_device			v4l2_dev;

	struct is_video			video_30s;
	struct is_video			video_30c;
	struct is_video			video_30p;
	struct is_video			video_30f;
	struct is_video			video_30g;
	struct is_video			video_30o;
	struct is_video			video_30l;
	struct is_video			video_31s;
	struct is_video			video_31c;
	struct is_video			video_31p;
	struct is_video			video_31f;
	struct is_video			video_31g;
	struct is_video			video_31o;
	struct is_video			video_31l;
	struct is_video			video_32s;
	struct is_video			video_32c;
	struct is_video			video_32p;
	struct is_video			video_32f;
	struct is_video			video_32g;
	struct is_video			video_32o;
	struct is_video			video_32l;
	struct is_video			video_33s;
	struct is_video			video_33c;
	struct is_video			video_33p;
	struct is_video			video_33f;
	struct is_video			video_33g;
	struct is_video			video_33o;
	struct is_video			video_33l;
	struct is_video			video_lme0;
	struct is_video			video_lme0s;
	struct is_video			video_lme0c;
	struct is_video			video_lme1;
	struct is_video			video_lme1s;
	struct is_video			video_lme1c;
	struct is_video			video_i0s;
	struct is_video			video_i0c;
	struct is_video			video_i0p;
	struct is_video			video_i0t;
	struct is_video			video_i0g;
	struct is_video			video_i0v;
	struct is_video			video_i0w;
	struct is_video			video_i1s;
	struct is_video			video_i1c;
	struct is_video			video_i1p;
	struct is_video			video_me0c;
	struct is_video			video_me1c;
	struct is_video			video_orb0c;
	struct is_video			video_orb1c;
	struct is_video			video_m0s;
	struct is_video			video_m1s;
	struct is_video			video_m0p;
	struct is_video			video_m1p;
	struct is_video			video_m2p;
	struct is_video			video_m3p;
	struct is_video			video_m4p;
	struct is_video			video_m5p;
	struct is_video			video_vra;
	struct is_video			video_paf0s;
	struct is_video			video_paf1s;
	struct is_video			video_paf2s;
	struct is_video			video_paf3s;
	struct is_video			video_cl0s;
	struct is_video			video_cl0c;
	struct is_video			video_ypp;

	/* spi */
	struct is_spi			spi0;
	struct is_spi			spi1;

	struct mutex				i2c_lock[SENSOR_CONTROL_I2C_MAX];
	atomic_t				i2c_rsccount[SENSOR_CONTROL_I2C_MAX];

	spinlock_t				shared_rsc_slock[MAX_SENSOR_SHARED_RSC];
	atomic_t				shared_rsc_count[MAX_SENSOR_SHARED_RSC];

	struct is_vender			vender;
#if IS_ENABLED(CONFIG_SECURE_CAMERA_USE)
	struct mutex				secure_state_lock;
	unsigned long				secure_state;
#endif
	ulong					secure_mem_info[2];	/* size, addr */
	ulong					non_secure_mem_info[2];	/* size, addr */
	u32					scenario;

	unsigned long                           sensor_map;
	struct is_dual_info		dual_info;
	struct mutex				ois_mode_lock;
	struct ois_mcu_dev			*mcu;
	struct mutex				laser_lock;
	atomic_t					laser_refcount;
#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
	/* structure for sysevent framework */
	struct sysevent_desc		sysevent_desc;
	struct sysevent_device		*sysevent_dev;
#endif
};

int is_secure_func(struct is_core *core,
	struct is_device_sensor *device, u32 type, u32 scenario, ulong smc_cmd);
struct is_device_sensor *is_get_sensor_device(struct is_core *core);
int is_put_sensor_device(struct is_core *core);
struct is_device_ischain *is_get_ischain_device(u32 instance);
void is_print_frame_dva(struct is_subdev *subdev);
void is_cleanup(struct is_core *core);

#define CALL_POPS(s, op, args...) (((s) && (s)->pdata && (s)->pdata->op) ? ((s)->pdata->op(&(s)->pdev->dev)) : -EPERM)

#endif /* IS_CORE_H_ */
