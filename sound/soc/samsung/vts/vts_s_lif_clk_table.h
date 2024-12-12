/* sound/soc/samsung/vts/vts_s_lif_clk.h
 *
 * ALSA SoC - Samsung VTS Serial Local Interface driver
 *
 * Copyright (c) 2019 Samsung Electronics Co. Ltd.
  *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_VTS_S_LIF_CLK_TABLE_H
#define __SND_SOC_VTS_S_LIF_CLK_TABLE_H

/* #define CLK_TABLE_INDEX_MAX (7) */

enum vts_s_lif_clk_src {
	CLK_SRC_AUD0,
	CLK_SRC_AUD1
};

enum vts_s_lif_clk_table_id {
	CLK_TABLE_8K_16,
	CLK_TABLE_8K_24,
	CLK_TABLE_16K_16,
	CLK_TABLE_16K_24,
	CLK_TABLE_48K_16,
	CLK_TABLE_48K_24,
	CLK_TABLE_96K_16,
	CLK_TABLE_96K_24,
	CLK_TABLE_ID_END
};

enum vts_s_lif_clk_table_index {
	CLK_TABLE_ID,
	CLK_TABLE_RATE,
	CLK_TABLE_WIDTH,
	CLK_TABLE_SRC,
	CLK_TABLE_DMIC_AUD,
	CLK_TABLE_DMIC_AUD_PAD,
	CLK_TABLE_DMIC_AUD_DIV2,
	CLK_TABLE_SERIAL_LIF,
	CLK_TABLE_SYS_SEL,
	CLK_TABLE_INDEX_END
};

int vts_s_lif_clk_table_get(int clk_table_id, int clk_table_index);

int vts_s_lif_clk_table_id_search(int rate, int width);

#endif
