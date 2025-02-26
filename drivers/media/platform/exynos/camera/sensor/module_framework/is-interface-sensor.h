/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_SENSOR_INTERFACE_H
#define IS_SENSOR_INTERFACE_H

#include "is-core.h"
#include "is-mem.h"
#include "is-config.h"
#include "exynos-is-sensor.h"
#include "is-metadata.h"
#include "is-binary.h"
#include "is-device-sensor.h"

#define SENSOR_INTERFACE_MAGIC 0xDCBA98FE

#define NUM_OF_DUMMY_FRAME		(3) /* N + 2 Frame delay */
#define NUM_FRAMES_DMA			(NUM_OF_DUMMY_FRAME + 2 + 1)
#define NEXT_NEXT_FRAME_DMA		(NUM_OF_DUMMY_FRAME + 2)

/* This OFFSET is set to 0 when enable 3AA 3FRAME DELAY, set to 1 in other cases */
#define OTF_VSYNC_3AA_FRAME_DELAY_OFFSET	0

#define NUM_FRAMES_OTF			(NEXT_NEXT_FRAME_OTF + 1)
#define NEXT_NEXT_FRAME_OTF		(2 + OTF_VSYNC_3AA_FRAME_DELAY_OFFSET)

#define NEXT_FRAME			1
#define CURRENT_FRAME			0

#define NUM_FRAMES			(NUM_OF_DUMMY_FRAME + 2 + 1)

#define EXPOSURE_GAIN_INDEX		0
#define LONG_EXPOSURE_GAIN_INDEX	1
#define SHORT_EXPOSURE_GAIN_INDEX	2
#define MAX_EXPOSURE_GAIN_PER_FRAME	3

#define ACTUATOR_MAX_SOFT_LANDING_NUM	32 /* Actuator interface */
#define ACTUATOR_MAX_FOCUS_POSITIONS	4096

#define	INVALID_LASER_DISTANCE	0

#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define MAX(a,b) (((a) > (b)) ? (a) : (b))

#define FPS_TO_DURATION_US(x)  ((x == 0) ? (0) : ((1000 * 1000) / x))
#define DURATION_US_TO_FPS(x)  ((x == 0) ? (0) : ((1000 * 1000) / x))

/* static memory size for DDK/RTA backup data */
#define STATIC_DATA_SIZE	200

#define MAX_ACT_POSITION	4095

enum DIFF_BET_SEN_ISP { /* Set to 0: 3AA 3frame delay, 1: 3AA 4frame delay, 3: M2M */
	DIFF_OTF_DELAY	= 0,
	DIFF_M2M_DELAY	= 3
};

enum SENSOR_CONTROL_DELAY {
	N_PLUS_TWO_FRAME = 0,
	N_PLUS_ONE_FRAME = 1,
};

enum {
	ITF_CTRL_ID_DDK = 0,
	ITF_CTRL_ID_RTA = 1,
};

/* DEVICE SENSOR INTERFACE */
#define SENSOR_REGISTER_FUNC_ADDR	(DDK_LIB_ADDR + 0x40)
#define SENSOR_REGISTER_FUNC_ADDR_RTA	(RTA_LIB_ADDR + 0x40)

typedef int (*register_sensor_interface)(void *itf);

struct ae_param {
	union {
		u32 val;
		u32 long_val;
	};
	u32 short_val;
	u32 middle_val;
};

typedef struct {
	/* Normal parameter */
	/* Each CB has a specific parameter type.(Ex. SetSize Cb has size info param.) */
	void *param;
	unsigned int return_value;
} cis_setting_info;

enum itf_vc_stat_type {
	VC_STAT_TYPE_INVALID = -1,

	/* Types for SW PDAF(tail mode buffer type) */
	VC_STAT_TYPE_TAIL_FOR_SW_PDAF = 100,

	/* Types for IMX PDAF sensors */
	VC_STAT_TYPE_IMX_FLEXIBLE = 200,
	VC_STAT_TYPE_IMX_STATIC,

	/* Types for PAF_STAT */
	VC_STAT_TYPE_PAFSTAT_FLOATING = 300,
	VC_STAT_TYPE_PAFSTAT_STATIC,

	/* Types for PDP 1.0 in Lhotse/Makalu EVT0 */
	VC_STAT_TYPE_OLD_PDP_1_0_PDAF_STAT0 = 400,
	VC_STAT_TYPE_OLD_PDP_1_0_PDAF_STAT1,

	/* Types for PDP 1.1 in Makalu EVT1 */
	VC_STAT_TYPE_OLD_PDP_1_1_PDAF_STAT0 = 500,
	VC_STAT_TYPE_OLD_PDP_1_1_PDAF_STAT1,

	/* Types for 3HDR */
	VC_STAT_TYPE_TAIL_FOR_3HDR_LSI = 600,
	VC_STAT_TYPE_TAIL_FOR_3HDR_IMX,
	VC_STAT_TYPE_TAIL_FOR_3HDR_IMX_2_STAT0,
	VC_STAT_TYPE_TAIL_FOR_3HDR_IMX_2_STAT1,

	/* Types for PDP 1.0 in 2020 EVT0 */
	VC_STAT_TYPE_PDP_1_0_PDAF_STAT0 = 700,
	VC_STAT_TYPE_PDP_1_0_PDAF_STAT1,

	/* Types for PDP 1.1 in 2020 EVT1 */
	VC_STAT_TYPE_PDP_1_1_PDAF_STAT0 = 800,
	VC_STAT_TYPE_PDP_1_1_PDAF_STAT1,

	/* Types for PDP 3.0 in Olympus EVT0 */
	VC_STAT_TYPE_PDP_3_0_PDAF_STAT0 = 900,
	VC_STAT_TYPE_PDP_3_0_PDAF_STAT1,
	VC_STAT_TYPE_PDP_3_0_PDAF_PD_DUMP,

	/* Types for PDP 3.1 in Olympus EVT1 */
	VC_STAT_TYPE_PDP_3_1_PDAF_STAT0 = 1000,
	VC_STAT_TYPE_PDP_3_1_PDAF_STAT1,
	VC_STAT_TYPE_PDP_3_1_PDAF_PD_DUMP,
};

enum itf_vc_sensor_mode {
	VC_SENSOR_MODE_INVALID = -1,

	/* 2PD */
	VC_SENSOR_MODE_2PD_MODE1 = 100,
	VC_SENSOR_MODE_2PD_MODE2,
	VC_SENSOR_MODE_2PD_MODE3,
	VC_SENSOR_MODE_2PD_MODE4,
	VC_SENSOR_MODE_2PD_MODE1_HDR,
	VC_SENSOR_MODE_2PD_MODE2_HDR,
	VC_SENSOR_MODE_2PD_MODE3_HDR,
	VC_SENSOR_MODE_2PD_MODE4_HDR,

	/* MSPD */
	VC_SENSOR_MODE_MSPD_NORMAL = 200,
	VC_SENSOR_MODE_MSPD_TAIL,
	VC_SENSOR_MODE_MSPD_GLOBAL_NORMAL,
	VC_SENSOR_MODE_MSPD_GLOBAL_TAIL,

	/* Ultra PD */
	VC_SENSOR_MODE_ULTRA_PD_NORMAL = 300,
	VC_SENSOR_MODE_ULTRA_PD_TAIL,
	VC_SENSOR_MODE_ULTRA_PD_2_NORMAL,
	VC_SENSOR_MODE_ULTRA_PD_2_TAIL,
	VC_SENSOR_MODE_ULTRA_PD_3_NORMAL,
	VC_SENSOR_MODE_ULTRA_PD_3_TAIL,

	/* Super PD */
	VC_SENSOR_MODE_SUPER_PD_NORMAL = 400,
	VC_SENSOR_MODE_SUPER_PD_TAIL,
	VC_SENSOR_MODE_SUPER_PD_2_NORMAL,
	VC_SENSOR_MODE_SUPER_PD_2_TAIL,
	VC_SENSOR_MODE_SUPER_PD_3_NORMAL,
	VC_SENSOR_MODE_SUPER_PD_3_TAIL,        // GW2(Hubble X1/X2/Z3), GH1(Hubble Z3). LLLLL RRRRR
	VC_SENSOR_MODE_SUPER_PD_4_NORMAL_FULL,
	VC_SENSOR_MODE_SUPER_PD_4_NORMAL_3BIN,
	VC_SENSOR_MODE_SUPER_PD_4_NORMAL_6BIN,
	VC_SENSOR_MODE_SUPER_PD_4_TAIL,        // HM1(Hubble Z3). LRLRLRLRLRLRLRLR
	VC_SENSOR_MODE_SUPER_PD_5_NORMAL_FULL,
	VC_SENSOR_MODE_SUPER_PD_5_NORMAL_2BIN,
	VC_SENSOR_MODE_SUPER_PD_5_NORMAL_4BIN,
	VC_SENSOR_MODE_SUPER_PD_5_TAIL,        /* GM2(PD1962). LLLLL RRRRR */

	/* IMX PDAF */
	VC_SENSOR_MODE_IMX_PDAF = 500,
	VC_SENSOR_MODE_IMX_2X1OCL_1_NORMAL,	/* IMX576. One line: LLLLLLLLRRRRRRRR */
	VC_SENSOR_MODE_IMX_2X1OCL_1_TAIL,	/* IMX576. One line: LLLLLLLLRRRRRRRR */
	VC_SENSOR_MODE_IMX_2X1OCL_2_NORMAL,	/* IMX586. One line: LRLRLRLRLRLRLRLR */
	VC_SENSOR_MODE_IMX_2X1OCL_2_TAIL,	/* IMX586. One line: LRLRLRLRLRLRLRLR */

	/* 3HDR */
	VC_SENSOR_MODE_3HDR_LSI = 600,
	VC_SENSOR_MODE_3HDR_IMX,
	VC_SENSOR_MODE_3HDR_IMX_2,

	/* OV SENSOR */
	VC_SENSOR_MODE_OV_PDAF_TYPE1 = 700,
};

struct vc_buf_info_t {
	enum itf_vc_stat_type	stat_type;
	enum itf_vc_sensor_mode sensor_mode;
	u32			width;
	u32			height;
	u32			element_size;
};

typedef struct {
	unsigned int exposure;
	unsigned int analog_gain;
	unsigned int digital_gain;
	unsigned int long_exposure;
	unsigned int long_analog_gain;
	unsigned int long_digital_gain;
	unsigned int short_exposure;
	unsigned int short_analog_gain;
	unsigned int short_digital_gain;
	unsigned int middle_exposure;
	unsigned int middle_analog_gain;
	unsigned int middle_digital_gain;
} ae_setting;

typedef struct {
	unsigned int long_exposure_coarse;
	unsigned int long_exposure_fine;
	unsigned int long_exposure_analog_gain;
	unsigned int long_exposure_digital_gain;
	unsigned int long_exposure_companion_digital_gain;
	unsigned int short_exposure_coarse;
	unsigned int short_exposure_fine;
	unsigned int short_exposure_analog_gain;
	unsigned int short_exposure_digital_gain;
	unsigned int short_exposure_companion_digital_gain;
} preprocessor_ae_setting;

typedef struct {
	bool stream_on;

	unsigned int config_idx;
	bool bypass;
	bool paf_stat_enable;
	bool caf_stat_enable;
	bool wdr_enable;
	bool enable_lsc;
	bool enable_drc;
	bool enable_pdaf_bpc;
	bool enable_xtalk;

	enum camera2_sensor_hdr_mode sensor_hdr_mode;
	enum camera2_wdr_mode wdr_mode;
	enum camera2_wdr_mode pre_wdr_mode;
	enum camera2_disparity_mode disparity_mode;
	enum camera2_paf_mode paf_mode;
	enum aa_scene_mode scene_mode;
} is_shared_data;

typedef struct {
	bool ois_available;
	unsigned int mode;
	unsigned int factory_step;
} ois_shared_data;

struct wb_gains {
	u32 gr;
	u32 r;
	u32 b;
	u32 gb;
};

struct roi_setting_t {
	bool    update;
#ifdef SUPPORT_SENSOR_SEAMLESS_3HDR
	u16     roi_start_x[2];
	u16     roi_start_y[2];
	u16     roi_end_x[2];
	u16     roi_end_y[2];
#else
	u16 	roi_start_x;
	u16 	roi_start_y;
	u16 	roi_end_x;
	u16 	roi_end_y;
#endif
};

struct sensor_lsi_3hdr_stat_control_mode_change {
	int r_weight;
	int b_weight;
	int g_weight;
	int low_gate_thr;
	int high_gate_thr;
	struct roi_setting_t y_sum_roi;
};

struct sensor_lsi_3hdr_stat_control_per_frame {
	int r_weight;
	int b_weight;
	int g_weight;
};

struct sensor_imx_3hdr_stat_control_mode_change {
		struct roi_setting_t y_sum_roi;
};

struct sensor_imx_3hdr_stat_control_per_frame {
	u8 pgain;
	u8 ngain;
	u8 fc_correct_intensity;
	u16 wbd_r_gr;
};

struct sensor_imx_3hdr_lsc_table_init {
		u16 ram_table[13*10*2];
};

struct sensor_imx_3hdr_tone_control {
	bool gmt_tc2_enable;
	u8 gmt_tc2_ratio;
	u8 manual21_frame_p1;
	u8 manual21_frame_p2;
	u8 manual12_frame_p1;
	u8 manual12_frame_p2;
	u8 manual_tc_ratio;
	u8 ltc_ratio;
	u16 hdr_tc_ratio_1;
	u16 hdr_tc_ratio_2;
	u16 hdr_tc_ratio_3;
	u16 hdr_tc_ratio_4;
	u16 hdr_tc_ratio_5;
};

struct sensor_imx_3hdr_ev_control {
	u8 evc_pgain;
	u8 evc_ngain;
};

typedef struct {
	/** The length of a frame is specified as a number of lines, frame_length_lines.
	  @remarks
	  'frame length lines' is changed in dynamic AE mode.
	 */
	unsigned int frame_length_lines;
	unsigned int frame_time; // unit: ms

	/** The length of a line is specified as a number of pixel clocks, line_length_pck. */
	unsigned int line_length_pck;
	unsigned int line_readOut_time; // unit: ns
	unsigned long long rolling_shutter_skew;

	/** Video Timing Pixel Clock, vt_pix_clk_freq. */
	u64 pclk;
	unsigned int min_frame_us_time;
#ifdef CAMERA_REAR2
	unsigned int min_sync_frame_us_time;
#endif

	/** Frame valid time */
	unsigned int frame_valid_us_time;

	unsigned int min_coarse_integration_time;
	unsigned int max_coarse_integration_time;
	unsigned int min_fine_integration_time;
	unsigned int max_fine_integration_time;
	unsigned int max_margin_coarse_integration_time;
	unsigned int max_margin_fine_integration_time;
	unsigned int min_analog_gain[2]; // 0: code, 1: times
	unsigned int max_analog_gain[2];
	unsigned int min_digital_gain[2];
	unsigned int max_digital_gain[2];
	unsigned int cur_coarse_integration_time_step;

	unsigned int cur_frame_us_time;
	unsigned int cur_width;
	unsigned int cur_height;
	unsigned int cur_pattern_mode;
	unsigned int pre_width;
	unsigned int pre_height;

	/** Current analogue_gain_code_global */
	unsigned int cur_exposure_coarse;
	unsigned int cur_exposure_fine;
	unsigned int cur_analog_gain;
	unsigned int cur_analog_gain_permille;
	unsigned int cur_digital_gain;
	unsigned int cur_digital_gain_permille;
	unsigned int cur_long_exposure_coarse;
	unsigned int cur_long_exposure_fine;
	unsigned int cur_long_analog_gain;
	unsigned int cur_long_analog_gain_permille;
	unsigned int cur_long_digital_gain;
	unsigned int cur_long_digital_gain_permille;
	unsigned int cur_short_exposure_coarse;
	unsigned int cur_short_exposure_fine;
	unsigned int cur_short_analog_gain;
	unsigned int cur_short_analog_gain_permille;
	unsigned int cur_short_digital_gain;
	unsigned int cur_short_digital_gain_permille;

	unsigned int stream_on;

	/* Moved from SensorEntry.cpp Jong 20121008 */
	unsigned int sen_vsync_count;
	unsigned int sen_frame_id;
	unsigned int stream_id;
	unsigned int product_name; /* sensor names such as IMX134, IMX135, and S5K3L2 */
	unsigned int sens_config_index_cur;
	unsigned int sens_config_index_pre;
	unsigned int sens_config_ex_mode_cur;
	unsigned int cur_frame_rate;
	unsigned int pre_frame_rate;
	bool is_active_area;

	/* bool bFirstRegSet; */
	unsigned int low_expo_start;

	/* To deal with N + 1 or N +2 setting timing, 0: Previous input, 1: Current input */
	unsigned int analog_gain[2];
	unsigned int digital_gain[2];

	unsigned int max_fps;

	unsigned int mipi_err_int_cnt[10];
	unsigned int mipi_err_print_out_cnt[10];

	bool called_common_sensor_setting; /* [hc0105.kim, 2013/09/13] Added to avoid calling common sensor register setting again. */
/* #ifdef C1_LSC_CHANGE // [ist.song 2014.08.19] Added to inform videomode to sensor. */
	bool video_mode;
#ifdef USE_NEW_PER_FRAME_CONTROL
	unsigned int num_of_frame;
#endif
/* #endif */
	unsigned int actuator_position;

	is_shared_data is_data;
	ois_shared_data ois_data;
	ae_setting auto_exposure[2];
	preprocessor_ae_setting preproc_auto_exposure[2];

/*	SysSema_t pFlashCtrl_IsrSema; //[2014.08.13, kh14.koo] to add for sema of KTD2692 (flash dirver) */

	bool binning; /* If binning is set, sensor should binning for size */
	bool dual_slave;
	bool lte_multi_capture_mode;
	bool highres_capture_mode;

	/* set aeb mode */
	u32 cur_aeb_mode;
	u32 pre_aeb_mode;

	u32 cis_rev;
	u32 cis_model_id;
	u32 group_param_hold;

	/* set low noise mode */
	u32 cur_lownoise_mode;
	u32 pre_lownoise_mode;

	/* set 12bit mode */
	u32 cur_12bit_mode;
	u32 pre_12bit_mode;

	u32 frame_length_lines_shifter;

	/* for deliver crop coordinate for RTA */
	u32	crop_x;
	u32	crop_y;
} cis_shared_data;

struct v4l2_subdev;
typedef int (*cis_func_type)(struct v4l2_subdev *subdev, cis_setting_info *info);
struct is_cis_ops {
        int (*cis_init)(struct v4l2_subdev *subdev);
        int (*cis_deinit)(struct v4l2_subdev *subdev);
        int (*cis_log_status)(struct v4l2_subdev *subdev);
        int (*cis_group_param_hold)(struct v4l2_subdev *subdev, bool hold);
        int (*cis_set_global_setting)(struct v4l2_subdev *subdev);
        int (*cis_mode_change)(struct v4l2_subdev *subdev, u32 mode);
        int (*cis_set_size)(struct v4l2_subdev *subdev, cis_shared_data *cis_data);
        int (*cis_stream_on)(struct v4l2_subdev *subdev);
        int (*cis_stream_off)(struct v4l2_subdev *subdev);
        int (*cis_adjust_frame_duration)(struct v4l2_subdev *subdev,
						u32 input_exposure_time,
						u32 *target_duration);
	/* Set dynamic frame duration value */
        int (*cis_set_frame_duration)(struct v4l2_subdev *subdev, u32 frame_duration);
	/* Set min fps value */
        int (*cis_set_frame_rate)(struct v4l2_subdev *subdev, u32 min_fps);
        int (*cis_get_min_exposure_time)(struct v4l2_subdev *subdev, u32 *min_expo);
        int (*cis_get_max_exposure_time)(struct v4l2_subdev *subdev, u32 *max_expo);
	cis_func_type cis_adjust_expoure_time; /* TBD */
        int (*cis_set_exposure_time)(struct v4l2_subdev *subdev, struct ae_param *target_exposure);
        int (*cis_get_min_analog_gain)(struct v4l2_subdev *subdev, u32 *min_again);
        int (*cis_get_max_analog_gain)(struct v4l2_subdev *subdev, u32 *max_again);
        int (*cis_adjust_analog_gain)(struct v4l2_subdev *subdev, u32 input_again, u32 *target_permile);
        int (*cis_set_analog_gain)(struct v4l2_subdev *subdev, struct ae_param *again);
        int (*cis_get_analog_gain)(struct v4l2_subdev *subdev, u32 *again);
        int (*cis_get_min_digital_gain)(struct v4l2_subdev *subdev, u32 *min_dgain);
        int (*cis_get_max_digital_gain)(struct v4l2_subdev *subdev, u32 *max_dgain);
	cis_func_type cis_adjust_digital_gain; /* TBD */
        int (*cis_set_digital_gain)(struct v4l2_subdev *subdev, struct ae_param *dgain);
        int (*cis_get_digital_gain)(struct v4l2_subdev *subdev, u32 *dgain);
	int (*cis_compensate_gain_for_extremely_br)(struct v4l2_subdev *subdev, u32 expo, u32 *again, u32 *dgain);
	cis_func_type cis_get_line_readout_time_ns; /* TBD */
	cis_func_type cis_read_sysreg; /* TBD */
	cis_func_type cis_read_userreg; /* TBD */
	int (*cis_wait_streamoff)(struct v4l2_subdev *subdev);
	int (*cis_wait_streamon)(struct v4l2_subdev *subdev);
	void (*cis_data_calculation)(struct v4l2_subdev *subdev, u32 mode);
	int (*cis_set_long_term_exposure)(struct v4l2_subdev *subdev);
	int (*cis_set_adjust_sync)(struct v4l2_subdev *subdev, u32 adjust_sync);
#ifdef USE_CAMERA_SENSOR_RETENTION
	int (*cis_retention_prepare)(struct v4l2_subdev *subdev);
	int (*cis_retention_crc_check)(struct v4l2_subdev *subdev);
#endif
#ifdef USE_CAMERA_EMBEDDED_HEADER
	int (*cis_get_frame_id)(struct v4l2_subdev *subdev, u8 *embedded_buf, u16 *frame_id);
#endif
	int (*cis_set_frs_control)(struct v4l2_subdev *subdev, u32 command);
	int (*cis_set_super_slow_motion_roi)(struct v4l2_subdev *subdev, struct v4l2_rect *ssm_roi);
	int (*cis_set_super_slow_motion_setting)(struct v4l2_subdev *subdev, struct v4l2_rect *setting);
#ifdef CAMERA_REAR2_SENSOR_SHIFT_CROP
	int (*cis_update_pdaf_tail_size)(struct v4l2_subdev *subdev, struct is_sensor_cfg *select);
#endif
	int (*cis_check_rev_on_init)(struct v4l2_subdev *subdev);
	int (*cis_set_super_slow_motion_threshold)(struct v4l2_subdev *subdev, u32 threshold);
	int (*cis_get_super_slow_motion_threshold)(struct v4l2_subdev *subdev, u32 *threshold);
	int (*cis_set_initial_exposure)(struct v4l2_subdev *subdev);
	int (*cis_get_super_slow_motion_gmc)(struct v4l2_subdev *subdev, u32 *gmc);
	int (*cis_get_super_slow_motion_frame_id)(struct v4l2_subdev *subdev, u32 *frameid);
	int (*cis_set_super_slow_motion_flicker)(struct v4l2_subdev *subdev, u32 flicker);
	int (*cis_get_super_slow_motion_flicker)(struct v4l2_subdev *subdev, u32 *flicker);
	int (*cis_get_super_slow_motion_md_threshold)(struct v4l2_subdev *subdev, u32 *threshold);
	int (*cis_set_super_slow_motion_gmc_table_idx)(struct v4l2_subdev *subdev, u32 idx);
	int (*cis_set_super_slow_motion_gmc_block_with_md_low)(struct v4l2_subdev *subdev, u32 idx);
	int (*cis_recover_stream_on)(struct v4l2_subdev *subdev);
	int (*cis_recover_stream_off)(struct v4l2_subdev *subdev);
	int (*cis_set_laser_control)(struct v4l2_subdev *subdev, u32 onoff);
	int (*cis_set_laser_mode)(struct v4l2_subdev *subdev, u32 mode);
	int (*cis_set_factory_control)(struct v4l2_subdev *subdev, u32 command);
	int (*cis_set_laser_current)(struct v4l2_subdev *subdev, u32 value);
	int (*cis_get_laser_photo_diode)(struct v4l2_subdev *subdev, u16 *value);
	int (*cis_get_tof_tx_freq)(struct v4l2_subdev *subdev, u32 *value);
	int (*cis_get_tof_laser_error_flag)(struct v4l2_subdev *subdev, u32 mode, int *value);
	int (*cis_set_tof_tx_freq)(struct v4l2_subdev *subdev, u32 value);
	int (*cis_set_wb_gains)(struct v4l2_subdev *subdev, struct wb_gains wb_gains);
	int (*cis_set_roi_stat)(struct v4l2_subdev *subdev, struct roi_setting_t roi_control);
	int (*cis_set_3hdr_stat)(struct v4l2_subdev *subdev, bool streaming, void *data);
	void (*cis_check_wdr_mode)(struct v4l2_subdev *subdev, u32 mode_idx);
	int (*cis_check_model_id)(struct v4l2_subdev *subdev);
	int (*cis_active_test)(struct v4l2_subdev *subdev);
	int (*cis_set_dual_setting)(struct v4l2_subdev *subdev, u32 mode);
	int (*cis_get_binning_ratio)(struct v4l2_subdev *subdev, u32 mode, int *binning_ratio);
	int (*cis_init_3hdr_lsc_table)(struct v4l2_subdev *subdev, void *data);
	int (*cis_set_tone_stat)(struct v4l2_subdev *subdev, struct sensor_imx_3hdr_tone_control tone_control);
	int (*cis_set_ev_stat)(struct v4l2_subdev *subdev, struct sensor_imx_3hdr_ev_control ev_control);
	int (*cis_set_totalgain)(struct v4l2_subdev *subdev, struct ae_param *target_exposure, struct ae_param *again, struct ae_param *dgain);
	int (*cis_set_fake_retention)(struct v4l2_subdev *subdev, bool enable);
	int (*cis_wait_ln_mode_delay)(struct v4l2_subdev *subdev);
	int (*cis_set_test_pattern)(struct v4l2_subdev *subdev, camera2_sensor_ctl_t *sensor_ctl);
};

struct is_sensor_ctl
{
	//===================================================================================//
	// Ctl updtate timing
	// (01) ~ (04) are updated by "SensorEntry::CopyCam2P0Ctl" when "SIRC_ISP_CAMERA_EVENT_FRAME_START" interrupt is occured.

	/* (01) */  u32 ctl_frame_number;
	/* (02) */  camera2_sensor_ctl_t cur_cam20_sensor_ctrl;
	bool valid_sensor_ctrl;
	/* (03) */  camera2_flash_ctl_t cur_cam20_flash_ctrl;
	bool valid_flash_ctrl;
	/* (04) */  camera2_lens_ctl_t cur_cam20_lens_ctrl;
	bool is_valid_lens_ctrl;
	//===================================================================================//

	//===================================================================================//
	// UCtl update timing

	// Sensor
	// (05) ~ (07) are updated by "SensorEntry::ReportCam2P0SensorDone" when "SIRC_ISP_CAMERA_EVENT_FRAME_END" interrupt is occured.
	/* (05) */  u32 sensor_frame_number;
	/* (06) */  bool is_sensor_request;
	/* (07) */  camera2_sensor_uctl_t cur_cam20_sensor_udctrl;
	bool is_valid_sensor_udctrl;

	// Flash
	// (08) ~ (10) are updated by "SensorEntry::ReportCam2P0FlashDone" when "SircSenHal_RequestFlash" function is called.
	/* (08) */  u32 flash_frame_number;
	/* (09) */  bool is_flash_request;
	/* (10) */  camera2_flash_uctl_t cur_cam20_flash_udctrl;
	bool valid_flash_udctrl;

	// Lens
	// (11) ~ (13) are updated by "SensorEntry::ReportCam2P0LensDone" when "SircSenHal_SetActuatorPosition" function is called
	// or "SIRC_ISP_CAMERA_EVENT_FRAME_END" interrupt is occured.
	/* (11) */  u32 lens_frame_number;
	/* (12) */  bool is_lens_request;
	/* (13) */  camera2_lens_uctl_t cur_cam20_lens_udctrl;
	bool is_valid_lens_udctrl;
	//===================================================================================//

	bool alg_reset_flag;

	// Frame number that indicating shot. Currntly, it is not used.
	/* (14) */  bool shot_frame_number;

	/* For WB(White Balance) gain update */
	struct wb_gains wb_gains;
	bool update_wb_gains;

	/* force_update set when need to update w/o DDK or RTA */
	bool force_update;

	/* for update 3DHDR sensor stats */
	struct roi_setting_t roi_control;
	bool update_roi;
	struct sensor_lsi_3hdr_stat_control_per_frame lsi_stat_control;
	struct sensor_imx_3hdr_stat_control_per_frame imx_stat_control;
	bool update_3hdr_stat;
	struct sensor_imx_3hdr_tone_control imx_tone_control;
	bool update_tone;
	struct sensor_imx_3hdr_ev_control imx_ev_control;
	bool update_ev;
};

typedef enum is_sensor_adjust_direction_ {
	SENSOR_ADJUST_TO_SHORT	= 1,
	SENSOR_ADJUST_TO_LONG	= 2,
} is_sensor_adjust_direction;

/* If companion statistics are used, then 3A algorithms need to know whether current stat. are ready to use or not. */
enum itf_cis_hdr_stat_status {
	SENSOR_STAT_STATUS_NO_DATA = 0,
	SENSOR_STAT_STATUS_DONE = 1,
};

enum itf_cis_interface {
	ITF_CIS_SMIA = 0,
	ITF_CIS_SMIA_WDR,
	ITF_CIS_MAX,
};

enum itf_param_type {
	ITF_CIS_PARAM_TOTAL_GAIN,
	ITF_CIS_PARAM_ANALOG_GAIN,
	ITF_CIS_PARAM_DIGITAL_GAIN,
	ITF_CIS_PARAM_EXPOSURE,
	ITF_CIS_PARAM_FLASH_INTENSITY,
	ITF_CIS_PARAM_MAX,
};

/* This peri state is check available each sensor device. */
enum is_sensor_peri_state {
	IS_SENSOR_ACTUATOR_AVAILABLE,
	IS_SENSOR_FLASH_AVAILABLE,
	IS_SENSOR_PREPROCESSOR_AVAILABLE,
	IS_SENSOR_OIS_AVAILABLE,
	IS_SENSOR_PDP_AVAILABLE,
	IS_SENSOR_APERTURE_AVAILABLE,
	IS_SENSOR_PAFSTAT_AVAILABLE,
	IS_SENSOR_EEPROM_AVAILABLE,
	IS_SENSOR_LASER_AF_AVAILABLE,
	IS_SENSOR_TOF_AF_AVAILABLE,
};

enum is_actuator_pos_size_bit {
	ACTUATOR_POS_SIZE_8BIT = 8,
	ACTUATOR_POS_SIZE_9BIT = 9,
	ACTUATOR_POS_SIZE_10BIT = 10,
	ACTUATOR_POS_SIZE_11BIT = 11,
	ACTUATOR_POS_SIZE_12BIT = 12,
};

enum is_actuator_direction {
	ACTUATOR_RANGE_INF_TO_MAC = 0,
	ACTUATOR_RANGE_MAC_TO_INF,
};

enum is_actuator_status {
	ACTUATOR_STATUS_NO_BUSY = 0,
	ACTUATOR_STATUS_BUSY
};

enum is_actuator_state {
	ACTUATOR_STATE_INIT = 0,
	ACTUATOR_STATE_STANDBY,
	ACTUATOR_STATE_SLEEP,
	ACTUATOR_STATE_ACTIVE,
};

enum is_cis_lownoise_mode {
	IS_CIS_LNOFF = 0, /* Low Noise Off */
	IS_CIS_LN2, /* Low Noise 2 */
	IS_CIS_LN4, /* Low Noise 4 */
	IS_CIS_LN2_PEDESTAL128, /* Low Noise 2 + pedestal 128 */
	IS_CIS_LN4_PEDESTAL128, /* Low Noise 4 + pedestal 128 */
	IS_CIS_LOWNOISE_MODE_MAX,
};

enum is_exposure_gain_count {
	EXPOSURE_GAIN_COUNT_INVALID = 0,
	EXPOSURE_GAIN_COUNT_1,
	EXPOSURE_GAIN_COUNT_2,
	EXPOSURE_GAIN_COUNT_3,
	EXPOSURE_GAIN_COUNT_END
};

enum is_exposure_gain_type {
	EXPOSURE_GAIN_LONG = 0,
	EXPOSURE_GAIN_SHORT,
	EXPOSURE_GAIN_MIDDLE,
	EXPOSURE_GAIN_MAX
};

enum is_sensor_stat_control {
	SENSOR_STAT_NOTHING = 0, /* Default */
	SENSOR_STAT_LSI_3DHDR, /* LSI 3DHDR stat control */
	SENSOR_STAT_IMX_3DHDR, /* IMX 3DHDR stat control */
	SENSOR_STAT_CONTROL_MAX,
};

enum is_sensor_hdr_mode
{
    SENSOR_HDR_MODE_SINGLE,
    SENSOR_HDR_MODE_2HDR,
    SENSOR_HDR_MODE_3HDR,
    SENSOR_HDR_MODE_2AEB,
};

enum is_aperture_control_step {
	APERTURE_STEP_STATIONARY = 0,
	APERTURE_STEP_PREPARE,
	APERTURE_STEP_MOVING,
};

enum is_sensor_aeb_mode {
	SENSOR_AEB_MODE_OFF,
	SENSOR_AEB_MODE_ON,
};

enum is_sensor_12bit_mode
{
	SENSOR_12BIT_MODE_NONE = 0,
	SENSOR_12BIT_MODE_PSEUDO_ONLY,
	SENSOR_12BIT_MODE_REAL_ONLY,
	SENSOR_12BIT_MODE_PSEUDO_REAL_SEAMLESS,
};

enum is_sensor_12bit_state
{
	SENSOR_12BIT_STATE_OFF = 0,
	SENSOR_12BIT_STATE_PSEUDO_12BIT,
	SENSOR_12BIT_STATE_REAL_12BIT,
};

typedef int (*actuator_func_type)(struct v4l2_subdev *subdev, u32 *info);
struct is_actuator_ops {
	actuator_func_type actuator_init;
	actuator_func_type actuator_get_status;
	actuator_func_type actuator_set_pos;
	actuator_func_type actuator_cal_data;
#ifdef USE_AF_SLEEP_MODE
	int (*set_active)(struct v4l2_subdev *subdev, int enable);
#endif
	int (*soft_landing_on_recording)(struct v4l2_subdev *subdev);
};

struct is_aperture_ops {
	int (*set_aperture_value)(struct v4l2_subdev *subdev, int value);
#if !defined(CONFIG_CAMERA_USE_INTERNAL_MCU)
	int (*set_aperture_start_value_step1)(struct v4l2_subdev *subdev, int value);
	int (*set_aperture_start_value_step2)(struct v4l2_subdev *subdev, int value);
	int (*prepare_ois_autotest)(struct v4l2_subdev *subdev);
#endif
	int (*aperture_deinit)(struct v4l2_subdev *subdev, int value);
};

/* for SetAlgResFlag API */
struct is_3a_res_to_sensor {
	u32 hdr_ratio;
	u32 red_gain;
	u32 green_gain;
	u32 blue_gain;
	u32 hdr_enabled;
	u32 hdr_state;
	u32 thermal_mode;
	bool video_mode;
};

/* Flash */
struct is_flash_ops {
	int (*flash_control)(struct v4l2_subdev *subdev, enum flash_mode mode, u32 intensity);
};

struct is_preproc_lemode_set{
	u16 lemode;
	bool every_frame;
};

/* Long Term Exposure mode(LTE mode) interface */
struct is_long_term_expo_mode {
	u32 expo[2];
	u32 tgain[2];
	u32 again[2];
	u32 dgain[2];
	bool sen_strm_off_on_enable;
	u32 sen_strm_off_on_step;
	u32 frm_num_strm_off_on[2];
	u32 frm_num_strm_off_on_interval;
	struct is_preproc_lemode_set lemode_set;
	/* Back up now line_length_pck when start LTE mode */
	u32 pre_line_length_pck;
	/* Control to frame_interval in sensor driver when LTE mode */
	u32 frame_interval;
};

struct is_ois_hall_data {
	u64 readTimeStamp;
	u32 counter;
	int16_t X_AngVel[4];
	int16_t Y_AngVel[4];
	int16_t Z_AngVel[4];
};

/* OIS */
struct is_ois_ops {
	int (*ois_init)(struct v4l2_subdev *subdev);
	int (*ois_init_fac)(struct v4l2_subdev *subdev);
	void (*ois_init_rear2)(struct is_core *core);
#if defined (CONFIG_CAMERA_USE_INTERNAL_MCU)
	int (*ois_deinit)(struct v4l2_subdev *subdev);
#endif
#ifdef USE_OIS_SLEEP_MODE
	int (*ois_start)(struct v4l2_subdev *subdev);
	int (*ois_stop)(struct v4l2_subdev *subdev);
#endif
	int (*ois_set_mode)(struct v4l2_subdev *subdev, int mode);
	int (*ois_shift_compensation)(struct v4l2_subdev *subdev, int position, int resolution);
#ifdef CONFIG_OIS_DIRECT_FW_CONTROL
	int (*ois_fw_update)(struct v4l2_subdev *subdev);
#else
	void (*ois_fw_update)(struct is_core *core);
#endif
	int (*ois_self_test)(struct is_core *core);
#if !defined (CONFIG_CAMERA_USE_INTERNAL_MCU)
	bool (*ois_diff_test)(struct is_core *core, int *x_diff, int *y_diff);
#endif
	bool (*ois_auto_test)(struct is_core *core,
				int threshold, bool *x_result, bool *y_result, int *sin_x, int *sin_y,
				bool *x_result_2nd, bool *y_result_2nd, int *sin_x_2nd, int *sin_y_2nd,
				bool *x_result_3rd, bool *y_result_3rd, int *sin_x_3rd, int *sin_y_3rd);
#ifdef CAMERA_2ND_OIS
	bool (*ois_auto_test_rear2)(struct is_core *core,
				int threshold, bool *x_result, bool *y_result, int *sin_x, int *sin_y,
				bool *x_result_2nd, bool *y_result_2nd, int *sin_x_2nd, int *sin_y_2nd);
	int (*ois_set_power_mode)(struct v4l2_subdev *subdev);
#endif
	bool (*ois_check_fw)(struct is_core *core);
	void (*ois_enable)(struct is_core *core);
	int (*ois_disable)(struct v4l2_subdev *subdev);
	bool (*ois_offset_test)(struct is_core *core, long *raw_data_x, long *raw_data_y);
	void (*ois_get_offset_data)(struct is_core *core, long *raw_data_x, long *raw_data_y);
	void (*ois_gyro_sleep)(struct is_core *core);
	void (*ois_exif_data)(struct is_core *core);
	u8 (*ois_read_status)(struct is_core *core);
	u8 (*ois_read_cal_checksum)(struct is_core *core);
	int (*ois_set_coef)(struct v4l2_subdev *subdev, u8 coef);
	int (*ois_read_fw_ver)(char *name, char *ver);
	int (*ois_center_shift)(struct v4l2_subdev *subdev);
	int (*ois_set_center)(struct v4l2_subdev *subdev);
	u8 (*ois_read_mode)(struct v4l2_subdev *subdev);
#if defined (CONFIG_CAMERA_USE_INTERNAL_MCU)
	bool (*ois_calibration_test)(struct is_core *core, long *raw_data_x, long *raw_data_y);
#endif
	bool(*ois_read_gyro_noise)(struct is_core *core, long *raw_data_x, long *raw_data_y);
	int (*ois_set_af_active)(struct v4l2_subdev *subdev, int enable);
	int (*ois_set_af_position)(struct v4l2_subdev *subdev, u32 position);
	void (*ois_get_hall_pos)(struct is_core *core, u16 *targetPos, u16 *hallPos);
	int (*ois_check_cross_talk)(struct v4l2_subdev *subdev, u16 *hall_data);
	int (*ois_check_hall_cal)(struct v4l2_subdev *subdev, u16 *hall_cal_data);
	void (*ois_check_valid)(struct v4l2_subdev *subdev, u8 *value);
#ifdef USE_OIS_HALL_DATA_FOR_VDIS
	int (*ois_get_hall_data)(struct v4l2_subdev *subdev, struct is_ois_hall_data *halldata);
#endif
	bool(*ois_get_active)(void);
	int(*ois_read_ext_clock)(struct v4l2_subdev *subdev, u32 *clock);
	void(*ois_parsing_raw_data)(uint8_t *buf, long efs_size, long *raw_data_x, long *raw_data_y);
};

struct is_sensor_interface;

/* new APIs */
struct is_cis_interface_ops {
	int (*get_)(struct is_sensor_interface *itf);

	int (*request_reset_interface)(struct is_sensor_interface *itf,
					u32 exposure,
					u32 total_gain,
					u32 analog_gain,
					u32 digital_gain);

	int (*get_calibrated_size)(struct is_sensor_interface *itf,
					u32 *width,
					u32 *height);

	int (*get_bayer_order)(struct is_sensor_interface *itf,
					u32 *bayer_order);

	u32 (*get_min_exposure_time)(struct is_sensor_interface *itf);
	u32 (*get_max_exposure_time)(struct is_sensor_interface *itf);
	u32 (*get_min_analog_gain)(struct is_sensor_interface *itf);
	u32 (*get_max_analog_gain)(struct is_sensor_interface *itf);
	u32 (*get_min_digital_gain)(struct is_sensor_interface *itf);
	u32 (*get_max_digital_gain)(struct is_sensor_interface *itf);

	u32 (*get_vsync_count)(struct is_sensor_interface *itf);
	u32 (*get_vblank_count)(struct is_sensor_interface *itf);
	bool (*is_vvalid_period)(struct is_sensor_interface *itf);

	int (*request_exposure)(struct is_sensor_interface *itf,
		enum is_exposure_gain_count num_data, u32 *exposure);

	int (*adjust_exposure)(struct is_sensor_interface *itf,
				enum is_exposure_gain_count num_data,
				u32 *exposure,
				u32 *available_exposure,
				is_sensor_adjust_direction adjust_direction);

	int (*get_next_frame_timing)(struct is_sensor_interface *itf,
				enum is_exposure_gain_count num_data,
				u32 *exposure,
				u32 *frame_period,
				u64 *line_period);

	int (*get_frame_timing)(struct is_sensor_interface *itf,
				enum is_exposure_gain_count num_data,
				u32 *exposure,
				u32 *frame_period,
				u64 *line_period);

	int (*request_analog_gain)(struct is_sensor_interface *itf,
				enum is_exposure_gain_count num_data,
				u32 *analog_gain);

	int (*request_gain)(struct is_sensor_interface *itf,
				enum is_exposure_gain_count num_data,
				u32 *total_gain,
				u32 *analog_gain,
				u32 *digital_gain);

	int (*adjust_analog_gain)(struct is_sensor_interface *itf,
				enum is_exposure_gain_count num_data,
				u32 *desired_analog_gain,
				u32 *actual_gain,
				is_sensor_adjust_direction adjust_direction);

	int (*get_next_analog_gain)(struct is_sensor_interface *itf,
				enum is_exposure_gain_count num_data,
				u32 *analog_gain);

	int (*get_analog_gain)(struct is_sensor_interface *itf,
				enum is_exposure_gain_count num_data,
				u32 *analog_gain);

	int (*get_next_digital_gain)(struct is_sensor_interface *itf,
				enum is_exposure_gain_count num_data,
				u32 *digital_gain);

	int (*get_digital_gain)(struct is_sensor_interface *itf,
				enum is_exposure_gain_count num_data,
				u32 *digital_gain);

	bool (*is_actuator_available)(struct is_sensor_interface *itf);
	bool (*is_flash_available)(struct is_sensor_interface *itf);
	int (*get_sensor_type)(struct is_sensor_interface *itf);
	bool (*is_ois_available)(struct is_sensor_interface *itf);
	bool (*is_aperture_available)(struct is_sensor_interface *itf);
#if !defined(DISABLE_LASER_AF)
	bool (*is_laser_af_available)(struct is_sensor_interface *itf);
#endif
	bool (*is_tof_af_available)(struct is_sensor_interface *itf);
	int (*get_sensor_frame_timing)(struct is_sensor_interface *itf,
				u32 *vvalid_time,
				u32 *vblank_time);

	int (*get_sensor_cur_size)(struct is_sensor_interface *itf,
				u32 *cur_x,
				u32 *cur_y,
				u32 *cur_width,
				u32 *cur_height);

	int (*get_sensor_max_fps)(struct is_sensor_interface *itf,
				u32 *max_fps);

	int (*get_sensor_cur_fps)(struct is_sensor_interface *itf,
				u32 *cur_fps);

	int (*get_hdr_ratio_ctl_by_again)(struct is_sensor_interface *itf,
				u32 *ctrl_by_again);

	int (*get_sensor_use_dgain)(struct is_sensor_interface *itf,
				u32 *use_dgain);

	int (*set_alg_reset_flag)(struct is_sensor_interface *itf,
				bool executed);

	int (*get_sensor_initial_aperture)(struct is_sensor_interface *itf,
				u32 *aperture);

	int (*set_initial_exposure_of_setfile)(struct is_sensor_interface *itf,
				u32 expo);

#ifdef USE_NEW_PER_FRAME_CONTROL
	int (*reserved0)(struct is_sensor_interface *itf,
				bool reserved0);

	int (*set_num_of_frame_per_one_3aa)(struct is_sensor_interface *itf,
				u32 *num_of_frame);

	int (*reserved1)(struct is_sensor_interface *itf,
				u32 *reserved1);
#else
	int (*set_video_mode_of_setfile)(struct is_sensor_interface *itf,
				bool video_mode);

	int (*get_num_of_frame_per_one_3aa)(struct is_sensor_interface *itf,
				u32 *num_of_frame);

	int (*get_offset_from_cur_result)(struct is_sensor_interface *itf,
				u32 *offset);
#endif
	int (*set_cur_uctl_list)(struct is_sensor_interface *itf);

	int (*apply_sensor_setting)(struct is_sensor_interface *itf);

	/* reset exposure and gain for Flash */
	int (*request_reset_expo_gain)(struct is_sensor_interface *itf,
					enum is_exposure_gain_count num_data,
					u32 *expo,
					u32 *tgain,
					u32 *again,
					u32 *dgain);
	int (*set_sensor_info_mode_change)(struct is_sensor_interface *itf,
					enum is_exposure_gain_count num_data,
					u32 *expo,
					u32 *again,
					u32 *dgain);
	int (*update_sensor_dynamic_meta)(struct is_sensor_interface *itf,
					u32 frame_count,
					camera2_ctl_t *ctrl,
					camera2_dm_t *dm,
					camera2_udm_t *udm);
	int (*copy_sensor_ctl)(struct is_sensor_interface *itf,
					u32 frame_count,
					camera2_shot_t *shot);

	/* Get sensor module id */
	int (*get_module_id)(struct is_sensor_interface *itf,
					u32 *module_id);

	/* Get sensor module position */
	int (*get_module_position)(struct is_sensor_interface *itf,
					enum exynos_sensor_position *real_module);

	/* Set sensor 3a mode - OTF/M2M */
	int (*set_sensor_3a_mode)(struct is_sensor_interface *itf,
					u32 mode);
	int (*get_initial_exposure_gain_of_sensor)(struct is_sensor_interface *itf,
					enum is_exposure_gain_count num_data,
					u32 *expo,
					u32 *again,
					u32 *dgain);
	u32 (*get_sensor_frameid)(struct is_sensor_interface *itf,
					u32 *frameid);
};

struct is_cis_ext_interface_ops {
	int (*get_sensor_hdr_stat)(struct is_sensor_interface *itf,
			enum itf_cis_hdr_stat_status *status);

	int (*set_3a_alg_res_to_sens)(struct is_sensor_interface *itf,
			struct is_3a_res_to_sensor *sensor_setting);

	/* In order to change a current CIS mode when an user select the WDR (long and short exposure) mode or the normal AE mo */
	int (*change_cis_mode)(struct is_sensor_interface *itf,
			enum itf_cis_interface cis_mode);
	u32 (*set_adjust_sync)(struct is_sensor_interface *itf, u32 setsync);
	u32 (*request_frame_length_line)(struct is_sensor_interface *itf, u32 framelengthline);
	int (*request_sensitivity)(struct is_sensor_interface *itf,
								u32 sensitivity);
	int (*get_sensor_flag)(struct is_sensor_interface *itf,
			enum is_sensor_stat_control *stat_control_type,
			enum is_sensor_hdr_mode *hdr_mode_type,
			enum is_sensor_12bit_mode *sensor_12bit_mode_type,
			u32 *exposure_count);
	int (*set_sensor_stat_control_mode_change)(struct is_sensor_interface *itf,
			enum is_sensor_stat_control stat_control_type,
			void *stat_control);
	int (*set_sensor_roi_control)(struct is_sensor_interface *itf,
			enum is_sensor_stat_control stat_control_type,
			void *roi_control);
	int (*set_sensor_stat_control_per_frame)(struct is_sensor_interface *itf,
			enum is_sensor_stat_control stat_control_type,
			void *stat_control);
	u32 (*set_sensor_12bit_state)(struct is_sensor_interface *itf,
			enum is_sensor_12bit_state state);
	u32 (*get_sensor_12bit_state)(struct is_sensor_interface *itf,
			enum is_sensor_12bit_state *state);
#ifdef SUPPORT_SENSOR_SEAMLESS_3HDR
	int (*set_sensor_lsc_table_init)(struct is_sensor_interface *itf,
			enum is_sensor_stat_control stat_control_type,
			void *lsc_table);
	int (*set_sensor_tone_control)(struct is_sensor_interface *itf,
			enum is_sensor_stat_control stat_control_type,
			void *tone_control);
	int (*set_sensor_ev_control)(struct is_sensor_interface *itf,
			enum is_sensor_stat_control stat_control_type,
			void *ev_control);
#endif
};

struct is_cis_ext2_interface_ops {
	int (*set_long_term_expo_mode)(struct is_sensor_interface *itf,
				struct is_long_term_expo_mode *long_term_expo_mode);
	int (*set_low_noise_mode)(struct is_sensor_interface *itf, u32 mode);
	int (*get_sensor_max_dynamic_fps)(struct is_sensor_interface *itf,
				u32 *max_dynamic_fps);
	/* Get static memory address for DDK/RTA backup data */
	int (*get_static_mem)(int ctrl_id, void **mem, int *size);
	int (*get_open_close_hint)(int* opening, int* closing);
	int (*request_wb_gain)(struct is_sensor_interface *itf,
				u32 gr_gain, u32 r_gain, u32 b_gain, u32 gb_gain);
	int (*set_sensor_info_mfhdr_mode_change)(struct is_sensor_interface *itf,
				u32 count, u32 *long_expo, u32 *long_again, u32 *long_dgain,
				u32 *expo, u32 *again, u32 *dgain, u32 *sensitivity);
	int (*set_mainflash_duration)(struct is_sensor_interface *itf,
				u32 mainflash_duration);
	int (*set_previous_dm)(struct is_sensor_interface *itf);
	int (*request_direct_flash)(struct is_sensor_interface *itf,
				u32 mode,
				bool on,
				u32 intensity,
				u32 time);
	int (*set_lte_multi_capture_mode)(struct is_sensor_interface *itf,
				bool lte_multi_capture_mode);
	int (*get_delayed_preflash_time)(struct is_sensor_interface *itf, u32 *delayedTime);
	int (*set_aeb_mode)(struct is_sensor_interface *itf, u32 mode);
	void *reserved[8];
};

struct is_cis_event_ops {
	int (*start_of_frame)(struct is_sensor_interface *itf);
	int (*end_of_frame)(struct is_sensor_interface *itf);
	int (*apply_frame_settings)(struct is_sensor_interface *itf);
};

/* end of new APIs */

/* Actuator interface */
struct is_actuator_softlanding_table {
	bool enable;
	u32 step_delay;
	u32 position_num;
	u32 virtual_table[ACTUATOR_MAX_SOFT_LANDING_NUM];
	u32 hw_table[ACTUATOR_MAX_SOFT_LANDING_NUM];
};

struct is_actuator_position_table {
	bool enable;
	u32 hw_table[ACTUATOR_MAX_FOCUS_POSITIONS];
};

struct is_actuator_interface {
	/* ToDo: consider M2M scenario */
	u32 virtual_pos;
	u32 hw_pos;

	/*
	 * This values are specific information for AF Tick noise
	 * when turning on a camera.
	 */
	bool initialized;

	struct is_actuator_position_table position_table;
	struct is_actuator_softlanding_table soft_landing_table;
};

struct is_actuator_interface_ops {
	int (*set_actuator_position_table) (struct is_sensor_interface *itf,
					u32 *position_table);
	int (*set_soft_landing_config) (struct is_sensor_interface *itf,
					u32 step_delay,
					u32 position_num,
					u32 *position_table);
	int (*set_position) (struct is_sensor_interface *itf, u32 position, int realDomainOffset);

	int (*get_cur_frame_position) (struct is_sensor_interface *itf, u32 *position);
	int (*get_applied_actual_position) (struct is_sensor_interface *itf, u32 *position);
	int (*get_prev_frame_position) (struct is_sensor_interface *itf,
					u32 *position, u32 frame_diff);

	int (*set_af_window_position) (struct is_sensor_interface *itf,
					u32 left_x, u32 left_y,
					u32 right_x, u32 right_y);

	int (*get_status) (struct is_sensor_interface *itf, u32 *status,
					u32 *temperature_valid, int *temperature,
					u32 *voltage_valid, int *voltage);
	int (*copy_lens_ctl)(struct is_sensor_interface *itf,
					u32 frame_count,
					camera2_shot_t *shot);
};

struct is_apature_info_t {
	int	cur_value;
	bool	zoom_running;
};

struct is_aperture_interface_ops {
	int (*set_aperture_value)(struct is_sensor_interface *itf, int value);
	int (*get_aperture_value)(struct is_sensor_interface *itf, struct is_apature_info_t *param);
};

/* Flash interface */
struct is_flash_expo_gain {
	u32 expo[2];
	u32 tgain[2];
	u32 again[2];
	u32 dgain[2];
	u32 long_expo[2];
	u32 long_tgain[2];
	u32 long_again[2];
	u32 long_dgain[2];
	u32 short_expo[2];
	u32 short_tgain[2];
	u32 short_again[2];
	u32 short_dgain[2];
	bool pre_fls_ae_reset; /* true: Pre-flash off */
	u32 frm_num_pre_fls; /* If it is set to 100, then Pre-flash is automatically turned off at 100-th frame. */
	bool main_fls_ae_reset; /* true: Main-flash on/off */
	/* If they are set to 200 and 201, then Main-flash sensor settings are applied to a sensor, and the flash is fired at 200-th frame.
	   After the Main-flash, ambient exposure and gains are set to the sensor at 201-th frame. */
	u32 frm_num_main_fls[2];
	u32 main_fls_strm_on_off_step; /* 0: main/pre-flash exposure and gains, 1: ambient exposure and gains */
	u32 flash_capture_cnt;
};

struct is_flash_interface_ops {
	int (*request_flash)(struct is_sensor_interface *itf,
				u32 mode,
				bool on,
				u32 intensity,
				u32 time);
	int (*request_flash_expo_gain)(struct is_sensor_interface *itf,
				struct is_flash_expo_gain *flash_ae);
	int (*update_flash_dynamic_meta)(struct is_sensor_interface *itf,
					u32 frame_count,
					camera2_ctl_t *ctrl,
					camera2_dm_t *dm,
					camera2_udm_t *udm);
	int (*copy_flash_ctl)(struct is_sensor_interface *itf,
					u32 frame_count,
					camera2_shot_t *shot);
};

/* arguments: stat_type, frame_count, notifier_data */
typedef int (*vc_dma_notifier_t)(int, unsigned int, void *);

struct is_csi_interface_ops {
	int (*get_vc_dma_buf)(struct is_sensor_interface *itf,
				enum itf_vc_buf_data_type request_data_type,
				u32 frame_count,
				u32 *buf_index,
				u64 *buf_addr);
	int (*put_vc_dma_buf)(struct is_sensor_interface *itf,
				enum itf_vc_buf_data_type request_data_type,
				u32 index);
	int (*get_vc_dma_buf_info)(struct is_sensor_interface *itf,
				enum itf_vc_buf_data_type request_data_type,
				struct vc_buf_info_t *buf_info);
	int (*get_vc_dma_buf_max_size)(struct is_sensor_interface *itf,
				enum itf_vc_buf_data_type request_data_type,
				u32 *width,
				u32 *height,
				u32 *element_size);
	int (*register_vc_dma_notifier)(struct is_sensor_interface *itf,
					enum itf_vc_stat_type type,
					vc_dma_notifier_t notifier, void *data);
	int (*unregister_vc_dma_notifier)(struct is_sensor_interface *itf,
					enum itf_vc_stat_type type,
					vc_dma_notifier_t notifier);
	int (*reserved[2])(struct is_sensor_interface *itf);
};

struct paf_setting_t {
	u32 reg_addr;
	u32 reg_data;
};

struct is_paf_interface_ops {
	int (*set_paf_param)(struct is_sensor_interface *itf,
				struct paf_setting_t *regs, u32 regs_size);
	int (*get_paf_ready)(struct is_sensor_interface *itf, u32 *ready);
	int (*reserved[6])(struct is_sensor_interface *itf);
};

struct is_dual_interface_ops {
	int (*get_sensor_state)(struct is_sensor_interface *itf);
	int (*get_reuse_3a_state)(struct is_sensor_interface *itf,
				u32 *position, u32 *ae_exposure, u32 *ae_deltaev, bool is_clear);
	int (*set_reuse_ae_exposure)(struct is_sensor_interface *itf,
				u32 ae_exposure, u32 ae_deltaev);
	int (*reserved[2])(struct is_sensor_interface *itf);
};

struct is_eeprom_ops {
	int (*eeprom_read)(struct v4l2_subdev *subdev);
	int (*eeprom_check_all_crc)(struct v4l2_subdev *subdev);
	int (*eeprom_check_address)(struct v4l2_subdev *subdev);
	int (*eeprom_check_info)(struct v4l2_subdev *subdev);
	int (*eeprom_check_awb)(struct v4l2_subdev *subdev);
	int (*eeprom_check_af)(struct v4l2_subdev *subdev);
	int (*eeprom_check_ae)(struct v4l2_subdev *subdev);
	int (*eeprom_check_lsc)(struct v4l2_subdev *subdev);
	int (*eeprom_check_ois)(struct v4l2_subdev *subdev);
	int (*eeprom_check_pdaf)(struct v4l2_subdev *subdev);
	int (*eeprom_check_dual)(struct v4l2_subdev *subdev);
	int (*eeprom_check_sfr)(struct v4l2_subdev *subdev);
	int (*eeprom_get_sensor_id)(struct v4l2_subdev *subdev, int *sensor_id);
	int (*eeprom_cal_variation)(struct v4l2_subdev *subdev, int type, u16 offset, u16 val);
};

/* sync with "laser_af_name" in exynos-is-sensor.h */
enum itf_laser_af_type
{
	ITF_LASER_AF_TYPE_INVALID = 0,
	ITF_LASER_AF_TYPE_VL53L5,
	ITF_LASER_AF_TYPE_TOF8801,  // Legacy
	ITF_LASER_AF_TYPE_VL53L1,
	ITF_LASER_AF_TYPE_END,
};

#define NUMBER_OF_ZONE   64
#define NUMBER_OF_TARGET  2

struct itf_laser_af_data_VL53L5
{

	s16 range_data[NUMBER_OF_ZONE][NUMBER_OF_TARGET];
	u8 status[NUMBER_OF_ZONE][NUMBER_OF_TARGET];
};

struct itf_laser_af_data_VL53L1
{
	u32 distance_mm; // unit: mm
	u32 confidence; // 0~1023 (u10b)
};

union itf_laser_af_data
{
	struct itf_laser_af_data_VL53L5 vl53l5;
	struct itf_laser_af_data_VL53L1 vl53l1;
};

struct is_laser_af_ops
{
	int (*set_active)(struct v4l2_subdev *subdev, bool is_active);
	int (*get_distance)(struct v4l2_subdev *subdev, void *data, u32 *size);
};

#if !defined(DISABLE_LASER_AF)
struct is_laser_af_interface_ops
{
	int (*get_distance)(struct is_sensor_interface *itf, enum itf_laser_af_type *type, union itf_laser_af_data *data);
};
#endif

struct is_tof_af_interface_ops
{
	int (*get_data)(struct is_sensor_interface *itf, struct tof_data_t *data);
};

struct is_sensor_interface {
	u32					magic;
	struct is_cis_interface_ops	cis_itf_ops;
	struct is_cis_event_ops		cis_evt_ops;
	struct is_actuator_interface	actuator_itf;
	struct is_actuator_interface_ops	actuator_itf_ops;
	struct is_flash_interface_ops	flash_itf_ops;
	struct is_aperture_interface_ops	aperture_itf_ops;
	struct is_paf_interface_ops	paf_itf_ops;
#if !defined(DISABLE_LASER_AF)
	struct is_laser_af_interface_ops	laser_af_itf_ops;
#endif
	struct is_tof_af_interface_ops		tof_af_itf_ops;

	bool			vsync_flag;
	bool			otf_flag_3aa;
	/* Different frame count between sensor and ISP */
	enum DIFF_BET_SEN_ISP	diff_bet_sen_isp;
	enum itf_cis_interface	cis_mode;

	u32			total_gain[EXPOSURE_GAIN_MAX][NUM_FRAMES];
	u32			analog_gain[EXPOSURE_GAIN_MAX][NUM_FRAMES];
	u32			digital_gain[EXPOSURE_GAIN_MAX][NUM_FRAMES];
	u32			exposure[EXPOSURE_GAIN_MAX][NUM_FRAMES];

	u32			flash_mode[NUM_FRAMES];
	u32			flash_intensity[NUM_FRAMES];
	u32			flash_firing_duration[NUM_FRAMES];
	struct is_cis_ext_interface_ops	cis_ext_itf_ops;
	struct is_csi_interface_ops	csi_itf_ops;
	/* Add interface for LTE mode */
	struct is_cis_ext2_interface_ops	cis_ext2_itf_ops;
	/* Add interface for DUAL scenario */
	struct is_dual_interface_ops	dual_itf_ops;
};


int init_sensor_interface(struct is_sensor_interface *itf);

/* Sensor interface helper function */
struct is_actuator *get_subdev_actuator(struct is_sensor_interface *itf);
u32 get_frame_count(struct is_sensor_interface *itf);

int sensor_get_ctrl(struct is_sensor_interface *itf, u32 ctrl_id, u32 *val);

/* Actuator interface function */
int set_actuator_position_table(struct is_sensor_interface *itf,
			u32 *position_table);
int set_soft_landing_config(struct is_sensor_interface *itf,
			u32 step_delay, u32 position_num, u32 *hw_pos_table);
int set_position(struct is_sensor_interface *itf, u32 position, int domainoffset);
int get_cur_frame_position(struct is_sensor_interface *itf, u32 *position);
int get_applied_actual_position(struct is_sensor_interface *itf, u32 *position);
int get_prev_frame_position(struct is_sensor_interface *itf,
			u32 *position, u32 frame_diff);
int set_af_window_position(struct is_sensor_interface *itf,
					u32 left_x, u32 left_y,
					u32 right_x, u32 right_y);
int get_status(struct is_sensor_interface *itf, u32 *status,
					u32 *temperature_valid, int *temperature,
					u32 *voltage_valid, int *voltage);

#endif
