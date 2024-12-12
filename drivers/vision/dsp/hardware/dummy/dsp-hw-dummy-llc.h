/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DUMMY_DSP_HW_DUMMY_LLC_H__
#define __HW_DUMMY_DSP_HW_DUMMY_LLC_H__

#include "hardware/dsp-llc.h"

int dsp_hw_dummy_llc_get_by_name(struct dsp_llc *llc, unsigned char *name);
int dsp_hw_dummy_llc_put_by_name(struct dsp_llc *llc, unsigned char *name);
int dsp_hw_dummy_llc_get_by_id(struct dsp_llc *llc, unsigned int id);
int dsp_hw_dummy_llc_put_by_id(struct dsp_llc *llc, unsigned int id);
int dsp_hw_dummy_llc_all_put(struct dsp_llc *llc);

int dsp_hw_dummy_llc_open(struct dsp_llc *llc);
int dsp_hw_dummy_llc_close(struct dsp_llc *llc);
int dsp_hw_dummy_llc_probe(struct dsp_system *sys);
void dsp_hw_dummy_llc_remove(struct dsp_llc *llc);

#endif
