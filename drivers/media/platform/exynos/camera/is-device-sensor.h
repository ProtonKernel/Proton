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

#ifndef IS_DEVICE_SENSOR_H
#define IS_DEVICE_SENSOR_H

#include <dt-bindings/camera/exynos_is_dt.h>

#include <exynos-is-sensor.h>
#include "is-mem.h"
#include "is-video.h"
#include "is-resourcemgr.h"
#include "is-groupmgr.h"
#include "is-device-csi.h"
#include "is-helper-i2c.h"

struct is_video_ctx;
struct is_device_ischain;

#define EXPECT_FRAME_START	0
#define EXPECT_FRAME_END	1
#define LOG_INTERVAL_OF_DROPS 30

#define FLITE_NOTIFY_FSTART		0
#define FLITE_NOTIFY_FEND		1
#define FLITE_NOTIFY_DMA_END		2
#define CSIS_NOTIFY_FSTART		3
#define CSIS_NOTIFY_FEND		4
#define CSIS_NOTIFY_DMA_END		5
#define CSIS_NOTIFY_DMA_END_VC_EMBEDDED	6
#define CSIS_NOTIFY_DMA_END_VC_MIPISTAT	7
#define CSIS_NOTIFY_LINE		8

#define SENSOR_MAX_ENUM			4 /* 4 modules(2 rear, 2 front) for same csis */
#define ACTUATOR_MAX_ENUM		IS_SENSOR_COUNT
#define SENSOR_DEFAULT_FRAMERATE	30

#define SENSOR_MODE_MASK		0xFFFF0000
#define SENSOR_MODE_SHIFT		16
#define SENSOR_MODE_DEINIT		0xFFFF

#define SENSOR_SCENARIO_MASK		0xF0000000
#define SENSOR_SCENARIO_SHIFT		28
#define ASB_SCENARIO_MASK		0xF000
#define ASB_SCENARIO_SHIFT        	12
#define SENSOR_MODULE_MASK		0x00000FFF
#define SENSOR_MODULE_SHIFT		0

#define SENSOR_SSTREAM_MASK		0x0000000F
#define SENSOR_SSTREAM_SHIFT		0
#define SENSOR_USE_STANDBY_MASK		0x000000F0
#define SENSOR_USE_STANDBY_SHIFT	4
#define SENSOR_INSTANT_MASK		0x0FFF0000
#define SENSOR_INSTANT_SHIFT		16
#define SENSOR_NOBLOCK_MASK		0xF0000000
#define SENSOR_NOBLOCK_SHIFT		28

#define SENSOR_I2C_CH_MASK		0xFF
#define SENSOR_I2C_CH_SHIFT		0
#define ACTUATOR_I2C_CH_MASK		0xFF00
#define ACTUATOR_I2C_CH_SHIFT		8
#define OIS_I2C_CH_MASK 		0xFF0000
#define OIS_I2C_CH_SHIFT		16

#define SENSOR_I2C_ADDR_MASK		0xFF
#define SENSOR_I2C_ADDR_SHIFT		0
#define ACTUATOR_I2C_ADDR_MASK		0xFF00
#define ACTUATOR_I2C_ADDR_SHIFT		8
#define OIS_I2C_ADDR_MASK		0xFF0000
#define OIS_I2C_ADDR_SHIFT		16

#define SENSOR_OTP_PAGE			256
#define SENSOR_OTP_PAGE_SIZE		64

#define SENSOR_SIZE_WIDTH_MASK		0xFFFF0000
#define SENSOR_SIZE_WIDTH_SHIFT		16
#define SENSOR_SIZE_HEIGHT_MASK		0xFFFF
#define SENSOR_SIZE_HEIGHT_SHIFT	0

#define IS_TIMESTAMP_HASH_KEY	20

#define IS_SENSOR_CFG(w, h, f, s, m, l, ls, itlv, pd,			\
	vc0_in, vc0_out, vc1_in, vc1_out, vc2_in, vc2_out, vc3_in, vc3_out) {	\
	.width				= w,					\
	.height				= h,					\
	.framerate			= f,					\
	.settle				= s,					\
	.mode				= m,					\
	.lanes				= l,					\
	.mipi_speed			= ls,					\
	.interleave_mode		= itlv,					\
	.lrte				= LRTE_DISABLE,				\
	.pd_mode			= pd,					\
	.ex_mode			= EX_NONE,					\
	.input[CSI_VIRTUAL_CH_0]	= vc0_in,				\
	.output[CSI_VIRTUAL_CH_0]	= vc0_out,				\
	.input[CSI_VIRTUAL_CH_1]	= vc1_in,				\
	.output[CSI_VIRTUAL_CH_1]	= vc1_out,				\
	.input[CSI_VIRTUAL_CH_2]	= vc2_in,				\
	.output[CSI_VIRTUAL_CH_2]	= vc2_out,				\
	.input[CSI_VIRTUAL_CH_3]	= vc3_in,				\
	.output[CSI_VIRTUAL_CH_3]	= vc3_out,				\
}

#define IS_SENSOR_CFG_EX(w, h, f, s, m, l, ls, itlv, pd, ex,			\
	vc0_in, vc0_out, vc1_in, vc1_out, vc2_in, vc2_out, vc3_in, vc3_out) {	\
	.width				= w,					\
	.height				= h,					\
	.framerate			= f,					\
	.settle				= s,					\
	.mode				= m,					\
	.lanes				= l,					\
	.mipi_speed			= ls,					\
	.interleave_mode		= itlv,					\
	.lrte				= LRTE_DISABLE,				\
	.pd_mode			= pd,					\
	.ex_mode			= ex,					\
	.input[CSI_VIRTUAL_CH_0]	= vc0_in,				\
	.output[CSI_VIRTUAL_CH_0]	= vc0_out,				\
	.input[CSI_VIRTUAL_CH_1]	= vc1_in,				\
	.output[CSI_VIRTUAL_CH_1]	= vc1_out,				\
	.input[CSI_VIRTUAL_CH_2]	= vc2_in,				\
	.output[CSI_VIRTUAL_CH_2]	= vc2_out,				\
	.input[CSI_VIRTUAL_CH_3]	= vc3_in,				\
	.output[CSI_VIRTUAL_CH_3]	= vc3_out,				\
}

/*
 * @map:	VC parsing info.
 *		This is determined by sensor output format.
 * @hwformat:	DT parsing info.
 *		This is determined by sensor output format.
 * @width:	Real sensor output. For example, 2PD width is twice of DMA ouput width.
 *		If there is not VC output of sensor, set "0".
 * @heith:	Real sensor output.
 *		If there is not VC output of sensor, set "0".
 */
#define VC_IN(m, hwf, w, h) {			\
	.map = m,				\
	.hwformat = hwf,			\
	.width = w,				\
	.height = h,				\
}

/*
 * @hwformat:	It is same value width input hwformat, when input & output is same.
 *		But when the PDP is used, input & ouput is differnet.
 * @type:	It is used only for internal VC. There are VC_TAILPDAF, VC_MIPISTAT.
 *		If the buffer is used by HAL, set VC_NOTHING.
 * @width:	It is used only for internal VC. It is VC data size.
 *		If the buffer is used by HAL, set "0".
 * @height:	It is used only for internal VC. It is VC data size.
 *		If the buffer is used by HAL, set "0".
 */
#define VC_OUT(hwf, t, w, h) {			\
	.hwformat = hwf,			\
	.type = t,				\
	.width = w,				\
	.height = h,				\
}

enum is_sensor_subdev_ioctl {
	SENSOR_IOCTL_DMA_CANCEL,
	SENSOR_IOCTL_PATTERN_ENABLE,
	SENSOR_IOCTL_PATTERN_DISABLE,
	SENSOR_IOCTL_REGISTE_VOTF,
	SENSOR_IOCTL_G_FRAME_ID,
	SENSOR_IOCTL_G_HW_FCOUNT,
	SENSOR_IOCTL_DISABLE_VC1_VOTF,
	SENSOR_IOCTL_CSI_DMA_DISABLE,
	SENSOR_IOCTL_CSI_DMA_ATTACH,
	SENSOR_IOCTL_G_DMA_CH,

	SENSOR_IOCTL_CSI_S_CTRL,
	SENSOR_IOCTL_CSI_G_CTRL,
	SENSOR_IOCTL_MOD_S_CTRL,
	SENSOR_IOCTL_MOD_G_CTRL,
	SENSOR_IOCTL_MOD_S_EXT_CTRL,
	SENSOR_IOCTL_MOD_G_EXT_CTRL,
	SENSOR_IOCTL_ACT_S_CTRL,
	SENSOR_IOCTL_ACT_G_CTRL,
	SENSOR_IOCTL_FLS_S_CTRL,
	SENSOR_IOCTL_FLS_G_CTRL,
};

#if defined(SECURE_CAMERA_IRIS)
enum is_sensor_smc_state {
        IS_SENSOR_SMC_INIT = 0,
        IS_SENSOR_SMC_CFW_ENABLE,
        IS_SENSOR_SMC_PREPARE,
        IS_SENSOR_SMC_UNPREPARE,
};
#endif

enum is_sensor_output_entity {
	IS_SENSOR_OUTPUT_NONE = 0,
	IS_SENSOR_OUTPUT_FRONT,
};

enum is_sensor_force_stop {
	IS_BAD_FRAME_STOP = 0,
	IS_MIF_THROTTLING_STOP = 1,
	IS_FLITE_OVERFLOW_STOP = 2,
};

enum is_module_state {
	IS_MODULE_GPIO_ON,
	IS_MODULE_STANDBY_ON
};

enum is_img_pd_ratio {
	IS_IMG_PD_RATIO_1_1 = 1,
};

struct is_fid_loc {
	u32 valid;
	u32 line;
	u32 byte;
};

struct is_sensor_cfg {
	u32 width;
	u32 height;
	u32 framerate;
	u32 max_fps;
	u32 settle;
	u32 mode;
	u32 lanes;
	u32 mipi_speed;
	u32 interleave_mode;
	u32 lrte;
	u32 pd_mode;
	u32 ex_mode;
	u32 votf;
	u32 vvalid_time; /* unit(us) */
	u32 req_vvalid_time; /* unit(us) */
	unsigned int wdma_ch_hint; /* Designated CSI WDMA ch for special function */
	u32 binning; /* binning ratio */
	int special_mode;
	enum is_img_pd_ratio img_pd_ratio;
	u32 dummy_pixel[CSI_VIRTUAL_CH_MAX];
	struct is_vci_config input[CSI_VIRTUAL_CH_MAX];
	struct is_vci_config output[CSI_VIRTUAL_CH_MAX];
	struct is_fid_loc fid_loc;
};


struct is_sensor_vc_extra_info {
	int stat_type;
	int sensor_mode;
	u32 max_width;
	u32 max_height;
	u32 max_element;
};

struct is_sensor_ops {
	int (*stream_on)(struct v4l2_subdev *subdev);
	int (*stream_off)(struct v4l2_subdev *subdev);

	int (*s_duration)(struct v4l2_subdev *subdev, u64 duration);
	int (*g_min_duration)(struct v4l2_subdev *subdev);
	int (*g_max_duration)(struct v4l2_subdev *subdev);

	int (*s_exposure)(struct v4l2_subdev *subdev, u64 exposure);
	int (*g_min_exposure)(struct v4l2_subdev *subdev);
	int (*g_max_exposure)(struct v4l2_subdev *subdev);

	int (*s_again)(struct v4l2_subdev *subdev, u64 sensivity);
	int (*g_min_again)(struct v4l2_subdev *subdev);
	int (*g_max_again)(struct v4l2_subdev *subdev);

	int (*s_dgain)(struct v4l2_subdev *subdev);
	int (*g_min_dgain)(struct v4l2_subdev *subdev);
	int (*g_max_dgain)(struct v4l2_subdev *subdev);

	int (*s_shutterspeed)(struct v4l2_subdev *subdev, u64 shutterspeed);
	int (*g_min_shutterspeed)(struct v4l2_subdev *subdev);
	int (*g_max_shutterspeed)(struct v4l2_subdev *subdev);
};

struct is_module_enum {
	u32						instance;	/* logical stream id */
	u32						sensor_id;	/* physical module enum */
	u32						device;		/* physical sensor node id: for searching for sensor device */

	struct v4l2_subdev				*subdev; /* connected module subdevice */
	unsigned long					state;
	u32						pixel_width;
	u32						pixel_height;
	u32						active_width;
	u32						active_height;
	u32                                             margin_left;
	u32                                             margin_right;
	u32                                             margin_top;
	u32                                             margin_bottom;
	u32						max_framerate;
	u32						position;
	u32						bitwidth;
	u32						cfgs;
	u64						act_available_time;
	struct is_sensor_cfg			*cfg;
	struct is_sensor_vc_extra_info		vc_extra_info[VC_BUF_DATA_TYPE_MAX];
	struct i2c_client				*client;
	struct sensor_open_extended			ext;
	struct is_sensor_ops			*ops;
	char						*sensor_maker;
	char						*sensor_name;
	char						*setfile_name;
	struct hrtimer					vsync_timer;
	struct work_struct				vsync_work;
	void						*private_data;
	struct exynos_platform_is_module		*pdata;
	struct device					*dev;
};

enum is_sensor_state {
	IS_SENSOR_PROBE,
	IS_SENSOR_OPEN,
	IS_SENSOR_MCLK_ON,
	IS_SENSOR_ICLK_ON,
	IS_SENSOR_GPIO_ON,
	IS_SENSOR_S_INPUT,
	IS_SENSOR_S_CONFIG,	/* Deprecated: not use */
	IS_SENSOR_DRIVING,		/* Deprecated: from device-sensor_v2 */
	IS_SENSOR_STAND_ALONE,	/* SOC sensor, Iris sensor, Vision mode without IS chain */
	IS_SENSOR_FRONT_START,
	IS_SENSOR_FRONT_DTP_STOP,
	IS_SENSOR_BACK_START,
	IS_SENSOR_BACK_NOWAIT_STOP,	/* Deprecated: There is no special meaning from device-sensor_v2 */
	IS_SENSOR_SUBDEV_MODULE_INIT,	/* Deprecated: from device-sensor_v2 */
	IS_SENSOR_OTF_OUTPUT,
	IS_SENSOR_ITF_REGISTER,	/* to check whether sensor interfaces are registered */
	IS_SENSOR_WAIT_STREAMING,
	SENSOR_MODULE_GOT_INTO_TROUBLE,
	IS_SENSOR_RUNTIME_MODULE_SELECTED,
	IS_SENSOR_I2C_DUMMY_MODULE_SELECTED,
	IS_SENSOR_ESD_RECOVERY,
};

enum sensor_subdev_internel_use {
	SUBDEV_SSVC0_INTERNAL_USE,
	SUBDEV_SSVC1_INTERNAL_USE,
	SUBDEV_SSVC2_INTERNAL_USE,
	SUBDEV_SSVC3_INTERNAL_USE,
};

/*
 * Cal data status
 * [0]: NO ERROR
 * [1]: CRC FAIL
 * [2]: LIMIT FAIL
 * => Check AWB out of the ratio EEPROM/OTP data
 */

enum is_sensor_cal_status {
	CRC_NO_ERROR = 0,
	CRC_ERROR,
	LIMIT_FAILURE,
};

struct is_device_sensor {
	u32						instance;	/* logical stream id: decide at open time*/
	u32						device_id;	/* physical sensor node id: it is decided at probe time */
	u32						sensor_id;	/* physical module enum */
	u32						position;

	struct v4l2_device				v4l2_dev;
	struct platform_device				*pdev;
	struct is_mem					mem;

	struct is_image					image;

	struct is_video_ctx				*vctx;
	struct is_video					video;

	struct is_device_ischain   			*ischain;
	struct is_groupmgr				*groupmgr;
	struct is_resourcemgr				*resourcemgr;
	struct is_devicemgr				*devicemgr;
	struct is_module_enum				module_enum[SENSOR_MAX_ENUM];
	struct is_sensor_cfg				*cfg;

	/* current control value */
	struct camera2_sensor_ctl			sensor_ctl;
	struct camera2_lens_ctl				lens_ctl;
	struct camera2_flash_ctl			flash_ctl;
	u64						timestamp[IS_TIMESTAMP_HASH_KEY];
	u64						timestampboot[IS_TIMESTAMP_HASH_KEY];
	u64						frame_id[IS_TIMESTAMP_HASH_KEY]; /* index 0 ~ 7 */
	u64						prev_timestampboot;

	u32						fcount;
	u32						line_fcount;
	u32						instant_cnt;
	int						instant_ret;
	wait_queue_head_t				instant_wait;
	struct work_struct				instant_work;
	unsigned long					state;
	spinlock_t					slock_state;
	struct mutex					mlock_state;
	atomic_t					group_open_cnt;
#if defined(SECURE_CAMERA_IRIS)
	enum is_sensor_smc_state			smc_state;
#endif

	/* hardware configuration */
	struct v4l2_subdev				*subdev_module;
	struct v4l2_subdev				*subdev_csi;

	/* sensor dma video node */
	struct is_video					video_ssxvc0;
	struct is_video					video_ssxvc1;
	struct is_video					video_ssxvc2;
	struct is_video					video_ssxvc3;

	/* subdev for dma */
	struct is_subdev				ssvc0;
	struct is_subdev				ssvc1;
	struct is_subdev				ssvc2;
	struct is_subdev				ssvc3;
	struct is_subdev				bns;

	/* gain boost */
	int						min_target_fps;
	int						max_target_fps;
	int						scene_mode;

	/* for vision control */
	int						exposure_time;
	u64						frame_duration;

	/* ENABLE_DTP */
	bool						dtp_check;
	struct timer_list				dtp_timer;
	unsigned long					force_stop;

	/* for early buffer done */
	u32						early_buf_done_mode;
	struct hrtimer					early_buf_timer;

	struct exynos_platform_is_sensor		*pdata;
	atomic_t					module_count;
	struct v4l2_subdev 				*subdev_actuator[ACTUATOR_MAX_ENUM];
	struct is_actuator				*actuator[ACTUATOR_MAX_ENUM];
	struct v4l2_subdev				*subdev_flash;
	struct is_flash					*flash;
	struct v4l2_subdev				*subdev_ois;
	struct is_ois					*ois;
	struct v4l2_subdev				*subdev_mcu;
	struct is_mcu					*mcu;
	struct v4l2_subdev				*subdev_aperture;
	struct is_aperture				*aperture;
	struct v4l2_subdev				*subdev_eeprom;
	struct is_eeprom				*eeprom;
	struct v4l2_subdev				*subdev_laser_af;
	struct is_laser_af				*laser_af;
	void						*private_data;
	struct is_group					group_sensor;
	struct is_path_info				path;

	u32						sensor_width;
	u32						sensor_height;

	int						num_of_ch_mode;
	bool						dma_abstract;
	u32						use_standby;
	u32						sstream;
	u32						ex_mode;
	u32						ex_mode_option;
	u32						ex_scenario;

#ifdef ENABLE_INIT_AWB
	/* backup AWB gains for use initial gain */
	float						init_wb[WB_GAIN_COUNT];
	float						last_wb[WB_GAIN_COUNT];
	float						chk_wb[WB_GAIN_COUNT];
	u32						init_wb_cnt;
#endif

#ifdef ENABLE_MODECHANGE_CAPTURE
	struct is_frame					*mode_chg_frame;
#endif

	bool					use_otp_cal;
	u32					cal_status[CAMERA_CRC_INDEX_MAX];
	u8					otp_cal_buf[SENSOR_OTP_PAGE][SENSOR_OTP_PAGE_SIZE];

	struct i2c_client			*client;
	struct mutex				mutex_reboot;
	bool					reboot;

	bool					use_stripe_flag;
};

int is_sensor_open(struct is_device_sensor *device,
	struct is_video_ctx *vctx);
int is_sensor_close(struct is_device_sensor *device);
#ifdef CONFIG_USE_SENSOR_GROUP
int is_sensor_s_input(struct is_device_sensor *device,
	u32 input,
	u32 scenario,
	u32 video_id,
	u32 stream_leader);
#else
int is_sensor_s_input(struct is_device_sensor *device,
	u32 input,
	u32 scenario);
#endif
int is_sensor_s_ctrl(struct is_device_sensor *device,
	struct v4l2_control *ctrl);
int is_sensor_s_ext_ctrls(struct is_device_sensor *device,
	struct v4l2_ext_controls *ctrls);
int is_sensor_subdev_buffer_queue(struct is_device_sensor *device,
	enum is_subdev_id subdev_id,
	u32 index);
int is_sensor_buffer_queue(struct is_device_sensor *device,
	struct is_queue *queue,
	u32 index);
int is_sensor_buffer_finish(struct is_device_sensor *device,
	u32 index);

int is_sensor_front_start(struct is_device_sensor *device,
	u32 instant_cnt,
	u32 nonblock);
int is_sensor_front_stop(struct is_device_sensor *device, bool fstop);
void is_sensor_group_force_stop(struct is_device_sensor *device, u32 group_id);

int is_sensor_s_framerate(struct is_device_sensor *device,
	struct v4l2_streamparm *param);
int is_sensor_s_bns(struct is_device_sensor *device,
	u32 reatio);

int is_sensor_s_frame_duration(struct is_device_sensor *device,
	u32 frame_duration);
int is_sensor_s_exposure_time(struct is_device_sensor *device,
	u32 exposure_time);
int is_sensor_s_fcount(struct is_device_sensor *device);
int is_sensor_s_again(struct is_device_sensor *device, u32 gain);
int is_sensor_s_shutterspeed(struct is_device_sensor *device, u32 shutterspeed);

struct is_sensor_cfg * is_sensor_g_mode(struct is_device_sensor *device);
int is_sensor_mclk_on(struct is_device_sensor *device, u32 scenario, u32 channel);
int is_sensor_mclk_off(struct is_device_sensor *device, u32 scenario, u32 channel);
int is_sensor_gpio_on(struct is_device_sensor *device);
int is_sensor_gpio_off(struct is_device_sensor *device);
int is_sensor_gpio_dbg(struct is_device_sensor *device);
void is_sensor_dump(struct is_device_sensor *device);

int is_sensor_g_ctrl(struct is_device_sensor *device,
	struct v4l2_control *ctrl);
int is_sensor_g_instance(struct is_device_sensor *device);
int is_sensor_g_ex_mode(struct is_device_sensor *device);
int is_sensor_g_framerate(struct is_device_sensor *device);
int is_sensor_g_fcount(struct is_device_sensor *device);
int is_sensor_g_width(struct is_device_sensor *device);
int is_sensor_g_height(struct is_device_sensor *device);
int is_sensor_g_bns_width(struct is_device_sensor *device);
int is_sensor_g_bns_height(struct is_device_sensor *device);
int is_sensor_g_bns_ratio(struct is_device_sensor *device);
int is_sensor_g_bratio(struct is_device_sensor *device);
int is_sensor_g_module(struct is_device_sensor *device,
	struct is_module_enum **module);
int is_sensor_deinit_module(struct is_module_enum *module);
int is_sensor_g_position(struct is_device_sensor *device);
int is_sensor_g_fast_mode(struct is_device_sensor *device);
int is_search_sensor_module_with_sensorid(struct is_device_sensor *device,
	u32 sensor_id, struct is_module_enum **module);
int is_search_sensor_module_with_position(struct is_device_sensor *device,
	u32 position, struct is_module_enum **module);
int is_sensor_votf_tag(struct is_device_sensor *device, struct is_subdev *subdev);
int is_sensor_dm_tag(struct is_device_sensor *device,
	struct is_frame *frame);
int is_sensor_buf_tag(struct is_device_sensor *device,
	struct is_subdev *f_subdev,
	struct v4l2_subdev *v_subdev,
	struct is_frame *ldr_frame);
int is_sensor_g_csis_error(struct is_device_sensor *device);
int is_sensor_register_itf(struct is_device_sensor *device);
int is_sensor_group_tag(struct is_device_sensor *device,
	struct is_frame *frame,
	struct camera2_node *ldr_node);
int is_sensor_dma_cancel(struct is_device_sensor *device);
extern const struct is_queue_ops is_sensor_ops;
extern const struct is_queue_ops is_sensor_subdev_ops;
int is_sensor_map_sensor_module(struct is_device_ischain *device, int position,
		int *sensor_id);
void is_sensor_g_max_size(u32 *max_width, u32 *max_height);
#define CALL_MOPS(s, op, args...) (((s)->ops->op) ? ((s)->ops->op(args)) : 0)

#define VOTF_BACK_FIRST_OFF_COND(group, cfg)			\
	((IS_ENABLED(VOTF_BACK_FIRST_OFF))			\
	&& (!IS_ENABLED(CONFIG_VOTF_ONESHOT))			\
	&& (test_bit(IS_GROUP_VOTF_OUTPUT, &group->state))	\
	&& (!CHECK_SDC_EN(cfg->input[CSI_VIRTUAL_CH_0].extformat)))

#endif
