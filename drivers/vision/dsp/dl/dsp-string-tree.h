/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DL_DSP_STRING_TREE_H__
#define __DL_DSP_STRING_TREE_H__

#include "dl/dsp-common.h"

#define STRING_NODE_MAX		(57)

struct dsp_string_tree_node {
	int ret_value;
	struct dsp_string_tree_node *next[STRING_NODE_MAX];
};

void dsp_string_tree_init(struct dsp_string_tree_node *node);
void dsp_string_tree_free(struct dsp_string_tree_node *node);

void dsp_string_tree_push(struct dsp_string_tree_node *head,
	const char *str, int ret_value);
int dsp_string_tree_get(struct dsp_string_tree_node *head, const char *str);

#endif
