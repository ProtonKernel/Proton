/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fab/unbound_dm_band.h
 *
 * Copyright (c) 2018 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef __UNBOUND_DM_BAND_H__
#define __UNBOUND_DM_BAND_H__

#include "../dynamic_mipi/band_info.h"

struct dm_band_info unbound_freq_range_850[] = {
	DEFINE_FREQ_RANGE(0, 0, 2, 0),
};

struct dm_band_info unbound_freq_range_900[] = {
	DEFINE_FREQ_RANGE(0, 0, 0, 0),
};

struct dm_band_info unbound_freq_range_1800[] = {
	DEFINE_FREQ_RANGE(0, 0, 0, 0),
};

struct dm_band_info unbound_freq_range_1900[] = {
	DEFINE_FREQ_RANGE(0, 0, 0, 0),
};

struct dm_band_info unbound_freq_range_wb01[] = {
	DEFINE_FREQ_RANGE(10562, 10838, 0, 0),
};

struct dm_band_info unbound_freq_range_wb02[] = {
	DEFINE_FREQ_RANGE(9662, 9938, 0, 0),
};

struct dm_band_info unbound_freq_range_wb03[] = {
	DEFINE_FREQ_RANGE(1162, 1404, 0, 0),
	DEFINE_FREQ_RANGE(1405, 1513, 2, 0),
};

struct dm_band_info unbound_freq_range_wb04[] = {
	DEFINE_FREQ_RANGE(1537, 1738, 0, 0),
};

struct dm_band_info unbound_freq_range_wb05[] = {
	DEFINE_FREQ_RANGE(4357, 4381, 2, 0),
	DEFINE_FREQ_RANGE(4382, 4399, 1, 0),
	DEFINE_FREQ_RANGE(4400, 4458, 0, 0),
};

struct dm_band_info unbound_freq_range_wb07[] = {
	DEFINE_FREQ_RANGE(2237, 2563, 0, 0),
};

struct dm_band_info unbound_freq_range_wb08[] = {
	DEFINE_FREQ_RANGE(2937, 3088, 0, 0),
};


struct dm_band_info unbound_freq_range_td1[] = {
	DEFINE_FREQ_RANGE(0, 0, 0, 0),
};

struct dm_band_info unbound_freq_range_td2[] = {
	DEFINE_FREQ_RANGE(0, 0, 2, 0),
};

struct dm_band_info unbound_freq_range_td3[] = {
	DEFINE_FREQ_RANGE(0, 0, 0, 0),
};

struct dm_band_info unbound_freq_range_td4[] = {
	DEFINE_FREQ_RANGE(0, 0, 2, 0),
};

struct dm_band_info unbound_freq_range_td5[] = {
	DEFINE_FREQ_RANGE(0, 0, 0, 0),
};

struct dm_band_info unbound_freq_range_td6[] = {
	DEFINE_FREQ_RANGE(0, 0, 2, 0),
};

struct dm_band_info unbound_freq_range_bc0[] = {
	DEFINE_FREQ_RANGE(0, 0, 2, 0),
};

struct dm_band_info unbound_freq_range_bc1[] = {
	DEFINE_FREQ_RANGE(0, 0, 0, 0),
};

struct dm_band_info unbound_freq_range_bc10[] = {
	DEFINE_FREQ_RANGE(0, 0, 2, 0),
};

struct dm_band_info unbound_freq_range_lb01[] = {
	DEFINE_FREQ_RANGE(0, 599, 0, 0),
};

struct dm_band_info unbound_freq_range_lb02[] = {
	DEFINE_FREQ_RANGE(600, 1199, 0, 0),
};

struct dm_band_info unbound_freq_range_lb03[] = {
	DEFINE_FREQ_RANGE(1200, 1709, 0, 0),
	DEFINE_FREQ_RANGE(1710, 1949, 2, 0),
};

struct dm_band_info unbound_freq_range_lb04[] = {
	DEFINE_FREQ_RANGE(1950, 2399, 0, 0),
};

struct dm_band_info unbound_freq_range_lb05[] = {
	DEFINE_FREQ_RANGE(2400, 2472, 2, 1),
	DEFINE_FREQ_RANGE(2473, 2509, 1, 1),
	DEFINE_FREQ_RANGE(2510, 2545, 0, 1),
	DEFINE_FREQ_RANGE(2546, 2649, 0, 0),
};

struct dm_band_info unbound_freq_range_lb07[] = {
	DEFINE_FREQ_RANGE(2750, 3449, 0, 0),
};

struct dm_band_info unbound_freq_range_lb08[] = {
	DEFINE_FREQ_RANGE(3450, 3799, 0, 0),
};

struct dm_band_info unbound_freq_range_lb12[] = {
	DEFINE_FREQ_RANGE(5010, 5179, 0, 0),
};

struct dm_band_info unbound_freq_range_lb13[] = {
	DEFINE_FREQ_RANGE(5180, 5279, 0, 0),
};

struct dm_band_info unbound_freq_range_lb14[] = {
	DEFINE_FREQ_RANGE(5280, 5379, 0, 0),
};

struct dm_band_info unbound_freq_range_lb17[] = {
	DEFINE_FREQ_RANGE(5730, 5849, 0, 0),
};

struct dm_band_info unbound_freq_range_lb18[] = {
	DEFINE_FREQ_RANGE(5850, 5999, 2, 0),
};

struct dm_band_info unbound_freq_range_lb19[] = {
	DEFINE_FREQ_RANGE(6000, 6012, 2, 0),
	DEFINE_FREQ_RANGE(6013, 6049, 1, 0),
	DEFINE_FREQ_RANGE(6050, 6149, 0, 0),
};

struct dm_band_info unbound_freq_range_lb20[] = {
	DEFINE_FREQ_RANGE(6150, 6449, 0, 0),
};

struct dm_band_info unbound_freq_range_lb21[] = {
	DEFINE_FREQ_RANGE(6450, 6599, 0, 0),
};

struct dm_band_info unbound_freq_range_lb25[] = {
	DEFINE_FREQ_RANGE(8040, 8689, 0, 0),
};

struct dm_band_info unbound_freq_range_lb26[] = {
	DEFINE_FREQ_RANGE(8690, 8862, 2, 1),
	DEFINE_FREQ_RANGE(8863, 8899, 1, 1),
	DEFINE_FREQ_RANGE(8900, 8935, 0, 1),
	DEFINE_FREQ_RANGE(8936, 9039, 0, 0),
};

struct dm_band_info unbound_freq_range_lb28[] = {
	DEFINE_FREQ_RANGE(9210, 9499, 0, 1),
	DEFINE_FREQ_RANGE(9500, 9659, 0, 0),
};

struct dm_band_info unbound_freq_range_lb29[] = {
	DEFINE_FREQ_RANGE(9660, 9769, 0, 0),
};

struct dm_band_info unbound_freq_range_lb30[] = {
	DEFINE_FREQ_RANGE(9770, 9869, 0, 0),
};

struct dm_band_info unbound_freq_range_lb32[] = {
	DEFINE_FREQ_RANGE(9920, 10359, 0, 0),
};

struct dm_band_info unbound_freq_range_lb34[] = {
	DEFINE_FREQ_RANGE(36200, 36349, 0, 0),
};

struct dm_band_info unbound_freq_range_lb38[] = {
	DEFINE_FREQ_RANGE(37750, 37837, 2, 0),
	DEFINE_FREQ_RANGE(37838, 37949, 1, 0),
	DEFINE_FREQ_RANGE(37950, 38249, 0, 0),
};

struct dm_band_info unbound_freq_range_lb39[] = {
	DEFINE_FREQ_RANGE(38250, 38427, 2, 0),
	DEFINE_FREQ_RANGE(38428, 38509, 1, 0),
	DEFINE_FREQ_RANGE(38510, 38649, 0, 0),
};

struct dm_band_info unbound_freq_range_lb40[] = {
	DEFINE_FREQ_RANGE(38650, 39339, 0, 0),
	DEFINE_FREQ_RANGE(39340, 39649, 2, 0),
};

struct dm_band_info unbound_freq_range_lb41[] = {
	DEFINE_FREQ_RANGE(39650, 40089, 0, 0),
	DEFINE_FREQ_RANGE(40090, 40477, 2, 0),
	DEFINE_FREQ_RANGE(40478, 40589, 1, 0),
	DEFINE_FREQ_RANGE(40590, 41589, 0, 0),
};

struct dm_band_info unbound_freq_range_lb42[] = {
	DEFINE_FREQ_RANGE(41590, 41890, 2, 0),
	DEFINE_FREQ_RANGE(41891, 42040, 1, 0),
	DEFINE_FREQ_RANGE(42041, 43250, 0, 0),
	DEFINE_FREQ_RANGE(43251, 43589, 2, 0),
};

struct dm_band_info unbound_freq_range_lb48[] = {
	DEFINE_FREQ_RANGE(55240, 55400, 0, 0),
	DEFINE_FREQ_RANGE(55401, 55742, 2, 0),
	DEFINE_FREQ_RANGE(55743, 55900, 1, 0),
	DEFINE_FREQ_RANGE(55901, 56739, 0, 0),
};

struct dm_band_info unbound_freq_range_lb66[] = {
	DEFINE_FREQ_RANGE(66436, 67315, 0, 0),
	DEFINE_FREQ_RANGE(67316, 67335, 2, 0),
};

struct dm_band_info unbound_freq_range_lb71[] = {
	DEFINE_FREQ_RANGE(68586, 68935, 0, 0),
};

struct dm_band_info unbound_freq_range_n005[] = {
	DEFINE_FREQ_RANGE(173800, 175240, 2, 1),
	DEFINE_FREQ_RANGE(175241, 175980, 1, 1),
	DEFINE_FREQ_RANGE(175981, 176700, 0, 1),
	DEFINE_FREQ_RANGE(176701, 178780, 0, 0),
};

struct dm_band_info unbound_freq_range_n008[] = {
	DEFINE_FREQ_RANGE(185000, 191980, 0, 0),
};

struct dm_band_info unbound_freq_range_n028[] = {
	DEFINE_FREQ_RANGE(151600, 157400, 0, 1),
	DEFINE_FREQ_RANGE(157401, 160580, 0, 0),
};

struct dm_band_info unbound_freq_range_n071[] = {
	DEFINE_FREQ_RANGE(123400, 130380, 0, 0),
};

struct dm_total_band_info unbound_dynamic_freq_set[FREQ_RANGE_MAX] = {
	[FREQ_RANGE_850] = DEFINE_FREQ_SET(unbound_freq_range_850),
	[FREQ_RANGE_900] = DEFINE_FREQ_SET(unbound_freq_range_900),
	[FREQ_RANGE_1800] = DEFINE_FREQ_SET(unbound_freq_range_1800),
	[FREQ_RANGE_1900] = DEFINE_FREQ_SET(unbound_freq_range_1900),
	[FREQ_RANGE_WB01] = DEFINE_FREQ_SET(unbound_freq_range_wb01),
	[FREQ_RANGE_WB02] = DEFINE_FREQ_SET(unbound_freq_range_wb02),
	[FREQ_RANGE_WB03] = DEFINE_FREQ_SET(unbound_freq_range_wb03),
	[FREQ_RANGE_WB04] = DEFINE_FREQ_SET(unbound_freq_range_wb04),
	[FREQ_RANGE_WB05] = DEFINE_FREQ_SET(unbound_freq_range_wb05),
	[FREQ_RANGE_WB07] = DEFINE_FREQ_SET(unbound_freq_range_wb07),
	[FREQ_RANGE_WB08] = DEFINE_FREQ_SET(unbound_freq_range_wb08),
	[FREQ_RANGE_TD1] = DEFINE_FREQ_SET(unbound_freq_range_td1),
	[FREQ_RANGE_TD2] = DEFINE_FREQ_SET(unbound_freq_range_td2),
	[FREQ_RANGE_TD3] = DEFINE_FREQ_SET(unbound_freq_range_td3),
	[FREQ_RANGE_TD4] = DEFINE_FREQ_SET(unbound_freq_range_td4),
	[FREQ_RANGE_TD5] = DEFINE_FREQ_SET(unbound_freq_range_td5),
	[FREQ_RANGE_TD6] = DEFINE_FREQ_SET(unbound_freq_range_td6),
	[FREQ_RANGE_BC0] = DEFINE_FREQ_SET(unbound_freq_range_bc0),
	[FREQ_RANGE_BC1] = DEFINE_FREQ_SET(unbound_freq_range_bc1),
	[FREQ_RANGE_BC10] = DEFINE_FREQ_SET(unbound_freq_range_bc10),
	[FREQ_RANGE_LB01] = DEFINE_FREQ_SET(unbound_freq_range_lb01),
	[FREQ_RANGE_LB02] = DEFINE_FREQ_SET(unbound_freq_range_lb02),
	[FREQ_RANGE_LB03] = DEFINE_FREQ_SET(unbound_freq_range_lb03),
	[FREQ_RANGE_LB04] = DEFINE_FREQ_SET(unbound_freq_range_lb04),
	[FREQ_RANGE_LB05] = DEFINE_FREQ_SET(unbound_freq_range_lb05),
	[FREQ_RANGE_LB07] = DEFINE_FREQ_SET(unbound_freq_range_lb07),
	[FREQ_RANGE_LB08] = DEFINE_FREQ_SET(unbound_freq_range_lb08),
	[FREQ_RANGE_LB12] = DEFINE_FREQ_SET(unbound_freq_range_lb12),
	[FREQ_RANGE_LB13] = DEFINE_FREQ_SET(unbound_freq_range_lb13),
	[FREQ_RANGE_LB14] = DEFINE_FREQ_SET(unbound_freq_range_lb14),
	[FREQ_RANGE_LB17] = DEFINE_FREQ_SET(unbound_freq_range_lb17),
	[FREQ_RANGE_LB18] = DEFINE_FREQ_SET(unbound_freq_range_lb18),
	[FREQ_RANGE_LB19] = DEFINE_FREQ_SET(unbound_freq_range_lb19),
	[FREQ_RANGE_LB20] = DEFINE_FREQ_SET(unbound_freq_range_lb20),
	[FREQ_RANGE_LB21] = DEFINE_FREQ_SET(unbound_freq_range_lb21),
	[FREQ_RANGE_LB25] = DEFINE_FREQ_SET(unbound_freq_range_lb25),
	[FREQ_RANGE_LB26] = DEFINE_FREQ_SET(unbound_freq_range_lb26),
	[FREQ_RANGE_LB28] = DEFINE_FREQ_SET(unbound_freq_range_lb28),
	[FREQ_RANGE_LB29] = DEFINE_FREQ_SET(unbound_freq_range_lb29),
	[FREQ_RANGE_LB30] = DEFINE_FREQ_SET(unbound_freq_range_lb30),
	[FREQ_RANGE_LB32] = DEFINE_FREQ_SET(unbound_freq_range_lb32),
	[FREQ_RANGE_LB34] = DEFINE_FREQ_SET(unbound_freq_range_lb34),
	[FREQ_RANGE_LB38] = DEFINE_FREQ_SET(unbound_freq_range_lb38),
	[FREQ_RANGE_LB39] = DEFINE_FREQ_SET(unbound_freq_range_lb39),
	[FREQ_RANGE_LB40] = DEFINE_FREQ_SET(unbound_freq_range_lb40),
	[FREQ_RANGE_LB41] = DEFINE_FREQ_SET(unbound_freq_range_lb41),
	[FREQ_RANGE_LB42] = DEFINE_FREQ_SET(unbound_freq_range_lb42),
	[FREQ_RANGE_LB48] = DEFINE_FREQ_SET(unbound_freq_range_lb48),
	[FREQ_RANGE_LB66] = DEFINE_FREQ_SET(unbound_freq_range_lb66),
	[FREQ_RANGE_LB71] = DEFINE_FREQ_SET(unbound_freq_range_lb71),
	[FREQ_RANGE_N005] = DEFINE_FREQ_SET(unbound_freq_range_n005),
	[FREQ_RANGE_N008] = DEFINE_FREQ_SET(unbound_freq_range_n008),
	[FREQ_RANGE_N028] = DEFINE_FREQ_SET(unbound_freq_range_n028),
	[FREQ_RANGE_N071] = DEFINE_FREQ_SET(unbound_freq_range_n071),
};

#endif
