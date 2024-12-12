/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef is_device_sensor_peri_H
#define is_device_sensor_peri_H

#ifdef CONFIG_MUIC_NOTIFIER
#include <linux/device.h>
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#endif

#include <linux/interrupt.h>
#include "is-mem.h"
#include "is-param.h"
#include "is-interface-sensor.h"
#include "is-control-sensor.h"
#include "is-hw-pdp.h"

#define HRTIMER_IMPOSSIBLE		0
#define HRTIMER_POSSIBLE		1
#define VIRTUAL_COORDINATE_WIDTH		32768
#define VIRTUAL_COORDINATE_HEIGHT		32768
#define IS_CIS_REV_MAX_LIST		3
#define VENDOR_SOFT_LANDING_STEP_MAX	2

enum cis_dual_sync_mode {
	DUAL_SYNC_NONE = 0,
	DUAL_SYNC_MASTER,
	DUAL_SYNC_SLAVE,
	DUAL_SYNC_STANDALONE,
	DUAL_SYNC_STREAMOFF,
	DUAL_SYNC_MAX,
};

enum cis_stream_state {
	CIS_STREAM_INIT = 0,
	CIS_STREAM_SET_DONE,
	CIS_STREAM_SET_ERR,
	CIS_STREAM_MAX,
};

enum cis_off_work_state {
	CIS_OFF_WORK_NONE = 0,
	CIS_OFF_WORK_REQUEST,
	CIS_OFF_WORK_READY,
	CIS_OFF_WORK_MAX,
};

struct is_cis {
	u32				id;
	struct v4l2_subdev		*subdev; /* connected module subdevice */
	u32				device; /* connected sensor device */
	struct i2c_client		*client;

	cis_shared_data			*cis_data;
	struct is_cis_ops		*cis_ops;
	enum otf_input_order		bayer_order;
	u32				aperture_num;
	bool				use_dgain;
	bool				hdr_ctrl_by_again;
	bool				use_wb_gain;
	bool				use_3hdr;
	bool				use_vendor_total_gain;

	struct is_sensor_ctl	sensor_ctls[CAM2P0_UCTL_LIST_SIZE];

	/* store current ctrl */
	camera2_sensor_uctl_t		cur_sensor_uctrl;

	/* settings for mode change */
	bool					need_mode_change;
	enum is_exposure_gain_count		exp_gain_cnt;
	ae_setting				mode_chg;
	struct wb_gains			mode_chg_wb_gains;

	/* expected dms */
	camera2_lens_dm_t		expecting_lens_dm[EXPECT_DM_NUM];
	camera2_sensor_dm_t		expecting_sensor_dm[EXPECT_DM_NUM];
#ifdef USE_OIS_HALL_DATA_FOR_VDIS
	camera2_aa_dm_t			expecting_aa_dm[EXPECT_DM_NUM];
#endif

	/* expected udm */
	camera2_lens_udm_t		expecting_lens_udm[EXPECT_DM_NUM];
	camera2_sensor_udm_t		expecting_sensor_udm[EXPECT_DM_NUM];

	/* For sensor status dump */
	struct work_struct		cis_status_dump_work;

	/* one more check_rev in mode_change */
	bool				rev_flag;

	/* For sensor revision checking */
	u32 				rev_addr;
	u32 				rev_byte;
	u32 				rev_valid_count;
	u32 				rev_valid_values[IS_CIS_REV_MAX_LIST];

	/* get a min, max fps to HAL */
	u32				min_fps;
	u32				max_fps;
	struct mutex			*i2c_lock;
	struct mutex			control_lock;
	bool				use_pdaf;

	/* Long Term Exposure Mode(LTE mode) structure */
	struct is_long_term_expo_mode	long_term_mode;
	struct work_struct			long_term_mode_work;
	bool lte_work_enable;

	/* sensor control delay(N+1 or N+2) */
	u32				ctrl_delay;

#ifdef USE_CAMERA_ADAPTIVE_MIPI_RUNTIME
	struct work_struct				mipi_clock_change_work;
#endif
	int				mipi_clock_index_new;
	int				mipi_clock_index_cur;
	const struct cam_mipi_sensor_mode *mipi_sensor_mode;
	u32				mipi_sensor_mode_size;
	bool			vendor_use_adaptive_mipi;

	u32				ae_exposure;
	u32				ae_deltaev;
#ifdef USE_CAMERA_FACTORY_DRAM_TEST
	struct work_struct				factory_dramtest_work;
	u32				factory_dramtest_section2_fcount;
#endif

	/* settings for initial AE */
	bool				use_initial_ae;
	ae_setting			init_ae_setting;
	ae_setting			last_ae_setting;

	/* settings for sensor stat */
	struct sensor_lsi_3hdr_stat_control_mode_change		lsi_sensor_stats;
	struct sensor_imx_3hdr_stat_control_mode_change		imx_sensor_stats;
	bool							sensor_stats;
	/* setting for 3HDR LSC */
	struct 				sensor_imx_3hdr_lsc_table_init imx_lsc_table_3hdr;
	bool				lsc_table_status;

	/* dual sync mode */
	u32				dual_sync_mode;
	u32				dual_sync_work_mode;
	struct work_struct		dual_sync_mode_work;

	/* check i2c fail of global/mode setting */
	enum cis_stream_state           stream_state;

	struct work_struct		global_setting_work;
	struct work_struct		mode_setting_work;

	/* cis_off work for streaming off onto vsync */
	struct work_struct		cis_off_work;
	int				cis_off_work_state;
};

struct is_actuator_data {
	struct timer_list		timer_wait;
	u32				check_time_out;

	bool				actuator_init;

	/* M2M AF */
	struct hrtimer              	afwindow_timer;
	struct work_struct		actuator_work;
	u32				timer_check;
};

struct is_actuator {
	u32			id;
	struct v4l2_subdev	*subdev; /* connected module subdevice */
	u32                     device; /* connected sensor device */
	struct i2c_client       *client;

	u32			position;
	u32			max_position;

	/* for M2M AF */
	struct timeval		start_time;
	struct timeval		end_time;
	u32			valid_flag;
	ulong			valid_time;

	/* softlanding */
	bool			need_softlanding;

	/* WinAf value for M2M AF */
	u32                     left_x;
	u32                     left_y;
	u32                     right_x;
	u32                     right_y;

	int                     actuator_index;

	u32                     pre_position[EXPECT_DM_NUM];
	u32                     pre_frame_cnt[EXPECT_DM_NUM];

	enum is_actuator_pos_size_bit	pos_size_bit;
	enum is_actuator_direction		pos_direction;

	struct is_actuator_data		actuator_data;
	struct is_device_sensor_peri	*sensor_peri;
	struct is_actuator_ops		*actuator_ops;
	struct mutex            *i2c_lock;
	struct work_struct			actuator_active_on;
	struct work_struct			actuator_active_off;

	u32				vendor_product_id;
	u32				vendor_sleep_to_standby_delay;
	u32				vendor_active_to_standby_delay;
	u32				vendor_first_pos;
	u32				vendor_first_delay;
	u32				vendor_soft_landing_list[VENDOR_SOFT_LANDING_STEP_MAX * 2];
	u32				vendor_soft_landing_list_len;
	u32				vendor_soft_landing_seqid;
	bool				vendor_use_sleep_mode;
	bool				vendor_use_standby_mode;
	bool				vendor_use_temperature;
	bool				vendor_use_voltage;

	bool                    init_cal_setting;
	bool					actual_pos_support;

	void					*priv_info;

	int						temperature;
	int						voltage;
	bool					temperature_available;
	bool					voltage_available;
	enum is_actuator_state	state;
};

struct is_aperture {
	u32				id;
	struct v4l2_subdev		*subdev; /* connected module subdevice */
	u32				device; /* connected sensor device */
	struct i2c_client		*client;
	struct is_aperture_ops		*aperture_ops;
	struct is_device_sensor_peri	*sensor_peri;
	struct mutex				*i2c_lock;
	struct mutex			control_lock;
	int				new_value;
	int				cur_value; /* need to mode when aperture value change */
	int				start_value;
	enum is_aperture_control_step	step;
	struct work_struct		aperture_set_start_work;
	struct work_struct		aperture_set_work;
};

struct is_eeprom {
	u32				id;
	struct v4l2_subdev			*subdev; /* connected module subdevice */
	u32					device; /* connected sensor device */
	struct i2c_client			*client;
	struct is_eeprom_ops		*eeprom_ops;
	struct is_device_sensor_peri	*sensor_peri;
	struct mutex				*i2c_lock;

	char					*data;
	u32					total_size;

	/* for use runtime module selection */
	u32					module_sel_cnt;
	u32					identifier_idx;
	u32					*module_sel_sensor_id;
	u32					*module_sel_val;

	/* for use vendor specific feature */
	u32					lsc_flipmirror_variation;
};

#define FLASH_LED_CH_MAX       (4)
struct is_flash_data {
	enum flash_mode			mode;
	u32				intensity;
	u32				firing_time_us;
	bool				flash_fired;
	bool				high_resolution_flash;
	struct work_struct		flash_fire_work;
	struct timer_list		flash_expire_timer;
	struct work_struct		flash_expire_work;

	/* used for LED calibration */
	u32				cal_input_curr[FLASH_LED_CH_MAX];
	bool				led_cal_en;
};

struct is_flash {
	u32				id;
	struct v4l2_subdev		*subdev; /* connected module subdevice */
	u32				device; /* connected sensor device */
	struct i2c_client		*client;

	int				flash_gpio;
	int				torch_gpio;

	/* for select led channel */
	int				led_ch[FLASH_LED_CH_MAX];

	struct is_flash_data	flash_data;
	struct is_flash_expo_gain  flash_ae;

#ifdef CONFIG_CAMERA_FLASH_I2C_OBJ
	struct notifier_block		flash_noti_ta;
	int				attach_ta;
	int				attach_sdp;
#endif
	struct is_device_sensor_peri	*sensor_peri;

	/* expecting dm */
	camera2_flash_dm_t		expecting_flash_dm[EXPECT_DM_NUM];

	struct mutex			control_lock;
};

struct is_ois {
	u32				id;
	u32				device; /* connected sensor device */
	u32				ois_mode; /* need to mode when ois mode change */
	u32				pre_ois_mode; /* need to mode when ois mode change */
	bool				ois_shift_available;
	bool				ois_shift_available_rear2; /* need to mode when ois mode change */
	struct v4l2_subdev		*subdev; /* connected module subdevice */
	struct i2c_client		*client;
	struct is_ois_ops		*ois_ops;
	struct is_device_sensor_peri	*sensor_peri;
	struct mutex				*i2c_lock;
	u8				coef;
	u8				pre_coef;
	bool				fadeupdown;
	bool				initial_centering_mode;
	int				ois_power_mode;
	struct work_struct			ois_set_init_work;
	struct work_struct			ois_set_deinit_work;
	int				af_pos_wide;
	int				af_pos_tele;
	int				af_pos_tele2;
#ifdef USE_OIS_INIT_WORK
	struct work_struct		init_work;
#endif
};

struct is_mcu {
	struct v4l2_subdev			*subdev; /* connected module subdevice */
	struct i2c_client 			*client;
	u32						id;
	u32						device; /* connected sensor device */
	u32						ver;
	u32						name;
	u32						mcu_ctrl_actuator;
	u8						vdrinfo_bin[4];
	u8						hw_bin[4];
	u8						vdrinfo_mcu[4];
	u8						hw_mcu[4];
	char						load_fw_name[50];
	bool						support_photo_fastae;
	bool						skip_video_fastae;
	bool						off_during_uwonly_mode;
	struct is_ois			*ois;
	struct v4l2_subdev			*subdev_ois;
	struct is_actuator 			*actuator;
	struct v4l2_subdev 			*subdev_actuator;
	struct is_device_ois	*ois_device;
	struct is_aperture		*aperture;
	struct v4l2_subdev		*subdev_aperture;
	struct is_aperture_ops		*aperture_ops;
	struct is_device_sensor_peri	*sensor_peri;
	struct mutex				*i2c_lock;
	void						*private_data;
};

struct is_laser_af_data {
	void *data;
	u32 size;
};

struct is_laser_af {
	u32				id;
	struct v4l2_subdev		*subdev; /*  connected module subdevice */
	u32				device; /*  connected sensor device */

	struct is_laser_af_ops		*laser_af_ops;
	enum camera2_range_sensor_mode	rs_mode;

	struct is_device_sensor_peri	*sensor_peri;
	struct mutex			*laser_lock;
	bool				active;
};

struct paf_action {
	enum itf_vc_stat_type	type;
	vc_dma_notifier_t		notifier;
	void			*data;
	unsigned int		flags;
	const char		*name;
	struct list_head	list;
};

struct is_pdp_ops {
	/* common paf interface */
	int (*set_param)(struct v4l2_subdev *subdev,
			struct paf_setting_t *regs, u32 regs_size);
	int (*get_ready)(struct v4l2_subdev *subdev, u32 *ready);
	int (*register_notifier)(struct v4l2_subdev *subdev,
			enum itf_vc_stat_type type,
			vc_dma_notifier_t notifier, void *data);
	int (*unregister_notifier)(struct v4l2_subdev *subdev,
			enum itf_vc_stat_type type,
			vc_dma_notifier_t notifier);
	void (*notify)(struct v4l2_subdev *subdev,
			unsigned int type,
			void *data);
};

struct is_pafstat_ops {
	/* common paf interface ops */
	int (*set_param)(struct v4l2_subdev *subdev,
			struct paf_setting_t *regs, u32 regs_size);
	int (*get_ready)(struct v4l2_subdev *subdev, u32 *ready);
	int (*register_notifier)(struct v4l2_subdev *subdev,
			enum itf_vc_stat_type type,
			vc_dma_notifier_t notifier, void *data);
	int (*unregister_notifier)(struct v4l2_subdev *subdev,
			enum itf_vc_stat_type type,
			vc_dma_notifier_t notifier);
	void (*notify)(struct v4l2_subdev *subdev,
			unsigned int type,
			void *data);

	/* device specific ops */
	int (*set_num_buffers)(struct v4l2_subdev *subdev,
			u32 num_buffers, struct is_device_sensor *sensor);
};

struct is_pafstat {
	u32				id;	/* 0: context0, 1: context1 */
	void __iomem			*regs_com;
	void __iomem			*regs;
	resource_size_t			regs_start;
	resource_size_t			regs_end;
	u8				*sfr_dump;
	void __iomem			*regs_b;
	resource_size_t			regs_b_start;
	resource_size_t			regs_b_end;
	u8				*sfr_b_dump;

	atomic_t			sfr_state;
	atomic_t			fs;
	atomic_t			cl;
	atomic_t			fe;
	atomic_t			fe_img;
	atomic_t			fe_stat;
	int				irq;
	atomic_t			Vvalid;
	wait_queue_head_t		wait_queue;

	/* 0: MSPD normal
	   1: 2PD mode 1
	   2: 2PD mode 2
	   3: 2PD mode 3
	   4: MSPD tail */
	u32				sensor_mode;
	size_t				in_width;
	size_t				in_height;
	size_t				pd_width;
	size_t				pd_height;
	u32				fro_cnt;

	u32				regs_max;
	struct paf_setting_t		*regs_set;
	struct is_pafstat_ops	*pafstat_ops;
	struct v4l2_subdev		*subdev; /* connected module subdevice */
	char				name[IS_STR_LEN];

	spinlock_t			slock_paf_action;
	struct list_head		list_of_paf_action;

	struct tasklet_struct		tasklet_fwin_stat;
	atomic_t			frameptr_fwin_stat;
	struct workqueue_struct		*wq_fwin_stat;
	struct work_struct		work_fwin_stat;
};

struct is_device_sensor_peri {
	struct is_module_enum	*module;

	struct is_cis		cis;
	struct v4l2_subdev		*subdev_cis;

	struct is_actuator		*actuator;
	struct v4l2_subdev		*subdev_actuator;

	struct is_flash		*flash;
	struct v4l2_subdev		*subdev_flash;

	struct is_ois		*ois;
	struct v4l2_subdev		*subdev_ois;

	struct is_pdp		*pdp;
	struct v4l2_subdev		*subdev_pdp;

	struct is_pafstat		*pafstat;
	struct v4l2_subdev		*subdev_pafstat;

	struct is_aperture		*aperture;
	struct v4l2_subdev		*subdev_aperture;

	struct is_mcu		*mcu;
	struct v4l2_subdev		*subdev_mcu;

	struct is_eeprom		*eeprom;
	struct v4l2_subdev		*subdev_eeprom;

	struct is_laser_af		*laser_af;
	struct v4l2_subdev		*subdev_laser_af;

	unsigned long			peri_state;

	/* Thread for sensor and high spped recording setting */
	bool				use_sensor_work;
	spinlock_t			sensor_work_lock;
	struct task_struct		*sensor_task;
	struct kthread_worker		sensor_worker;
	struct kthread_work		sensor_work;

	/* Variable for checking global setting done */
	bool				cis_global_complete;

	/* Variable for checking auto framing scenario */
	bool				check_auto_framing;

	/* first sensor mode setting flag */
        u32                             mode_change_first;

	struct is_sensor_interface		sensor_interface;
	int						reuse_3a_value;
};

void is_sensor_work_fn(struct kthread_work *work);
void is_sensor_global_setting_work_fn(struct kthread_work *work);
void is_sensor_mode_change_work_fn(struct kthread_work *work);
int is_sensor_init_sensor_thread(struct is_device_sensor_peri *sensor_peri);
void is_sensor_deinit_sensor_thread(struct is_device_sensor_peri *sensor_peri);
int is_sensor_init_mode_change_thread(struct is_device_sensor_peri *sensor_peri);
void is_sensor_deinit_mode_change_thread(struct is_device_sensor_peri *sensor_peri);

struct is_device_sensor_peri *find_peri_by_cis_id(struct is_device_sensor *device,
							u32 cis);
struct is_device_sensor_peri *find_peri_by_act_id(struct is_device_sensor *device,
							u32 actuator);
struct is_device_sensor_peri *find_peri_by_flash_id(struct is_device_sensor *device,
							u32 flash);
struct is_device_sensor_peri *find_peri_by_ois_id(struct is_device_sensor *device,
							u32 ois);
struct is_device_sensor_peri *find_peri_by_eeprom_id(struct is_device_sensor *device,
							u32 eeprom);
struct is_device_sensor_peri *find_peri_by_laser_af_id(struct is_device_sensor *device,
							u32 laser_af);

void is_sensor_set_cis_uctrl_list(struct is_device_sensor_peri *sensor_peri,
		enum is_exposure_gain_count num_data,
		u32 *exposure,
		u32 *total_gain,
		u32 *analog_gain,
		u32 *digital_gain);

int is_sensor_peri_notify_vsync(struct v4l2_subdev *subdev, void *arg);
int is_sensor_peri_notify_vblank(struct v4l2_subdev *subdev, void *arg);
int is_sensor_peri_notify_flash_fire(struct v4l2_subdev *subdev, void *arg);
int is_sensor_peri_pre_flash_fire(struct v4l2_subdev *subdev, void *arg);
int is_sensor_peri_notify_actuator(struct v4l2_subdev *subdev, void *arg);
int is_sensor_peri_notify_m2m_actuator(void *arg);
int is_sensor_peri_notify_actuator_init(struct v4l2_subdev *subdev);

int is_sensor_peri_call_m2m_actuator(struct is_device_sensor *device);
int is_sensor_initial_preprocessor_setting(struct is_device_sensor_peri *sensor_peri);

enum hrtimer_restart is_actuator_m2m_af_set(struct hrtimer *timer);

int is_actuator_notify_m2m_actuator(struct v4l2_subdev *subdev);

void is_sensor_peri_probe(struct is_device_sensor_peri *sensor_peri);
int is_sensor_peri_s_stream(struct is_device_sensor *device,
					bool on);

int is_sensor_peri_s_mode_change(struct is_device_sensor *device,
				struct seamless_mode_change_info *mode_change);
int is_sensor_peri_s_frame_duration(struct is_device_sensor *device,
				u32 frame_duration);
int is_sensor_peri_s_exposure_time(struct is_device_sensor *device,
				struct ae_param expo);
int is_sensor_peri_s_analog_gain(struct is_device_sensor *device,
				struct ae_param again);
int is_sensor_peri_s_digital_gain(struct is_device_sensor *device,
				struct ae_param dgain);
int is_sensor_peri_s_totalgain(struct is_device_sensor *device,
				struct ae_param expo,
				struct ae_param again,
				struct ae_param dgain);
int is_sensor_peri_s_wb_gains(struct is_device_sensor *device,
				struct wb_gains wb_gains);
int is_sensor_set_test_pattern(struct is_device_sensor *device,
				camera2_sensor_ctl_t *sensor_ctl);
int is_sensor_peri_s_sensor_stats(struct is_device_sensor *device,
				bool streaming,
				struct is_sensor_ctl *module_ctl,
				void *data);
int is_sensor_peri_adj_ctrl(struct is_device_sensor *device,
				u32 input, struct v4l2_control *ctrl);

int is_sensor_peri_compensate_gain_for_ext_br(struct is_device_sensor *device,
				u32 expo, u32 *again, u32 *dgain);

int is_sensor_peri_actuator_softlanding(struct is_device_sensor_peri *device);

int is_sensor_peri_debug_fixed(struct is_device_sensor *device);

void is_sensor_flash_fire_work(struct work_struct *data);
void is_sensor_flash_expire_work(struct work_struct *data);
int is_sensor_flash_fire(struct is_device_sensor_peri *device,
				u32 on);

void is_sensor_actuator_soft_move(struct work_struct *data);

void is_sensor_long_term_mode_set_work(struct work_struct *data);

int is_sensor_mode_change(struct is_cis *cis, u32 mode);
void is_sensor_peri_init_work(struct is_device_sensor_peri *sensor_peri);

#define CALL_CISOPS(s, op, args...) (((s)->cis_ops->op) ? ((s)->cis_ops->op(args)) : 0)
#define CALL_OISOPS(s, op, args...) (((s)->ois_ops->op) ? ((s)->ois_ops->op(args)) : 0)
#define CALL_ACTUATOROPS(s, op, args...) (((s)->actuator_ops->op) ? ((s)->actuator_ops->op(args)) : 0)
#define CALL_APERTUREOPS(s, op, args...) (((s)->aperture_ops->op) ? ((s)->aperture_ops->op(args)) : 0)
#define CALL_PDPOPS(s, op, args...) (((s)->pdp_ops->op) ? ((s)->pdp_ops->op(args)) : 0)
#define CALL_PAFSTATOPS(s, op, args...) (((s)->pafstat_ops->op) ? ((s)->pafstat_ops->op(args)) : 0)
#define CALL_EEPROMOPS(s, op, args...) (((s)->eeprom_ops->op) ? ((s)->eeprom_ops->op(args)) : 0)
#define CALL_LASEROPS(s, op, args...) (((s)->laser_af_ops->op) ? ((s)->laser_af_ops->op(args)) : 0)
#endif
