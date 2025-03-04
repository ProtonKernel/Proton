/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#include "../ssp.h"
#include "sensors.h"

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

static ssize_t light_circle_show(struct device *dev, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	switch(data->ap_type) {
#if defined(CONFIG_SENSORS_SSP_CANVAS)
		case 0:
			return snprintf(buf, PAGE_SIZE, "42.1 7.8 2.4\n");
		case 1:
			return snprintf(buf, PAGE_SIZE, "45.2 7.3 2.4\n");
#elif defined(CONFIG_SENSORS_SSP_PICASSO)
		case 0:
			return snprintf(buf, PAGE_SIZE, "45.1 8.2 2.4\n");
		case 1:
			return snprintf(buf, PAGE_SIZE, "42.6 8.0 2.4\n");
		case 2:
			return snprintf(buf, PAGE_SIZE, "44.0 8.0 2.4\n");
#endif
		default:
			return snprintf(buf, PAGE_SIZE, "0.0 0.0 0.0\n");
	}

	return snprintf(buf, PAGE_SIZE, "0.0 0.0 0.0\n");
}

struct light_t light_tmd4907 = {
	.name = "TMD4907",
	.vendor = "AMS",
	.get_light_circle = light_circle_show
};

struct light_t* get_light_tmd4907(){
	return &light_tmd4907;
}