/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include <exynos-is-sensor.h>
#include "is-hw.h"
#include "is-core.h"
#include "is-param.h"
#include "is-device-sensor.h"
#include "is-device-sensor-peri.h"
#include "is-resourcemgr.h"
#include "is-dt.h"

#include "is-helper-i2c.h"

#include "is-cis.h"

extern int sensor_module_power_reset(struct v4l2_subdev *subdev, struct is_device_sensor *device);

u32 sensor_cis_do_div64(u64 num, u32 den) {
	u64 res = 0;

	if (den != 0) {
		res = num;
		do_div(res, den);
	} else {
		err("Divide by zero!!!\n");
		WARN_ON(1);
	}

	return (u32)res;
}
EXPORT_SYMBOL_GPL(sensor_cis_do_div64);

int sensor_cis_set_registers(struct v4l2_subdev *subdev, const u32 *regs, const u32 size)
{
	int ret = 0;
	int i = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	int index_str = 0, index_next = 0;
	int burst_num = 1;
	u16 *addr_str = NULL;

	FIMC_BUG(!subdev);
	FIMC_BUG(!regs);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		return -EINVAL;
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		return -EINVAL;
	}

	/* Need to delay for sensor setting */
	usleep_range(3000, 3000);

	cis->stream_state = CIS_STREAM_INIT;

	for (i = 0; i < size; i += I2C_NEXT) {
		switch (regs[i + I2C_ADDR]) {
		case I2C_MODE_BURST_ADDR:
			index_str = i;
			break;
		case I2C_MODE_BURST_DATA:
			index_next = i + I2C_NEXT;
			if ((index_next < size) && (I2C_MODE_BURST_DATA == regs[index_next + I2C_ADDR])) {
				burst_num++;
				break;
			}

			addr_str = (u16 *)&regs[index_str + I2C_NEXT + I2C_DATA];
			ret = is_sensor_write16_burst(client, regs[index_str + I2C_DATA], addr_str, burst_num);
			if (ret < 0) {
				err("is_sensor_write16_burst fail, ret(%d), addr(%#x), data(%#x)",
						ret, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
				goto p_err;
			}
			burst_num = 1;
			break;
		case I2C_MODE_DELAY:
			usleep_range(regs[i + I2C_DATA], regs[i + I2C_DATA]);
			break;
		default:
			if (regs[i + I2C_BYTE] == 0x1) {
				ret = is_sensor_write8(client, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
				if (ret < 0) {
					err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)",
							ret, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
					goto p_err;
				}
			} else if (regs[i + I2C_BYTE] == 0x2) {
				ret = is_sensor_write16(client, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
				if (ret < 0) {
					err("is_sensor_write16 fail, ret(%d), addr(%#x), data(%#x)",
						ret, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
					goto p_err;
				}
			}
		}
	}

#if (CIS_TEST_PATTERN_MODE != 0)
	ret = is_sensor_write8(client, 0x0601, CIS_TEST_PATTERN_MODE);
#endif
	cis->stream_state = CIS_STREAM_SET_DONE;
	dbg_sensor(1, "[%s] sensor setting done\n", __func__);

p_err:
	if (ret) {
		if (cis)
			cis->stream_state = CIS_STREAM_SET_ERR;
		err("[%s] global/mode setting fail(%d)", __func__, ret);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(sensor_cis_set_registers);

int sensor_cis_set_registers_addr8(struct v4l2_subdev *subdev, const u32 *regs, const u32 size)
{
	int ret = 0;
	int i = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	int index_str = 0, index_next = 0;
	int burst_num = 1;
	u16 *addr_str = NULL;

	FIMC_BUG(!subdev);
	FIMC_BUG(!regs);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	msleep(3);

	for (i = 0; i < size; i += I2C_NEXT) {
		switch (regs[i + I2C_ADDR]) {
		case I2C_MODE_BURST_ADDR:
			index_str = i;
			break;
		case I2C_MODE_BURST_DATA:
			index_next = i + I2C_NEXT;
			if ((index_next < size) && (I2C_MODE_BURST_DATA == regs[index_next + I2C_ADDR])) {
				burst_num++;
				break;
			}

			addr_str = (u16 *)&regs[index_str + I2C_NEXT + I2C_DATA];
			ret = is_sensor_write16_burst(client, regs[index_str + I2C_DATA], addr_str, burst_num);
			if (ret < 0) {
				err("is_sensor_write16_burst fail, ret(%d), addr(%#x), data(%#x)",
						ret, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
			}
			burst_num = 1;
			break;
		case I2C_MODE_DELAY:
			usleep_range(regs[i + I2C_DATA], regs[i + I2C_DATA]);
			break;
		default:
			if (regs[i + I2C_BYTE] == 0x1) {
				ret = is_sensor_addr8_write8(client, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
				if (ret < 0) {
					err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)",
							ret, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
				}
			} else if (regs[i + I2C_BYTE] == 0x2) {
				ret = is_sensor_write16(client, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
				if (ret < 0) {
					err("is_sensor_write16 fail, ret(%d), addr(%#x), data(%#x)",
						ret, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
				}
			}
		}
	}

#if (CIS_TEST_PATTERN_MODE != 0)
	ret = is_sensor_write8(client, 0x0601, CIS_TEST_PATTERN_MODE);
#endif

	dbg_sensor(1, "[%s] sensor setting done\n", __func__);

p_err:
	return ret;
}
EXPORT_SYMBOL_GPL(sensor_cis_set_registers_addr8);

int sensor_cis_check_rev_on_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct i2c_client *client;
	struct is_cis *cis = NULL;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		return ret;
	}

	memset(cis->cis_data, 0, sizeof(cis_shared_data));

	ret = sensor_cis_check_rev(cis);

	return ret;
}
EXPORT_SYMBOL_GPL(sensor_cis_check_rev_on_init);

int sensor_cis_check_rev(struct is_cis *cis)
{
	int ret = 0;
	u8 rev8 = 0;
	u16 rev16 = 0;
	int i;
	struct i2c_client *client;

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	if (cis->rev_byte == 2) { // 2 BYTE read
		ret = is_sensor_read16(client, cis->rev_addr, &rev16);
		cis->cis_data->cis_rev = rev16;
	} else {
		ret = is_sensor_read8(client, cis->rev_addr, &rev8);	//default
		cis->cis_data->cis_rev = rev8;
	}

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	if (ret < 0) {
		err("is_sensor_read is fail, (ret %d)", ret);
		ret = -EAGAIN;
		goto p_err;
	}

	if (cis->rev_valid_count) {
		for (i = 0; i < cis->rev_valid_count; i++) {
			if (cis->cis_data->cis_rev == cis->rev_valid_values[i]) {
				pr_info("%s : [%d][%d] Sensor version. Rev. 0x%X\n", __func__, cis->device, cis->id, cis->cis_data->cis_rev);
				break;
			}
		}

		if (i == cis->rev_valid_count) {
			pr_info("%s : [%d][%d] Wrong sensor version. Rev. 0x%X\n", __func__, cis->device, cis->id, cis->cis_data->cis_rev);
#if defined(USE_CAMERA_CHECK_SENSOR_REV)
			ret = -EINVAL;
#endif
		}
	} else {
		pr_info("%s : [%d][%d] Skip rev checking. Rev. 0x%X\n", __func__, cis->device, cis->id, cis->cis_data->cis_rev);
	}

p_err:
	return ret;
}
EXPORT_SYMBOL_GPL(sensor_cis_check_rev);

int sensor_cis_check_model_id(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u16 model_id = 0;
	struct i2c_client *client;
	struct is_cis *cis = NULL;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_sensor_read16(client, 0x0000, &model_id);
	if (ret < 0) {
		err("is_sensor_read8 fail, (ret %d)", ret);
		goto p_err;
	}

	cis->cis_data->cis_model_id = model_id;

	dbg_sensor(1, "model_id: %#x\n", model_id);

p_err:
	return ret;
}
EXPORT_SYMBOL_GPL(sensor_cis_check_model_id);

u32 sensor_cis_calc_again_code(u32 permile)
{
	return (permile * 32 + 500) / 1000;
}
EXPORT_SYMBOL_GPL(sensor_cis_calc_again_code);

u32 sensor_cis_calc_again_permile(u32 code)
{
	return (code * 1000 + 16) / 32;
}
EXPORT_SYMBOL_GPL(sensor_cis_calc_again_permile);

u32 sensor_cis_calc_dgain_code(u32 permile)
{
	u8 buf[2] = {0, 0};
	buf[0] = permile / 1000;
	buf[1] = (((permile - (buf[0] * 1000)) * 256) / 1000);

	return (buf[0] << 8 | buf[1]);
}
EXPORT_SYMBOL_GPL(sensor_cis_calc_dgain_code);

u32 sensor_cis_calc_dgain_permile(u32 code)
{
	return (((code & 0xFF00) >> 8) * 1000) + ((code & 0xFF) * 1000 / 256);
}
EXPORT_SYMBOL_GPL(sensor_cis_calc_dgain_permile);

u8 sensor_cis_get_duration_shifter(struct is_cis *cis, u32 input_duration)
{
	u8 shifter = 0;
	u32 shifted_val = input_duration;
	u64 max_duration_16bit = (u64)0xFF00 * 1000000 * cis->cis_data->line_length_pck / cis->cis_data->pclk;

	if (input_duration > max_duration_16bit) {
		shifted_val /=  max_duration_16bit;
		while (shifted_val) {
			shifted_val >>= 1;
			shifter++;
		}
	}

	dbg_sensor(1, "%s [%d] input_duration(%d), max_duration(%d), shifter(%d)\n",
		__func__, cis->id, input_duration, max_duration_16bit, shifter);

	return shifter;
}
EXPORT_SYMBOL_GPL(sensor_cis_get_duration_shifter);

int sensor_cis_compensate_gain_for_extremely_br(struct v4l2_subdev *subdev, u32 expo, u32 *again, u32 *dgain)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;
	u16 coarse_int = 0;
	u32 compensated_again = 0;

	FIMC_BUG(!subdev);
	FIMC_BUG(!again);
	FIMC_BUG(!dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}
	cis_data = cis->cis_data;

	vt_pic_clk_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	if (line_length_pck <= 0) {
		err("[%s] invalid line_length_pck(%d)\n", __func__, line_length_pck);
		goto p_err;
	}

	coarse_int = ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int) / line_length_pck;
	if (coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, coarse_int, cis_data->min_coarse_integration_time);
		coarse_int = cis_data->min_coarse_integration_time;
	}

	if (coarse_int <= 15) {
		compensated_again = (*again * ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int)) / (line_length_pck * coarse_int);

		if (compensated_again < cis_data->min_analog_gain[1]) {
			*again = cis_data->min_analog_gain[1];
		} else if (*again >= cis_data->max_analog_gain[1]) {
			*dgain = (*dgain * ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int)) / (line_length_pck * coarse_int);
		} else {
			*again = compensated_again;
		}

		dbg_sensor(1, "[%s] exp(%d), again(%d), dgain(%d), coarse_int(%d), compensated_again(%d)\n",
			__func__, expo, *again, *dgain, coarse_int, compensated_again);
	}

p_err:
	return ret;
}
EXPORT_SYMBOL_GPL(sensor_cis_compensate_gain_for_extremely_br);

int sensor_cis_dump_registers(struct v4l2_subdev *subdev, const u32 *regs, const u32 size)
{
	int ret = 0;
	int i = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	u8 data8 = 0;
	u16 data16 = 0;

	FIMC_BUG(!subdev);
	FIMC_BUG(!regs);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	for (i = 0; i < size; i += I2C_NEXT) {
		if (regs[i + I2C_BYTE] == 0x2 && regs[i + I2C_ADDR] == 0x6028) {
			ret = is_sensor_write16(client, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
			if (ret < 0) {
				err("is_sensor_write16 fail, ret(%d), addr(%#x)",
						ret, regs[i + I2C_ADDR]);
			}
		}

		if (regs[i + I2C_BYTE] == 0x1) {
			ret = is_sensor_read8(client, regs[i + I2C_ADDR], &data8);
			if (ret < 0) {
				err("is_sensor_read8 fail, ret(%d), addr(%#x)",
						ret, regs[i + I2C_ADDR]);
			}
			pr_err("[SEN:DUMP] [0x%04X, 0x%04X\n", regs[i + I2C_ADDR], data8);
		} else {
			ret = is_sensor_read16(client, regs[i + I2C_ADDR], &data16);
			if (ret < 0) {
				err("is_sensor_read6 fail, ret(%d), addr(%#x)",
						ret, regs[i + I2C_ADDR]);
			}
			pr_err("[SEN:DUMP] [0x%04X, 0x%04X\n", regs[i + I2C_ADDR], data16);
		}
	}

p_err:
	return ret;
}
EXPORT_SYMBOL_GPL(sensor_cis_dump_registers);

int sensor_cis_parse_dt(struct device *dev, struct v4l2_subdev *subdev)
{
	int ret = 0;
	int i = 0;
	struct is_cis *cis;
	struct device_node *dnode;
	const u32 *rev_reg_spec;
	u32 rev_reg[IS_CIS_REV_MAX_LIST+2];
	u32 rev_reg_len;

	FIMC_BUG(!dev);
	FIMC_BUG(!dev->of_node);
	FIMC_BUG(!subdev);

	dnode = dev->of_node;

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	rev_reg_spec = of_get_property(dnode, "rev_reg", &rev_reg_len);
	if (rev_reg_spec) {
		rev_reg_len /= (unsigned int)sizeof(*rev_reg_spec);
		BUG_ON(rev_reg_len > (IS_CIS_REV_MAX_LIST + 2));

		if (!of_property_read_u32_array(dnode, "rev_reg", rev_reg, rev_reg_len)) {
			cis->rev_valid_count = rev_reg_len - 2;
			cis->rev_addr = rev_reg[0];
			cis->rev_byte = rev_reg[1];

			for (i = 2; i < rev_reg_len; i++)
				cis->rev_valid_values[i-2] = rev_reg[i];
		}
	} else {
		info("rev_reg read is fail(%d)", ret);
		cis->rev_addr = 0x0002;	// default 1byte 0x0002 read
		cis->rev_byte = 1;
		cis->rev_valid_count = 0;
		ret = 0;
	}

p_err:
	return ret;
}
EXPORT_SYMBOL_GPL(sensor_cis_parse_dt);

#if defined(USE_RECOVER_I2C_TRANS)
void sensor_cis_recover_i2c_fail(struct v4l2_subdev *subdev_cis)
{
	struct is_device_sensor *device;
	struct v4l2_subdev *subdev_module;

	FIMC_BUG_VOID(!subdev_cis);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev_cis);
	FIMC_BUG_VOID(!device);

	subdev_module = device->subdev_module;
	FIMC_BUG_VOID(!subdev_module);

	sensor_module_power_reset(subdev_module, device);
}
#endif

int sensor_cis_wait_streamoff(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	u32 wait_cnt = 0, time_out_cnt = 250;
	u8 sensor_fcount = 0;
	u32 i2c_fail_cnt = 0, i2c_fail_max_cnt = 5;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (unlikely(!cis)) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;
	if (unlikely(!cis_data)) {
		err("cis_data is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = is_sensor_read8(client, 0x0005, &sensor_fcount);
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	if (ret < 0)
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", 0x0005, sensor_fcount, ret);

	/*
	 * Read sensor frame counter (sensor_fcount address = 0x0005)
	 * stream on (0x00 ~ 0xFE), stream off (0xFF)
	 */
	while (sensor_fcount != 0xFF) {
		I2C_MUTEX_LOCK(cis->i2c_lock);
		ret = is_sensor_read8(client, 0x0005, &sensor_fcount);
		I2C_MUTEX_UNLOCK(cis->i2c_lock);
		if (ret < 0) {
			i2c_fail_cnt++;
			err("i2c transfer fail addr(%x), val(%x), try(%d), ret = %d\n",
				0x0005, sensor_fcount, i2c_fail_cnt, ret);

			if (i2c_fail_cnt >= i2c_fail_max_cnt) {
				err("[MOD:D:%d] %s, i2c fail, i2c_fail_cnt(%d) >= i2c_fail_max_cnt(%d), sensor_fcount(%d)",
						cis->id, __func__, i2c_fail_cnt, i2c_fail_max_cnt, sensor_fcount);
				ret = -EINVAL;
				goto p_err;
			}
		}
#if defined(USE_RECOVER_I2C_TRANS)
		if (i2c_fail_cnt >= USE_RECOVER_I2C_TRANS) {
			sensor_cis_recover_i2c_fail(subdev);
			err("[mod:d:%d] %s, i2c transfer, forcely power down/up",
				cis->id, __func__);
			ret = -EINVAL;
			goto p_err;
		}
#endif
		usleep_range(CIS_STREAM_OFF_WAIT_TIME, CIS_STREAM_OFF_WAIT_TIME);
		wait_cnt++;

		if (wait_cnt >= time_out_cnt) {
			err("[MOD:D:%d] %s, time out, wait_limit(%d) > time_out(%d), sensor_fcount(%d)",
					cis->id, __func__, wait_cnt, time_out_cnt, sensor_fcount);
			ret = -EINVAL;
			goto p_err;
		}

		dbg_sensor(1, "[MOD:D:%d] %s, sensor_fcount(%d), (wait_limit(%d) < time_out(%d))\n",
				cis->id, __func__, sensor_fcount, wait_cnt, time_out_cnt);
	}
p_err:
	return ret;
}
EXPORT_SYMBOL_GPL(sensor_cis_wait_streamoff);

int sensor_cis_wait_streamoff_mipi_end(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct is_device_sensor_peri *sensor_peri;
	u32 wait_cnt = 0, time_out_cnt = 250;
	struct is_device_sensor *device;
	struct is_device_csi *csi;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (unlikely(!cis)) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	FIMC_BUG(!device);

	csi = v4l2_get_subdevdata(device->subdev_csi);
	if (!csi) {
		err("csi is NULL");
		return -EINVAL;
	}

	/* wait stream off by waiting vblank state */
	do {
		usleep_range(CIS_STREAM_OFF_WAIT_TIME, CIS_STREAM_OFF_WAIT_TIME + 1);
		wait_cnt++;

		if (wait_cnt >= time_out_cnt) {
			err("[MOD:D:%d] %s, time out, wait_limit(%d) > time_out(%d)",
					cis->id, __func__, wait_cnt, time_out_cnt);
			ret = -EINVAL;
			goto p_err;
		}

		dbg_sensor(1, "[MOD:D:%d] %s, wait_limit(%d) < time_out(%d)\n",
				cis->id, __func__, wait_cnt, time_out_cnt);
	} while (csi->sw_checker == EXPECT_FRAME_END);

p_err:
	return ret;
}
EXPORT_SYMBOL_GPL(sensor_cis_wait_streamoff_mipi_end);

int sensor_cis_wait_streamon(struct v4l2_subdev *subdev)
{
	int ret = 0;
	int ret_err = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	u32 wait_cnt = 0, time_out_cnt = 250;
	u8 sensor_fcount = 0;
	u32 i2c_fail_cnt = 0;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (unlikely(!cis)) {
	    err("cis is NULL");
	    ret = -EINVAL;
	    goto p_err;
	}

	cis_data = cis->cis_data;
	if (unlikely(!cis_data)) {
	    err("cis_data is NULL");
	    ret = -EINVAL;
	    goto p_err;
	}

	client = cis->client;
	if (unlikely(!client)) {
	    err("client is NULL");
	    ret = -EINVAL;
	    goto p_err;
	}

	if (cis->long_term_mode.sen_strm_off_on_enable) {
		info("[MOD:d:%d] %s, Skip, LTE mode is operating", cis->id, __func__);
		return 0;
	}

	if (cis_data->cur_frame_us_time > 300000 && cis_data->cur_frame_us_time < 2000000) {
		time_out_cnt = (cis_data->cur_frame_us_time / CIS_STREAM_ON_WAIT_TIME) + 100; // for Hyperlapse night mode
	}

	if (cis_data->dual_slave == true || cis_data->highres_capture_mode == true) {
		time_out_cnt = time_out_cnt * 6;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = is_sensor_read8(client, 0x0005, &sensor_fcount);
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	if (ret < 0)
	    err("i2c transfer fail addr(%x), val(%x), ret = %d\n", 0x0005, sensor_fcount, ret);

	/*
	 * Read sensor frame counter (sensor_fcount address = 0x0005)
	 * stream on (0x00 ~ 0xFE), stream off (0xFF)
	 */
	while (sensor_fcount == 0xff) {
		usleep_range(CIS_STREAM_ON_WAIT_TIME, CIS_STREAM_ON_WAIT_TIME);
		wait_cnt++;

		I2C_MUTEX_LOCK(cis->i2c_lock);
		ret = is_sensor_read8(client, 0x0005, &sensor_fcount);
		I2C_MUTEX_UNLOCK(cis->i2c_lock);
		if (ret < 0) {
			i2c_fail_cnt++;
			err("i2c transfer fail addr(%x), val(%x), try(%d), ret = %d\n",
				0x0005, sensor_fcount, i2c_fail_cnt, ret);
		}
#if defined(USE_RECOVER_I2C_TRANS)
		if (i2c_fail_cnt >= USE_RECOVER_I2C_TRANS) {
			sensor_cis_recover_i2c_fail(subdev);
			err("[mod:d:%d] %s, i2c transfer, forcely power down/up",
				cis->id, __func__);
			ret = -EINVAL;
			goto p_err;
		}
#endif
		if (wait_cnt >= time_out_cnt) {
			err("[MOD:D:%d] %s, stream on wait failed (%d), wait_cnt(%d) > time_out_cnt(%d), framecount(%x), dual_slave(%d), highres(%d)",
				cis->id, __func__, cis_data->cur_frame_us_time, wait_cnt, time_out_cnt,
				sensor_fcount, cis_data->dual_slave, cis_data->highres_capture_mode);
			ret = -EINVAL;
			goto p_err;
		}

		dbg_sensor(1, "[MOD:D:%d] %s, sensor_fcount(%d), (wait_limit(%d) < time_out(%d))\n",
				cis->id, __func__, sensor_fcount, wait_cnt, time_out_cnt);
	}

#ifdef USE_CAMERA_SENSOR_RETENTION
	/* retention mode CRC wait calculation */
	usleep_range(1000, 1000);
#endif
	return 0;

p_err:
	CALL_CISOPS(cis, cis_log_status, subdev);

	ret_err = CALL_CISOPS(cis, cis_stream_off, subdev);
	if (ret_err < 0) {
		err("[MOD:D:%d] stream off fail", cis->id);
	} else {
		ret_err = CALL_CISOPS(cis, cis_wait_streamoff, subdev);
		if (ret_err < 0)
			err("[MOD:D:%d] sensor wait stream off fail", cis->id);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(sensor_cis_wait_streamon);

int sensor_cis_set_initial_exposure(struct v4l2_subdev *subdev)
{
	struct is_cis *cis;

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (unlikely(!cis)) {
		err("cis is NULL");
		return -EINVAL;
	}

	if (cis->use_initial_ae) {
		cis->init_ae_setting = cis->last_ae_setting;

		dbg_sensor(1, "[MOD:D:%d] %s short(exp:%u/again:%d/dgain:%d), long(exp:%u/again:%d/dgain:%d)\n",
			cis->id, __func__, cis->init_ae_setting.exposure, cis->init_ae_setting.analog_gain,
			cis->init_ae_setting.digital_gain, cis->init_ae_setting.long_exposure,
			cis->init_ae_setting.long_analog_gain, cis->init_ae_setting.long_digital_gain);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(sensor_cis_set_initial_exposure);

int sensor_cis_active_test(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (unlikely(!cis)) {
		err("cis is NULL");
		return -EINVAL;
	}

	/* sensor mode setting */
	ret = CALL_CISOPS(cis, cis_set_global_setting, subdev);
	if (ret < 0) {
		err("[%s] cis global setting fail\n", __func__);
		return ret;
	}

	ret = CALL_CISOPS(cis, cis_mode_change, subdev, 0);
	if (ret < 0) {
		err("[%s] cis mode setting(0) fail\n", __func__);
		return ret;
	}

	/* sensor stream on */
	ret = CALL_CISOPS(cis, cis_stream_on, subdev);
	if (ret < 0) {
		err("[%s] stream on fail\n", __func__);
		return ret;
	}

	ret = CALL_CISOPS(cis, cis_wait_streamon, subdev);
	if (ret < 0) {
		err("[%s] sensor wait stream on fail\n", __func__);
		return ret;
	}

	msleep(100);

	/* Sensor stream off */
	ret = CALL_CISOPS(cis, cis_stream_off, subdev);
	if (ret < 0) {
		err("[%s] stream off fail\n", __func__);
		return ret;
	}

	ret = CALL_CISOPS(cis, cis_wait_streamoff, subdev);
	if (ret < 0) {
		err("[%s] stream off fail\n", __func__);
		return ret;
	}

	info("[MOD:D:%d] %s: %d\n", cis->id, __func__, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(sensor_cis_active_test);

int sensor_cis_set_test_pattern(struct v4l2_subdev *subdev, camera2_sensor_ctl_t *sensor_ctl)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	dbg_sensor(1, "[MOD:D:%d] %s, cur_pattern_mode(%d), testPatternMode(%d)\n", cis->id, __func__,
			cis->cis_data->cur_pattern_mode, sensor_ctl->testPatternMode);

	if (cis->cis_data->cur_pattern_mode != sensor_ctl->testPatternMode) {
		cis->cis_data->cur_pattern_mode = sensor_ctl->testPatternMode;
		if (sensor_ctl->testPatternMode == SENSOR_TEST_PATTERN_MODE_OFF) {
			info("%s: set DEFAULT pattern! (testpatternmode : %d)\n", __func__, sensor_ctl->testPatternMode);

			I2C_MUTEX_LOCK(cis->i2c_lock);
			is_sensor_write16(client, 0x0600, 0x0000);
			I2C_MUTEX_UNLOCK(cis->i2c_lock);
		} else if (sensor_ctl->testPatternMode == SENSOR_TEST_PATTERN_MODE_BLACK) {
			info("%s: set BLACK pattern! (testpatternmode :%d), Data : 0x(%x, %x, %x, %x)\n",
				__func__, sensor_ctl->testPatternMode,
				(unsigned short)sensor_ctl->testPatternData[0],
				(unsigned short)sensor_ctl->testPatternData[1],
				(unsigned short)sensor_ctl->testPatternData[2],
				(unsigned short)sensor_ctl->testPatternData[3]);

			I2C_MUTEX_LOCK(cis->i2c_lock);
			is_sensor_write16(client, 0x0600, 0x0001);
			I2C_MUTEX_UNLOCK(cis->i2c_lock);
		}
	}

p_err:
	return ret;
}
EXPORT_SYMBOL_GPL(sensor_cis_set_test_pattern);
