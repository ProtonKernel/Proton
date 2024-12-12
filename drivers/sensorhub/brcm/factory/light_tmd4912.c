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
		case 0:
			return snprintf(buf, PAGE_SIZE, "41.1 11.3 2.8\n");
		case 1:
			return snprintf(buf, PAGE_SIZE, "44.1 11.4 2.8\n");
		case 2:
			return snprintf(buf, PAGE_SIZE, "47.2 7.4 2.8\n");
		default:
			return snprintf(buf, PAGE_SIZE, "0.0 0.0 0.0\n");
	}

	return snprintf(buf, PAGE_SIZE, "0.0 0.0 0.0\n");
}

struct light_t light_tmd4912 = {
	.name = "TMD4912",
	.vendor = "AMS",
	.get_light_circle = light_circle_show
};

struct light_t* get_light_tmd4912(){
	return &light_tmd4912;
}