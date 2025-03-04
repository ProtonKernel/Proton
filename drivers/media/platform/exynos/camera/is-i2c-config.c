/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/pinctrl/consumer.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#endif
#include "is-device-sensor-peri.h"
#include "is-i2c-config.h"
#include "is-core.h"
#include "is-vender-specific.h"

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
int is_i2c_pin_config(struct i2c_client *client, int state)
{
	int ret = 0;
	char pin_ctrl[10] = {0, };
	struct device *i2c_dev = NULL;
	struct pinctrl *pinctrl_i2c = NULL;

	if (client == NULL) {
		err("client is NULL.");
		return ret;
	}

	switch (state) {
	case I2C_PIN_STATE_ON:
		sprintf(pin_ctrl, "on_i2c");
		break;
	case I2C_PIN_STATE_OFF:
		sprintf(pin_ctrl, "off_i2c");
		break;
	default:
		sprintf(pin_ctrl, "default");
		break;
	}

	i2c_dev = client->dev.parent->parent;
	pinctrl_i2c = devm_pinctrl_get_select(i2c_dev, pin_ctrl);
	if (IS_ERR_OR_NULL(pinctrl_i2c))
		pr_err("%s: Failed to configure i2c pin\n", __func__);
	else
		devm_pinctrl_put(pinctrl_i2c);

	return ret;
}

int is_i2c_pin_control(struct is_module_enum *module, u32 scenario, u32 value)
{
	int ret = 0;
	struct is_device_sensor_peri *sensor_peri;
	struct is_device_sensor *device;
	struct is_core *core;
	int i2c_config_state = 0;
	u32 i2c_channel = 0;
	struct is_vender_specific *specific;

	if (value)
		i2c_config_state = I2C_PIN_STATE_ON;
	else
		i2c_config_state = I2C_PIN_STATE_OFF;

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	device = v4l2_get_subdev_hostdata(module->subdev);
	core = (struct is_core *)dev_get_drvdata(is_dev);

	FIMC_BUG(!core);

	specific = core->vender.private_data;

	switch (scenario) {
	case SENSOR_SCENARIO_NORMAL:
	case SENSOR_SCENARIO_VISION:
		if (sensor_peri->cis.client) {
			info("%s[%d] cis i2c config(%d), position(%d), scenario(%d)\n",
				__func__, __LINE__, i2c_config_state, module->position, scenario);
			ret = is_i2c_pin_config(sensor_peri->cis.client, i2c_config_state);
		}
		if (device->actuator[module->position]) {
			i2c_channel = module->ext.actuator_con.peri_setting.i2c.channel;

			if (i2c_config_state == I2C_PIN_STATE_ON)
				atomic_inc(&core->i2c_rsccount[i2c_channel]);
			else if (i2c_config_state == I2C_PIN_STATE_OFF)
				atomic_dec(&core->i2c_rsccount[i2c_channel]);

			if (atomic_read(&core->i2c_rsccount[i2c_channel]) == value) {
				info("%s[%d] actuator i2c config(%d), position(%d), scenario(%d), i2c_channel(%d)\n",
					__func__, __LINE__, i2c_config_state, module->position, scenario, i2c_channel);
				ret |= is_i2c_pin_config(device->actuator[module->position]->client, i2c_config_state);
			}
		}
		if (device->mcu && device->mcu->name != MCU_NAME_INTERNAL) {
			info("%s[%d] mcu i2c config(%d), position(%d), scenario(%d)\n",
				__func__, __LINE__, i2c_config_state, module->position, scenario);
			ret |= is_i2c_pin_config(device->mcu->client, i2c_config_state);
		}
		break;
	case SENSOR_SCENARIO_OIS_FACTORY:
		if (device->mcu && device->mcu->name != MCU_NAME_INTERNAL) {
			info("%s[%d] mcu i2c config(%d), position(%d), scenario(%d)\n",
				__func__, __LINE__, i2c_config_state, module->position, scenario);
			ret |= is_i2c_pin_config(device->mcu->client, i2c_config_state);
		}
		if (device->actuator[module->position]) {
			info("%s[%d] actuator i2c config(%d), position(%d), scenario(%d)\n",
				__func__, __LINE__, i2c_config_state, module->position, scenario);
			ret |= is_i2c_pin_config(device->actuator[module->position]->client, i2c_config_state);
		}
		break;
	case SENSOR_SCENARIO_READ_ROM:
		if (module->pdata->rom_id < ROM_ID_MAX) {
			info("%s[%d] eeprom i2c config(%d), rom_id(%d), scenario(%d)\n",
				__func__, __LINE__, i2c_config_state, module->pdata->rom_id, scenario);
#if defined(USE_CAMERA_DUALIZED)
			if (module->pdata->rom_type == ROM_TYPE_EEPROM) {
				ret |= is_i2c_pin_config(specific->eeprom_client[module->pdata->rom_id], i2c_config_state);
			} else if (module->pdata->rom_type == ROM_TYPE_OTPROM) {
				ret |= is_i2c_pin_config(specific->otprom_client[module->pdata->rom_id], i2c_config_state);
			}
#else
			ret |= is_i2c_pin_config(specific->eeprom_client[module->pdata->rom_id], i2c_config_state);
#endif
		}

		break;
	case SENSOR_SCENARIO_SECURE:
		if (module->client) {
			info("%s[%d] cis i2c config(%d), position(%d), scenario(%d)\n",
				__func__, __LINE__, i2c_config_state, module->position, scenario);
			ret = is_i2c_pin_config(module->client, i2c_config_state);
		}
		break;
	case SENSOR_SCENARIO_MATCH_SEQ:
		if (device->client) {
			info("%s[%d] cis i2c config(%d), position(%d), scenario(%d)\n",
				__func__, __LINE__, i2c_config_state, module->position, scenario);
			ret = is_i2c_pin_config(device->client, i2c_config_state);
		}
		break;
	default:
		err("%s[%d] undefined scenario, i2c config(%d), position(%d), scenario(%d)\n",
			__func__, __LINE__, i2c_config_state, module->position, scenario);
		break;
	}

	return ret;
}
#endif
