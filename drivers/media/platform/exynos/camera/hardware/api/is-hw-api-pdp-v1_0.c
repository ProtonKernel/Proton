/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-hw-api-pdp-v1.h"
#include "sfr/is-sfr-pdp-v1_0.h"
#include "is-device-sensor.h"

void pdp_hw_s_init(void __iomem *base)
{
}

void pdp_hw_s_global(void __iomem *base, u32 ch, u32 path, void *data)
{
}

void pdp_hw_s_context(void __iomem *base, u32 curr_ch, u32 curr_path)
{
}

void pdp_hw_s_path(void __iomem *base, u32 path)
{
}

void pdp_hw_s_wdma_enable(void __iomem *base, dma_addr_t address)
{
}

void pdp_hw_s_rdma_init(void __iomem *base, u32 width, u32 height, u32 hwformat, u32 pixelsize,
	u32 rmo, u32 en_sdc, u32 en_votf, u32 en_dma)
{
}

void pdp_hw_update_rdma_linegap(void __iomem *base,
		struct is_sensor_cfg *sensor_cfg,
		struct is_pdp *pdp,
		u32 en_votf)
{
	/* no ops */
}

void pdp_hw_s_af_rdma_init(void __iomem *base, u32 width, u32 height, u32 hwformat, u32 rmo,
	u32 en_votf, u32 en_dma)
{
}

void pdp_hw_g_int1_str(const char **int_str)
{
	int i;

	for (i = 0; i < PDP_INT1_CNT; i++)
		int_str[i] = pdp_int1_str[i];
}

void pdp_hw_g_int2_str(const char **int_str)
{
}

static int pdp_hw_s_irq_msk(void __iomem *base, u32 msk)
{
	is_hw_set_reg(base, &pdp_regs[PDP_R_PDP_INT], 0);
	is_hw_set_reg(base, &pdp_regs[PDP_R_PDP_INT_MASK], msk);

	return 0;
}

void pdp_hw_s_sensor_type(void __iomem *base, u32 sensor_type)
{
	is_hw_set_field(base, &pdp_regs[PDP_R_SENSOR_TYPE],
			&pdp_fields[PDP_F_SENSOR_TYPE], sensor_type);
}

void pdp_hw_s_core(void __iomem *base, bool pd_enable,
	u32 img_width, u32 img_height, u32 img_hwformat, u32 img_pixelsize,
	u32 pd_width, u32 pd_height, u32 pd_hwformat,
	u32 sensor_type, u32 path, int sensor_mode, u32 fps)
{
	if (enable)
		pdp_hw_s_irq_msk(base, 0x1F & ~(PDP_INT_FRAME_PAF_STAT0 | PDP_INT_FRAME_END | PDP_INT_FRAME_START));
	else
		pdp_hw_s_irq_msk(base, 0x1F);

	is_hw_set_field(base, &pdp_regs[PDP_R_PDP_CORE_ENABLE],
			&pdp_fields[PDP_F_PDP_CORE_ENABLE], enable);
}

unsigned int pdp_hw_g_irq_state(void __iomem *base, bool clear)
{
	u32 src;

	src = is_hw_get_reg(base, &pdp_regs[PDP_R_PDP_INT]);
	if (clear)
		is_hw_set_reg(base, &pdp_regs[PDP_R_PDP_INT], src);

	return src;
}

unsigned int pdp_hw_g_irq_mask(void __iomem *base)
{
	return ~(is_hw_get_reg(base, &pdp_regs[PDP_R_PDP_INT_MASK]));
}

int pdp_hw_g_stat0(void __iomem *base, void *buf, size_t len)
{
	/* PDP_R_PAF_STAT0_END be excepted from STAT0 */
	size_t stat0_len = pdp_regs[PDP_R_PAF_STAT0_END].sfr_offset
			- pdp_regs[PDP_R_PAF_STAT0_START].sfr_offset;

	if (len < stat0_len) {
		stat0_len = len;
		warn("the size of STAT0 buffer is too small: %zd < %zd",
							len, stat0_len);
	}

	memcpy((void *)buf, base + pdp_regs[PDP_R_PAF_STAT0_START].sfr_offset,
								stat0_len);

	return stat0_len;
}

void pdp_hw_s_config_default(void __iomem *base)
{
	int i;
	u32 index;
	u32 value;
	int count = ARRAY_SIZE(pdp_global_init_table);

	for (i = 0; i < count; i++) {
		index = pdp_global_init_table[i].index;
		value = pdp_global_init_table[i].init_value;
		is_hw_set_reg(base, &pdp_regs[index], value);
	}
}

bool pdp_hw_to_sensor_type(u32 pd_mode, u32 *sensor_type)
{
	bool enable;

	switch (pd_mode) {
	case PD_MSPD:
		*sensor_type = SENSOR_TYPE_MSPD;
		enable = true;
		break;
	case PD_MOD1:
		*sensor_type = SENSOR_TYPE_MOD1;
		enable = true;
		break;
	case PD_MOD2:
	case PD_MSPD_TAIL:
	case PD_NONE:
		*sensor_type = SENSOR_TYPE_MOD2;
		enable = false;
		break;
	case PD_MOD3:
		*sensor_type = SENSOR_TYPE_MOD3;
		enable = true;
		break;
	default:
		warn("PD MODE(%d) is invalid", pd_mode);
		*sensor_type = SENSOR_TYPE_MOD2;
		enable = false;
		break;
	}

	return enable;
}

unsigned int pdp_hw_is_occurred(unsigned int state, enum pdp_event_type type)
{
	return state & (1 << type);
}

int pdp_hw_dump(void __iomem *base)
{
	info("PDP SFR DUMP (v1.0)\n");

	is_hw_set_field(base, &pdp_regs[PDP_R_PDP_CORE_ENABLE],
			&pdp_fields[PDP_F_PDP_READSHADOWREG], 1);

	is_hw_dump_regs(base, pdp_regs, PDP_REG_CNT);

	is_hw_set_field(base, &pdp_regs[PDP_R_PDP_CORE_ENABLE],
			&pdp_fields[PDP_F_PDP_READSHADOWREG], 0);

	return 0;
}
