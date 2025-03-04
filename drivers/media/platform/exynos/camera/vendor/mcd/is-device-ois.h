/*
 * Samsung Exynos5 SoC series FIMC-IS OIS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_DEVICE_OIS_H
#define IS_DEVICE_OIS_H

struct is_ois_gpio {
	char *sda;
	char *scl;
	char *pinname;
};

struct is_device_ois {
	struct v4l2_device				v4l2_dev;
	struct platform_device				*pdev;
	unsigned long					state;
	struct exynos_platform_is_sensor		*pdata;
	struct i2c_client				*client;
	struct is_ois_ops				*ois_ops;
	int ois_en;
	bool ois_hsi2c_status;
	bool not_crc_bin;
};

struct is_ois_exif {
	int error_data;
	int status_data;
};

struct ois_rom_data {
	char xgg[5];
	char ygg[5];
	char xcoef[3];
	char ycoef[3];
	char supperssion_xratio[3];
	char supperssion_yratio[3];
	u8 cal_mark[2];
};

struct is_ois_info {
	char header_ver[9];
	char load_fw_name[50];
	struct ois_rom_data wide_romdata;
	struct ois_rom_data tele_romdata;
	struct ois_rom_data tele2_romdata;
	struct ois_rom_data tele_tilt_romdata;
	u8 checksum;
	u8 caldata;
	bool reset_check;
};

#ifdef USE_OIS_SLEEP_MODE
struct is_ois_shared_info {
	u8 oissel;
};
#endif

#define FIMC_OIS_FW_NAME_SEC		"ois_fw_sec.bin"
#define FIMC_OIS_FW_NAME_SEC_2		"ois_fw_sec_2.bin"
#define FIMC_OIS_FW_NAME_DOM		"ois_fw_dom.bin"
#define IS_OIS_SDCARD_PATH		"/data/vendor/camera/"

struct i2c_client *is_ois_i2c_get_client(struct is_core *core);
int is_ois_i2c_read(struct i2c_client *client, u16 addr, u8 *data);
int is_ois_i2c_write(struct i2c_client *client ,u16 addr, u8 data);
int is_ois_i2c_write_multi(struct i2c_client *client ,u16 addr, u8 *data, size_t size);
int is_ois_i2c_read_multi(struct i2c_client *client, u16 addr, u8 *data, size_t size);
void is_ois_enable(struct is_core *core);
bool is_ois_offset_test(struct is_core *core, long *raw_data_x, long *raw_data_y);
int is_ois_self_test(struct is_core *core);
bool is_ois_check_sensor(struct is_core *core);
int is_ois_gpio_on(struct is_core *core);
int is_ois_gpio_off(struct is_core *core);
int is_ois_get_module_version(struct is_ois_info **minfo);
int is_ois_get_phone_version(struct is_ois_info **minfo);
int is_ois_get_user_version(struct is_ois_info **uinfo);
bool is_ois_version_compare(char *fw_ver1, char *fw_ver2, char *fw_ver3);
bool is_ois_version_compare_default(char *fw_ver1, char *fw_ver2);
void is_ois_get_offset_data(struct is_core *core, long *raw_data_x, long *raw_data_y);
bool is_ois_diff_test(struct is_core *core, int *x_diff, int *y_diff);
bool is_ois_crc_check(struct is_core *core, char *buf);
u16 is_ois_calc_checksum(u8 *data, int size);

void is_ois_exif_data(struct is_core *core);
int is_ois_get_exif_data(struct is_ois_exif **exif_info);
void is_ois_fw_status(struct is_core *core);
void is_ois_fw_update(struct is_core *core);
void is_ois_fw_update_from_sensor(void *ois_core);
bool is_ois_check_fw(struct is_core *core);
bool is_ois_auto_test(struct is_core *core,
				int threshold, bool *x_result, bool *y_result, int *sin_x, int *sin_y,
				bool *x_result_2nd, bool *y_result_2nd, int *sin_x_2nd, int *sin_y_2nd,
				bool *x_result_3rd, bool *y_result_3rd, int *sin_x_3rd, int *sin_y_3rd);
bool is_ois_read_fw_ver(struct is_core *core, char *name, char *ver);
#ifdef CAMERA_2ND_OIS
bool is_ois_auto_test_rear2(struct is_core *core,
				int threshold, bool *x_result, bool *y_result, int *sin_x, int *sin_y,
				bool *x_result_2nd, bool *y_result_2nd, int *sin_x_2nd, int *sin_y_2nd);
#endif
void is_ois_gyro_sleep(struct is_core *core);
#ifdef USE_OIS_SLEEP_MODE
void is_ois_set_oissel_info(int oissel);
int is_ois_get_oissel_info(void);
#endif
int is_sec_get_ois_minfo(struct is_ois_info **minfo);
int is_sec_get_ois_pinfo(struct is_ois_info **pinfo);
#if defined(CONFIG_CAMERA_USE_INTERNAL_MCU)
bool is_ois_gyrocal_test(struct is_core *core, long *raw_data_x, long *raw_data_y);
#endif
bool is_ois_gyronoise_test(struct is_core *core, long *raw_data_x, long *raw_data_y);
void is_ois_get_hall_pos(struct is_core *core, u16 *targetPos, u16 *hallPos);
void is_ois_set_mode(struct is_core *core, int mode);
void is_ois_check_cross_talk(struct is_core *core, u16 *hall_data);
void is_ois_check_hall_cal(struct is_core *core, u16 *hall_cal_data);
void is_ois_check_valid(struct is_core *core, u8 *value);
int is_ois_read_ext_clock(struct is_core *core, u32 *clock);
void is_ois_init_rear2(struct is_core *core);
void is_ois_init_factory(struct is_core *core);
void is_ois_parsing_raw_data(struct is_core *core, uint8_t *buf, long efs_size, long *raw_data_x, long *raw_data_y);
int is_ois_control_gpio(struct is_core *core, int position, int onoff);
#endif
