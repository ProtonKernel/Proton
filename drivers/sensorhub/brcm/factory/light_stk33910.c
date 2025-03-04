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
			return snprintf(buf, PAGE_SIZE, "42.4 9.3 2.5\n");
		default:
			return snprintf(buf, PAGE_SIZE, "0.0 0.0 0.0\n");
	}

	return snprintf(buf, PAGE_SIZE, "0.0 0.0 0.0\n");
}

struct light_t light_stk33910 = {
	.name = "STK33910",
	.vendor = "SITRON",
	.get_light_circle = light_circle_show
};

struct light_t* get_light_stk33910(){
	return &light_stk33910;
}
