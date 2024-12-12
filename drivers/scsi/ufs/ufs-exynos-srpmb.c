/*
 * Secure RPMB Driver for Exynos scsi rpmb
 *
 * Copyright (C) 2016 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ufshcd.h"
#include "ufs-exynos.h"

bool rpmb_wlun_clr_uac = true;

bool exynos_ufs_srpmb_get_wlun_uac(void)
{
	return rpmb_wlun_clr_uac;
}
EXPORT_SYMBOL(exynos_ufs_srpmb_get_wlun_uac);

void exynos_ufs_srpmb_set_wlun_uac(bool flag)
{
	rpmb_wlun_clr_uac = flag;
}
EXPORT_SYMBOL(exynos_ufs_srpmb_set_wlun_uac);
