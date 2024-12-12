/*
 *  Copyright (C) 2019	Samsung Electronics Co., Ltd(http://www.samsung.com)
 *  Copyright (C) 2019  Hanjun Shin <hanjun.shin@samsung.com>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/module.h>
#include <linux/thermal.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <soc/samsung/dev_cooling.h>
#include <soc/samsung/debug-snapshot.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <soc/samsung/ect_parser.h>

#include "../thermal_core.h"
#include "exynos_tmu.h"
/**
 * struct dev_cooling_device - data for cooling device with dev
 * @id: unique integer value corresponding to each dev_cooling_device
 *	registered.
 * @cool_dev: thermal_cooling_device pointer to keep track of the
 *	registered cooling device.
 * @dev_state: integer value representing the current state of dev
 *	cooling	devices.
 * @dev_val: integer value representing the absolute value of the clipped
 *	freq.
 * @freq_table: frequency table to convert level to freq
 * @thermal_pm_qos_max: pm qos node to throttle max freq
 *
 * This structure is required for keeping information of each
 * dev_cooling_device registered. In order to prevent corruption of this a
 * mutex lock cooling_dev_lock is used.
 */
struct dev_cooling_device {
	int					id;
	struct					thermal_cooling_device *cool_dev;
	unsigned int				dev_state;
	unsigned int				dev_val;
	u32					max_state;
	struct exynos_devfreq_opp_table		*freq_table;
	struct exynos_pm_qos_request			thermal_pm_qos_max;
};

static DEFINE_MUTEX(cooling_dev_lock);
static unsigned int dev_count;

/* dev cooling device callback functions are defined below */
/**
 * dev_get_max_state - callback function to get the max cooling state.
 * @cdev: thermal cooling device pointer.
 * @state: fill this variable with the max cooling state.
 *
 * Callback for the thermal cooling device to return the dev
 * max cooling state.
 *
 * Return: 0 on success, an error code otherwise.
 */
static int dev_get_max_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct thermal_cooling_device *cdev_tmp =
		(struct thermal_cooling_device *)((unsigned long long)cdev & ~0x1ull);
	struct dev_cooling_device *dev = cdev_tmp->devdata;

	*state = dev->max_state;

	return 0;
}
/**
 * dev_get_cur_state - callback function to get the current cooling state.
 * @cdev: thermal cooling device pointer.
 * @state: fill this variable with the current cooling state.
 *
 * Callback for the thermal cooling device to return the dev
 * current cooling state.
 *
 * Return: 0 on success, an error code otherwise.
 */
static int dev_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct dev_cooling_device *dev = cdev->devdata;

	*state = dev->dev_state;

	return 0;
}

/**
 * dev_set_cur_state - callback function to set the current cooling state.
 * @cdev: thermal cooling device pointer.
 * @state: set this variable to the current cooling state.
 *
 * Callback for the thermal cooling device to change the dev
 * current cooling state.
 *
 * Return: 0 on success, an error code otherwise.
 */
static int dev_set_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long state)
{
	struct dev_cooling_device *dev = cdev->devdata;

	if (dev->dev_state == state)
		return 0;

	dev->dev_state = (unsigned int)state;
	dev->dev_val = dev->freq_table[state].freq;
	dbg_snapshot_thermal(NULL, 0, cdev->type, dev->dev_val);
	exynos_pm_qos_update_request(&dev->thermal_pm_qos_max, dev->dev_val);

	return 0;
}

/* Bind dev callbacks to thermal cooling device ops */
static struct thermal_cooling_device_ops const dev_cooling_ops = {
	.get_max_state = dev_get_max_state,
	.get_cur_state = dev_get_cur_state,
	.set_cur_state = dev_set_cur_state,
};

static int dev_cooling_get_level(struct thermal_cooling_device *cdev,
				 unsigned long value)
{
	struct dev_cooling_device *dev = cdev->devdata;
	int i, level = dev->max_state - 1;

	for (i = 0; i < dev->max_state; i++) {
		if (dev->freq_table[i].freq <= value) {
			level = i;
			break;
		}
	}
	return level;
}


static int parse_ect_cooling_level(struct thermal_cooling_device *cdev,
				   char *tz_name)
{
	struct thermal_instance *instance;
	struct thermal_zone_device *tz;
	bool foundtz = false;
	void *thermal_block;
	struct ect_ap_thermal_function *function;
	int i, temperature;
	unsigned int freq;

	mutex_lock(&cdev->lock);
	list_for_each_entry(instance, &cdev->thermal_instances, cdev_node) {
		tz = instance->tz;
		if (!strncasecmp(tz_name, tz->type, THERMAL_NAME_LENGTH)) {
			foundtz = true;
			break;
		}
	}
	mutex_unlock(&cdev->lock);

	if (!foundtz)
		goto skip_ect;

	thermal_block = ect_get_block(BLOCK_AP_THERMAL);
	if (!thermal_block)
		goto skip_ect;

	function = ect_ap_thermal_get_function(thermal_block, tz_name);
	if (!function)
		goto skip_ect;

	for (i = 0; i < function->num_of_range; ++i) {
		unsigned long max_level = 0;
		int level;

		temperature = function->range_list[i].lower_bound_temperature;
		freq = function->range_list[i].max_frequency;

		instance = get_thermal_instance(tz, cdev, i);
		if (!instance) {
			tmu_pr_err("%s: (%s, %d)instance isn't valid\n", __func__, tz_name, i);
			goto skip_ect;
		}

		cdev->ops->get_max_state(cdev, &max_level);
		level = dev_cooling_get_level(cdev, freq);

		if (level == THERMAL_CSTATE_INVALID)
			level = max_level;

		instance->upper = level;

		tmu_pr_info("Parsed From ECT : %s: [%d] Temperature : %d, frequency : %u, level: %d\n",
			tz_name, i, temperature, freq, level);
	}
skip_ect:
	return 0;
}

/**
 * __dev_cooling_register - helper function to create dev cooling device
 * @np: a valid struct device_node to the cooling device device tree node
 * @data: exynos_devfreq_data structure for using devfreq
 *
 * This interface function registers the dev cooling device with the name
 * "thermal-dev-%x". This api can support multiple instances of dev
 * cooling devices. It also gives the opportunity to link the cooling device
 * with a device tree node, in order to bind it via the thermal DT code.
 *
 * Return: a valid struct thermal_cooling_device pointer on success,
 * on failure, it returns a corresponding ERR_PTR().
 */
static struct thermal_cooling_device *
__dev_cooling_register(struct device_node *np, struct exynos_devfreq_data *data)
{
	struct thermal_cooling_device *cool_dev;
	struct dev_cooling_device *dev = NULL;
	char dev_name[THERMAL_NAME_LENGTH];
	char cooling_name[THERMAL_NAME_LENGTH];
	const char *name;

	dev = kzalloc(sizeof(struct dev_cooling_device), GFP_KERNEL);
	if (!dev)
		return ERR_PTR(-ENOMEM);

	if (of_property_read_string(np, "tz-cooling-name", &name)) {
		tmu_pr_err("%s: could not find tz-cooling-name\n", __func__);
		kfree(dev);
		return ERR_PTR(-EINVAL);
	}
	strncpy(cooling_name, name, sizeof(cooling_name));

	mutex_lock(&cooling_dev_lock);
	dev->id = dev_count;
	dev_count++;
	mutex_unlock(&cooling_dev_lock);

	snprintf(dev_name, sizeof(dev_name), "thermal-dev-%d", dev->id);

	dev->dev_state = 0;
	dev->freq_table = data->opp_list;
	dev->max_state = data->max_state;

	exynos_pm_qos_add_request(&dev->thermal_pm_qos_max, (int)data->pm_qos_class_max, data->max_freq);

	cool_dev = thermal_of_cooling_device_register(np, dev_name, dev,
						      &dev_cooling_ops);

	if (IS_ERR(cool_dev)) {
		kfree(dev);
		return cool_dev;
	}

	parse_ect_cooling_level(cool_dev, cooling_name);

	dev->cool_dev = cool_dev;

	return cool_dev;
}

/**
 * dev_cooling_unregister - function to remove dev cooling device.
 * @cdev: thermal cooling device pointer.
 *
 * This interface function unregisters the "thermal-dev-%x" cooling device.
 */
void dev_cooling_unregister(struct thermal_cooling_device *cdev)
{
	struct dev_cooling_device *dev;

	if (!cdev)
		return;

	dev = cdev->devdata;
	mutex_lock(&cooling_dev_lock);
	dev_count--;
	mutex_unlock(&cooling_dev_lock);

	thermal_cooling_device_unregister(dev->cool_dev);
	kfree(dev);
}
EXPORT_SYMBOL_GPL(dev_cooling_unregister);

/**
 * exynos_dev_cooling_register - function to remove dev cooling device.
 * @np: a valid struct device_node to the cooling device device tree node
 * @data: exynos_devfreq_data structure for using devfreq
 *
 * This interface function registers the exynos devfreq cooling device.
 */
struct thermal_cooling_device *exynos_dev_cooling_register(struct device_node *np, struct exynos_devfreq_data *data)
{
	return __dev_cooling_register(np, data);
}
EXPORT_SYMBOL_GPL(exynos_dev_cooling_register);
