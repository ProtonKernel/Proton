/* linux/drivers/video/fbdev/exynos/dpu/dqe_common.h
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SAMSUNG_DQE_H__
#define __SAMSUNG_DQE_H__

#include "decon.h"
#if defined(CONFIG_SOC_EXYNOS2100)
#include "./cal_2100/regs-dqe.h"
#endif

#define dqe_err(fmt, ...)							\
	do {									\
		if (dqe_log_level >= 3) {					\
			pr_err(pr_fmt(fmt), ##__VA_ARGS__);			\
		}								\
	} while (0)

#define dqe_warn(fmt, ...)							\
	do {									\
		if (dqe_log_level >= 4) {					\
			pr_warn(pr_fmt(fmt), ##__VA_ARGS__);			\
		}								\
	} while (0)

#define dqe_info(fmt, ...)							\
	do {									\
		if (dqe_log_level >= 6)					\
			pr_info(pr_fmt(fmt), ##__VA_ARGS__);			\
	} while (0)

#define dqe_dbg(fmt, ...)							\
	do {									\
		if (dqe_log_level >= 7)					\
			pr_info(pr_fmt(fmt), ##__VA_ARGS__);			\
	} while (0)

static inline u32 dqe_read(u32 reg_id)
{
	struct decon_device *decon = get_decon_drvdata(0);

	return readl(decon->res.dqe_regs + reg_id);
}

static inline u32 dqe_read_mask(u32 reg_id, u32 mask)
{
	u32 val = dqe_read(reg_id);

	val &= (mask);
	return val;
}

static inline void dqe_write(u32 reg_id, u32 val)
{
	struct decon_device *decon = get_decon_drvdata(0);

	writel(val, decon->res.dqe_regs + reg_id);
}

static inline void dqe_write_mask(u32 reg_id, u32 val, u32 mask)
{
	struct decon_device *decon = get_decon_drvdata(0);
	u32 old = dqe_read(reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, decon->res.dqe_regs + reg_id);
}

static inline bool IS_DQE_OFF_STATE(struct decon_device *decon)
{
	return decon == NULL ||
		decon->state == DECON_STATE_OFF ||
		decon->state == DECON_STATE_DOZE_SUSPEND ||
		decon->state == DECON_STATE_INIT;
}

struct dqe_reg_dump {
	u32 addr;
	u32 val;
};

struct dqe_ctx {
	struct dqe_reg_dump gamma_matrix[DQE_GAMMA_MATRIX_REG_MAX];
	struct dqe_reg_dump degamma_lut[DQE_DEGAMMA_REG_MAX];
	struct dqe_reg_dump cgc_con[DQE_CGC_CON_REG_MAX];
	struct dqe_reg_dump cgc_lut[3][DQE_CGC_REG_MAX];
	struct dqe_reg_dump regamma_lut[DQE_REGAMMA_REG_MAX];
	u32 gamma_matrix_on;
	u32 degamma_on;
	u32 cgc_on;
	u32 regamma_on;
	bool need_udpate;
	bool cgc_update[2];
};

enum dqe_state {
	DQE_STATE_DISABLE = 0,
	DQE_STATE_ENABLE,
};

struct dqe_device {
	struct device *dev;
	struct decon_device *decon;
	struct mutex lock;
	struct mutex restore_lock;
	struct dqe_ctx ctx;
	enum dqe_state state;
};

static inline bool IS_DQE_DISABLE(struct dqe_device *dqe)
{
	return dqe == NULL ||
		dqe->state == DQE_STATE_DISABLE;
}

extern int dqe_log_level;

/* CAL APIs list */
void dqe_reg_start(u32 id, struct exynos_panel_info *lcd_info);
void dqe_reg_stop(u32 id);

void __dqe_dump(u32 id, void __iomem *dqe_regs);

void dqe_reg_set_gamma_matrix_on(u32 on);
u32 dqe_reg_get_gamma_matrix_on(void);

void dqe_reg_set_degamma_on(u32 on);
u32 dqe_reg_get_degamma_on(void);

void dqe_reg_set_cgc_on(u32 on);
u32 dqe_reg_get_cgc_on(void);

void dqe_reg_set_regamma_on(u32 on);
u32 dqe_reg_get_regamma_on(void);

void dqe_reg_set_cgc_dither_on(u32 on);
u32 dqe_reg_get_cgc_dither_on(void);

void decon_dqe_restore_context(struct decon_device *decon);
void decon_dqe_restore_cgc_context(struct decon_device *decon);
void decon_dqe_start(struct decon_device *decon, struct exynos_panel_info *lcd_info);
void decon_dqe_enable(struct decon_device *decon);
void decon_dqe_disable(struct decon_device *decon);
int decon_dqe_create_interface(struct decon_device *decon);

int decon_dqe_set_color_mode(struct decon_color_mode_with_render_intent_info *color_mode);
int decon_dqe_set_color_transform(struct decon_color_transform_info *transform);

/* APIs for TUI */
int decon_dqe_get_e_path(struct decon_device *decon, enum dqe_state *state,
					enum decon_enhance_path *e_path);
int decon_dqe_set_e_path(struct decon_device *decon, enum dqe_state state,
					enum decon_enhance_path e_path);

/* APS GAMMA */
#define APS_GAMMA_BIT_LENGTH			(8)
#define APS_GAMMA_BIT				(8)
#define APS_GAMMA_PHASE_SHIFT			(APS_GAMMA_BIT - 6)
#define APS_GAMMA_INPUT_MASK			(0x3f << APS_GAMMA_PHASE_SHIFT)
#define APS_GAMMA_PHASE_MASK			((1 << APS_GAMMA_PHASE_SHIFT)-1)
#define APS_GAMMA_PIXEL_MAX			((1 << APS_GAMMA_BIT)-1)
#define APS_GAMMA_INTP_LIMIT			(0x7 << APS_GAMMA_BIT)
#define APS_GAMMA_DEFAULT_VALUE_ALIGN		(APS_GAMMA_BIT-8)

#endif
