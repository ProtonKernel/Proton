/*
 * power.h - Power-management defines for Cirrus Logic CS35L41 amplifier
 *
 * Copyright 2018 Cirrus Logic
 *
 * Author:	David Rhodes	<david.rhodes@cirrus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define CIRRUS_PWR_CSPL_PASSPORT_ENABLE		0x28003b8
#define CIRRUS_PWR_CSPL_OUTPUT_POWER_SQ		0x28003bc

#define CIRRUS_PWR_NUM_ATTRS_BASE	6
#define CIRRUS_PWR_NUM_ATTRS_AMP	5

void cirrus_pwr_start(const char *mfd_suffix);
void cirrus_pwr_stop(const char *mfd_suffix);
int cirrus_pwr_init(void);
void cirrus_pwr_exit(void);
