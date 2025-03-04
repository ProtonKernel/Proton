#ifndef _REGS_DPU_H_
#define _REGS_DPU_H_

#define DQE_BASE			0x2000
/* DQECON_SET */
#define DQECON				0x0000
#define DQE_LPD_MODE_EXIT_MASK		(1 << 24)
#define DQE_LPD_MODE_EXIT_GET(_v)	(((_v) >> 24) & 0x1)
#define DQE_APS_SW_RESET_MASK		(1 << 18)
#define DQE_APS_SW_RESET_GET(_v)	(((_v) >> 18) & 0x1)
#define DQE_HSC_SW_RESET_MASK		(1 << 16)
#define DQE_HSC_SW_RESET_GET(_v)	(((_v) >> 16) & 0x1)
#define DQE_APS_ON_MASK			(1 << 4)
#define DQE_APS_ON_GET(_v)		(((_v) >> 4) & 0x1)
#define DQE_HSC_ON_MASK			(1 << 3)
#define DQE_HSC_ON_GET(_v)		(((_v) >> 3) & 0x1)
#define DQE_GAMMA_ON_MASK		(1 << 2)
#define DQE_GAMMA_ON_GET(_v)		(((_v) >> 2) & 0x1)
#define DQE_CGC_ON_MASK			(0x3 << 0)
#define DQE_CGC_ON_GET(_v)		(((_v) >> 0) & 0x3)
#define DQECON_ALL_MASK			(0xffffffff << 0)

/* IMG_SIZE_SET */
#define DQEIMG_SIZESET			0x0004
#define DQEIMG_VSIZE_F(_v)		((_v) << 16)
#define DQEIMG_VSIZE_MASK		(0x1fff << 16)
#define DQEIMG_VSIZE_GET(_v)		(((_v) >> 16) & 0x1fff)
#define DQEIMG_HSIZE_F(_v)		((_v) << 0)
#define DQEIMG_HSIZE_MASK		(0x1fff << 0)
#define DQEIMG_HSIZE_GET(_v)		(((_v) >> 0) & 0x1fff)

/* CGC_SET */
#define DQECGC_CONTROL1			0x0010
#define CGC_GAMMA_ENC_EN(_v)		(((_v) & 0x1) << 20)
#define CGC_GAMMA_ENC_EN_MASK		(0x1 << 20)
#define CGC_GAMMA_DEC_EN(_v)		(((_v) & 0x1) << 16)
#define CGC_GAMMA_DEC_EN_MASK		(0x1 << 16)
#define CGC_DITHER_CON(_v)		(((_v) & 0x3) << 12)
#define CGC_DITHER_CON_MASK		(0x3 << 12)
#define CGC_DITHER_ON(_v)		(((_v) & 0x1) << 8)
#define CGC_DITHER_ON_MASK		(0x1 << 8)
#define CGC_DITHER_CNT_DIFF(_v)		(((_v) & 0x1) << 0)
#define CGC_DITHER_CNT_DIFF_MASK	(0x1 << 0)

#define DQECGC_CONTROL2			0x0014
#define CGC_GRAY_GAIN(_v)		(((_v) & 0x7ff) << 16)
#define CGC_GRAY_GAIN_MASK		(0x7ff << 16)
#define CGC_GRAY_BYPASS(_v)		(((_v) & 0x1) << 4)
#define CGC_GRAY_BYPASS_MASK		(0x1 << 4)
#define CGC_GRAY_MODE(_v)		(((_v) & 0x1) << 0)
#define CGC_GRAY_MODE_MASK		(0x1 << 0)

#define DQECGC_CONTROL3			0x0018
#define CGC_MC_GAIN(_v)			(((_v) & 0xfff) << 16)
#define CGC_MC_GAIN_MASK		(0xfff << 16)
#define CGC_GRAY_WEIGHT_TH(_v)		(((_v) & 0x3ff) << 0)
#define CGC_GRAY_WEIGHT_TH_MASK		(0x3ff << 0)

#define DQECGC1_RED1			0x001C
#define DQECGC_GRAYLUT_R1(_v)		(((_v) & 0x1ff) << 16)
#define DQECGC_GRAYLUT_R1_MASK		(0x1ff << 16)
#define DQECGC_GRAYLUT_R0(_v)		(((_v) & 0x1ff) << 0)
#define DQECGC_GRAYLUT_R0_MASK		(0x1ff << 0)

#define DQECGC1_GREEN1			0x0040
#define DQECGC_GRAYLUT_G1(_v)		(((_v) & 0x1ff) << 16)
#define DQECGC_GRAYLUT_G1_MASK		(0x1ff << 16)
#define DQECGC_GRAYLUT_G0(_v)		(((_v) & 0x1ff) << 0)
#define DQECGC_GRAYLUT_G0_MASK		(0x1ff << 0)

#define DQECGC1_BLUE1			0x0064
#define DQECGC_GRAYLUT_B1(_v)		(((_v) & 0x1ff) << 16)
#define DQECGC_GRAYLUT_B1_MASK		(0x1ff << 16)
#define DQECGC_GRAYLUT_B0(_v)		(((_v) & 0x1ff) << 0)
#define DQECGC_GRAYLUT_B0_MASK		(0x1ff << 0)

#define DQECGC1_LUT27_RED1		0x0088
#define DQECGC_LUT27_001_R(_v)		(((_v) & 0x1fff) << 16)
#define DQECGC_LUT27_001_R_MASK		(0x1fff << 16)
#define DQECGC_LUT27_000_R(_v)		(((_v) & 0x1fff) << 0)
#define DQECGC_LUT27_000_R_MASK		(0x1fff << 0)

#define DQECGC1_LUT27_GREEN1		0x00C0
#define DQECGC_LUT27_001_G(_v)		(((_v) & 0x1fff) << 16)
#define DQECGC_LUT27_001_G_MASK		(0x1fff << 16)
#define DQECGC_LUT27_000_G(_v)		(((_v) & 0x1fff) << 0)
#define DQECGC_LUT27_000_G_MASK		(0x1fff << 0)

#define DQECGC1_LUT27_BLUE1		0x00F8
#define DQECGC_LUT27_001_B(_v)		(((_v) & 0x1fff) << 16)
#define DQECGC_LUT27_001_B_MASK		(0x1fff << 16)
#define DQECGC_LUT27_000_B(_v)		(((_v) & 0x1fff) << 0)
#define DQECGC_LUT27_000_B_MASK		(0x1fff << 0)

#define DQECGCLUT_MAX			(72)
#define DQECGCLUT_BASE			0x0010

/* GAMMA_SET */
#define DQEGAMMA_OFFSET			132
#define DQEGAMMALUT_X_Y_BASE		0x0134
#define DQEGAMMALUT_MAX			(33 * 3)
#define DQEGAMMALUT_X(_v)		(((_v) & 0x7ff) << 0)
#define DQEGAMMALUT_Y(_v)		(((_v) & 0x7ff) << 16)
#define DQEGAMMALUT_X_MASK		(0x7ff << 0)
#define DQEGAMMALUT_Y_MASK		(0x7ff << 16)
#define DQEGAMMALUT_X_GET(_v)		(((_v) >> 0) & 0x7ff)
#define DQEGAMMALUT_Y_GET(_v)		(((_v) >> 16) & 0x7ff)

#define DQEGAMMALUT_R_01_00		0x0134
#define DQEGAMMALUT_R_03_02		0x0138
#define DQEGAMMALUT_R_05_04		0x013c
#define DQEGAMMALUT_R_07_06		0x0140
#define DQEGAMMALUT_R_09_08		0x0144
#define DQEGAMMALUT_R_11_10		0x0148
#define DQEGAMMALUT_R_13_12		0x014c
#define DQEGAMMALUT_R_15_14		0x0150
#define DQEGAMMALUT_R_17_16		0x0154
#define DQEGAMMALUT_R_19_18		0x0158
#define DQEGAMMALUT_R_21_20		0x015c
#define DQEGAMMALUT_R_23_22		0x0160
#define DQEGAMMALUT_R_25_24		0x0164
#define DQEGAMMALUT_R_27_26		0x0168
#define DQEGAMMALUT_R_29_28		0x016c
#define DQEGAMMALUT_R_31_30		0x0170
#define DQEGAMMALUT_R_33_32		0x0174
#define DQEGAMMALUT_R_35_34		0x0178
#define DQEGAMMALUT_R_37_36		0x017c
#define DQEGAMMALUT_R_39_38		0x0180
#define DQEGAMMALUT_R_41_40		0x0184
#define DQEGAMMALUT_R_43_42		0x0188
#define DQEGAMMALUT_R_45_44		0x018c
#define DQEGAMMALUT_R_47_46		0x0190
#define DQEGAMMALUT_R_49_48		0x0194
#define DQEGAMMALUT_R_51_50		0x0198
#define DQEGAMMALUT_R_53_52		0x019c
#define DQEGAMMALUT_R_55_54		0x01a0
#define DQEGAMMALUT_R_57_56		0x01a4
#define DQEGAMMALUT_R_59_58		0x01a8
#define DQEGAMMALUT_R_61_60		0x01ac
#define DQEGAMMALUT_R_63_62		0x01b0
#define DQEGAMMALUT_R_64		0x01b4
#define DQEGAMMALUT_G_01_00		0x01b8
#define DQEGAMMALUT_G_03_02		0x01bc
#define DQEGAMMALUT_G_05_04		0x01c0
#define DQEGAMMALUT_G_07_06		0x01c4
#define DQEGAMMALUT_G_09_08		0x01c8
#define DQEGAMMALUT_G_11_10		0x01cc
#define DQEGAMMALUT_G_13_12		0x01d0
#define DQEGAMMALUT_G_15_14		0x01d4
#define DQEGAMMALUT_G_17_16		0x01d8
#define DQEGAMMALUT_G_19_18		0x01dc
#define DQEGAMMALUT_G_21_20		0x01e0
#define DQEGAMMALUT_G_23_22		0x01e4
#define DQEGAMMALUT_G_25_24		0x01e8
#define DQEGAMMALUT_G_27_26		0x01ec
#define DQEGAMMALUT_G_29_28		0x01f0
#define DQEGAMMALUT_G_31_30		0x01f4
#define DQEGAMMALUT_G_33_32		0x01f8
#define DQEGAMMALUT_G_35_34		0x01fc
#define DQEGAMMALUT_G_37_36		0x0200
#define DQEGAMMALUT_G_39_38		0x0204
#define DQEGAMMALUT_G_41_40		0x0208
#define DQEGAMMALUT_G_43_42		0x020c
#define DQEGAMMALUT_G_45_44		0x0210
#define DQEGAMMALUT_G_47_46		0x0214
#define DQEGAMMALUT_G_49_48		0x0218
#define DQEGAMMALUT_G_51_50		0x021c
#define DQEGAMMALUT_G_53_52		0x0220
#define DQEGAMMALUT_G_55_54		0x0224
#define DQEGAMMALUT_G_57_56		0x0228
#define DQEGAMMALUT_G_59_58		0x022c
#define DQEGAMMALUT_G_61_60		0x0230
#define DQEGAMMALUT_G_63_62		0x0234
#define DQEGAMMALUT_G_64		0x0238
#define DQEGAMMALUT_B_01_00		0x023c
#define DQEGAMMALUT_B_03_02		0x0240
#define DQEGAMMALUT_B_05_04		0x0244
#define DQEGAMMALUT_B_07_06		0x0248
#define DQEGAMMALUT_B_09_08		0x024c
#define DQEGAMMALUT_B_11_10		0x0250
#define DQEGAMMALUT_B_13_12		0x0254
#define DQEGAMMALUT_B_15_14		0x0258
#define DQEGAMMALUT_B_17_16		0x025c
#define DQEGAMMALUT_B_19_18		0x0260
#define DQEGAMMALUT_B_21_20		0x0264
#define DQEGAMMALUT_B_23_22		0x0268
#define DQEGAMMALUT_B_25_24		0x026c
#define DQEGAMMALUT_B_27_26		0x0270
#define DQEGAMMALUT_B_29_28		0x0274
#define DQEGAMMALUT_B_31_30		0x0278
#define DQEGAMMALUT_B_33_32		0x027c
#define DQEGAMMALUT_B_35_34		0x0280
#define DQEGAMMALUT_B_37_36		0x0284
#define DQEGAMMALUT_B_39_38		0x0288
#define DQEGAMMALUT_B_41_40		0x028c
#define DQEGAMMALUT_B_43_42		0x0290
#define DQEGAMMALUT_B_45_44		0x0294
#define DQEGAMMALUT_B_47_46		0x0298
#define DQEGAMMALUT_B_49_48		0x029c
#define DQEGAMMALUT_B_51_50		0x02a0
#define DQEGAMMALUT_B_53_52		0x02a4
#define DQEGAMMALUT_B_55_54		0x02a8
#define DQEGAMMALUT_B_57_56		0x02ac
#define DQEGAMMALUT_B_59_58		0x02b0
#define DQEGAMMALUT_B_61_60		0x02b4
#define DQEGAMMALUT_B_63_62		0x02b8
#define DQEGAMMALUT_B_64		0x02bc

/*APS_SET*/
#define DQEAPSLUT_MAX			(15)
#define DQEAPSLUT_BASE			0x02C0

#define DQEAPSLUT_ST(_v)		(((_v) & 0xff) << 16)
#define DQEAPSLUT_NS(_v)		(((_v) & 0xff) << 8)
#define DQEAPSLUT_LT(_v)		(((_v) & 0xff) << 0)
#define DQEAPSLUT_PL_W2(_v)		(((_v) & 0xf) << 16)
#define DQEAPSLUT_PL_W1(_v)		(((_v) & 0xf) << 0)
#define DQEAPSLUT_CTMODE(_v)		(((_v) & 0x3) << 0)
#define DQEAPSLUT_PP_EN(_v)		(((_v) & 0x1) << 0)
#define DQEAPSLUT_TDR_MAX(_v)		(((_v) & 0x3ff) << 16)
#define DQEAPSLUT_TDR_MIN(_v)		(((_v) & 0x3ff) << 0)
#define DQEAPSLUT_AMBIENT_LIGHT(_v)	(((_v) & 0xff) << 0)
#define DQEAPSLUT_BACK_LIGHT(_v)	(((_v) & 0xff) << 0)
#define DQEAPSLUT_DSTEP(_v)		(((_v) & 0x3f) << 0)
#define DQEAPSLUT_SCALE_MODE(_v)	(((_v) & 0x3) << 0)
#define DQEAPSLUT_THRESHOLD_3(_v)	(((_v) & 0x3) << 4)
#define DQEAPSLUT_THRESHOLD_2(_v)	(((_v) & 0x3) << 2)
#define DQEAPSLUT_THRESHOLD_1(_v)	(((_v) & 0x3) << 0)
#define DQEAPSLUT_GAIN_LIMIT(_v)	(((_v) & 0x3ff) << 0)
#define DQEAPSLUT_ROI_SAME(_v)		(((_v) & 0x1) << 2)
#define DQEAPSLUT_UPDATE_METHOD(_v)	(((_v) & 0x1) << 1)
#define DQEAPSLUT_PARTIAL_FRAME(_v)	(((_v) & 0x1) << 0)
#define DQEAPSLUT_ROI_Y1(_v)		(((_v) & 0x1fff) << 16)
#define DQEAPSLUT_ROI_X1(_v)		(((_v) & 0x1fff) << 0)
#define DQEAPSLUT_IBSI_01(_v)		(((_v) & 0xffff) << 16)
#define DQEAPSLUT_IBSI_00(_v)		(((_v) & 0xffff) << 0)
#define DQEAPSLUT_IBSI_11(_v)		(((_v) & 0xffff) << 16)
#define DQEAPSLUT_IBSI_10(_v)		(((_v) & 0xffff) << 0)

#define DQEAPS_GAIN			0x02C0
#define DQEAPS_WEIGHT			0x02C4
#define DQEAPS_CTMODE			0x02C8
#define DQEAPS_PPEN			0x02CC
#define DQEAPS_TDRMINMAX		0x02D0
#define DQEAPS_AMBIENT_LIGHT		0x02D4
#define DQEAPS_BACK_LIGHT		0x02D8
#define DQEAPS_DSTEP			0x02DC
#define DQEAPS_SCALE_MODE		0x02E0
#define DQEAPS_THRESHOLD		0x02E4
#define DQEAPS_GAIN_LIMIT		0x02E8
#define DQEAPS_DIMMING_DONE_INTR	0x02EC

/*HSC_SET */
#define DQEHSC_CONTROL0			0x0304
#define HSC_PARTIAL_UPDATE_METHOD(_v)	(((_v) & 0x1) << 18)
#define HSC_PARTIAL_UPDATE_METHOD_MASK	(0x1 << 18)
#define HSC_ROI_SAME(_v)		(((_v) & 0x1) << 17)
#define HSC_ROI_SAME_MASK		(0x1 << 17)
#define HSC_IMG_PARTIAL_FRAME(_v)	(((_v) & 0x1) << 16)
#define HSC_IMG_PARTIAL_FRAME_MASK	(0x1 << 16)
#define HSC_LBC_GROUPMODE(_v)		(((_v) & 0x3) << 12)
#define HSC_LBC_GROUPMODE_MASK		(0x3 << 12)
#define HSC_LHC_GROUPMODE(_v)		(((_v) & 0x3) << 8)
#define HSC_LHC_GROUPMODE_MASK		(0x3 << 8)
#define HSC_LSC_GROUPMODE(_v)		(((_v) & 0x3) << 4)
#define HSC_LSC_GROUPMODE_MASK		(0x3 << 4)
#define HSC_LBC_ON(_v)			(((_v) & 0x1) << 3)
#define HSC_LBC_ON_MASK			(0x1 << 3)
#define HSC_LHC_ON(_v)			(((_v) & 0x1) << 2)
#define HSC_LHC_ON_MASK			(0x1 << 2)
#define HSC_LSC_ON(_v)			(((_v) & 0x1) << 1)
#define HSC_LSC_ON_MASK			(0x1 << 1)

#define DQEHSC_CONTROL1			0x0308
#define HSC_FULL_PXL_NUM_MASK		(0x03ffffff << 0)
#define HSC_FULL_PXL_NUM_GET(_v)	(((_v) >> 0) & 0x03ffffff)

#define DQEHSC_CONTROL2			0x030C
#define HSC_GSC_GAIN(_v)		(((_v) & 0x7ff) << 16)
#define HSC_GSC_GAIN_MASK		(0x7ff << 16)
#define HSC_GBC_ON(_v)			(((_v) & 0x1) << 2)
#define HSC_GBC_ON_MASK			(0x1 << 2)
#define HSC_GHC_ON(_v)			(((_v) & 0x1) << 1)
#define HSC_GHC_ON_MASK			(0x1 << 1)
#define HSC_GSC_ON(_v)			(((_v) & 0x1) << 0)
#define HSC_GSC_ON_MASK			(0x1 << 0)

#define DQEHSC_CONTROL3			0x0310
#define HSC_GBC_GAIN(_v)		(((_v) & 0x7ff) << 16)
#define HSC_GBC_GAIN_MASK		(0x7ff << 16)
#define HSC_GHC_GAIN(_v)		(((_v) & 0x3ff) << 0)
#define HSC_GHC_GAIN_MASK		(0x3ff << 0)

#define DQEHSC_CONTROL_ALPHA_SAT	0x0314
#define HSC_ALPHA_SAT_SHIFT2(_v)	(((_v) & 0x7) << 28)
#define HSC_ALPHA_SAT_SHIFT2_MASK	(0x7 << 28)
#define HSC_ALPHA_SAT_SHIFT1(_v)	(((_v) & 0x1ff) << 16)
#define HSC_ALPHA_SAT_SHIFT1_MASK	(0x1ff << 16)
#define HSC_ALPHA_SAT_SCALE(_v)		(((_v) & 0xf) << 4)
#define HSC_ALPHA_SAT_SCALE_MASK	(0xf << 4)
#define HSC_ALPHA_SAT_ON(_v)		(((_v) & 0x1) << 0)
#define HSC_ALPHA_SAT_ON_MASK		(0x1 << 0)

#define DQEHSC_CONTROL_ALPHA_BRI	0x0318
#define HSC_ALPHA_BRI_SHIFT2(_v)	(((_v) & 0x7) << 28)
#define HSC_ALPHA_BRI_SHIFT2_MASK	(0x7 << 28)
#define HSC_ALPHA_BRI_SHIFT1(_v)	(((_v) & 0x1ff) << 16)
#define HSC_ALPHA_BRI_SHIFT1_MASK	(0x1ff << 16)
#define HSC_ALPHA_BRI_SCALE(_v)		(((_v) & 0xf) << 4)
#define HSC_ALPHA_BRI_SCALE_MASK	(0xf << 4)
#define HSC_ALPHA_BRI_ON(_v)		(((_v) & 0x1) << 0)
#define HSC_ALPHA_BRI_ON_MASK		(0x1 << 0)

#define DQEHSC_CONTROL_MC1		0x031C
#define HSC_MC_SAT_GAIN(_v)		(((_v) & 0x7ff) << 16)
#define HSC_MC_SAT_GAIN_MASK		(0x7ff << 16)
#define HSC_MC_BOUNDARY_CONTROL_SAT(_v)		(((_v) & 0x3) << 8)
#define HSC_MC_BOUNDARY_CONTROL_SAT_MASK	(0x3 << 8)
#define HSC_MC_BOUNDARY_CONTROL_HUE(_v)		(((_v) & 0x3) << 4)
#define HSC_MC_BOUNDARY_CONTROL_HUE_MASK	(0x3 << 4)
#define HSC_MC_ON(_v)			(((_v) & 0x1) << 0)
#define HSC_MC_ON_MASK			(0x1 << 0)

#define DQEHSC_CONTROL_MC_BRI		0x0320
#define HSC_MC_BRI_GAIN(_v)		(((_v) & 0x7ff) << 16)
#define HSC_MC_BRI_GAIN_MASK		(0x7ff << 16)
#define HSC_MC_HUE_GAIN(_v)		(((_v) & 0x3ff) << 0)
#define HSC_MC_HUE_GAIN_MASK		(0x3ff << 0)

#define DQEHSC_CONTROL_MC2		0x0324
#define HSC_MC_S2(_v)			(((_v) & 0x7ff) << 16)
#define HSC_MC_S2_MASK			(0x7ff << 16)
#define HSC_MC_S1(_v)			(((_v) & 0x3ff) << 0)
#define HSC_MC_S1_MASK			(0x3ff << 0)

#define DQEHSC_CONTROL_MC3		0x0328
#define HSC_MC_H2(_v)			(((_v) & 0xfff) << 16)
#define HSC_MC_H2_MASK			(0xfff << 16)
#define HSC_MC_H1(_v)			(((_v) & 0xfff) << 0)
#define HSC_MC_H1_MASK			(0xfff << 0)

#define DQEHSC_CONTROL_YCOMP		0x032C
#define HSC_BLEND_MANUAL_GAIN(_v)	(((_v) & 0xff) << 16)
#define HSC_BLEND_MANUAL_GAIN_MASK	(0xff << 16)
#define HSC_YCOMP_GAIN(_v)		(((_v) & 0xf) << 8)
#define HSC_YCOMP_GAIN_MASK		(0xf << 8)
#define HSC_BLEND_ON(_v)		(((_v) & 0x1) << 2)
#define HSC_BLEND_ON_MASK		(0x1 << 2)
#define HSC_YCOMP_DITH_ON(_v)		(((_v) & 0x1) << 1)
#define HSC_YCOMP_DITH_ON_MASK		(0x1 << 1)
#define HSC_YCOMP_ON(_v)		(((_v) & 0x1) << 0)
#define HSC_YCOMP_ON_MASK		(0x1 << 0)

#define DQEHSC_LSC_GAIN_P1_0		0x0330
#define HSC_LSC_GAIN_P1_1(_v)		(((_v) & 0x7ff) << 16)
#define HSC_LSC_GAIN_P1_1_MASK		(0x7ff << 16)
#define HSC_LSC_GAIN_P1_0(_v)		(((_v) & 0x7ff) << 0)
#define HSC_LSC_GAIN_P1_0_MASK		(0x7ff << 0)

#define DQEHSC_LSC_GAIN_P2_0		0x0348
#define HSC_LSC_GAIN_P2_1(_v)		(((_v) & 0x7ff) << 16)
#define HSC_LSC_GAIN_P2_1_MASK		(0x7ff << 16)
#define HSC_LSC_GAIN_P2_0(_v)		(((_v) & 0x7ff) << 0)
#define HSC_LSC_GAIN_P2_0_MASK		(0x7ff << 0)

#define DQEHSC_LSC_GAIN_P3_0		0x0360
#define HSC_LSC_GAIN_P3_1(_v)		(((_v) & 0x7ff) << 16)
#define HSC_LSC_GAIN_P3_1_MASK		(0x7ff << 16)
#define HSC_LSC_GAIN_P3_0(_v)		(((_v) & 0x7ff) << 0)
#define HSC_LSC_GAIN_P3_0_MASK		(0x7ff << 0)

#define DQEHSC_LHC_GAIN_P1_0		0x0378
#define HSC_LHC_GAIN_P1_1(_v)		(((_v) & 0x3ff) << 16)
#define HSC_LHC_GAIN_P1_1_MASK		(0x3ff << 16)
#define HSC_LHC_GAIN_P1_0(_v)		(((_v) & 0x3ff) << 0)
#define HSC_LHC_GAIN_P1_0_MASK		(0x3ff << 0)

#define DQEHSC_LHC_GAIN_P2_0		0x0390
#define HSC_LHC_GAIN_P2_1(_v)		(((_v) & 0x3ff) << 16)
#define HSC_LHC_GAIN_P2_1_MASK		(0x3ff << 16)
#define HSC_LHC_GAIN_P2_0(_v)		(((_v) & 0x3ff) << 0)
#define HSC_LHC_GAIN_P2_0_MASK		(0x3ff << 0)

#define DQEHSC_LHC_GAIN_P3_0		0x03A8
#define HSC_LHC_GAIN_P3_1(_v)		(((_v) & 0x3ff) << 16)
#define HSC_LHC_GAIN_P3_1_MASK		(0x3ff << 16)
#define HSC_LHC_GAIN_P3_0(_v)		(((_v) & 0x3ff) << 0)
#define HSC_LHC_GAIN_P3_0_MASK		(0x3ff << 0)

#define DQEHSC_LBC_GAIN_P1_0		0x03C0
#define HSC_LBC_GAIN_P1_1(_v)		(((_v) & 0x7ff) << 16)
#define HSC_LBC_GAIN_P1_1_MASK		(0x7ff << 16)
#define HSC_LBC_GAIN_P1_0(_v)		(((_v) & 0x7ff) << 0)
#define HSC_LBC_GAIN_P1_0_MASK		(0x7ff << 0)

#define DQEHSC_LBC_GAIN_P2_0		0x03D8
#define HSC_LBC_GAIN_P2_1(_v)		(((_v) & 0x7ff) << 16)
#define HSC_LBC_GAIN_P2_1_MASK		(0x7ff << 16)
#define HSC_LBC_GAIN_P2_0(_v)		(((_v) & 0x7ff) << 0)
#define HSC_LBC_GAIN_P2_0_MASK		(0x7ff << 0)

#define DQEHSC_LBC_GAIN_P3_0		0x03F0
#define HSC_LBC_GAIN_P3_1(_v)		(((_v) & 0x7ff) << 16)
#define HSC_LBC_GAIN_P3_1_MASK		(0x7ff << 16)
#define HSC_LBC_GAIN_P3_0(_v)		(((_v) & 0x7ff) << 0)
#define HSC_LBC_GAIN_P3_0_MASK		(0x7ff << 0)

#define DQEHSC_POLY0			0x0408
#define HSC_POLY_CURVE_2(_v)		(((_v) & 0x3ff) << 16)
#define HSC_POLY_CURVE_2_MASK		(0x3ff << 16)
#define HSC_POLY_CURVE_1(_v)		(((_v) & 0x3ff) << 0)
#define HSC_POLY_CURVE_1_MASK		(0x3ff << 0)

#define DQEHSCLUT_MAX			(69)
#define DQEHSCLUT_BASE			0x0304

/*APS_SET*/
#define DQEAPS_PARTIAL_CON		0x0500

#define DQEAPS_FULL_IMG_SIZESET		0x0504
#define DQEAPS_FULL_IMG_VSIZE_F(_v)	((_v) << 16)
#define DQEAPS_FULL_IMG_VSIZE_MASK	(0x1fff << 16)
#define DQEAPS_FULL_IMG_VSIZE_GET(_v)	(((_v) >> 16) & 0x1fff)
#define DQEAPS_FULL_IMG_HSIZE_F(_v)	((_v) << 0)
#define DQEAPS_FULL_IMG_HSIZE_MASK	(0x1fff << 0)
#define DQEAPS_FULL_IMG_HSIZE_GET(_v)	(((_v) >> 0) & 0x1fff)

#define DQEAPS_FULL_PXL_NUM		0x0508
#define DQEAPS_FULL_PXL_NUM_MASK	(0x03ffffff << 0)
#define DQEAPS_FULL_PXL_NUM_GET(_v)	(((_v) >> 0) & 0x03ffffff)

#define DQEAPS_PARTIAL_ROI_UP_LEFT_POS	0x050C
#define DQEAPS_PARTIAL_IBSI_01_00	0x0510
#define DQEAPS_PARTIAL_IBSI_11_10	0x0514

#define DQE_LPD_DATA_CONTROL		0x0600
#define DQE_LPD_WR_DIR(_v)		(((_v) & 0x1) << 31)
#define DQE_LPD_WR_DIR_MASK		(0x1 << 31)
#define DQE_LPD_ADDR(_v)		(((_v) & 0x7f) << 24)
#define DQE_LPD_ADDR_MASK		(0x7f << 24)
#define DQE_LPD_DATA(_v)		(((_v) & 0xfffff) << 0)
#define DQE_LPD_DATA_MASK		(0xfffff << 0)
#define DQE_LPD_DATA_ALL_MASK		(0xffffffff << 0)

#define SHADOW_DQE_OFFSET		0x9000

#endif
