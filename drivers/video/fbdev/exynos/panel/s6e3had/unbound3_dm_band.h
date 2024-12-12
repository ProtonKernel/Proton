/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3had/s6e3had_u3_dynamic_freq.h
 *
 * Copyright (c) 2018 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef __S6E3HAD_U3_DYNAMIC_FREQ_TABLE__
#define __S6E3HAD_U3_DYNAMIC_FREQ_TABLE__

#include "../dynamic_mipi/band_info.h"

struct dm_band_info u3_freq_range_850[] = {
	DEFINE_FREQ_RANGE(0, 0, 0, 0),
};

struct dm_band_info u3_freq_range_900[] = {
	DEFINE_FREQ_RANGE(0, 0, 0, 0),
};

struct dm_band_info u3_freq_range_1800[] = {
	DEFINE_FREQ_RANGE(0, 0, 1, 0),
};

struct dm_band_info u3_freq_range_1900[] = {
	DEFINE_FREQ_RANGE(0, 0, 1, 0),
};

struct dm_band_info u3_freq_range_wb01[] = {
	DEFINE_FREQ_RANGE(10562, 10787, 0, 0),
	DEFINE_FREQ_RANGE(10788, 10838, 1, 0),
};

struct dm_band_info u3_freq_range_wb02[] = {
	DEFINE_FREQ_RANGE(9662, 9885, 0, 0),
	DEFINE_FREQ_RANGE(9886, 9938, 1, 0),
};

struct dm_band_info u3_freq_range_wb03[] = {
	DEFINE_FREQ_RANGE(1162, 1178, 1, 0),
	DEFINE_FREQ_RANGE(1179, 1513, 0, 0),
};

struct dm_band_info u3_freq_range_wb04[] = {
	DEFINE_FREQ_RANGE(1537, 1738, 0, 0),
};

struct dm_band_info u3_freq_range_wb05[] = {
	DEFINE_FREQ_RANGE(4357, 4458, 0, 0),
};

struct dm_band_info u3_freq_range_wb07[] = {
	DEFINE_FREQ_RANGE(2237, 2563, 0, 0),
};

struct dm_band_info u3_freq_range_wb08[] = {
	DEFINE_FREQ_RANGE(2937, 3088, 0, 0),
};

struct dm_band_info u3_freq_range_td1[] = {
	DEFINE_FREQ_RANGE(0, 0, 0, 0),
};

struct dm_band_info u3_freq_range_td2[] = {
	DEFINE_FREQ_RANGE(0, 0, 0, 0),
};

struct dm_band_info u3_freq_range_td3[] = {
	DEFINE_FREQ_RANGE(0, 0, 0, 0),
};

struct dm_band_info u3_freq_range_td4[] = {
	DEFINE_FREQ_RANGE(0, 0, 0, 0),
};

struct dm_band_info u3_freq_range_td5[] = {
	DEFINE_FREQ_RANGE(0, 0, 1, 0),
};

struct dm_band_info u3_freq_range_td6[] = {
	DEFINE_FREQ_RANGE(0, 0, 0, 0),
};

struct dm_band_info u3_freq_range_bc0[] = {
	DEFINE_FREQ_RANGE(0, 0, 0, 0),
};

struct dm_band_info u3_freq_range_bc1[] = {
	DEFINE_FREQ_RANGE(0, 0, 1, 0),
};

struct dm_band_info u3_freq_range_bc10[] = {
	DEFINE_FREQ_RANGE(0, 0, 0, 0),
};

struct dm_band_info u3_freq_range_lb01[] = {
	DEFINE_FREQ_RANGE(0, 474, 0, 0),
	DEFINE_FREQ_RANGE(475, 599, 1, 0),
};

struct dm_band_info u3_freq_range_lb02[] = {
	DEFINE_FREQ_RANGE(600, 1071, 0, 0),
	DEFINE_FREQ_RANGE(1072, 1199, 1, 0),
};

struct dm_band_info u3_freq_range_lb03[] = {
	DEFINE_FREQ_RANGE(1200, 1257, 1, 0),
	DEFINE_FREQ_RANGE(1258, 1949, 0, 0),
};

struct dm_band_info u3_freq_range_lb04[] = {
	DEFINE_FREQ_RANGE(1950, 2399, 0, 0),
};


struct dm_band_info u3_freq_range_lb05[] = {
	DEFINE_FREQ_RANGE(2400, 2649, 0, 0),
};

struct dm_band_info u3_freq_range_lb07[] = {
	DEFINE_FREQ_RANGE(2750, 3449, 0, 0),
};

struct dm_band_info u3_freq_range_lb08[] = {
	DEFINE_FREQ_RANGE(3450, 3799, 0, 0),
};

struct dm_band_info u3_freq_range_lb12[] = {
	DEFINE_FREQ_RANGE(5010, 5179, 0, 0),
};

struct dm_band_info u3_freq_range_lb13[] = {
	DEFINE_FREQ_RANGE(5180, 5279, 0, 0),
};

struct dm_band_info u3_freq_range_lb14[] = {
	DEFINE_FREQ_RANGE(5280, 5379, 0, 0),
};

struct dm_band_info u3_freq_range_lb17[] = {
	DEFINE_FREQ_RANGE(5730, 5849, 0, 0),
};

struct dm_band_info u3_freq_range_lb18[] = {
	DEFINE_FREQ_RANGE(5850, 5999, 0, 0),
};

struct dm_band_info u3_freq_range_lb19[] = {
	DEFINE_FREQ_RANGE(6000, 6149, 0, 0),
};

struct dm_band_info u3_freq_range_lb20[] = {
	DEFINE_FREQ_RANGE(6150, 6449, 0, 0),
};

struct dm_band_info u3_freq_range_lb21[] = {
	DEFINE_FREQ_RANGE(6450, 6599, 0, 0),
};

struct dm_band_info u3_freq_range_lb25[] = {
	DEFINE_FREQ_RANGE(8040, 8511, 0, 0),
	DEFINE_FREQ_RANGE(8512, 8651, 1, 0),
	DEFINE_FREQ_RANGE(8652, 8689, 0, 0),
};

struct dm_band_info u3_freq_range_lb26[] = {
	DEFINE_FREQ_RANGE(8690, 9039, 0, 0),
};

struct dm_band_info u3_freq_range_lb28[] = {
	DEFINE_FREQ_RANGE(9210, 9659, 0, 0),
};

struct dm_band_info u3_freq_range_lb29[] = {
	DEFINE_FREQ_RANGE(9660, 9729, 1, 0),
	DEFINE_FREQ_RANGE(9730, 9769, 2, 0),
};

struct dm_band_info u3_freq_range_lb30[] = {
	DEFINE_FREQ_RANGE(9770, 9788, 1, 0),
	DEFINE_FREQ_RANGE(9789, 9869, 0, 0),
};

struct dm_band_info u3_freq_range_lb32[] = {
	DEFINE_FREQ_RANGE(9920, 10359, 0, 0),
};

struct dm_band_info u3_freq_range_lb34[] = {
	DEFINE_FREQ_RANGE(36200, 36349, 0, 0),
};

struct dm_band_info u3_freq_range_lb38[] = {
	DEFINE_FREQ_RANGE(37750, 38249, 0, 0),
};

struct dm_band_info u3_freq_range_lb39[] = {
	DEFINE_FREQ_RANGE(38250, 38649, 0, 0),
};

struct dm_band_info u3_freq_range_lb40[] = {
	DEFINE_FREQ_RANGE(38650, 39028, 0, 0),
	DEFINE_FREQ_RANGE(39029, 39168, 1, 0),
	DEFINE_FREQ_RANGE(39169, 39649, 0, 0),
};

struct dm_band_info u3_freq_range_lb41[] = {
	DEFINE_FREQ_RANGE(39650, 39872, 0, 0),
	DEFINE_FREQ_RANGE(39873, 40012, 1, 0),
	DEFINE_FREQ_RANGE(40013, 41589, 0, 0),
};

struct dm_band_info u3_freq_range_lb42[] = {
	DEFINE_FREQ_RANGE(41590, 41791, 0, 0),
	DEFINE_FREQ_RANGE(41792, 41931, 1, 0),
	DEFINE_FREQ_RANGE(41932, 43589, 0, 0),
};

struct dm_band_info u3_freq_range_lb48[] = {
	DEFINE_FREQ_RANGE(55240, 55745, 0, 0),
	DEFINE_FREQ_RANGE(55746, 55885, 1, 0),
	DEFINE_FREQ_RANGE(55886, 56739, 0, 0),
};

struct dm_band_info u3_freq_range_lb66[] = {
	DEFINE_FREQ_RANGE(66436, 66910, 0, 0),
	DEFINE_FREQ_RANGE(66911, 67050, 1, 0),
	DEFINE_FREQ_RANGE(67051, 67335, 0, 0),
};

struct dm_band_info u3_freq_range_lb71[] = {
	DEFINE_FREQ_RANGE(68586, 68935, 0, 0),
};

struct dm_band_info u3_freq_range_n005[] = {
	DEFINE_FREQ_RANGE(173800, 175360, 0, 0),
	DEFINE_FREQ_RANGE(175361, 177740, 1, 0),
	DEFINE_FREQ_RANGE(177741, 178780, 2, 0),
};

struct dm_band_info u3_freq_range_n008[] = {
	DEFINE_FREQ_RANGE(185000, 191980, 0, 0),
};

struct dm_band_info u3_freq_range_n028[] = {
	DEFINE_FREQ_RANGE(151600, 160580, 0, 0),
};

struct dm_band_info u3_freq_range_n071[] = {
	DEFINE_FREQ_RANGE(123400, 130380, 0, 0),
};

struct dm_total_band_info u3_dynamic_freq_set[FREQ_RANGE_MAX] = {
	[FREQ_RANGE_850] = DEFINE_FREQ_SET(u3_freq_range_850),
	[FREQ_RANGE_900] = DEFINE_FREQ_SET(u3_freq_range_900),
	[FREQ_RANGE_1800] = DEFINE_FREQ_SET(u3_freq_range_1800),
	[FREQ_RANGE_1900] = DEFINE_FREQ_SET(u3_freq_range_1900),
	[FREQ_RANGE_WB01] = DEFINE_FREQ_SET(u3_freq_range_wb01),
	[FREQ_RANGE_WB02] = DEFINE_FREQ_SET(u3_freq_range_wb02),
	[FREQ_RANGE_WB03] = DEFINE_FREQ_SET(u3_freq_range_wb03),
	[FREQ_RANGE_WB04] = DEFINE_FREQ_SET(u3_freq_range_wb04),
	[FREQ_RANGE_WB05] = DEFINE_FREQ_SET(u3_freq_range_wb05),
	[FREQ_RANGE_WB07] = DEFINE_FREQ_SET(u3_freq_range_wb07),
	[FREQ_RANGE_WB08] = DEFINE_FREQ_SET(u3_freq_range_wb08),
	[FREQ_RANGE_TD1] = DEFINE_FREQ_SET(u3_freq_range_td1),
	[FREQ_RANGE_TD2] = DEFINE_FREQ_SET(u3_freq_range_td2),
	[FREQ_RANGE_TD3] = DEFINE_FREQ_SET(u3_freq_range_td3),
	[FREQ_RANGE_TD4] = DEFINE_FREQ_SET(u3_freq_range_td4),
	[FREQ_RANGE_TD5] = DEFINE_FREQ_SET(u3_freq_range_td5),
	[FREQ_RANGE_TD6] = DEFINE_FREQ_SET(u3_freq_range_td6),
	[FREQ_RANGE_BC0] = DEFINE_FREQ_SET(u3_freq_range_bc0),
	[FREQ_RANGE_BC1] = DEFINE_FREQ_SET(u3_freq_range_bc1),
	[FREQ_RANGE_BC10] = DEFINE_FREQ_SET(u3_freq_range_bc10),
	[FREQ_RANGE_LB01] = DEFINE_FREQ_SET(u3_freq_range_lb01),
	[FREQ_RANGE_LB02] = DEFINE_FREQ_SET(u3_freq_range_lb02),
	[FREQ_RANGE_LB03] = DEFINE_FREQ_SET(u3_freq_range_lb03),
	[FREQ_RANGE_LB04] = DEFINE_FREQ_SET(u3_freq_range_lb04),
	[FREQ_RANGE_LB05] = DEFINE_FREQ_SET(u3_freq_range_lb05),
	[FREQ_RANGE_LB07] = DEFINE_FREQ_SET(u3_freq_range_lb07),
	[FREQ_RANGE_LB08] = DEFINE_FREQ_SET(u3_freq_range_lb08),
	[FREQ_RANGE_LB12] = DEFINE_FREQ_SET(u3_freq_range_lb12),
	[FREQ_RANGE_LB13] = DEFINE_FREQ_SET(u3_freq_range_lb13),
	[FREQ_RANGE_LB14] = DEFINE_FREQ_SET(u3_freq_range_lb14),
	[FREQ_RANGE_LB17] = DEFINE_FREQ_SET(u3_freq_range_lb17),
	[FREQ_RANGE_LB18] = DEFINE_FREQ_SET(u3_freq_range_lb18),
	[FREQ_RANGE_LB19] = DEFINE_FREQ_SET(u3_freq_range_lb19),
	[FREQ_RANGE_LB20] = DEFINE_FREQ_SET(u3_freq_range_lb20),
	[FREQ_RANGE_LB21] = DEFINE_FREQ_SET(u3_freq_range_lb21),
	[FREQ_RANGE_LB25] = DEFINE_FREQ_SET(u3_freq_range_lb25),
	[FREQ_RANGE_LB26] = DEFINE_FREQ_SET(u3_freq_range_lb26),
	[FREQ_RANGE_LB28] = DEFINE_FREQ_SET(u3_freq_range_lb28),
	[FREQ_RANGE_LB29] = DEFINE_FREQ_SET(u3_freq_range_lb29),
	[FREQ_RANGE_LB30] = DEFINE_FREQ_SET(u3_freq_range_lb30),
	[FREQ_RANGE_LB32] = DEFINE_FREQ_SET(u3_freq_range_lb32),
	[FREQ_RANGE_LB34] = DEFINE_FREQ_SET(u3_freq_range_lb34),
	[FREQ_RANGE_LB38] = DEFINE_FREQ_SET(u3_freq_range_lb38),
	[FREQ_RANGE_LB39] = DEFINE_FREQ_SET(u3_freq_range_lb39),
	[FREQ_RANGE_LB40] = DEFINE_FREQ_SET(u3_freq_range_lb40),
	[FREQ_RANGE_LB41] = DEFINE_FREQ_SET(u3_freq_range_lb41),
	[FREQ_RANGE_LB42] = DEFINE_FREQ_SET(u3_freq_range_lb42),
	[FREQ_RANGE_LB48] = DEFINE_FREQ_SET(u3_freq_range_lb48),
	[FREQ_RANGE_LB66] = DEFINE_FREQ_SET(u3_freq_range_lb66),
	[FREQ_RANGE_LB71] = DEFINE_FREQ_SET(u3_freq_range_lb71),
	[FREQ_RANGE_N005] = DEFINE_FREQ_SET(u3_freq_range_n005),
	[FREQ_RANGE_N008] = DEFINE_FREQ_SET(u3_freq_range_n008),
	[FREQ_RANGE_N028] = DEFINE_FREQ_SET(u3_freq_range_n028),
	[FREQ_RANGE_N071] = DEFINE_FREQ_SET(u3_freq_range_n071),
};

#endif
