/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DSP_BUS_H__
#define __HW_DSP_BUS_H__

#include <linux/mutex.h>

#define DSP_BUS_SCENARIO_NAME_LEN	(32)

struct dsp_system;
struct dsp_bus;

struct dsp_bus_scenario {
	unsigned int		id;
	char			name[DSP_BUS_SCENARIO_NAME_LEN];
	unsigned int		bts_scen_idx;
	struct mutex		lock;
	bool			enabled;
};

struct dsp_bus_ops {
	int (*mo_get_by_name)(struct dsp_bus *bus, unsigned char *name);
	int (*mo_put_by_name)(struct dsp_bus *bus, unsigned char *name);
	int (*mo_get_by_id)(struct dsp_bus *bus, unsigned int id);
	int (*mo_put_by_id)(struct dsp_bus *bus, unsigned int id);
	int (*mo_all_put)(struct dsp_bus *bus);

	int (*open)(struct dsp_bus *bus);
	int (*close)(struct dsp_bus *bus);
	int (*probe)(struct dsp_system *sys);
	void (*remove)(struct dsp_bus *bus);
};

struct dsp_bus {
	struct dsp_bus_scenario		*scen;

	const struct dsp_bus_ops	*ops;
	struct dsp_system		*sys;
};

int dsp_bus_set_ops(struct dsp_bus *bus, unsigned int dev_id);
int dsp_bus_register_ops(unsigned int dev_id, const struct dsp_bus_ops *ops);

#endif
