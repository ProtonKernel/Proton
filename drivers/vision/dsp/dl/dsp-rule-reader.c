// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dl/dsp-rule-reader.h"

void dsp_type_print(struct dsp_type *type)
{
	if (type->sign == SIGNED)
		DL_BUF_STR("s");
	else
		DL_BUF_STR("u");

	DL_BUF_STR("%d\n", type->bit_sz);
	DL_PRINT_BUF(INFO);
}

void dsp_exp_binary_op(struct dsp_list_head *head, struct dsp_list_head *head1,
	struct dsp_list_head *head2, char op)
{
	struct dsp_exp_element *tmp = (struct dsp_exp_element *)dsp_dl_malloc(
			sizeof(*tmp), "Exp elem");

	tmp->type = EXP_OP;
	tmp->op = op;
	dsp_list_node_init(&tmp->list_node);
	dsp_list_merge(head, head1, head2);
	dsp_list_node_push_back(head, &tmp->list_node);
}

void dsp_exp_print(struct dsp_list_head *head)
{
	struct dsp_list_node *node;

	dsp_list_for_each(node, head) {
		struct dsp_exp_element *element =
			container_of(node, struct dsp_exp_element, list_node);

		if (element->type == EXP_INTEGER)
			DL_BUF_STR("%d ", element->integer);

		if (element->type == EXP_OP)
			DL_BUF_STR("%c ", element->op);

		if (element->type == EXP_LINKER_VALUE) {
			switch (element->linker_value) {
			case ADDEND:
				DL_BUF_STR("addend ");
				break;
			case BLOCK_ADDR_AR:
				DL_BUF_STR("block_addr_AR ");
				break;
			case BLOCK_ADDR_BR:
				DL_BUF_STR("block_addr_BR ");
				break;
			case ITEM_ADDR_AR:
				DL_BUF_STR("item_addr_AR ");
				break;
			case ITEM_ADDR_BR:
				DL_BUF_STR("item_addr_BR ");
				break;
			case ITEM_VALUE:
				DL_BUF_STR("item_value ");
				break;
			case STORED_VALUE:
				DL_BUF_STR("stored_value ");
				break;
			case SYMBOL_ADDR_AR:
				DL_BUF_STR("symbol_addr_AR ");
				break;
			case SYMBOL_ADDR_BR:
				DL_BUF_STR("symbol_addr_BR ");
				break;
			case SYMBOL_BLOCK_ADDR_AR:
				DL_BUF_STR("symbol_block_addr_AR ");
				break;
			case SYMBOL_BLOCK_ADDR_BR:
				DL_BUF_STR("symbol_block_addr_BR ");
				break;
			}
		}
	}
	DL_BUF_STR("\n");
	DL_PRINT_BUF(INFO);
}

int dsp_exp_import(struct dsp_exp_element *exp, struct dsp_dl_lib_file *file)
{
	int ret;

	ret = dsp_dl_lib_file_read((char *)exp, sizeof(*exp), file);
	if (ret == -1) {
		DL_ERROR("[%s] CHK_ERR\n", __func__);
		return -1;
	}

	return 0;
}

void dsp_exp_free(struct dsp_list_head *head)
{
	dsp_list_free(head, struct dsp_exp_element, list_node);
}

void dsp_bit_ext_print(enum dsp_bit_ext_type ext)
{
	switch (ext) {
	case BIT_ONE:
		DL_BUF_STR("one ");
		break;
	case BIT_ZERO:
		DL_BUF_STR("zero ");
		break;
	case BIT_SIGN:
		DL_BUF_STR("sign ");
		break;
	case BIT_NONE:
		break;
	}
}

void dsp_bit_slice_print(struct dsp_list_head *head)
{
	struct dsp_list_node *node;

	dsp_list_for_each(node, head) {
		struct dsp_bit_slice *elem =
			container_of(node, struct dsp_bit_slice, list_node);

		if (elem->high == elem->low) {
			DL_BUF_STR("[ ");
			dsp_bit_ext_print(elem->h_ext);
			DL_BUF_STR("%d ", elem->high);
			dsp_bit_ext_print(elem->l_ext);
			DL_BUF_STR("]@%d ", elem->value);
		} else {
			DL_BUF_STR("[");
			dsp_bit_ext_print(elem->h_ext);
			DL_BUF_STR("%d..%d", elem->high, elem->low);
			dsp_bit_ext_print(elem->l_ext);
			DL_BUF_STR("]");
			DL_BUF_STR("@%d ", elem->value);
		}
	}
	DL_BUF_STR("\n");
	DL_PRINT_BUF(INFO);
}

int dsp_bit_slice_import(struct dsp_bit_slice *bs, struct dsp_dl_lib_file *file)
{
	int ret;

	ret = dsp_dl_lib_file_read((char *)bs, sizeof(*bs), file);
	if (ret == -1) {
		DL_ERROR("[%s] CHK_ERR\n", __func__);
		return -1;
	}

	return 0;
}

void dsp_bit_slice_free(struct dsp_list_head *head)
{
	dsp_list_free(head, struct dsp_bit_slice, list_node);
}

void dsp_pos_print(struct dsp_list_head *head)
{
	struct dsp_list_node *node;

	dsp_list_for_each(node, head) {
		struct dsp_pos *elem =
			container_of(node, struct dsp_pos, list_node);

		if (elem->type == BIT_POS)
			DL_INFO("@%d\n", elem->bit_pos);
		else
			dsp_bit_slice_print(&elem->bit_slice_list);
	}
}

int dsp_pos_import(struct dsp_pos *pos, struct dsp_dl_lib_file *file)
{
	int ret, idx;

	ret = dsp_dl_lib_file_read((char *)pos, sizeof(*pos), file);
	if (ret == -1) {
		DL_ERROR("[%s] CHK_ERR\n", __func__);
		return -1;
	}

	if (pos->type == BIT_SLICE_LIST) {
		int list_num = pos->bit_slice_list.num;

		dsp_list_head_init(&pos->bit_slice_list);

		for (idx = 0; idx < list_num; idx++) {
			struct dsp_bit_slice *bs =
				(struct dsp_bit_slice *)dsp_dl_malloc(
					sizeof(*bs),
					"Bit slice");

			dsp_bit_slice_import(bs, file);
			dsp_list_node_init(&bs->list_node);
			dsp_list_node_push_back(&pos->bit_slice_list,
				&bs->list_node);
		}
	}

	return 0;
}

void dsp_pos_free(struct dsp_list_head *head)
{
	struct dsp_list_node *cur, *next;

	cur = head->next;

	while (cur != NULL) {
		struct dsp_pos *pos =
			container_of(cur, struct dsp_pos, list_node);

		next = cur->next;

		if (pos->type == BIT_SLICE_LIST)
			dsp_bit_slice_free(&pos->bit_slice_list);

		dsp_dl_free(pos);
		cur = next;
	}
}

void dsp_cont_print(struct dsp_cont *cont)
{
	if (cont->inst_num > 1)
		DL_BUF_STR("[%d]", cont->inst_num);

	dsp_type_print(&cont->type);
}

void dsp_range_print(enum dsp_range_type ran)
{
	if (ran == RANGE_STRICT)
		DL_BUF_STR("strict\n");
	else if (ran == RANGE_NOSTRICT)
		DL_BUF_STR("nostrict\n");

	DL_PRINT_BUF(INFO);
}

void dsp_reloc_rule_print(struct dsp_reloc_rule *rule)
{
	DL_INFO("Reloc rule[%d]\n", rule->idx);
	DL_INFO("Exp:\n");
	dsp_exp_print(&rule->exp);

	DL_BUF_STR("Type: ");
	dsp_type_print(&rule->type);

	DL_INFO("Pos(%d):\n", rule->pos_list.num);
	dsp_pos_print(&rule->pos_list);

	DL_BUF_STR("Cont: ");
	dsp_cont_print(&rule->cont);

	DL_BUF_STR("Range: ");
	dsp_range_print(rule->range_chk);
}

int dsp_reloc_rule_import(struct dsp_reloc_rule *rule,
	struct dsp_dl_lib_file *file)
{
	int ret;
	int list_num, idx;

	ret = dsp_dl_lib_file_read((char *)rule, sizeof(*rule), file);
	if (ret == -1) {
		DL_ERROR("[%s] CHK_ERR\n", __func__);
		return -1;
	}

	list_num = rule->exp.num;
	dsp_list_head_init(&rule->exp);

	for (idx = 0; idx < list_num; idx++) {
		struct dsp_exp_element *exp =
			(struct dsp_exp_element *)
			dsp_dl_malloc(
				sizeof(*exp),
				"Exp elem");

		ret = dsp_exp_import(exp, file);

		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		dsp_list_node_init(&exp->list_node);
		dsp_list_node_push_back(&rule->exp, &exp->list_node);
	}

	list_num = rule->pos_list.num;
	dsp_list_head_init(&rule->pos_list);

	for (idx = 0; idx < list_num; idx++) {
		struct dsp_pos *pos = (struct dsp_pos *)dsp_dl_malloc(
				sizeof(*pos), "Pos");

		ret = dsp_pos_import(pos, file);

		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		dsp_list_node_init(&pos->list_node);
		dsp_list_node_push_back(&rule->pos_list, &pos->list_node);
	}

	return 0;
}

void dsp_reloc_rule_free(struct dsp_reloc_rule *rule)
{
	dsp_exp_free(&rule->exp);
	dsp_pos_free(&rule->pos_list);
}

void dsp_reloc_rule_list_print(struct dsp_reloc_rule_list *list)
{
	int idx;

	DL_INFO(DL_BORDER);
	DL_INFO("Reloc rules\n");

	for (idx = 0; idx < list->cnt; idx++) {
		dsp_reloc_rule_print(list->list[idx]);
		DL_INFO("\n");
	}
}

int dsp_reloc_rule_list_import(struct dsp_reloc_rule_list *list,
	struct dsp_dl_lib_file *file)
{
	int ret;
	char magic[4];
	int idx;

	DL_DEBUG("Rule list import\n");

	dsp_dl_lib_file_reset(file);
	ret = dsp_dl_lib_file_read(magic, sizeof(char) * 4, file);
	if (ret == -1) {
		DL_ERROR("[%s] CHK_ERR\n", __func__);
		return -1;
	}

	if (magic[0] != 'R' || magic[1] != 'U' || magic[2] != 'L' ||
		magic[3] != 'E') {
		DL_ERROR("Magic number is incorrect\n");
		return -1;
	}

	ret = dsp_dl_lib_file_read((char *)&list->cnt, sizeof(int), file);
	if (ret == -1) {
		DL_ERROR("[%s] CHK_ERR\n", __func__);
		return -1;
	}

	if (list->cnt > RULE_MAX) {
		DL_ERROR("Rule list count(%d) is over RULE_MAX(%d)\n",
				list->cnt, RULE_MAX);
		return -1;
	}
	DL_DEBUG("Rule list count %d\n", list->cnt);

	for (idx = 0; idx < list->cnt; idx++) {
		list->list[idx] = (struct dsp_reloc_rule *)dsp_dl_malloc(
				sizeof(*list->list[idx]),
				"Reloc rule");
		dsp_reloc_rule_import(list->list[idx], file);
	}

	return 0;
}

void dsp_reloc_rule_list_free(struct dsp_reloc_rule_list *list)
{
	int idx;

	for (idx = 0; idx < list->cnt; idx++) {
		struct dsp_reloc_rule *rule = list->list[idx];

		dsp_reloc_rule_free(rule);
		dsp_dl_free(rule);
	}
}
