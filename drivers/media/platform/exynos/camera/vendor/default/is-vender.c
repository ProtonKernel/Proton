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

#include <linux/slab.h>

#include "is-vender.h"
#include "is-vender-specific.h"
#include "is-core.h"
#include "is-interface-library.h"
#include "is-device-sensor-peri.h"

extern int is_create_sysfs(void);

static u32  rear_sensor_id;
static u32  front_sensor_id;
static u32  rear2_sensor_id;
#if IS_ENABLED(CONFIG_SECURE_CAMERA_USE)
static u32  secure_sensor_id;
#endif
static u32  front2_sensor_id;
static u32  rear3_sensor_id;
static u32  rear4_sensor_id;
static u32  ois_sensor_index;
static u32  aperture_sensor_index;

static void is_vendor_get_rf_channel(struct cam_cp_noti_info *ch)
{

}

static void is_vendor_get_legacy_rf_channel(struct cam_cp_noti_info *ch)
{

}

static int compare_rf_channel(const void *key, const void *element)
{
	return 0;
}

int is_vendor_select_mipi_by_rf_channel(const struct cam_mipi_channel *channel_list, const int size)
{
	return 0;
}

int is_vendor_verify_mipi_channel(const struct cam_mipi_channel *channel_list, const int size)
{
	return 0;
}

int is_vendor_set_mipi_clock(struct is_device_sensor *device)
{
	return 0;
}

int is_vendor_get_mipi_clock_string(struct is_device_sensor *device, char *cur_mipi_str)
{
	return 0;
}

int is_vendor_update_mipi_info(struct is_device_sensor *device)
{
	return 0;
}

void is_vendor_csi_stream_on(struct is_device_csi *csi)
{

}

void is_vendor_csi_stream_off(struct is_device_csi *csi)
{

}

bool is_vender_aeb_mode_on(void *cis_data)
{
	cis_shared_data *cis = NULL;
	bool ret = false;

	BUG_ON(!cis_data);

	cis = (cis_shared_data *)cis_data;

	if (cis->is_data.sensor_hdr_mode == CAMERA_SENSOR_HDR_MODE_2AEB)
		ret = true;

	return ret;
}
EXPORT_SYMBOL_GPL(is_vender_aeb_mode_on);

void is_vender_csi_err_handler(struct is_device_csi *csi)
{

}

int is_vender_probe(struct is_vender *vender)
{
	int ret = 0;
	struct is_vender_specific *priv;

	BUG_ON(!vender);

	snprintf(vender->fw_path, sizeof(vender->fw_path), "%s%s", IS_FW_DUMP_PATH, IS_FW);
	snprintf(vender->request_fw_path, sizeof(vender->request_fw_path), "%s", IS_FW);

	is_load_ctrl_init();

	priv = (struct is_vender_specific *)kzalloc(
					sizeof(struct is_vender_specific), GFP_KERNEL);
	if (!priv) {
		probe_err("failed to allocate vender specific");
		return -ENOMEM;
	}

	ret = is_create_sysfs();
	if (ret)
		probe_warn("failed to create sysfs");

	priv->rear_sensor_id = rear_sensor_id;
	priv->front_sensor_id = front_sensor_id;
	priv->rear2_sensor_id = rear2_sensor_id;
#if IS_ENABLED(CONFIG_SECURE_CAMERA_USE)
	priv->secure_sensor_id = secure_sensor_id;
#endif
	priv->front2_sensor_id = front2_sensor_id;
	priv->rear3_sensor_id = rear3_sensor_id;
	priv->rear4_sensor_id = rear4_sensor_id;
	priv->ois_sensor_index = ois_sensor_index;
	priv->aperture_sensor_index = aperture_sensor_index;

	vender->private_data = priv;

	return ret;
}

#ifdef MODULE
int is_vender_driver_init(void)
{
	return 0;
}

int is_vender_driver_exit(void)
{
	return 0;
}
#endif

int is_vender_dt(struct device_node *np)
{
	int ret = 0;

	ret = of_property_read_u32(np, "rear_sensor_id", &rear_sensor_id);
	if (ret)
		probe_err("rear_sensor_id read is fail(%d)", ret);

	ret = of_property_read_u32(np, "front_sensor_id", &front_sensor_id);
	if (ret)
		probe_err("front_sensor_id read is fail(%d)", ret);

	ret = of_property_read_u32(np, "rear2_sensor_id", &rear2_sensor_id);
	if (ret)
		probe_err("rear2_sensor_id read is fail(%d)", ret);

#if IS_ENABLED(CONFIG_SECURE_CAMERA_USE)
	ret = of_property_read_u32(np, "secure_sensor_id", &secure_sensor_id);
	if (ret)
		probe_err("secure_sensor_id read is fail(%d)", ret);
#endif
	ret = of_property_read_u32(np, "front2_sensor_id", &front2_sensor_id);
	if (ret)
		probe_err("front2_sensor_id read is fail(%d)", ret);

	ret = of_property_read_u32(np, "rear3_sensor_id", &rear3_sensor_id);
	if (ret)
		probe_err("rear3_sensor_id read is fail(%d)", ret);

	ret = of_property_read_u32(np, "rear4_sensor_id", &rear4_sensor_id);
	if (ret)
		probe_err("rear4_sensor_id read is fail(%d)", ret);

	ret = of_property_read_u32(np, "ois_sensor_index", &ois_sensor_index);
	if (ret)
		probe_err("ois_sensor_index read is fail(%d)", ret);

	ret = of_property_read_u32(np, "aperture_sensor_index", &aperture_sensor_index);
	if (ret)
		probe_err("aperture_sensor_index read is fail(%d)", ret);

	return ret;
}

int is_vender_fw_prepare(struct is_vender *vender)
{
	int ret = 0;

	return ret;
}

int is_vender_fw_filp_open(struct is_vender *vender, struct file **fp, int bin_type)
{
	return FW_SKIP;
}

int is_vender_preproc_fw_load(struct is_vender *vender)
{
	int ret = 0;

	return ret;
}

void is_vender_resource_get(struct is_vender *vender, u32 rsc_type)
{

}

void is_vender_resource_put(struct is_vender *vender, u32 rsc_type)
{

}

#if !defined(ENABLE_CAL_LOAD)
int is_vender_cal_load(struct is_vender *vender,
	void *module_data)
{
	int ret = 0;

	return ret;
}
#else
#ifdef FLASH_CAL_DATA_ENABLE
static int is_led_cal_file_read(const char *file_name, const void *data, unsigned long size)
{
	int ret = 0;
#ifdef USE_KERNEL_VFS_READ_WRITE
	long fsize, nread;
	struct file *fp;

	fp = filp_open(file_name, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(fp)) {
		ret = PTR_ERR(fp);
		err("file_open(%s) fail(%d)!!\n", file_name, ret);
		goto p_err;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;

	nread = kernel_read(fp, data, size, &fp->f_pos);
	info("%s(): read to file(%s) size(%ld)\n", __func__, file_name, nread);
p_err:
	if (!IS_ERR_OR_NULL(fp))
		filp_close(fp, NULL);

#else
	err("not support %s due to kernel_read!", __func__);
	ret = -EINVAL;
#endif
	return ret;
}
#endif

int is_vender_cal_load(struct is_vender *vender,
	void *module_data)
{
	struct is_core *core;
	struct is_module_enum *module = module_data;
	struct is_device_sensor *sensor;
	ulong cal_addr = 0;
	bool led_cal_en = false;
#ifdef FLASH_CAL_DATA_ENABLE
	int ret = 0;
#endif
	core = container_of(vender, struct is_core, vender);
	sensor = v4l2_get_subdev_hostdata(module->subdev);

	cal_addr = core->resourcemgr.minfo.kvaddr_cal[module->position];
	if (!cal_addr) {
		err("%s, wrong cal address(0x%lx)\n", __func__, cal_addr);
		return -EINVAL;
	}

	if (sensor->subdev_eeprom && sensor->eeprom->data) {
		memcpy((void *)(cal_addr), (void *)sensor->eeprom->data, sensor->eeprom->total_size);
	} else if (sensor->use_otp_cal) {
		memcpy((void *)(cal_addr), (void *)sensor->otp_cal_buf, sizeof(sensor->otp_cal_buf));
	} else {
		info("%s, not used EEPROM/OTP cal. skip", __func__);
		return 0;
	}

#ifdef FLASH_CAL_DATA_ENABLE
	ret = is_led_cal_file_read(IS_LED_CAL_DATA_PATH, (void *)(cal_addr + CALDATA_SIZE),
			LED_CAL_SIZE);
	if (ret) {
		/* if getting led_cal_data_file is failed, fill buf with 0xff */
		memset((void *)(cal_addr + CALDATA_SIZE), 0xff, LED_CAL_SIZE);
		warn("get led_cal_data fail\n");
	} else {
		led_cal_en = true;
	}
#else
	memset((void *)(cal_addr + CALDATA_SIZE), 0xff, LED_CAL_SIZE);
#endif

	info("CAL data(%d) loading complete(led:%s): 0x%lx\n",
			module->position, cal_addr, led_cal_en ? "EN" : "NA");

	return 0;
}
#endif

int is_vender_module_sel(struct is_vender *vender, void *module_data)
{
	int ret = 0;

	return ret;
}

int is_vender_module_del(struct is_vender *vender, void *module_data)
{
	int ret = 0;

	return ret;
}

int is_vender_fw_sel(struct is_vender *vender)
{
	int ret = 0;

	return ret;
}

int is_vender_setfile_sel(struct is_vender *vender,
	char *setfile_name,
	int position)
{
	int ret = 0;

	BUG_ON(!vender);
	BUG_ON(!setfile_name);

	snprintf(vender->setfile_path[position], sizeof(vender->setfile_path[position]),
		"%s%s", IS_SETFILE_SDCARD_PATH, setfile_name);
	snprintf(vender->request_setfile_path[position],
		sizeof(vender->request_setfile_path[position]),
		"%s", setfile_name);

	return ret;
}

int is_vender_preprocessor_gpio_on_sel(struct is_vender *vender, u32 scenario, u32 *gpio_scneario)
{
	int ret = 0;

	return ret;
}

int is_vender_preprocessor_gpio_on(struct is_vender *vender, u32 scenario, u32 gpio_scenario)
{
	int ret = 0;
	return ret;
}

int is_vender_sensor_gpio_on_sel(struct is_vender *vender, u32 scenario, u32 *gpio_scenario,
	void *module_data)
{
	int ret = 0;
	return ret;
}

int is_vender_sensor_gpio_on(struct is_vender *vender, u32 scenario, u32 gpio_scenario
	, void *module_data)
{
	int ret = 0;
	return ret;
}

int is_vender_preprocessor_gpio_off_sel(struct is_vender *vender, u32 scenario, u32 *gpio_scneario,
	void *module_data)
{
	int ret = 0;

	return ret;
}

int is_vender_preprocessor_gpio_off(struct is_vender *vender, u32 scenario, u32 gpio_scenario)
{
	int ret = 0;

	return ret;
}

int is_vender_sensor_gpio_off_sel(struct is_vender *vender, u32 scenario, u32 *gpio_scenario,
	void *module_data)
{
	int ret = 0;

	return ret;
}

int is_vender_sensor_gpio_off(struct is_vender *vender, u32 scenario, u32 gpio_scenario,
	void *module_data)
{
	int ret = 0;

	return ret;
}

void is_vendor_sensor_suspend(struct platform_device *pdev)
{
	return;
}

void is_vendor_resource_clean(struct is_core *core)
{
	return;
}
void is_vender_itf_open(struct is_vender *vender, struct sensor_open_extended *ext_info)
{
	return;
}

int is_vender_set_torch(struct camera2_shot *shot)
{
	return 0;
}

int is_vender_video_s_ctrl(struct v4l2_control *ctrl,
	void *device_data)
{
	return 0;
}

int is_vender_ssx_video_s_ctrl(struct v4l2_control *ctrl,
	void *device_data)
{
	return 0;
}

int is_vender_ssx_video_g_ctrl(struct v4l2_control *ctrl,
	void *device_data)
{
	return 0;
}

void is_vender_sensor_s_input(struct is_vender *vender, void *module)
{
	return;
}

bool is_vender_wdr_mode_on(void *cis_data)
{
	return false;
}
EXPORT_SYMBOL_GPL(is_vender_wdr_mode_on);

bool is_vender_enable_wdr(void *cis_data)
{
	return false;
}

/**
  * is_vender_request_binary: send loading request to the loader
  * @bin: pointer to is_binary structure
  * @path: path of binary file
  * @name: name of binary file
  * @device: device for which binary is being loaded
  **/
int is_vender_request_binary(struct is_binary *bin, const char *path1, const char *path2,
				const char *name, struct device *device)
{

	return 0;
}

int is_vender_s_ctrl(struct is_vender *vender)
{
	return 0;
}

int is_vender_remove_dump_fw_file(void)
{
	return 0;
}
