/* sound/soc/samsung/abox/abox_udma.h
 *
 * ALSA SoC Audio Layer - Samsung Abox UDMA driver
 *
 * Copyright (c) 2020 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_ABOX_UDMA_H
#define __SND_SOC_ABOX_UDMA_H

#include "abox_dma.h"

extern struct abox_dma_of_data abox_udma_rd_of_data;
extern struct abox_dma_of_data abox_udma_wr_of_data;
extern struct abox_dma_of_data abox_udma_wr_dual_of_data;
extern struct abox_dma_of_data abox_udma_wr_debug_of_data;

/**
 * initialize abox udma module
 * @param[in]	data	abox_data
 * @return	0 or error code
 */
extern int abox_udma_init(struct abox_data *data);

#endif /* __SND_SOC_ABOX_UDMA_H */
