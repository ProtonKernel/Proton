/* sound/soc/samsung/abox/abox_tplg.h
 *
 * ALSA SoC Audio Layer - Samsung Abox Topology driver
 *
 * Copyright (c) 2018 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __SND_SOC_ABOX_TPLG_H
#define __SND_SOC_ABOX_TPLG_H

#include <linux/device.h>
#include <sound/soc.h>

#define ABOX_TPLG_GID_DEFAULT 0xab0c

enum abox_tplg_tkn {
	ABOX_TKN_NONE = 0,
	ABOX_TKN_ID = 1,
	ABOX_TKN_GID,
	ABOX_TKN_VOLATILE,
	ABOX_TKN_RATE,
	ABOX_TKN_WIDTH,
	ABOX_TKN_CHANNELS,
	ABOX_TKN_PERIOD_SIZE,
	ABOX_TKN_PERIODS,
	ABOX_TKN_PACKED,
	ABOX_TKN_MIN,
	ABOX_TKN_COUNT,
	ABOX_TKN_ADDRESS,
	ABOX_TKN_WEAK,
	ABOX_TKN_SYNCHRONOUS,
	ABOX_BIN_IDX = 0x100,
	ABOX_BIN_NAME = 0x101,
	ABOX_BIN_AREA = 0x102,
	ABOX_BIN_OFFSET = 0x103,
	ABOX_BIN_CHANGEABLE = 0x104,
};

enum abox_tplg_event_types {
	ABOX_EVENT_NONE = 0,
	ABOX_EVENT_MIXER = 1,
	ABOX_EVENT_MUX,
};

/**
 * restore topology objects to firmware
 * @param[in]	dev	pointer to calling device
 * @return		0 or error code
 */
extern int abox_tplg_restore(struct device *dev);

#endif /* __SND_SOC_ABOX_TPLG_H */
