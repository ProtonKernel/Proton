// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dl/dsp-string-tree.h"

static unsigned int __get_idx(char ch)
{
	if (ch == '_')
		return 56;
	else if (ch >= 'a' && ch <= 'z')
		return ch - 'a';
	else if (ch >= 'A' && ch <= 'Z')
		return ch - 'A' + 28;

	return 0;
}

void dsp_string_tree_init(struct dsp_string_tree_node *node)
{
	int idx;

	node->ret_value = -1;

	for (idx = 0; idx < STRING_NODE_MAX; idx++)
		node->next[idx] = NULL;
}

void dsp_string_tree_free(struct dsp_string_tree_node *node)
{
	int idx;

	for (idx = 0; idx < STRING_NODE_MAX; idx++) {
		struct dsp_string_tree_node *next_node = node->next[idx];

		if (next_node) {
			dsp_string_tree_free(next_node);
			dsp_dl_free(next_node);
		}
	}
}

void dsp_string_tree_push(struct dsp_string_tree_node *head, const char *str,
	int ret_value)
{
	struct dsp_string_tree_node *cur_node = head;
	unsigned int idx;

	for (idx = 0; idx < strlen(str); idx++) {
		char ch = str[idx];
		unsigned int str_idx = __get_idx(ch);
		struct dsp_string_tree_node *next_node;

		next_node = cur_node->next[str_idx];
		if (!next_node) {
			next_node =
				(struct dsp_string_tree_node *)dsp_dl_malloc(
					sizeof(*next_node),
					"String tree node");
			dsp_string_tree_init(next_node);
			cur_node->next[str_idx] = next_node;
		}

		cur_node = next_node;

		if (idx == strlen(str) - 1)
			cur_node->ret_value = ret_value;
	}
}

int dsp_string_tree_get(struct dsp_string_tree_node *head, const char *str)
{
	struct dsp_string_tree_node *cur_node = head;
	unsigned int idx;

	for (idx = 0; idx < strlen(str); idx++) {
		char ch = str[idx];
		unsigned int str_idx = __get_idx(ch);
		struct dsp_string_tree_node *next_node;

		next_node = cur_node->next[str_idx];
		if (!next_node)
			return -1;

		cur_node = next_node;
	}

	return cur_node->ret_value;
}
