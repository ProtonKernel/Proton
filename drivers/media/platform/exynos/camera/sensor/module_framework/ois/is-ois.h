/*
 * Samsung Exynos5 SoC series OIS driver
 *
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_OIS_FW_H
#define IS_OIS_FW_H

#define IS_OIS_SDCARD_PATH			"/vendor/firmware/"
#define IS_OIS_FW_NAME_SEC			"ois_fw_sec.bin"
#define IS_OIS_FW_NAME_DOM			"ois_fw_dom.bin"

#ifdef CONFIG_OIS_DIRECT_FW_CONTROL
int is_ois_fw_open(struct is_ois *ois, char *name);
int is_ois_fw_ver_copy(struct is_ois *ois, u8 *buf, long size);
#endif
#endif
