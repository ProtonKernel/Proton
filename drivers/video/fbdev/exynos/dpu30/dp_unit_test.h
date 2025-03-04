/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * DP unit test
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _DP_UNIT_TEST_H_
#define _DP_UNIT_TEST_H_

__visible_for_testing int displayport_init_sst_info(struct displayport_device *displayport);
__visible_for_testing int displayport_make_avi_infoframe_data(u32 sst_id, struct infoframe *avi_infoframe);
__visible_for_testing int displayport_make_audio_infoframe_data(struct infoframe *audio_infoframe,
		struct displayport_audio_config_data *audio_config_data);
__visible_for_testing int displayport_make_hdr_infoframe_data
	(struct infoframe *hdr_infoframe, struct exynos_hdr_static_info *hdr_info);
__visible_for_testing int displayport_make_spd_infoframe_data(struct infoframe *spd_infoframe);





int unit_edid_checksum(u8 *data, int block);
u32 unit_edid_update(struct displayport_device *displayport);
int unit_displayport_init_sst_info(struct displayport_device *displayport);
bool unit_edid_set_preferred_preset(int mode);
int unit_edid_find_resolution(u16 xres, u16 yres, u16 refresh);

#endif
