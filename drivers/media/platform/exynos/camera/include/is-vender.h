/*
* Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is vender functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_VENDER_H
#define IS_VENDER_H

#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/videodev2.h>
#include <linux/device.h>
#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

#include "exynos-is-sensor.h"
#include "is-device-csi.h"
#include "is-device-sensor.h"
#include "is-vendor-mipi.h"

#define IS_PATH_LEN 100
#define VENDER_S_CTRL 0
#define VENDER_G_CTRL 0

#ifndef EV_LIST_SIZE
#define EV_LIST_SIZE  24
#endif

#ifndef MANUAL_LIST_SIZE
#define MANUAL_LIST_SIZE  5
#endif

struct is_vender {
	char fw_path[IS_PATH_LEN];
	char request_fw_path[IS_PATH_LEN];
	char setfile_path[SENSOR_POSITION_MAX][IS_PATH_LEN];
	char request_setfile_path[SENSOR_POSITION_MAX][IS_PATH_LEN];
	void *private_data;
	int companion_crc_error;
	int opening_hint;
	int closing_hint;
	int isp_max_width;
	int isp_max_height;
	int statndby_sensorid;  /* for block shared_rsc_count initializing*/
};

enum {
	FW_SKIP,
	FW_SUCCESS,
	FW_FAIL,
};

enum is_rom_id {
	ROM_ID_REAR		= 0,
	ROM_ID_FRONT	= 1,
	ROM_ID_REAR2	= 2,
	ROM_ID_FRONT2	= 3,
	ROM_ID_REAR3	= 4,
	ROM_ID_FRONT3	= 5,
	ROM_ID_REAR4	= 6,
	ROM_ID_FRONT4	= 7,
	ROM_ID_MAX,
	ROM_ID_NOTHING	= 100
};

enum is_rom_type {
	ROM_TYPE_NONE	= 0,
	ROM_TYPE_FROM	= 1,
	ROM_TYPE_EEPROM	= 2,
	ROM_TYPE_OTPROM	= 3,
	ROM_TYPE_MAX,
};

enum is_rom_cal_index {
	ROM_CAL_MASTER	= 0,
	ROM_CAL_SLAVE0	= 1,
	ROM_CAL_SLAVE1	= 2,
	ROM_CAL_MAX,
	ROM_CAL_NOTHING	= 100
};

enum is_rom_dualcal_index {
	ROM_DUALCAL_SLAVE0	= 0,
	ROM_DUALCAL_SLAVE1	= 1,
	ROM_DUALCAL_SLAVE2	= 2,
	ROM_DUALCAL_SLAVE3	= 3,
	ROM_DUALCAL_MAX,
	ROM_DUALCAL_NOTHING	= 100
};

#define AF_CAL_MAX 9

#define TOF_AF_SIZE 1200

struct tof_data_t {
	u64 timestamp;
	u32 width;
	u32 height;
	u16 data[TOF_AF_SIZE];
	int AIFCaptureNum;
};

struct tof_info_t {
	u16 TOFExposure;
	u16 TOFFps;
};

struct capture_intent_info_t {
	u16 captureIntent;
	u16 captureCount;
	s16 captureEV;
	u32 captureIso;
	u16 captureAeExtraMode;
	char captureMultiEVList[EV_LIST_SIZE];
	u32 captureMultiIsoList[MANUAL_LIST_SIZE];
	u32 CaptureMultiExposureList[MANUAL_LIST_SIZE];
	u16 captureAeMode;
};

#define TOF_CAL_SIZE_MAX 10
#define TOF_CAL_VALID_MAX 10

#define CROSSTALK_CAL_MAX (3 * 13)

#ifdef USE_CAMERA_HW_BIG_DATA
#define CAM_HW_ERR_CNT_FILE_PATH "/data/vendor/camera/camera_hw_err_cnt.dat"

struct cam_hw_param {
	u32 i2c_sensor_err_cnt;
	u32 i2c_comp_err_cnt;
	u32 i2c_ois_err_cnt;
	u32 i2c_af_err_cnt;
	u32 i2c_aperture_err_cnt;
	u32 mipi_sensor_err_cnt;
	u32 mipi_comp_err_cnt;
} __attribute__((__packed__));

struct cam_hw_param_collector {
	struct cam_hw_param rear_hwparam;
	struct cam_hw_param rear2_hwparam;
	struct cam_hw_param rear3_hwparam;
	struct cam_hw_param rear4_hwparam;
	struct cam_hw_param front_hwparam;
	struct cam_hw_param front2_hwparam;
	struct cam_hw_param rear_tof_hwparam;
	struct cam_hw_param front_tof_hwparam;
	struct cam_hw_param iris_hwparam;
} __attribute__((__packed__));

void is_sec_init_err_cnt(struct cam_hw_param *hw_param);
void is_sec_get_hw_param(struct cam_hw_param **hw_param, u32 position);
#ifdef CAMERA_HW_BIG_DATA_FILE_IO
bool is_sec_need_update_to_file(void);
void is_sec_copy_err_cnt_from_file(void);
void is_sec_copy_err_cnt_to_file(void);
#endif /* CAMERA_HW_BIG_DATA_FILE_IO */
#endif /* USE_CAMERA_HW_BIG_DATA */
bool is_sec_is_valid_moduleid(char *moduleid);

void is_vendor_csi_stream_on(struct is_device_csi *csi);
void is_vendor_csi_stream_off(struct is_device_csi *csi);
void is_vender_csi_err_handler(struct is_device_csi *csi);
void is_vender_csi_err_print_debug_log(struct is_device_sensor *device);

int is_vender_probe(struct is_vender *vender);
#ifdef MODULE
int is_vender_driver_init(void);
int is_vender_driver_exit(void);
#endif
int is_vender_dt(struct device_node *np);
int is_vendor_rom_parse_dt(struct device_node *dnode, int rom_id);
int is_vender_fw_prepare(struct is_vender *vender);
int is_vender_fw_filp_open(struct is_vender *vender, struct file **fp, int bin_type);
int is_vender_preproc_fw_load(struct is_vender *vender);
int is_vender_s_ctrl(struct is_vender *vender);
int is_vender_cal_load(struct is_vender *vender, void *module_data);
int is_vender_module_sel(struct is_vender *vender, void *module_data);
int is_vender_module_del(struct is_vender *vender, void *module_data);
int is_vender_fw_sel(struct is_vender *vender);
int is_vender_setfile_sel(struct is_vender *vender, char *setfile_name, int position);
int is_vender_preprocessor_gpio_on_sel(struct is_vender *vender, u32 scenario, u32 *gpio_scenario);
int is_vender_preprocessor_gpio_on(struct is_vender *vender, u32 scenario, u32 gpio_scenario);
int is_vender_sensor_gpio_on_sel(struct is_vender *vender, u32 scenario, u32 *gpio_scenario, void *module_data);
int is_vender_sensor_gpio_on(struct is_vender *vender, u32 scenario, u32 gpio_scenario, void *module_data);
int is_vender_preprocessor_gpio_off_sel(struct is_vender *vender, u32 scenario, u32 *gpio_scenario,
	void *module_data);
int is_vender_preprocessor_gpio_off(struct is_vender *vender, u32 scenario, u32 gpio_scenario);
int is_vender_sensor_gpio_off_sel(struct is_vender *vender, u32 scenario, u32 *gpio_scenario,
	void *module_data);
int is_vender_sensor_gpio_off(struct is_vender *vender, u32 scenario, u32 gpio_scenario, void *module_data);
void is_vendor_sensor_suspend(struct platform_device *pdev);
#ifdef USE_CAMERA_SENSOR_RETENTION
void is_vender_check_retention(struct is_vender *vender, void *module_data);
#endif
void is_vender_itf_open(struct is_vender *vender, struct sensor_open_extended *ext_info);
int is_vender_set_torch(struct camera2_shot *shot);
void is_vender_update_meta(struct is_vender *vender, struct camera2_shot *shot);
int is_vender_video_s_ctrl(struct v4l2_control *ctrl, void *device_data);
int is_vender_ssx_video_s_ctrl(struct v4l2_control *ctrl, void *device_data);
int is_vender_ssx_video_g_ctrl(struct v4l2_control *ctrl, void *device_data);
int is_vender_ssx_video_s_ext_ctrl(struct v4l2_ext_controls *ctrls, void *device_data);
int is_vender_hw_init(struct is_vender *vender);
void is_vender_check_hw_init_running(void);
void is_vender_sensor_s_input(struct is_vender *vender, void *module);
bool is_vender_wdr_mode_on(void *cis_data);
bool is_vender_aeb_mode_on(void *cis_data);
bool is_vender_enable_wdr(void *cis_data);
int is_vender_request_binary(struct is_binary * bin, const char * path1, const char * path2, const char * name, struct device * device);
void is_vender_resource_get(struct is_vender *vender, u32 rsc_type);
void is_vender_resource_put(struct is_vender *vender, u32 rsc_type);
void is_vender_mcu_power_on(bool use_shared_rsc);
void is_vender_mcu_power_off(bool use_shared_rsc);
long is_vender_read_efs(char *efs_path, u8 *buf, int buflen);
int is_vender_remove_dump_fw_file(void);
int is_vendor_get_module_from_position(int position, struct is_module_enum ** module);
int is_vendor_get_rom_id_from_position(int position);
int is_vendor_get_sensor_id_from_position(int position);
int is_vendor_get_position_from_rom_id(int rom_id);
void is_vendor_get_rom_info_from_position(int position, int *rom_type, int *rom_id, int *rom_cal_index);
void is_vendor_get_rom_dualcal_info_from_position(int position, int *rom_type, int *rom_dualcal_id, int *rom_dualcal_index);
bool is_vendor_check_camera_running(int position);
#ifdef USE_TOF_AF
void is_vender_store_af(struct is_vender *vender, struct tof_data_t *data);
#endif
void is_vendor_store_tof_info(struct is_vender *vender, struct tof_info_t *info);
int is_vendor_shaking_gpio_on(struct is_vender *vender);
int is_vendor_shaking_gpio_off(struct is_vender *vender);

int is_vendor_set_mipi_clock(struct is_device_sensor *device);
int is_vendor_get_mipi_clock_string(struct is_device_sensor *device, char *cur_mipi_str);
int is_vendor_update_mipi_info(struct is_device_sensor *device);
#endif
