/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fab/s6e3fab_unbound1_a3_s0_panel_dimming.h
 *
 * Header file for S6E3FAB Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FAB_UNBOUND1_A3_S0_PANEL_DIMMING_H___
#define __S6E3FAB_UNBOUND1_A3_S0_PANEL_DIMMING_H___
#include "../dimming.h"
#include "../panel_dimming.h"

/*
 * PANEL INFORMATION
 * LDI : S6E3FAB
 * PANEL : UNBOUND1_A3_S0
 */

#define S6E3FAB_NR_STEP (256)
#define S6E3FAB_HBM_STEP (204)
#define S6E3FAB_TOTAL_STEP (S6E3FAB_NR_STEP + S6E3FAB_HBM_STEP)

static unsigned int unbound1_a3_s0_brt_tbl[S6E3FAB_TOTAL_STEP] = {
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
	BRT(256), BRT(257), BRT(258), BRT(259), BRT(260), BRT(261), BRT(262), BRT(263), BRT(264), BRT(265),
	BRT(266), BRT(267), BRT(268), BRT(269), BRT(270), BRT(271), BRT(272), BRT(273), BRT(274), BRT(275),
	BRT(276), BRT(277), BRT(278), BRT(279), BRT(280), BRT(281), BRT(282), BRT(283), BRT(284), BRT(285),
	BRT(286), BRT(287), BRT(288), BRT(289), BRT(290), BRT(291), BRT(292), BRT(293), BRT(294), BRT(295),
	BRT(296), BRT(297), BRT(298), BRT(299), BRT(300), BRT(301), BRT(302), BRT(303), BRT(304), BRT(305),
	BRT(306), BRT(307), BRT(308), BRT(309), BRT(310), BRT(311), BRT(312), BRT(313), BRT(314), BRT(315),
	BRT(316), BRT(317), BRT(318), BRT(319), BRT(320), BRT(321), BRT(322), BRT(323), BRT(324), BRT(325),
	BRT(326), BRT(327), BRT(328), BRT(329), BRT(330), BRT(331), BRT(332), BRT(333), BRT(334), BRT(335),
	BRT(336), BRT(337), BRT(338), BRT(339), BRT(340), BRT(341), BRT(342), BRT(343), BRT(344), BRT(345),
	BRT(346), BRT(347), BRT(348), BRT(349), BRT(350), BRT(351), BRT(352), BRT(353), BRT(354), BRT(355),
	BRT(356), BRT(357), BRT(358), BRT(359), BRT(360), BRT(361), BRT(362), BRT(363), BRT(364), BRT(365),
	BRT(366), BRT(367), BRT(368), BRT(369), BRT(370), BRT(371), BRT(372), BRT(373), BRT(374), BRT(375),
	BRT(376), BRT(377), BRT(378), BRT(379), BRT(380), BRT(381), BRT(382), BRT(383), BRT(384), BRT(385),
	BRT(386), BRT(387), BRT(388), BRT(389), BRT(390), BRT(391), BRT(392), BRT(393), BRT(394), BRT(395),
	BRT(396), BRT(397), BRT(398), BRT(399), BRT(400), BRT(401), BRT(402), BRT(403), BRT(404), BRT(405),
	BRT(406), BRT(407), BRT(408), BRT(409), BRT(410), BRT(411), BRT(412), BRT(413), BRT(414), BRT(415),
	BRT(416), BRT(417), BRT(418), BRT(419), BRT(420), BRT(421), BRT(422), BRT(423), BRT(424), BRT(425),
	BRT(426), BRT(427), BRT(428), BRT(429), BRT(430), BRT(431), BRT(432), BRT(433), BRT(434), BRT(435),
	BRT(436), BRT(437), BRT(438), BRT(439), BRT(440), BRT(441), BRT(442), BRT(443), BRT(444), BRT(445),
	BRT(446), BRT(447), BRT(448), BRT(449), BRT(450), BRT(451), BRT(452), BRT(453), BRT(454), BRT(455),
	BRT(456), BRT(457), BRT(458), BRT(459),
};

static unsigned int unbound1_a3_s0_lum_tbl[S6E3FAB_TOTAL_NR_LUMINANCE] = {
	/* normal 8x32 */
	2, 2, 3, 3, 4, 5, 5, 6,
	7, 8, 8, 9, 10, 11, 12, 13,
	14, 15, 16, 17, 18, 19, 20, 21,
	22, 23, 24, 25, 26, 27, 28, 29,
	31, 32, 33, 34, 35, 37, 38, 39,
	40, 42, 43, 44, 45, 47, 48, 49,
	51, 52, 53, 55, 56, 57, 59, 60,
	61, 63, 64, 65, 67, 68, 70, 71,
	73, 74, 75, 77, 78, 80, 81, 83,
	84, 86, 87, 89, 90, 92, 93, 95,
	96, 98, 99, 101, 102, 104, 106, 107,
	109, 110, 112, 113, 115, 117, 118, 120,
	122, 123, 125, 126, 128, 130, 131, 133,
	135, 136, 138, 140, 141, 143, 145, 146,
	148, 150, 151, 153, 155, 157, 158, 160,
	162, 163, 165, 167, 169, 170, 172, 174,
	176, 177, 179, 181, 183, 185, 186, 188,
	190, 192, 194, 195, 197, 199, 201, 203,
	204, 206, 208, 210, 212, 214, 215, 217,
	219, 221, 223, 225, 227, 229, 230, 232,
	234, 236, 238, 240, 242, 244, 246, 247,
	249, 251, 253, 255, 257, 259, 261, 263,
	265, 267, 269, 271, 273, 275, 276, 278,
	280, 282, 284, 286, 288, 290, 292, 294,
	296, 298, 300, 302, 304, 306, 308, 310,
	312, 314, 316, 318, 320, 322, 324, 326,
	329, 331, 333, 335, 337, 339, 341, 343,
	345, 346, 350, 354, 358, 362, 366, 370,
	374, 378, 382, 387, 391, 395, 399, 403,
	407, 411, 415, 419, 423, 427, 431, 435,
	439, 443, 447, 451, 455, 459, 464, 468,
	472, 476, 480, 484, 488, 492, 496, 500,
	/* hbm 8x25+4 */
	502, 504, 506, 508, 510, 512, 514, 516,
	518, 520, 522, 524, 525, 527, 529, 531,
	533, 535, 537, 539, 541, 543, 545, 547,
	549, 551, 553, 555, 557, 559, 561, 563,
	565, 567, 569, 571, 573, 574, 576, 578,
	580, 582, 584, 586, 588, 590, 592, 594,
	596, 598, 600, 602, 604, 606, 608, 610,
	612, 614, 616, 618, 620, 622, 623, 625,
	627, 629, 631, 633, 635, 637, 639, 641,
	643, 645, 647, 649, 651, 653, 655, 657,
	659, 661, 663, 665, 667, 669, 670, 672,
	674, 676, 678, 680, 682, 684, 686, 688,
	690, 692, 694, 696, 698, 700, 702, 704,
	706, 708, 710, 712, 714, 716, 718, 720,
	722, 724, 726, 728, 729, 731, 733, 735,
	737, 739, 741, 743, 745, 747, 749, 751,
	753, 755, 757, 759, 761, 763, 765, 767,
	769, 771, 773, 775, 777, 778, 780, 782,
	784, 786, 788, 790, 792, 794, 796, 798,
	800, 802, 804, 806, 808, 810, 812, 814,
	816, 818, 820, 822, 824, 826, 827, 829,
	831, 833, 835, 837, 839, 841, 843, 845,
	847, 849, 851, 853, 855, 857, 859, 861,
	863, 865, 867, 869, 871, 873, 875, 876,
	878, 880, 882, 884, 886, 888, 890, 892,
	894, 896, 898, 900,
};

static unsigned int unbound1_a3_s0_step_cnt_tbl[S6E3FAB_TOTAL_STEP] = {
	[0 ... S6E3FAB_TOTAL_STEP - 1] = 1,
};

enum {
	S6E3FAB_UNBOUND1_GAMMA_BR_0,
	S6E3FAB_UNBOUND1_GAMMA_BR_2,
	S6E3FAB_UNBOUND1_GAMMA_BR_4,
	S6E3FAB_UNBOUND1_GAMMA_BR_5,
	S6E3FAB_UNBOUND1_GAMMA_BR_6,
	S6E3FAB_UNBOUND1_GAMMA_BR_7,
	S6E3FAB_UNBOUND1_GAMMA_BR_8,
	S6E3FAB_UNBOUND1_GAMMA_BR_9,
	S6E3FAB_UNBOUND1_GAMMA_BR_10,
	S6E3FAB_UNBOUND1_GAMMA_BR_11,
	S6E3FAB_UNBOUND1_GAMMA_BR_12,
	S6E3FAB_UNBOUND1_GAMMA_BR_13,
	S6E3FAB_UNBOUND1_GAMMA_BR_14,
	S6E3FAB_UNBOUND1_GAMMA_BR_15,
	S6E3FAB_UNBOUND1_GAMMA_BR_23,
	S6E3FAB_UNBOUND1_GAMMA_BR_87,
	S6E3FAB_UNBOUND1_GAMMA_BR_117,
	S6E3FAB_UNBOUND1_GAMMA_BR_159,
	S6E3FAB_UNBOUND1_GAMMA_BR_208,
	MAX_S6E3FAB_UNBOUND1_GAMMA_BR_INDEX,
};

static int unbound1_a3_s0_vrr_gamma_br_range[MAX_S6E3FAB_UNBOUND1_GAMMA_BR_INDEX] = {
	0, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 23, 87, 117, 159, 208
};

static s32 unbound1_a3_s0_ctbl_dummy[S6E3FAB_NR_TP*3] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static s32 unbound1_a3_s0_ctbl_48hs_br117_0[S6E3FAB_NR_TP*3] = { 0, 0, 0, -38, -45, -36, -17, -26, -18, -6, -12, -7, -4, -5, -3, 1, 0, 1, 0, -1, 0, 3, 2, 4, 2, 1, 2, 6, 4, 5, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_48hs_br117_1[S6E3FAB_NR_TP*3] = { -2, -2, -2, -41, -44, -40, -19, -29, -20, -14, -19, -11, -5, -7, -4, -3, -3, -3, 1, -1, 1, 1, 0, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3 };

static s32 unbound1_a3_s0_ctbl_60hs_br87_1[S6E3FAB_NR_TP*3] = { 0, 0, 0, -33, -36, -29, -20, -26, -18, -10, -15, -8, -3, -6, -3, -2, -2, -1, 0, -2, 0, 0, 1, 0, 1, 1, 1, 2, 2, 2, 2, 2, 2 };
static s32 unbound1_a3_s0_ctbl_60hs_br159_1[S6E3FAB_NR_TP*3] = { 0, 0, 0, -33, -36, -29, -18, -25, -16, -12, -17, -10, -3, -7, -3, -1, -2, -1, 1, -2, 0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 2, 2, 2 };
static s32 unbound1_a3_s0_ctbl_60hs_br208_0[S6E3FAB_NR_TP*3] = { 0, 0, 0, -18, -20, -19, -18, -20, -19, -10, -10, -10, -4, -4, -4, -2, -2, -2, -1, -1, -1, 1, 1, 1, 0, 0, 0, 2, 2, 2, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_60hs_br208_1[S6E3FAB_NR_TP*3] = { -2, -2, -2, -35, -33, -33, -18, -24, -16, -10, -15, -8, -3, -7, -3, -1, -2, -1, 1, -2, 0, 0, 0, 0, 2, 1, 2, 2, 2, 2, 2, 2, 2 };

static s32 unbound1_a3_s0_ctbl_96hs_br0_1[S6E3FAB_NR_TP*3] = { 0, 0, 0, -11, -10, -11, -8, -9, -8, -4, -6, -4, -1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2 };
static s32 unbound1_a3_s0_ctbl_96hs_br0_2[S6E3FAB_NR_TP*3] = { 0, 0, 0, -9, -9, -11, -9, -9, -7, -7, -6, -7, -3, -3, -3, -1, -1, -1, 0, -1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br0_3[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -9, -11, -13, -7, -8, -7, -3, -3, -3, -2, -2, -2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br0_4[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -10, -10, -9, -8, -9, -6, -9, -6, -5, -5, -5, -3, -3, -3, -1, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br0_5[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -10, -10, -10, -10, -10, -10, -9, -9, -9, -6, -6, -6, -4, -4, -4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br0_6[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -8, -8, -8, -8, -8, -9, -7, -6, -7, -6, -6, -6, -2, -2, -2, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br0_7[S6E3FAB_NR_TP*3] = { 0, 0, 0, -20, -13, -16, -20, -13, -16, -14, -12, -14, -14, -12, -13, -12, -13, -12, -11, -11, -11, -7, -7, -7, -5, -5, -5, 0, 0, 0, 0, 0, 0 };

static s32 unbound1_a3_s0_ctbl_96hs_br2_1[S6E3FAB_NR_TP*3] = { 0, 0, 0, -11, -10, -11, -8, -9, -8, -4, -6, -4, -1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2 };
static s32 unbound1_a3_s0_ctbl_96hs_br2_2[S6E3FAB_NR_TP*3] = { 0, 0, 0, -9, -9, -11, -9, -9, -7, -7, -6, -7, -3, -3, -3, -1, -1, -1, 0, -1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br2_3[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -9, -11, -13, -7, -8, -7, -3, -3, -3, -2, -2, -2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br2_4[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -10, -10, -9, -8, -9, -6, -9, -6, -5, -5, -5, -3, -3, -3, -1, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br2_5[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -10, -10, -10, -10, -10, -10, -9, -9, -9, -6, -6, -6, -4, -4, -4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br2_6[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -8, -8, -8, -8, -8, -9, -7, -6, -7, -4, -4, -4, -1, -1, -1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br2_7[S6E3FAB_NR_TP*3] = { 0, 0, 0, -12, -14, -11, -11, -8, -11, -9, -9, -10, -8, -9, -8, -7, -8, -7, -6, -7, -6, -3, -3, -3, 3, 3, 3, 0, 0, 0, 0, 0, 0 };

static s32 unbound1_a3_s0_ctbl_96hs_br4_1[S6E3FAB_NR_TP*3] = { 0, 0, 0, -11, -10, -11, -8, -9, -8, -4, -6, -4, -1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2 };
static s32 unbound1_a3_s0_ctbl_96hs_br4_2[S6E3FAB_NR_TP*3] = { 0, 0, 0, -9, -9, -11, -9, -9, -7, -7, -6, -7, -3, -3, -3, -1, -1, -1, 0, -1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br4_3[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -9, -11, -13, -7, -8, -7, -3, -3, -3, -2, -2, -2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br4_4[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -10, -10, -9, -8, -9, -6, -9, -6, -5, -5, -5, -3, -3, -3, -1, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br4_5[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -10, -10, -10, -10, -10, -10, -9, -9, -9, -6, -6, -6, -4, -4, -4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br4_6[S6E3FAB_NR_TP*3] = { 0, 0, 0, -7, -6, -9, -9, -9, -8, -8, -7, -8, -7, -6, -7, -6, -7, -6, -4, -4, -4, -2, -2, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br4_7[S6E3FAB_NR_TP*3] = { 0, 0, 0, -12, -14, -12, -17, -12, -18, -14, -14, -14, -13, -13, -13, -10, -10, -10, -10, -10, -10, -7, -7, -7, -3, -3, -3, 0, 0, 0, 0, 0, 0 };

static s32 unbound1_a3_s0_ctbl_96hs_br5_1[S6E3FAB_NR_TP*3] = { 0, 0, 0, -11, -10, -11, -8, -9, -8, -4, -6, -4, -1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2 };
static s32 unbound1_a3_s0_ctbl_96hs_br5_2[S6E3FAB_NR_TP*3] = { 0, 0, 0, -9, -9, -11, -9, -9, -7, -7, -6, -7, -3, -3, -3, -1, -1, -1, 0, -1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br5_3[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -9, -11, -13, -7, -8, -7, -3, -3, -3, -2, -2, -2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br5_4[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -10, -10, -9, -8, -9, -6, -9, -6, -5, -5, -5, -3, -3, -3, -1, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br5_5[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -8, -8, -8, -10, -10, -10, -6, -6, -6, -4, -4, -4, -3, -3, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br5_6[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -7, -9, -11, -7, -10, -9, -8, -8, -5, -7, -5, -4, -6, -4, -1, -3, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br5_7[S6E3FAB_NR_TP*3] = { 0, 0, 0, -12, -14, -12, -10, -10, -10, -10, -10, -10, -9, -9, -9, -8, -8, -8, -7, -7, -7, -2, -2, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static s32 unbound1_a3_s0_ctbl_96hs_br6_1[S6E3FAB_NR_TP*3] = { 0, 0, 0, -11, -10, -11, -8, -9, -8, -4, -6, -4, -1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2 };
static s32 unbound1_a3_s0_ctbl_96hs_br6_2[S6E3FAB_NR_TP*3] = { 0, 0, 0, -9, -9, -11, -9, -9, -7, -7, -6, -7, -3, -3, -3, -1, -1, -1, 0, -1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br6_3[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -9, -11, -13, -7, -8, -7, -3, -3, -3, -2, -2, -2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br6_4[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -10, -10, -9, -8, -9, -6, -9, -6, -5, -5, -5, -3, -3, -3, -1, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br6_5[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -8, -8, -8, -10, -10, -10, -6, -6, -6, -4, -4, -4, -3, -3, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br6_6[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -10, -7, -9, -9, -7, -8, -4, -7, -4, -4, -7, -4, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br6_7[S6E3FAB_NR_TP*3] = { 0, 0, 0, -12, -14, -12, -10, -10, -10, -10, -10, -10, -9, -9, -9, -8, -8, -8, -7, -7, -7, -2, -2, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static s32 unbound1_a3_s0_ctbl_96hs_br7_1[S6E3FAB_NR_TP*3] = { 0, 0, 0, -11, -10, -11, -8, -9, -8, -4, -6, -4, -1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2 };
static s32 unbound1_a3_s0_ctbl_96hs_br7_2[S6E3FAB_NR_TP*3] = { 0, 0, 0, -9, -9, -11, -9, -9, -7, -7, -6, -7, -3, -3, -3, -1, -1, -1, 0, -1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br7_3[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -9, -11, -13, -7, -8, -7, -3, -3, -3, -2, -2, -2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br7_4[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -10, -10, -9, -8, -9, -6, -9, -6, -5, -5, -5, -3, -3, -3, -1, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br7_5[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -8, -8, -8, -10, -10, -10, -6, -6, -6, -4, -4, -4, -3, -3, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br7_6[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -11, -8, -10, -7, -7, -8, -7, -9, -7, -6, -8, -6, -2, -2, -2, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br7_7[S6E3FAB_NR_TP*3] = { 0, 0, 0, -12, -14, -12, -10, -10, -10, -10, -10, -10, -9, -9, -9, -8, -8, -8, -7, -7, -7, -2, -2, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static s32 unbound1_a3_s0_ctbl_96hs_br8_1[S6E3FAB_NR_TP*3] = { 0, 0, 0, -11, -10, -11, -8, -9, -8, -4, -6, -4, -1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2 };
static s32 unbound1_a3_s0_ctbl_96hs_br8_2[S6E3FAB_NR_TP*3] = { 0, 0, 0, -9, -9, -11, -9, -9, -7, -7, -6, -7, -3, -3, -3, -1, -1, -1, 0, -1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br8_3[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -9, -11, -13, -7, -8, -7, -3, -3, -3, -2, -2, -2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br8_4[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -10, -10, -9, -8, -9, -6, -9, -6, -5, -5, -5, -3, -3, -3, -1, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br8_5[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -9, -7, -10, -10, -10, -10, -6, -6, -6, -4, -4, -4, -3, -3, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br8_6[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -9, -7, -8, -6, -6, -7, -3, -4, -3, -3, -6, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br8_7[S6E3FAB_NR_TP*3] = { 0, 0, 0, -12, -14, -12, -10, -10, -10, -10, -10, -10, -9, -9, -9, -8, -8, -8, -7, -7, -7, -2, -2, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static s32 unbound1_a3_s0_ctbl_96hs_br9_1[S6E3FAB_NR_TP*3] = { 0, 0, 0, -11, -10, -11, -8, -9, -8, -4, -6, -4, -1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2 };
static s32 unbound1_a3_s0_ctbl_96hs_br9_2[S6E3FAB_NR_TP*3] = { 0, 0, 0, -9, -9, -11, -9, -9, -7, -7, -6, -7, -3, -3, -3, -1, -1, -1, 0, -1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br9_3[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -9, -11, -13, -7, -8, -7, -3, -3, -3, -2, -2, -2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br9_4[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -10, -10, -9, -8, -9, -6, -9, -6, -5, -5, -5, -3, -3, -3, -1, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br9_5[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -9, -7, -8, -10, -10, -10, -6, -6, -6, -4, -4, -4, -3, -3, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br9_6[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -11, -8, -10, -7, -7, -8, -7, -9, -7, -6, -8, -6, -2, -2, -2, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br9_7[S6E3FAB_NR_TP*3] = { 0, 0, 0, -12, -14, -12, -10, -10, -10, -10, -10, -10, -9, -9, -9, -8, -8, -8, -7, -7, -7, -2, -2, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static s32 unbound1_a3_s0_ctbl_96hs_br10_1[S6E3FAB_NR_TP*3] = { 0, 0, 0, -11, -10, -11, -8, -9, -8, -4, -6, -4, -1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2 };
static s32 unbound1_a3_s0_ctbl_96hs_br10_2[S6E3FAB_NR_TP*3] = { 0, 0, 0, -9, -9, -11, -9, -9, -7, -7, -6, -7, -3, -3, -3, -1, -1, -1, 0, -1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br10_3[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -9, -11, -13, -7, -8, -7, -3, -3, -3, -2, -2, -2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br10_4[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -10, -10, -9, -8, -9, -6, -9, -6, -5, -5, -5, -3, -3, -3, -1, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br10_5[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -12, -8, -10, -8, -9, -9, -6, -7, -6, -5, -6, -5, -3, -3, -3, -2, -2, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br10_6[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -13, -13, -13, -10, -11, -11, -9, -11, -9, -6, -7, -6, -4, -5, -4, -4, -5, -4, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br10_7[S6E3FAB_NR_TP*3] = { 0, 0, 0, -12, -14, -12, -13, -15, -13, -13, -13, -14, -14, -14, -14, -5, -6, -5, -11, -14, -12, -5, -7, -6, -6, -6, -6, 0, 0, 0, 0, 0, 0 };

static s32 unbound1_a3_s0_ctbl_96hs_br11_1[S6E3FAB_NR_TP*3] = { 0, 0, 0, -11, -10, -11, -8, -9, -8, -4, -6, -4, -1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2 };
static s32 unbound1_a3_s0_ctbl_96hs_br11_2[S6E3FAB_NR_TP*3] = { 0, 0, 0, -9, -9, -11, -9, -9, -7, -7, -6, -7, -3, -3, -3, -1, -1, -1, 0, -1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br11_3[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -9, -11, -13, -7, -8, -7, -3, -3, -3, -2, -2, -2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br11_4[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -10, -10, -9, -8, -9, -6, -9, -6, -5, -5, -5, -3, -3, -3, -1, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br11_5[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -12, -8, -11, -8, -9, -9, -6, -7, -6, -5, -6, -5, -3, -3, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br11_6[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -13, -13, -13, -10, -11, -11, -9, -11, -9, -6, -7, -6, -4, -5, -4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br11_7[S6E3FAB_NR_TP*3] = { 0, 0, 0, -12, -14, -12, -13, -15, -13, -13, -13, -14, -14, -14, -14, -5, -6, -5, -11, -14, -12, -5, -7, -6, -6, -6, -6, 0, 0, 0, 0, 0, 0 };

static s32 unbound1_a3_s0_ctbl_96hs_br12_1[S6E3FAB_NR_TP*3] = { 0, 0, 0, -11, -10, -11, -8, -9, -8, -4, -6, -4, -1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2 };
static s32 unbound1_a3_s0_ctbl_96hs_br12_2[S6E3FAB_NR_TP*3] = { 0, 0, 0, -9, -9, -11, -9, -9, -7, -7, -6, -7, -3, -3, -3, -1, -1, -1, 0, -1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br12_3[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -9, -11, -13, -7, -8, -7, -3, -3, -3, -2, -2, -2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br12_4[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -10, -10, -9, -8, -9, -6, -9, -6, -5, -5, -5, -3, -3, -3, -1, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br12_5[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -12, -8, -10, -8, -9, -9, -6, -7, -6, -5, -6, -5, -3, -3, -3, -2, -2, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br12_6[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -13, -13, -13, -10, -11, -11, -9, -11, -9, -6, -7, -6, -4, -5, -4, -4, -5, -4, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br12_7[S6E3FAB_NR_TP*3] = { 0, 0, 0, -12, -14, -12, -13, -15, -13, -13, -13, -14, -14, -14, -14, -5, -6, -5, -11, -14, -12, -5, -7, -6, -6, -6, -6, 0, 0, 0, 0, 0, 0 };

static s32 unbound1_a3_s0_ctbl_96hs_br13_1[S6E3FAB_NR_TP*3] = { 0, 0, 0, -11, -10, -11, -5, -7, -5, -3, -4, -3, -1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2 };
static s32 unbound1_a3_s0_ctbl_96hs_br13_2[S6E3FAB_NR_TP*3] = { 0, 0, 0, -9, -10, -11, -6, -7, -5, -3, -5, -3, -1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br13_3[S6E3FAB_NR_TP*3] = { 0, 0, 0, -8, -12, -10, -8, -8, -8, -5, -7, -5, -3, -3, -3, -2, -2, -2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br13_4[S6E3FAB_NR_TP*3] = { 0, 0, 0, -9, -8, -10, -6, -8, -8, -5, -7, -6, -3, -3, -3, -2, -2, -2, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br13_5[S6E3FAB_NR_TP*3] = { 0, 0, 0, -11, -9, -11, -11, -8, -11, -8, -9, -9, -7, -7, -7, -4, -5, -4, -3, -3, -3, -2, -2, -2, -2, -2, -2, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br13_6[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -13, -10, -13, -10, -10, -11, -9, -9, -9, -1, -2, -1, -3, -4, -3, 0, 0, 0, 5, 5, 5, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br13_7[S6E3FAB_NR_TP*3] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static s32 unbound1_a3_s0_ctbl_96hs_br14_1[S6E3FAB_NR_TP*3] = { 0, 0, 0, -11, -10, -11, -5, -7, -5, -3, -4, -3, -1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2 };
static s32 unbound1_a3_s0_ctbl_96hs_br14_2[S6E3FAB_NR_TP*3] = { 0, 0, 0, -9, -10, -11, -6, -7, -5, -3, -5, -3, -1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br14_3[S6E3FAB_NR_TP*3] = { 0, 0, 0, -8, -12, -10, -8, -8, -8, -5, -7, -5, -3, -3, -3, -2, -2, -2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br14_4[S6E3FAB_NR_TP*3] = { 0, 0, 0, -9, -8, -10, -7, -7, -7, -5, -9, -6, -3, -3, -3, -2, -2, -2, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br14_5[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -7, -9, -10, -7, -8, -6, -7, -6, -5, -5, -4, -2, -3, -2, -1, -1, -1, 1, 1, 1, 4, 4, 4, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br14_6[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -13, -10, -13, -10, -10, -11, -9, -9, -9, -5, -6, -5, -3, -4, -3, 0, 0, 0, 5, 5, 5, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br14_7[S6E3FAB_NR_TP*3] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static s32 unbound1_a3_s0_ctbl_96hs_br15_1[S6E3FAB_NR_TP*3] = { 0, 0, 0, -11, -10, -11, -5, -7, -5, -3, -4, -3, -1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2  };
static s32 unbound1_a3_s0_ctbl_96hs_br15_2[S6E3FAB_NR_TP*3] = { 0, 0, 0, -9, -10, -11, -6, -7, -5, -3, -5, -3, -1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br15_3[S6E3FAB_NR_TP*3] = { 0, 0, 0, -8, -12, -10, -8, -8, -8, -5, -7, -5, -3, -3, -3, -2, -2, -2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br15_4[S6E3FAB_NR_TP*3] = { 0, 0, 0, -9, -8, -10, -6, -8, -8, -5, -7, -6, -3, -3, -3, -2, -2, -2, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br15_5[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -11, -11, -13, -10, -12, -9, -10, -9, -9, -9, -9, -5, -5, -5, -3, -3, -3, -2, -2, -2, -2, -2, -2, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br15_6[S6E3FAB_NR_TP*3] = { 0, 0, 0, -10, -9, -10, -13, -10, -13, -10, -10, -11, -9, -9, -9, -1, -2, -1, -3, -4, -3, 0, 0, 0, 5, 5, 5, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br15_7[S6E3FAB_NR_TP*3] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static s32 unbound1_a3_s0_ctbl_96hs_br23_0[S6E3FAB_NR_TP*3] = { -7, -7, -7, -10, -10, -8, -4, -7, -5, -3, -3, -3, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br23_1[S6E3FAB_NR_TP*3] = { -3, -3, -3, -11, -10, -11, -5, -7, -5, -3, -5, -3, -1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2 };
static s32 unbound1_a3_s0_ctbl_96hs_br23_2[S6E3FAB_NR_TP*3] = { 0, 0, 0, -9, -10, -11, -6, -7, -5, -3, -5, -3, -1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br23_3[S6E3FAB_NR_TP*3] = { 0, 0, 0, -8, -12, -10, -8, -8, -8, -5, -7, -5, -3, -3, -3, -2, -2, -2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br23_4[S6E3FAB_NR_TP*3] = { 0, 0, 0, -9, -8, -10, -8, -8, -6, -4, -8, -5, -4, -4, -4, -2, -2, -2, 1, 1, 1, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br23_5[S6E3FAB_NR_TP*3] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br23_6[S6E3FAB_NR_TP*3] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static s32 unbound1_a3_s0_ctbl_96hs_br23_7[S6E3FAB_NR_TP*3] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static struct gm2_dimming_lut unbound1_a3_s0_dimming_lut_120hs[MAX_S6E3FAB_UNBOUND1_GAMMA_BR_INDEX][MAX_S6E3FAB_FQ_SET] = {
	[S6E3FAB_UNBOUND1_GAMMA_BR_0 ... MAX_S6E3FAB_UNBOUND1_GAMMA_BR_INDEX - 1] = {
		[S6E3FAB_FQ_HIGH_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_0")),
		[S6E3FAB_FQ_HIGH_1] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_1")),
		[S6E3FAB_FQ_HIGH_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_2")),
		[S6E3FAB_FQ_HIGH_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_3")),
		[S6E3FAB_FQ_HIGH_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_4")),
		[S6E3FAB_FQ_HIGH_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_5")),
		[S6E3FAB_FQ_HIGH_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_6")),
		[S6E3FAB_FQ_HIGH_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_7")),
		[S6E3FAB_FQ_LOW_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_0")),
		[S6E3FAB_FQ_LOW_1] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_1")),
		[S6E3FAB_FQ_LOW_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_2")),
		[S6E3FAB_FQ_LOW_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_3")),
		[S6E3FAB_FQ_LOW_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_4")),
		[S6E3FAB_FQ_LOW_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_5")),
		[S6E3FAB_FQ_LOW_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_6")),
		[S6E3FAB_FQ_LOW_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_7")),
	},
};

static struct gm2_dimming_lut unbound1_a3_s0_dimming_lut_96hs[MAX_S6E3FAB_UNBOUND1_GAMMA_BR_INDEX][MAX_S6E3FAB_FQ_SET] = {
	[S6E3FAB_UNBOUND1_GAMMA_BR_0] = {
		[S6E3FAB_FQ_HIGH_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_0")),
		[S6E3FAB_FQ_HIGH_1] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br0_1, RESNAME("gamma_mtp_120hs_1")),
		[S6E3FAB_FQ_HIGH_2] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br0_2, RESNAME("gamma_mtp_120hs_2")),
		[S6E3FAB_FQ_HIGH_3] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br0_3, RESNAME("gamma_mtp_120hs_3")),
		[S6E3FAB_FQ_HIGH_4] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br0_4, RESNAME("gamma_mtp_120hs_4")),
		[S6E3FAB_FQ_HIGH_5] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br0_5, RESNAME("gamma_mtp_120hs_5")),
		[S6E3FAB_FQ_HIGH_6] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br0_6, RESNAME("gamma_mtp_120hs_6")),
		[S6E3FAB_FQ_HIGH_7] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br0_7, RESNAME("gamma_mtp_120hs_7")),
		[S6E3FAB_FQ_LOW_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_0")),
		[S6E3FAB_FQ_LOW_1] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_1")),
		[S6E3FAB_FQ_LOW_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_2")),
		[S6E3FAB_FQ_LOW_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_3")),
		[S6E3FAB_FQ_LOW_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_4")),
		[S6E3FAB_FQ_LOW_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_5")),
		[S6E3FAB_FQ_LOW_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_6")),
		[S6E3FAB_FQ_LOW_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_7")),
	},
	[S6E3FAB_UNBOUND1_GAMMA_BR_2] = {
		[S6E3FAB_FQ_HIGH_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_0")),
		[S6E3FAB_FQ_HIGH_1] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br2_1, RESNAME("gamma_mtp_120hs_1")),
		[S6E3FAB_FQ_HIGH_2] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br2_2, RESNAME("gamma_mtp_120hs_2")),
		[S6E3FAB_FQ_HIGH_3] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br2_3, RESNAME("gamma_mtp_120hs_3")),
		[S6E3FAB_FQ_HIGH_4] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br2_4, RESNAME("gamma_mtp_120hs_4")),
		[S6E3FAB_FQ_HIGH_5] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br2_5, RESNAME("gamma_mtp_120hs_5")),
		[S6E3FAB_FQ_HIGH_6] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br2_6, RESNAME("gamma_mtp_120hs_6")),
		[S6E3FAB_FQ_HIGH_7] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br2_7, RESNAME("gamma_mtp_120hs_7")),
		[S6E3FAB_FQ_LOW_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_0")),
		[S6E3FAB_FQ_LOW_1] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_1")),
		[S6E3FAB_FQ_LOW_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_2")),
		[S6E3FAB_FQ_LOW_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_3")),
		[S6E3FAB_FQ_LOW_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_4")),
		[S6E3FAB_FQ_LOW_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_5")),
		[S6E3FAB_FQ_LOW_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_6")),
		[S6E3FAB_FQ_LOW_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_7")),
	},
	[S6E3FAB_UNBOUND1_GAMMA_BR_4] = {
		[S6E3FAB_FQ_HIGH_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_0")),
		[S6E3FAB_FQ_HIGH_1] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br4_1, RESNAME("gamma_mtp_120hs_1")),
		[S6E3FAB_FQ_HIGH_2] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br4_2, RESNAME("gamma_mtp_120hs_2")),
		[S6E3FAB_FQ_HIGH_3] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br4_3, RESNAME("gamma_mtp_120hs_3")),
		[S6E3FAB_FQ_HIGH_4] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br4_4, RESNAME("gamma_mtp_120hs_4")),
		[S6E3FAB_FQ_HIGH_5] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br4_5, RESNAME("gamma_mtp_120hs_5")),
		[S6E3FAB_FQ_HIGH_6] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br4_6, RESNAME("gamma_mtp_120hs_6")),
		[S6E3FAB_FQ_HIGH_7] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br4_7, RESNAME("gamma_mtp_120hs_7")),
		[S6E3FAB_FQ_LOW_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_0")),
		[S6E3FAB_FQ_LOW_1] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_1")),
		[S6E3FAB_FQ_LOW_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_2")),
		[S6E3FAB_FQ_LOW_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_3")),
		[S6E3FAB_FQ_LOW_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_4")),
		[S6E3FAB_FQ_LOW_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_5")),
		[S6E3FAB_FQ_LOW_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_6")),
		[S6E3FAB_FQ_LOW_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_7")),
	},
	[S6E3FAB_UNBOUND1_GAMMA_BR_5] = {
		[S6E3FAB_FQ_HIGH_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_0")),
		[S6E3FAB_FQ_HIGH_1] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br5_1, RESNAME("gamma_mtp_120hs_1")),
		[S6E3FAB_FQ_HIGH_2] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br5_2, RESNAME("gamma_mtp_120hs_2")),
		[S6E3FAB_FQ_HIGH_3] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br5_3, RESNAME("gamma_mtp_120hs_3")),
		[S6E3FAB_FQ_HIGH_4] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br5_4, RESNAME("gamma_mtp_120hs_4")),
		[S6E3FAB_FQ_HIGH_5] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br5_5, RESNAME("gamma_mtp_120hs_5")),
		[S6E3FAB_FQ_HIGH_6] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br5_6, RESNAME("gamma_mtp_120hs_6")),
		[S6E3FAB_FQ_HIGH_7] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br5_7, RESNAME("gamma_mtp_120hs_7")),
		[S6E3FAB_FQ_LOW_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_0")),
		[S6E3FAB_FQ_LOW_1] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_1")),
		[S6E3FAB_FQ_LOW_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_2")),
		[S6E3FAB_FQ_LOW_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_3")),
		[S6E3FAB_FQ_LOW_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_4")),
		[S6E3FAB_FQ_LOW_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_5")),
		[S6E3FAB_FQ_LOW_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_6")),
		[S6E3FAB_FQ_LOW_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_7")),
	},
	[S6E3FAB_UNBOUND1_GAMMA_BR_6] = {
		[S6E3FAB_FQ_HIGH_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_0")),
		[S6E3FAB_FQ_HIGH_1] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br6_1, RESNAME("gamma_mtp_120hs_1")),
		[S6E3FAB_FQ_HIGH_2] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br6_2, RESNAME("gamma_mtp_120hs_2")),
		[S6E3FAB_FQ_HIGH_3] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br6_3, RESNAME("gamma_mtp_120hs_3")),
		[S6E3FAB_FQ_HIGH_4] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br6_4, RESNAME("gamma_mtp_120hs_4")),
		[S6E3FAB_FQ_HIGH_5] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br6_5, RESNAME("gamma_mtp_120hs_5")),
		[S6E3FAB_FQ_HIGH_6] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br6_6, RESNAME("gamma_mtp_120hs_6")),
		[S6E3FAB_FQ_HIGH_7] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br6_7, RESNAME("gamma_mtp_120hs_7")),
		[S6E3FAB_FQ_LOW_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_0")),
		[S6E3FAB_FQ_LOW_1] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_1")),
		[S6E3FAB_FQ_LOW_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_2")),
		[S6E3FAB_FQ_LOW_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_3")),
		[S6E3FAB_FQ_LOW_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_4")),
		[S6E3FAB_FQ_LOW_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_5")),
		[S6E3FAB_FQ_LOW_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_6")),
		[S6E3FAB_FQ_LOW_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_7")),
	},
	[S6E3FAB_UNBOUND1_GAMMA_BR_7] = {
		[S6E3FAB_FQ_HIGH_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_0")),
		[S6E3FAB_FQ_HIGH_1] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br7_1, RESNAME("gamma_mtp_120hs_1")),
		[S6E3FAB_FQ_HIGH_2] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br7_2, RESNAME("gamma_mtp_120hs_2")),
		[S6E3FAB_FQ_HIGH_3] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br7_3, RESNAME("gamma_mtp_120hs_3")),
		[S6E3FAB_FQ_HIGH_4] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br7_4, RESNAME("gamma_mtp_120hs_4")),
		[S6E3FAB_FQ_HIGH_5] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br7_5, RESNAME("gamma_mtp_120hs_5")),
		[S6E3FAB_FQ_HIGH_6] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br7_6, RESNAME("gamma_mtp_120hs_6")),
		[S6E3FAB_FQ_HIGH_7] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br7_7, RESNAME("gamma_mtp_120hs_7")),
		[S6E3FAB_FQ_LOW_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_0")),
		[S6E3FAB_FQ_LOW_1] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_1")),
		[S6E3FAB_FQ_LOW_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_2")),
		[S6E3FAB_FQ_LOW_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_3")),
		[S6E3FAB_FQ_LOW_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_4")),
		[S6E3FAB_FQ_LOW_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_5")),
		[S6E3FAB_FQ_LOW_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_6")),
		[S6E3FAB_FQ_LOW_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_7")),
	},
	[S6E3FAB_UNBOUND1_GAMMA_BR_8] = {
		[S6E3FAB_FQ_HIGH_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_0")),
		[S6E3FAB_FQ_HIGH_1] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br8_1, RESNAME("gamma_mtp_120hs_1")),
		[S6E3FAB_FQ_HIGH_2] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br8_2, RESNAME("gamma_mtp_120hs_2")),
		[S6E3FAB_FQ_HIGH_3] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br8_3, RESNAME("gamma_mtp_120hs_3")),
		[S6E3FAB_FQ_HIGH_4] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br8_4, RESNAME("gamma_mtp_120hs_4")),
		[S6E3FAB_FQ_HIGH_5] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br8_5, RESNAME("gamma_mtp_120hs_5")),
		[S6E3FAB_FQ_HIGH_6] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br8_6, RESNAME("gamma_mtp_120hs_6")),
		[S6E3FAB_FQ_HIGH_7] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br8_7, RESNAME("gamma_mtp_120hs_7")),
		[S6E3FAB_FQ_LOW_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_0")),
		[S6E3FAB_FQ_LOW_1] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_1")),
		[S6E3FAB_FQ_LOW_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_2")),
		[S6E3FAB_FQ_LOW_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_3")),
		[S6E3FAB_FQ_LOW_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_4")),
		[S6E3FAB_FQ_LOW_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_5")),
		[S6E3FAB_FQ_LOW_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_6")),
		[S6E3FAB_FQ_LOW_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_7")),
	},
	[S6E3FAB_UNBOUND1_GAMMA_BR_9] = {
		[S6E3FAB_FQ_HIGH_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_0")),
		[S6E3FAB_FQ_HIGH_1] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br9_1, RESNAME("gamma_mtp_120hs_1")),
		[S6E3FAB_FQ_HIGH_2] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br9_2, RESNAME("gamma_mtp_120hs_2")),
		[S6E3FAB_FQ_HIGH_3] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br9_3, RESNAME("gamma_mtp_120hs_3")),
		[S6E3FAB_FQ_HIGH_4] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br9_4, RESNAME("gamma_mtp_120hs_4")),
		[S6E3FAB_FQ_HIGH_5] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br9_5, RESNAME("gamma_mtp_120hs_5")),
		[S6E3FAB_FQ_HIGH_6] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br9_6, RESNAME("gamma_mtp_120hs_6")),
		[S6E3FAB_FQ_HIGH_7] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br9_7, RESNAME("gamma_mtp_120hs_7")),
		[S6E3FAB_FQ_LOW_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_0")),
		[S6E3FAB_FQ_LOW_1] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_1")),
		[S6E3FAB_FQ_LOW_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_2")),
		[S6E3FAB_FQ_LOW_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_3")),
		[S6E3FAB_FQ_LOW_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_4")),
		[S6E3FAB_FQ_LOW_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_5")),
		[S6E3FAB_FQ_LOW_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_6")),
		[S6E3FAB_FQ_LOW_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_7")),
	},
	[S6E3FAB_UNBOUND1_GAMMA_BR_10] = {
		[S6E3FAB_FQ_HIGH_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_0")),
		[S6E3FAB_FQ_HIGH_1] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br10_1, RESNAME("gamma_mtp_120hs_1")),
		[S6E3FAB_FQ_HIGH_2] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br10_2, RESNAME("gamma_mtp_120hs_2")),
		[S6E3FAB_FQ_HIGH_3] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br10_3, RESNAME("gamma_mtp_120hs_3")),
		[S6E3FAB_FQ_HIGH_4] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br10_4, RESNAME("gamma_mtp_120hs_4")),
		[S6E3FAB_FQ_HIGH_5] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br10_5, RESNAME("gamma_mtp_120hs_5")),
		[S6E3FAB_FQ_HIGH_6] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br10_6, RESNAME("gamma_mtp_120hs_6")),
		[S6E3FAB_FQ_HIGH_7] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br10_7, RESNAME("gamma_mtp_120hs_7")),
		[S6E3FAB_FQ_LOW_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_0")),
		[S6E3FAB_FQ_LOW_1] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_1")),
		[S6E3FAB_FQ_LOW_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_2")),
		[S6E3FAB_FQ_LOW_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_3")),
		[S6E3FAB_FQ_LOW_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_4")),
		[S6E3FAB_FQ_LOW_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_5")),
		[S6E3FAB_FQ_LOW_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_6")),
		[S6E3FAB_FQ_LOW_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_7")),
	},
	[S6E3FAB_UNBOUND1_GAMMA_BR_11] = {
		[S6E3FAB_FQ_HIGH_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_0")),
		[S6E3FAB_FQ_HIGH_1] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br11_1, RESNAME("gamma_mtp_120hs_1")),
		[S6E3FAB_FQ_HIGH_2] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br11_2, RESNAME("gamma_mtp_120hs_2")),
		[S6E3FAB_FQ_HIGH_3] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br11_3, RESNAME("gamma_mtp_120hs_3")),
		[S6E3FAB_FQ_HIGH_4] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br11_4, RESNAME("gamma_mtp_120hs_4")),
		[S6E3FAB_FQ_HIGH_5] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br11_5, RESNAME("gamma_mtp_120hs_5")),
		[S6E3FAB_FQ_HIGH_6] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br11_6, RESNAME("gamma_mtp_120hs_6")),
		[S6E3FAB_FQ_HIGH_7] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br11_7, RESNAME("gamma_mtp_120hs_7")),
		[S6E3FAB_FQ_LOW_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_0")),
		[S6E3FAB_FQ_LOW_1] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_1")),
		[S6E3FAB_FQ_LOW_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_2")),
		[S6E3FAB_FQ_LOW_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_3")),
		[S6E3FAB_FQ_LOW_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_4")),
		[S6E3FAB_FQ_LOW_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_5")),
		[S6E3FAB_FQ_LOW_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_6")),
		[S6E3FAB_FQ_LOW_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_7")),
	},
	[S6E3FAB_UNBOUND1_GAMMA_BR_12] = {
		[S6E3FAB_FQ_HIGH_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_0")),
		[S6E3FAB_FQ_HIGH_1] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br12_1, RESNAME("gamma_mtp_120hs_1")),
		[S6E3FAB_FQ_HIGH_2] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br12_2, RESNAME("gamma_mtp_120hs_2")),
		[S6E3FAB_FQ_HIGH_3] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br12_3, RESNAME("gamma_mtp_120hs_3")),
		[S6E3FAB_FQ_HIGH_4] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br12_4, RESNAME("gamma_mtp_120hs_4")),
		[S6E3FAB_FQ_HIGH_5] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br12_5, RESNAME("gamma_mtp_120hs_5")),
		[S6E3FAB_FQ_HIGH_6] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br12_6, RESNAME("gamma_mtp_120hs_6")),
		[S6E3FAB_FQ_HIGH_7] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br12_7, RESNAME("gamma_mtp_120hs_7")),
		[S6E3FAB_FQ_LOW_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_0")),
		[S6E3FAB_FQ_LOW_1] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_1")),
		[S6E3FAB_FQ_LOW_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_2")),
		[S6E3FAB_FQ_LOW_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_3")),
		[S6E3FAB_FQ_LOW_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_4")),
		[S6E3FAB_FQ_LOW_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_5")),
		[S6E3FAB_FQ_LOW_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_6")),
		[S6E3FAB_FQ_LOW_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_7")),
	},
	[S6E3FAB_UNBOUND1_GAMMA_BR_13] = {
		[S6E3FAB_FQ_HIGH_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_0")),
		[S6E3FAB_FQ_HIGH_1] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br13_1, RESNAME("gamma_mtp_120hs_1")),
		[S6E3FAB_FQ_HIGH_2] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br13_2, RESNAME("gamma_mtp_120hs_2")),
		[S6E3FAB_FQ_HIGH_3] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br13_3, RESNAME("gamma_mtp_120hs_3")),
		[S6E3FAB_FQ_HIGH_4] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br13_4, RESNAME("gamma_mtp_120hs_4")),
		[S6E3FAB_FQ_HIGH_5] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br13_5, RESNAME("gamma_mtp_120hs_5")),
		[S6E3FAB_FQ_HIGH_6] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br13_6, RESNAME("gamma_mtp_120hs_6")),
		[S6E3FAB_FQ_HIGH_7] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br13_7, RESNAME("gamma_mtp_120hs_7")),
		[S6E3FAB_FQ_LOW_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_0")),
		[S6E3FAB_FQ_LOW_1] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_1")),
		[S6E3FAB_FQ_LOW_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_2")),
		[S6E3FAB_FQ_LOW_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_3")),
		[S6E3FAB_FQ_LOW_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_4")),
		[S6E3FAB_FQ_LOW_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_5")),
		[S6E3FAB_FQ_LOW_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_6")),
		[S6E3FAB_FQ_LOW_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_7")),
	},
	[S6E3FAB_UNBOUND1_GAMMA_BR_14] = {
		[S6E3FAB_FQ_HIGH_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_0")),
		[S6E3FAB_FQ_HIGH_1] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br14_1, RESNAME("gamma_mtp_120hs_1")),
		[S6E3FAB_FQ_HIGH_2] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br14_2, RESNAME("gamma_mtp_120hs_2")),
		[S6E3FAB_FQ_HIGH_3] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br14_3, RESNAME("gamma_mtp_120hs_3")),
		[S6E3FAB_FQ_HIGH_4] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br14_4, RESNAME("gamma_mtp_120hs_4")),
		[S6E3FAB_FQ_HIGH_5] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br14_5, RESNAME("gamma_mtp_120hs_5")),
		[S6E3FAB_FQ_HIGH_6] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br14_6, RESNAME("gamma_mtp_120hs_6")),
		[S6E3FAB_FQ_HIGH_7] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br14_7, RESNAME("gamma_mtp_120hs_7")),
		[S6E3FAB_FQ_LOW_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_0")),
		[S6E3FAB_FQ_LOW_1] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_1")),
		[S6E3FAB_FQ_LOW_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_2")),
		[S6E3FAB_FQ_LOW_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_3")),
		[S6E3FAB_FQ_LOW_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_4")),
		[S6E3FAB_FQ_LOW_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_5")),
		[S6E3FAB_FQ_LOW_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_6")),
		[S6E3FAB_FQ_LOW_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_7")),
	},
	[S6E3FAB_UNBOUND1_GAMMA_BR_15] = {
		[S6E3FAB_FQ_HIGH_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_0")),
		[S6E3FAB_FQ_HIGH_1] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br15_1, RESNAME("gamma_mtp_120hs_1")),
		[S6E3FAB_FQ_HIGH_2] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br15_2, RESNAME("gamma_mtp_120hs_2")),
		[S6E3FAB_FQ_HIGH_3] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br15_3, RESNAME("gamma_mtp_120hs_3")),
		[S6E3FAB_FQ_HIGH_4] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br15_4, RESNAME("gamma_mtp_120hs_4")),
		[S6E3FAB_FQ_HIGH_5] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br15_5, RESNAME("gamma_mtp_120hs_5")),
		[S6E3FAB_FQ_HIGH_6] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br15_6, RESNAME("gamma_mtp_120hs_6")),
		[S6E3FAB_FQ_HIGH_7] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br15_7, RESNAME("gamma_mtp_120hs_7")),
		[S6E3FAB_FQ_LOW_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_0")),
		[S6E3FAB_FQ_LOW_1] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_1")),
		[S6E3FAB_FQ_LOW_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_2")),
		[S6E3FAB_FQ_LOW_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_3")),
		[S6E3FAB_FQ_LOW_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_4")),
		[S6E3FAB_FQ_LOW_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_5")),
		[S6E3FAB_FQ_LOW_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_6")),
		[S6E3FAB_FQ_LOW_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_7")),
	},

	[S6E3FAB_UNBOUND1_GAMMA_BR_23 ... MAX_S6E3FAB_UNBOUND1_GAMMA_BR_INDEX - 1] = {
		[S6E3FAB_FQ_HIGH_0] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br23_0, RESNAME("gamma_mtp_120hs_0")),
		[S6E3FAB_FQ_HIGH_1] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br23_1, RESNAME("gamma_mtp_120hs_1")),
		[S6E3FAB_FQ_HIGH_2] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br23_2, RESNAME("gamma_mtp_120hs_2")),
		[S6E3FAB_FQ_HIGH_3] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br23_3, RESNAME("gamma_mtp_120hs_3")),
		[S6E3FAB_FQ_HIGH_4] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br23_4, RESNAME("gamma_mtp_120hs_4")),
		[S6E3FAB_FQ_HIGH_5] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br23_5, RESNAME("gamma_mtp_120hs_5")),
		[S6E3FAB_FQ_HIGH_6] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br23_6, RESNAME("gamma_mtp_120hs_6")),
		[S6E3FAB_FQ_HIGH_7] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_96hs_br23_7, RESNAME("gamma_mtp_120hs_7")),
		[S6E3FAB_FQ_LOW_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_0")),
		[S6E3FAB_FQ_LOW_1] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_1")),
		[S6E3FAB_FQ_LOW_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_2")),
		[S6E3FAB_FQ_LOW_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_3")),
		[S6E3FAB_FQ_LOW_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_4")),
		[S6E3FAB_FQ_LOW_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_5")),
		[S6E3FAB_FQ_LOW_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_6")),
		[S6E3FAB_FQ_LOW_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_7")),
	},
};

static struct gm2_dimming_lut unbound1_a3_s0_dimming_lut_60hs[MAX_S6E3FAB_UNBOUND1_GAMMA_BR_INDEX][MAX_S6E3FAB_FQ_SET] = {
	[S6E3FAB_UNBOUND1_GAMMA_BR_0 ... S6E3FAB_UNBOUND1_GAMMA_BR_87 - 1] = {
		[S6E3FAB_FQ_HIGH_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_0")),
		[S6E3FAB_FQ_HIGH_1] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_1")),
		[S6E3FAB_FQ_HIGH_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_2")),
		[S6E3FAB_FQ_HIGH_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_3")),
		[S6E3FAB_FQ_HIGH_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_4")),
		[S6E3FAB_FQ_HIGH_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_5")),
		[S6E3FAB_FQ_HIGH_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_6")),
		[S6E3FAB_FQ_HIGH_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_7")),
		[S6E3FAB_FQ_LOW_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_0")),
		[S6E3FAB_FQ_LOW_1] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_1")),
		[S6E3FAB_FQ_LOW_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_2")),
		[S6E3FAB_FQ_LOW_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_3")),
		[S6E3FAB_FQ_LOW_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_4")),
		[S6E3FAB_FQ_LOW_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_5")),
		[S6E3FAB_FQ_LOW_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_6")),
		[S6E3FAB_FQ_LOW_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_7")),
	},
	[S6E3FAB_UNBOUND1_GAMMA_BR_87 ... S6E3FAB_UNBOUND1_GAMMA_BR_159 - 1] = {
		[S6E3FAB_FQ_HIGH_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_0")),
		[S6E3FAB_FQ_HIGH_1] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_1")),
		[S6E3FAB_FQ_HIGH_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_2")),
		[S6E3FAB_FQ_HIGH_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_3")),
		[S6E3FAB_FQ_HIGH_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_4")),
		[S6E3FAB_FQ_HIGH_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_5")),
		[S6E3FAB_FQ_HIGH_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_6")),
		[S6E3FAB_FQ_HIGH_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_7")),
		[S6E3FAB_FQ_LOW_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_0")),
		[S6E3FAB_FQ_LOW_1] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_60hs_br87_1, RESNAME("gamma_mtp_120hs_1")),
		[S6E3FAB_FQ_LOW_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_2")),
		[S6E3FAB_FQ_LOW_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_3")),
		[S6E3FAB_FQ_LOW_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_4")),
		[S6E3FAB_FQ_LOW_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_5")),
		[S6E3FAB_FQ_LOW_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_6")),
		[S6E3FAB_FQ_LOW_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_7")),
	},
	[S6E3FAB_UNBOUND1_GAMMA_BR_159 ... S6E3FAB_UNBOUND1_GAMMA_BR_208 - 1] = {
		[S6E3FAB_FQ_HIGH_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_0")),
		[S6E3FAB_FQ_HIGH_1] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_1")),
		[S6E3FAB_FQ_HIGH_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_2")),
		[S6E3FAB_FQ_HIGH_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_3")),
		[S6E3FAB_FQ_HIGH_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_4")),
		[S6E3FAB_FQ_HIGH_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_5")),
		[S6E3FAB_FQ_HIGH_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_6")),
		[S6E3FAB_FQ_HIGH_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_7")),
		[S6E3FAB_FQ_LOW_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_0")),
		[S6E3FAB_FQ_LOW_1] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_60hs_br159_1, RESNAME("gamma_mtp_120hs_1")),
		[S6E3FAB_FQ_LOW_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_2")),
		[S6E3FAB_FQ_LOW_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_3")),
		[S6E3FAB_FQ_LOW_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_4")),
		[S6E3FAB_FQ_LOW_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_5")),
		[S6E3FAB_FQ_LOW_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_6")),
		[S6E3FAB_FQ_LOW_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_7")),
	},
	[S6E3FAB_UNBOUND1_GAMMA_BR_208 ... MAX_S6E3FAB_UNBOUND1_GAMMA_BR_INDEX - 1] = {
		[S6E3FAB_FQ_HIGH_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_0")),
		[S6E3FAB_FQ_HIGH_1] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_1")),
		[S6E3FAB_FQ_HIGH_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_2")),
		[S6E3FAB_FQ_HIGH_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_3")),
		[S6E3FAB_FQ_HIGH_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_4")),
		[S6E3FAB_FQ_HIGH_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_5")),
		[S6E3FAB_FQ_HIGH_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_6")),
		[S6E3FAB_FQ_HIGH_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_7")),
		[S6E3FAB_FQ_LOW_0] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_60hs_br208_0, RESNAME("gamma_mtp_120hs_0")),
		[S6E3FAB_FQ_LOW_1] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_60hs_br208_1, RESNAME("gamma_mtp_120hs_1")),
		[S6E3FAB_FQ_LOW_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_2")),
		[S6E3FAB_FQ_LOW_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_3")),
		[S6E3FAB_FQ_LOW_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_4")),
		[S6E3FAB_FQ_LOW_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_5")),
		[S6E3FAB_FQ_LOW_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_6")),
		[S6E3FAB_FQ_LOW_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_7")),
	},
};

static struct gm2_dimming_lut unbound1_a3_s0_dimming_lut_48hs[MAX_S6E3FAB_UNBOUND1_GAMMA_BR_INDEX][MAX_S6E3FAB_FQ_SET] = {
	[S6E3FAB_UNBOUND1_GAMMA_BR_0 ... S6E3FAB_UNBOUND1_GAMMA_BR_117 - 1] = {
		[S6E3FAB_FQ_HIGH_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_0")),
		[S6E3FAB_FQ_HIGH_1] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_1")),
		[S6E3FAB_FQ_HIGH_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_2")),
		[S6E3FAB_FQ_HIGH_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_3")),
		[S6E3FAB_FQ_HIGH_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_4")),
		[S6E3FAB_FQ_HIGH_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_5")),
		[S6E3FAB_FQ_HIGH_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_6")),
		[S6E3FAB_FQ_HIGH_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_7")),
		[S6E3FAB_FQ_LOW_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_0")),
		[S6E3FAB_FQ_LOW_1] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_1")),
		[S6E3FAB_FQ_LOW_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_2")),
		[S6E3FAB_FQ_LOW_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_3")),
		[S6E3FAB_FQ_LOW_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_4")),
		[S6E3FAB_FQ_LOW_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_5")),
		[S6E3FAB_FQ_LOW_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_6")),
		[S6E3FAB_FQ_LOW_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_7")),
	},
	[S6E3FAB_UNBOUND1_GAMMA_BR_117 ... MAX_S6E3FAB_UNBOUND1_GAMMA_BR_INDEX - 1] = {
		[S6E3FAB_FQ_HIGH_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_0")),
		[S6E3FAB_FQ_HIGH_1] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_1")),
		[S6E3FAB_FQ_HIGH_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_2")),
		[S6E3FAB_FQ_HIGH_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_3")),
		[S6E3FAB_FQ_HIGH_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_4")),
		[S6E3FAB_FQ_HIGH_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_5")),
		[S6E3FAB_FQ_HIGH_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_6")),
		[S6E3FAB_FQ_HIGH_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_7")),
		[S6E3FAB_FQ_LOW_0] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_48hs_br117_0, RESNAME("gamma_mtp_120hs_0")),
		[S6E3FAB_FQ_LOW_1] = GM2_LUT_V0_INIT_SRC(unbound1_a3_s0_ctbl_48hs_br117_1, RESNAME("gamma_mtp_120hs_1")),
		[S6E3FAB_FQ_LOW_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_2")),
		[S6E3FAB_FQ_LOW_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_3")),
		[S6E3FAB_FQ_LOW_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_4")),
		[S6E3FAB_FQ_LOW_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_5")),
		[S6E3FAB_FQ_LOW_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_6")),
		[S6E3FAB_FQ_LOW_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60hs_7")),
	},
};

static struct gm2_dimming_lut unbound1_a3_s0_dimming_lut_60ns[MAX_S6E3FAB_UNBOUND1_GAMMA_BR_INDEX][MAX_S6E3FAB_FQ_SET] = {
	[S6E3FAB_UNBOUND1_GAMMA_BR_0 ... MAX_S6E3FAB_UNBOUND1_GAMMA_BR_INDEX - 1] = {
		[S6E3FAB_FQ_HIGH_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_0")),
		[S6E3FAB_FQ_HIGH_1] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_1")),
		[S6E3FAB_FQ_HIGH_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_2")),
		[S6E3FAB_FQ_HIGH_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_3")),
		[S6E3FAB_FQ_HIGH_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_4")),
		[S6E3FAB_FQ_HIGH_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_5")),
		[S6E3FAB_FQ_HIGH_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_6")),
		[S6E3FAB_FQ_HIGH_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_120hs_7")),
		[S6E3FAB_FQ_LOW_0] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60ns_0")),
		[S6E3FAB_FQ_LOW_1] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60ns_1")),
		[S6E3FAB_FQ_LOW_2] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60ns_2")),
		[S6E3FAB_FQ_LOW_3] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60ns_3")),
		[S6E3FAB_FQ_LOW_4] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60ns_4")),
		[S6E3FAB_FQ_LOW_5] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60ns_5")),
		[S6E3FAB_FQ_LOW_6] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60ns_6")),
		[S6E3FAB_FQ_LOW_7] = GM2_LUT_V0_SRC(RESNAME("gamma_mtp_60ns_7")),
	},
};

struct gm2_dimming_init_info s6e3had_unbound1_a3_s0_gm2_dimming_init_info[MAX_S6E3FAB_VRR] = {
	[S6E3FAB_VRR_60NS] = {
		.name = "s6e3fab_unbound1_a3_s0_gm2_dimming_60ns",
		.nr_tp = S6E3FAB_NR_TP,
		.tp = s6e3fab_unbound_tp,
		.dim_lut = (struct gm2_dimming_lut *)unbound1_a3_s0_dimming_lut_60ns,
		.nr_dim_lut = MAX_S6E3FAB_UNBOUND1_GAMMA_BR_INDEX * MAX_S6E3FAB_FQ_SET,
		.brt_range = unbound1_a3_s0_vrr_gamma_br_range,
		.sz_brt_range = ARRAY_SIZE(unbound1_a3_s0_vrr_gamma_br_range),
	},
	[S6E3FAB_VRR_48NS] = {
		.name = "s6e3fab_unbound1_a3_s0_gm2_dimming_48ns",
		.nr_tp = S6E3FAB_NR_TP,
		.tp = s6e3fab_unbound_tp,
		.dim_lut = (struct gm2_dimming_lut *)unbound1_a3_s0_dimming_lut_60ns,
		.nr_dim_lut = MAX_S6E3FAB_UNBOUND1_GAMMA_BR_INDEX * MAX_S6E3FAB_FQ_SET,
		.brt_range = unbound1_a3_s0_vrr_gamma_br_range,
		.sz_brt_range = ARRAY_SIZE(unbound1_a3_s0_vrr_gamma_br_range),
	},
	[S6E3FAB_VRR_120HS] = {
		.name = "s6e3fab_unbound1_a3_s0_gm2_dimming_120hs",
		.nr_tp = S6E3FAB_NR_TP,
		.tp = s6e3fab_unbound_tp,
		.dim_lut = (struct gm2_dimming_lut *)unbound1_a3_s0_dimming_lut_120hs,
		.nr_dim_lut = MAX_S6E3FAB_UNBOUND1_GAMMA_BR_INDEX * MAX_S6E3FAB_FQ_SET,
		.brt_range = unbound1_a3_s0_vrr_gamma_br_range,
		.sz_brt_range = ARRAY_SIZE(unbound1_a3_s0_vrr_gamma_br_range),
	},
	[S6E3FAB_VRR_96HS] = {
		.name = "s6e3fab_unbound1_a3_s0_gm2_dimming_96hs",
		.nr_tp = S6E3FAB_NR_TP,
		.tp = s6e3fab_unbound_tp,
		.dim_lut = (struct gm2_dimming_lut *)unbound1_a3_s0_dimming_lut_96hs,
		.nr_dim_lut = MAX_S6E3FAB_UNBOUND1_GAMMA_BR_INDEX * MAX_S6E3FAB_FQ_SET,
		.brt_range = unbound1_a3_s0_vrr_gamma_br_range,
		.sz_brt_range = ARRAY_SIZE(unbound1_a3_s0_vrr_gamma_br_range),
	},
	[S6E3FAB_VRR_60HS] = {
		.name = "s6e3fab_unbound1_a3_s0_gm2_dimming_60hs",
		.nr_tp = S6E3FAB_NR_TP,
		.tp = s6e3fab_unbound_tp,
		.dim_lut = (struct gm2_dimming_lut *)unbound1_a3_s0_dimming_lut_60hs,
		.nr_dim_lut = MAX_S6E3FAB_UNBOUND1_GAMMA_BR_INDEX * MAX_S6E3FAB_FQ_SET,
		.brt_range = unbound1_a3_s0_vrr_gamma_br_range,
		.sz_brt_range = ARRAY_SIZE(unbound1_a3_s0_vrr_gamma_br_range),
	},
	[S6E3FAB_VRR_60HS_120HS_TE_HW_SKIP_1] = {
		.name = "s6e3fab_unbound1_a3_s0_gm2_dimming_60hs_120hs_te_skip_1",
		.nr_tp = S6E3FAB_NR_TP,
		.tp = s6e3fab_unbound_tp,
		.dim_lut = (struct gm2_dimming_lut *)unbound1_a3_s0_dimming_lut_120hs,
		.nr_dim_lut = MAX_S6E3FAB_UNBOUND1_GAMMA_BR_INDEX * MAX_S6E3FAB_FQ_SET,
		.brt_range = unbound1_a3_s0_vrr_gamma_br_range,
		.sz_brt_range = ARRAY_SIZE(unbound1_a3_s0_vrr_gamma_br_range),
	},
	[S6E3FAB_VRR_48HS] = {
		.name = "s6e3fab_unbound1_a3_s0_gm2_dimming_48hs",
		.nr_tp = S6E3FAB_NR_TP,
		.tp = s6e3fab_unbound_tp,
		.dim_lut = (struct gm2_dimming_lut *)unbound1_a3_s0_dimming_lut_48hs,
		.nr_dim_lut = MAX_S6E3FAB_UNBOUND1_GAMMA_BR_INDEX * MAX_S6E3FAB_FQ_SET,
		.brt_range = unbound1_a3_s0_vrr_gamma_br_range,
		.sz_brt_range = ARRAY_SIZE(unbound1_a3_s0_vrr_gamma_br_range),
	},
	[S6E3FAB_VRR_48HS_96HS_TE_HW_SKIP_1] = {
		.name = "s6e3fab_unbound1_a3_s0_gm2_dimming_48hs_96hs_te_skip_1",
		.nr_tp = S6E3FAB_NR_TP,
		.tp = s6e3fab_unbound_tp,
		.dim_lut = (struct gm2_dimming_lut *)unbound1_a3_s0_dimming_lut_96hs,
		.nr_dim_lut = MAX_S6E3FAB_UNBOUND1_GAMMA_BR_INDEX * MAX_S6E3FAB_FQ_SET,
		.brt_range = unbound1_a3_s0_vrr_gamma_br_range,
		.sz_brt_range = ARRAY_SIZE(unbound1_a3_s0_vrr_gamma_br_range),
	},
};

struct brightness_table s6e3fab_unbound1_a3_s0_panel_brightness_table = {
	.control_type = BRIGHTNESS_CONTROL_TYPE_GAMMA_MODE2,
	.brt = unbound1_a3_s0_brt_tbl,
	.sz_brt = ARRAY_SIZE(unbound1_a3_s0_brt_tbl),
	.sz_ui_brt = S6E3FAB_NR_STEP,
	.sz_hbm_brt = S6E3FAB_HBM_STEP,
	.lum = unbound1_a3_s0_lum_tbl,
	.sz_lum = S6E3FAB_TOTAL_NR_LUMINANCE,
	.sz_ui_lum = S6E3FAB_NR_LUMINANCE,
	.sz_hbm_lum = S6E3FAB_NR_HBM_LUMINANCE,
	.sz_ext_hbm_lum = 0,
	.brt_to_step = NULL,
	.sz_brt_to_step = 0,
	.step_cnt = unbound1_a3_s0_step_cnt_tbl,
	.sz_step_cnt = ARRAY_SIZE(unbound1_a3_s0_step_cnt_tbl),
	.vtotal = 3232,
};

static struct panel_dimming_info s6e3fab_unbound1_a3_s0_panel_dimming_info = {
	.name = "s6e3fab_unbound1_a3_s0",
	.dim_init_info = {
		NULL,
	},
	.target_luminance = -1,
	.nr_luminance = 0,
	.hbm_target_luminance = -1,
	.nr_hbm_luminance = 0,
	.extend_hbm_target_luminance = -1,
	.nr_extend_hbm_luminance = 0,
	.brt_tbl = &s6e3fab_unbound1_a3_s0_panel_brightness_table,
	/* dimming parameters */
	.dimming_maptbl = NULL,
	.dim_flash_on = false,
	.hbm_aor = NULL,
	.gm2_dim_init_info = s6e3had_unbound1_a3_s0_gm2_dimming_init_info,
	.nr_gm2_dim_init_info = ARRAY_SIZE(s6e3had_unbound1_a3_s0_gm2_dimming_init_info),
};
#endif /* __S6E3FAB_UNBOUND1_A3_S0_PANEL_DIMMING_H___ */
