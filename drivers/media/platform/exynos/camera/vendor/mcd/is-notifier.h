/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_NOTIFIER_H
#define IS_NOTIFIER_H

#include <linux/notifier.h>
#include "is-vendor-config.h"

void is_eeprom_info_update(int rom_id, char *header_ver);
int is_eeprom_wacom_update_notifier(void);
int is_register_notifier(struct raw_notifier_head*, struct notifier_block*);
int is_unregister_notifier(struct raw_notifier_head*, struct notifier_block*);

#endif /* IS_NOTIFIER_H */
