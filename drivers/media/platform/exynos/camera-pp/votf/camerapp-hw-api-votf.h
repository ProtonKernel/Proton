/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Exynos CAMERA-PP VOTF driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef CAMERAPP_HW_API_VOTF_H
#define CAMERAPP_HW_API_VOTF_H

void camerapp_hw_votf_create_ring(void __iomem *base_addr, int ip, int module);
void camerapp_hw_votf_destroy_ring(void __iomem *base_addr, int ip, int module);
void camerapp_hw_votf_set_sel_reg(void __iomem *base_addr, u32 set, u32 mode);
void camerapp_hw_votf_reset(void __iomem *base_addr, int module);
void camerapp_hw_votf_sw_core_reset(void __iomem *base_addr, int module);
u32 camerapp_hw_votf_wrapper_reset(void __iomem *base_addr);

void camerapp_hw_votf_set_flush(void __iomem *votf_addr, u32 offset);

void camerapp_hw_votf_set_crop_start(void __iomem *votf_addr, u32 offset, bool start);
u32 camerapp_hw_votf_get_crop_start(void __iomem *votf_addr, u32 offset);
void camerapp_hw_votf_set_crop_enable(void __iomem *votf_addr, u32 offset, bool enable);
u32 camerapp_hw_votf_get_crop_enable(void __iomem *votf_addr, u32 offset);

void camerapp_hw_votf_set_recover_enable(void __iomem *base_addr, u32 offset, u32 cfg);
void camerapp_hw_votf_set_token_size(void __iomem *votf_addr, u32 offset, u32 token_size);
void camerapp_hw_votf_set_first_token_size(void __iomem *votf_addr, u32 offset, u32 token_size);
void camerapp_hw_votf_set_frame_size(void __iomem *votf_addr, u32 offset, u32 frame_size);

void camerapp_hw_votf_set_enable(void __iomem *votf_addr, u32 offset, bool enable);
u32 camerapp_hw_votf_get_enable(void __iomem *votf_addr, u32 offset);
void camerapp_hw_votf_set_limit(void __iomem *votf_addr, u32 offset, u32 limit);
void camerapp_hw_votf_set_dest(void __iomem *votf_addr, u32 offset, u32 dest);

void camerapp_hw_votf_set_start(void __iomem *votf_addr, u32 offset, u32 start);
void camerapp_hw_votf_set_finish(void __iomem *votf_addr, u32 offset, u32 finish);
void camerapp_hw_votf_set_threshold(void __iomem *votf_addr, u32 offset, u32 value);
u32 camerapp_hw_votf_get_threshold(void __iomem *votf_addr, u32 offset);
void camerapp_hw_votf_set_read_bytes(void __iomem *votf_addr, u32 offset, u32 bytes);
u32 camerapp_hw_votf_get_fullness(void __iomem *votf_addr, u32 offset);
u32 camerapp_hw_votf_get_busy(void __iomem *votf_addr, u32 offset);
void camerapp_hw_votf_set_irq_enable(void __iomem *votf_addr, u32 offset, u32 irq);
void camerapp_hw_votf_set_irq_status(void __iomem *votf_addr, u32 offset, u32 irq);
void camerapp_hw_votf_set_irq(void __iomem *votf_addr, u32 offset, u32 irq);
void camerapp_hw_votf_set_irq_clear(void __iomem *votf_addr, u32 offset, u32 irq);

bool camerapp_check_votf_ring(void __iomem *base_addr, int module);
int camerapp_hw_votf_get_debug_state(void __iomem *base_addr, int module, int id, int service,
	u32 *value, u32 *state);
void camerapp_hw_votf_rejection_token(void __iomem *base_addr, int module,
	int trs_ip, int trs_id, int tws_ip, int tws_id);

void camerapp_hw_votf_sfr_dump(void __iomem *base_addr, int module, int module_type);
#endif /* CAMERAPP_HW_API_VOTF_H */
