/*
 * linux/drivers/video/fbdev/exynos/panel/ea8082/ea8082_ea8082_a2_s0_panel_hmd_dimming.h
 *
 * Header file for EA8082 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EA8082_R9_A2_S0_PANEL_HMD_DIMMING_H__
#define __EA8082_R9_A2_S0_PANEL_HMD_DIMMING_H__
#include "../dimming.h"
#include "../panel_dimming.h"
#include "ea8082_dimming.h"

#define EA8082_HMD_NR_STEP (256)
#define EA8082_HMD_HBM_STEP (0)
#define EA8082_HMD_TOTAL_STEP (EA8082_HMD_NR_STEP + EA8082_HMD_HBM_STEP)

static unsigned int ea8082_a2_s0_hmd_brt_tbl[EA8082_HMD_TOTAL_STEP] = {
	BRT(0), BRT(1), BRT(2), BRT(3), BRT(4), BRT(5), BRT(6), BRT(7), BRT(8), BRT(9), BRT(10),
	BRT(11), BRT(12), BRT(13), BRT(14), BRT(15), BRT(16), BRT(17), BRT(18), BRT(19), BRT(20),
	BRT(21), BRT(22), BRT(23), BRT(24), BRT(25), BRT(26), BRT(27), BRT(28), BRT(29), BRT(30),
	BRT(31), BRT(32), BRT(33), BRT(34), BRT(35), BRT(36), BRT(37), BRT(38), BRT(39), BRT(40),
	BRT(41), BRT(42), BRT(43), BRT(44), BRT(45), BRT(46), BRT(47), BRT(48), BRT(49), BRT(50),
	BRT(51), BRT(52), BRT(53), BRT(54), BRT(55), BRT(56), BRT(57), BRT(58), BRT(59), BRT(60),
	BRT(61), BRT(62), BRT(63), BRT(64), BRT(65), BRT(66), BRT(67), BRT(68), BRT(69), BRT(70),
	BRT(71), BRT(72), BRT(73), BRT(74), BRT(75), BRT(76), BRT(77), BRT(78), BRT(79), BRT(80),
	BRT(81), BRT(82), BRT(83), BRT(84), BRT(85), BRT(86), BRT(87), BRT(88), BRT(89), BRT(90),
	BRT(91), BRT(92), BRT(93), BRT(94), BRT(95), BRT(96), BRT(97), BRT(98), BRT(99), BRT(100),
	BRT(101), BRT(102), BRT(103), BRT(104), BRT(105), BRT(106), BRT(107), BRT(108), BRT(109), BRT(110),
	BRT(111), BRT(112), BRT(113), BRT(114), BRT(115), BRT(116), BRT(117), BRT(118), BRT(119), BRT(120),
	BRT(121), BRT(122), BRT(123), BRT(124), BRT(125), BRT(126), BRT(127), BRT(128), BRT(129), BRT(130),
	BRT(131), BRT(132), BRT(133), BRT(134), BRT(135), BRT(136), BRT(137), BRT(138), BRT(139), BRT(140),
	BRT(141), BRT(142), BRT(143), BRT(144), BRT(145), BRT(146), BRT(147), BRT(148), BRT(149), BRT(150),
	BRT(151), BRT(152), BRT(153), BRT(154), BRT(155), BRT(156), BRT(157), BRT(158), BRT(159), BRT(160),
	BRT(161), BRT(162), BRT(163), BRT(164), BRT(165), BRT(166), BRT(167), BRT(168), BRT(169), BRT(170),
	BRT(171), BRT(172), BRT(173), BRT(174), BRT(175), BRT(176), BRT(177), BRT(178), BRT(179), BRT(180),
	BRT(181), BRT(182), BRT(183), BRT(184), BRT(185), BRT(186), BRT(187), BRT(188), BRT(189), BRT(190),
	BRT(191), BRT(192), BRT(193), BRT(194), BRT(195), BRT(196), BRT(197), BRT(198), BRT(199), BRT(200),
	BRT(201), BRT(202), BRT(203), BRT(204), BRT(205), BRT(206), BRT(207), BRT(208), BRT(209), BRT(210),
	BRT(211), BRT(212), BRT(213), BRT(214), BRT(215), BRT(216), BRT(217), BRT(218), BRT(219), BRT(220),
	BRT(221), BRT(222), BRT(223), BRT(224), BRT(225), BRT(226), BRT(227), BRT(228), BRT(229), BRT(230),
	BRT(231), BRT(232), BRT(233), BRT(234), BRT(235), BRT(236), BRT(237), BRT(238), BRT(239), BRT(240),
	BRT(241), BRT(242), BRT(243), BRT(244), BRT(245), BRT(246), BRT(247), BRT(248), BRT(249), BRT(250),
	BRT(251), BRT(252), BRT(253), BRT(254), BRT(255),
};

static unsigned int ea8082_a2_s0_hmd_lum_tbl[EA8082_HMD_TOTAL_STEP] = {
	20, 20, 21, 21, 21, 22, 22, 22, 23, 23,
	23, 24, 24, 24, 25, 25, 25, 26, 26, 26,
	27, 27, 27, 28, 28, 28, 29, 29, 29, 30,
	30, 30, 31, 31, 31, 32, 32, 32, 33, 33,
	33, 34, 34, 34, 35, 35, 35, 36, 36, 36,
	37, 37, 37, 38, 38, 38, 39, 39, 39, 40,
	40, 40, 41, 41, 41, 42, 42, 42, 43, 43,
	43, 44, 44, 44, 45, 45, 45, 46, 46, 46,
	47, 47, 47, 48, 48, 48, 49, 49, 49, 50,
	50, 50, 51, 51, 51, 52, 52, 52, 53, 53,
	53, 54, 54, 54, 55, 55, 55, 56, 56, 56,
	57, 57, 57, 58, 58, 58, 59, 59, 59, 60,
	60, 60, 61, 61, 61, 62, 62, 62, 63, 63,
	63, 64, 64, 64, 65, 65, 65, 66, 66, 66,
	67, 67, 67, 68, 68, 68, 69, 69, 69, 70,
	70, 70, 71, 71, 71, 72, 72, 72, 73, 73,
	73, 74, 74, 74, 75, 75, 75, 76, 76, 76,
	77, 77, 77, 78, 78, 78, 79, 79, 79, 80,
	80, 80, 81, 81, 81, 82, 82, 82, 83, 83,
	83, 84, 84, 84, 85, 85, 85, 86, 86, 86,
	87, 87, 87, 88, 88, 88, 89, 89, 89, 90,
	90, 90, 91, 91, 91, 92, 92, 92, 93, 93,
	93, 94, 94, 94, 95, 95, 95, 96, 96, 96,
	97, 97, 97, 98, 98, 98, 99, 99, 99, 100,
	100, 100, 101, 101, 101, 102, 102, 102, 103, 103,
	103, 104, 104, 104, 105, 105,
};

struct gm2_dimming_init_info ea8082_ea8082_a2_s0_gm2_hmd_dimming_init_info[] = {
	{
		.name = "ea8082_ea8082_a2_s0_gm2_hmd_dimming",
	},
};

static unsigned int ea8082_a2_s0_hmd_step_cnt_tbl[EA8082_HMD_TOTAL_STEP] = {
	[0 ... EA8082_HMD_TOTAL_STEP - 1] = 1,
};

static struct brightness_table ea8082_ea8082_a2_s0_panel_hmd_brightness_table = {
	.control_type = BRIGHTNESS_CONTROL_TYPE_GAMMA_MODE2,
	.brt = ea8082_a2_s0_hmd_brt_tbl,
	.sz_brt = ARRAY_SIZE(ea8082_a2_s0_hmd_brt_tbl),
	.sz_ui_brt = EA8082_HMD_NR_STEP,
	.sz_hbm_brt = EA8082_HMD_HBM_STEP,
	.lum = ea8082_a2_s0_hmd_lum_tbl,
	.sz_lum = ARRAY_SIZE(ea8082_a2_s0_hmd_lum_tbl),
	.sz_ui_lum = EA8082_UNBOUND_HMD_NR_LUMINANCE,
	.sz_hbm_lum = 0,
	.sz_ext_hbm_lum = 0,
	.brt_to_step = NULL,
	.sz_brt_to_step = 0,
	.step_cnt = ea8082_a2_s0_hmd_step_cnt_tbl,
	.sz_step_cnt = ARRAY_SIZE(ea8082_a2_s0_hmd_step_cnt_tbl),
};

static struct panel_dimming_info ea8082_ea8082_a2_s0_panel_hmd_dimming_info = {
	.name = "ea8082_ea8082_a2_s0_hmd",
	.dim_init_info = {
		NULL,
	},
	.gm2_dim_init_info = ea8082_ea8082_a2_s0_gm2_hmd_dimming_init_info,
	.nr_gm2_dim_init_info = ARRAY_SIZE(ea8082_ea8082_a2_s0_gm2_hmd_dimming_init_info),
	.target_luminance = EA8082_UNBOUND_HMD_TARGET_LUMINANCE,
	.nr_luminance = EA8082_UNBOUND_HMD_NR_LUMINANCE,
	.hbm_target_luminance = -1,
	.nr_hbm_luminance = 0,
	.extend_hbm_target_luminance = -1,
	.nr_extend_hbm_luminance = 0,
	.brt_tbl = &ea8082_ea8082_a2_s0_panel_hmd_brightness_table,
	/* dimming parameters */
	.dimming_maptbl = NULL,
	.dim_flash_on = false,	/* read dim flash when probe or not */
};
#endif /* __EA8082_R9_A2_S0_PANEL_HMD_DIMMING_H__ */
