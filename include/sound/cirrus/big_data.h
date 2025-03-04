/*
 * big_data.h  --  Big Data defines for Cirrus Logic CS35L41 codec
 *
 * Copyright 2017 Cirrus Logic
 *
 * Author:	David Rhodes	<david.rhodes@cirrus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* These values are specific to Playback 5.00.5 */

#define CS35L41_BD_MAX_TEMP		0x28003a4
#define CS35L41_BD_MAX_EXC		0x28003a8
#define CS35L41_BD_OVER_TEMP_COUNT	0x28003ac
#define CS35L41_BD_OVER_EXC_COUNT	0x28003b0
#define CS35L41_BD_ABNORMAL_MUTE	0x28003b4

#define CS35L41_BD_TEMP_RADIX		14
#define CS35L41_BD_EXC_RADIX		19

#define CIRRUS_BD_NUM_ATTRS_BASE	1
#define CIRRUS_BD_NUM_ATTRS_AMP		7

void cirrus_bd_store_values(const char *mfd_suffix);
int cirrus_bd_init(void);
void cirrus_bd_exit(void);
