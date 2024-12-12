/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "is-notifier.h"


/*
 * Notifier list for kernel code which wants to know
 * camera module versions
 */
static RAW_NOTIFIER_HEAD(dev_cam_eeprom_noti_chain);

/* Add notifier_head for additional notification event from camera*/


#ifdef USE_CAMERA_NOTIFY_WACOM /* Wacom driver specific */

/*
 * notify_value		- store camera eeprom information
 * notify_cam_order	- defined for camera order. the value represent bit offset
 * 
 * each camera use 1 byte.
 *
 * store up to 8 cameras. (64bit)
 */
static u64 notify_value = 0;
static int notify_cam_order[] = { 0, 32, 8, 40, 16, -1, 24};

static char cam_notify_header_list[] = {'S','C', 'P', 'M', 'V', 'N', 'A', 'Y', 'H'};

/**
 * is_eeprom_info_update - update camera eeprom information to wacom_notify_value
 *	@rom_id:	eeprom rom id
 *	@header_ver:	eeprom header string. char[10]
 */
void is_eeprom_info_update(int rom_id, char *header_ver)
{
	uint checkIndex;

	if (notify_cam_order[rom_id] == -1) {
		printk(KERN_ERR "[@] rom_id:%d err, unsupported id", rom_id);
		return;
	}

	for (checkIndex = 0; checkIndex < ARRAY_SIZE(cam_notify_header_list); checkIndex++) {
		if (header_ver[9] == cam_notify_header_list[checkIndex]) {
			notify_value |= (u64)(checkIndex + 1) << notify_cam_order[rom_id];
			break;
		}
	}

	if (checkIndex == ARRAY_SIZE(cam_notify_header_list))
		printk(KERN_INFO "[@] not matched");

	printk(KERN_INFO "[@] current header(%d:%s), value :%llx", rom_id, header_ver, notify_value);
}

/**
 * is_eeprom_wacom_update_notifier - Send wacom_notify_value to notifier block.
 *
 *	Define the mask if you need to send only information from certain cameras.
 */
int is_eeprom_wacom_update_notifier()
{
#ifdef CAMERA_NOTIFY_WACOM_CAM_MASK
	u64 value = CAMERA_NOTIFY_WACOM_CAM_MASK & notify_value;
#else
	u64 value = notify_value;
#endif

	printk(KERN_INFO "[@] notify value 0x%llx", value);
	return raw_notifier_call_chain(&dev_cam_eeprom_noti_chain, value, NULL);
}
#endif /* USE_CAMERA_NOTIFY_WACOM */

/* Common Notifier API */
inline int is_register_eeprom_notifier(struct notifier_block *nb)
{
	return is_register_notifier(&dev_cam_eeprom_noti_chain, nb);
}
EXPORT_SYMBOL_GPL(is_register_eeprom_notifier);

inline int is_unregister_eeprom_notifier(struct notifier_block *nb)
{
	return is_unregister_notifier(&dev_cam_eeprom_noti_chain, nb);
}
EXPORT_SYMBOL_GPL(is_unregister_eeprom_notifier);

int is_register_notifier(struct raw_notifier_head *head, struct notifier_block *nb)
{
	if (!nb)
		return -ENOENT;

	return raw_notifier_chain_register(head,nb);
}

int is_unregister_notifier(struct raw_notifier_head *head, struct notifier_block *nb)
{
	if (!nb)
		return -ENOENT;

	return raw_notifier_chain_unregister(head,nb);
}
