/*
 * Samsung Exynos SoC series FIMC-IS2 driver
 *
 * exynos fimc-is2 device interface functions
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_DEVICE_ITF_H
#define IS_DEVICE_ITF_H

#include "is-core.h"
#include "is-groupmgr.h"
#include "is-device-ischain.h"

int is_itf_s_param_wrap(struct is_device_ischain *device,
	u32 lindex, u32 hindex, u32 indexes);
int is_itf_a_param_wrap(struct is_device_ischain *device, u32 group);
int is_itf_f_param_wrap(struct is_device_ischain *device, u32 group);
int is_itf_enum_wrap(struct is_device_ischain *device);
void is_itf_storefirm_wrap(struct is_device_ischain *device);
void is_itf_restorefirm_wrap(struct is_device_ischain *device);
int is_itf_set_fwboot_wrap(struct is_device_ischain *device, u32 val);
int is_itf_open_wrap(struct is_device_ischain *device, u32 module_id,
	u32 flag, u32 offset_path);
int is_itf_close_wrap(struct is_device_ischain *device);
int is_itf_change_chain_wrap(struct is_device_ischain *device, struct is_group *group, u32 next_id);
int is_itf_setaddr_wrap(struct is_interface *itf,
	struct is_device_ischain *device, ulong *setfile_addr);
int is_itf_setfile_wrap(struct is_interface *itf, ulong setfile_addr,
	struct is_device_ischain *device);
int is_itf_map_wrap(struct is_device_ischain *device,
	u32 group, u32 shot_addr, u32 shot_size);
int is_itf_unmap_wrap(struct is_device_ischain *device, u32 group);
int is_itf_stream_on_wrap(struct is_device_ischain *device);
int is_itf_stream_off_wrap(struct is_device_ischain *device);
int is_itf_process_on_wrap(struct is_device_ischain *device, u32 group);
int is_itf_process_off_wrap(struct is_device_ischain *device, u32 group,
	u32 fstop);
void is_itf_sudden_stop_wrap(struct is_device_ischain *device, u32 instance, struct is_group *group);
int is_itf_power_down_wrap(struct is_interface *interface, u32 instance);
int is_itf_sys_ctl_wrap(struct is_device_ischain *device,
	int cmd, int val);
int is_itf_sensor_mode_wrap(struct is_device_ischain *device,
	struct is_sensor_cfg *cfg);
int is_itf_shot_wrap(struct is_device_ischain *device,
	struct is_group *group, struct is_frame *frame);
void is_itf_sfr_dump_wrap(struct is_device_ischain *device, u32 group, bool flag_print_log);

#endif
