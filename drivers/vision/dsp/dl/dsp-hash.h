/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DL_DSP_HASH_H__
#define __DL_DSP_HASH_H__

#include "dl/dsp-common.h"
#include "dl/dsp-list.h"

#define DSP_HASH_MAX		(300)

struct dsp_hash_node {
	unsigned int key;
	char *str;
	void *value;
	struct dsp_list_node node;
};

struct dsp_hash_tab {
	struct dsp_list_head list[DSP_HASH_MAX];
};

void dsp_hash_tab_init(struct dsp_hash_tab *tab);
unsigned int dsp_hash_get_key(const char *k);

void dsp_hash_push(struct dsp_hash_tab *tab, const char *k, void *value);
void dsp_hash_pop(struct dsp_hash_tab *tab, const char *k, int free_data);

int dsp_hash_get(struct dsp_hash_tab *tab, const char *k, void **value);
void dsp_hash_free(struct dsp_hash_tab *tab, int free_data);

#endif
