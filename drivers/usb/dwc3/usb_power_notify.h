/*
 * Copyright (C) 2015-2017 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

  /* usb_power_notify.h */

#ifndef __LINUX_USB_POWER_NOTIFY_H__
#define __LINUX_USB_POWER_NOTIFY_H__

#define HUB_MAX_DEPTH	7

extern void register_usb_power_notify(void);
extern void unregister_usb_power_notify(void);

enum usb_port_state {
	PORT_EMPTY = 0,		/* OTG only */
	PORT_USB2,		/* usb 2.0 device only */
	PORT_USB3,		/* usb 3.0 device only */
	PORT_HUB,		/* usb hub single */
	PORT_DP			/* DP device */
};

extern enum usb_port_state port_state;
#endif

