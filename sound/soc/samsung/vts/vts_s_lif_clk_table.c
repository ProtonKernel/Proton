/* sound/soc/samsung/vts/vts_s_lif_clk_table.c
 *
 * ALSA SoC - Samsung VTS Serial Local Interface driver
 *
 * Copyright (c) 2019 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* #define DEBUG */

#include <linux/clk.h>

#include "vts_s_lif_clk_table.h"

static int clk_table_normal[CLK_TABLE_ID_END][CLK_TABLE_INDEX_END] = {
/*
	CLK_TABLE_8K_16,	: id
	48000,			: sample rate
	16,			: bit depth(=bit width)
	CLK_SRC_AUD0,		: clk soruce (PLL_AUD0 or PLL_AUD1)
	12288000,		: AUD clk
	3072000,		: AUD PAD clk
	3072000,		: AUD DIV2 clk
	6144000,		: It means 8 channel clk value.
				to change channel and calurate blck,
				you should divide ch num.
				ex> 6ch 16bit : 614400 * 6(ch) / 8(ch) = 460800
	0			: dmic sys sel value
*/
	{
		CLK_TABLE_8K_16,
		8000,
		16,
		CLK_SRC_AUD0,
		2048000,
		2048000,
		2048000,
		1024000,
		2
	},
	{
		CLK_TABLE_8K_24,
		8000,
		24,
		CLK_SRC_AUD0,
		2048000,
		2048000,
		2048000,
		1536000,
		2
	},
	{
		CLK_TABLE_16K_16,
		16000,
		16,
		CLK_SRC_AUD0,
		4096000,
		2048000,
		2048000,
		2048000,
		1
	},
	{
		CLK_TABLE_16K_24,
		16000,
		24,
		CLK_SRC_AUD0,
		4096000,
		2048000,
		2048000,
		3072000,
		1
	},
	{
		CLK_TABLE_48K_16,
		48000,
		16,
		CLK_SRC_AUD0,
		12288000,
		3072000,
		3072000,
		6144000,
		0
	},
	{
		CLK_TABLE_48K_24,
		48000,
		24,
		CLK_SRC_AUD0,
		12288000,
		3072000,
		3072000,
		9216000,
		0
	},
	{
		CLK_TABLE_96K_16,
		96000,
		16,
		CLK_SRC_AUD0,
		24576000,
		3072000,
		3072000,
		12288000,
		5
	},
	{
		CLK_TABLE_96K_24,
		96000,
		24,
		CLK_SRC_AUD0,
		24576000,
		3072000,
		3072000,
		18432000,
		5
	}
};

int vts_s_lif_clk_table_get(int id, int index)
{
	if ((id > CLK_TABLE_ID_END - 1) ||
			(index > CLK_TABLE_INDEX_END - 1))
		return -EINVAL;

	else
		return clk_table_normal[id][index];
}

int vts_s_lif_clk_table_id_search(int rate, int width)
{
	int i = 0;

	for (i = 0; i < CLK_TABLE_ID_END - 1; i++) {
		if ((clk_table_normal[i][CLK_TABLE_RATE] == rate) &&
			(clk_table_normal[i][CLK_TABLE_WIDTH] == width)) {
			pr_info("vts_s_lif id(%d), rate(%d) width(%d)\n",
					i, rate, width);

			return clk_table_normal[i][CLK_TABLE_ID];
		}
	}

	return -1;
}
