#ifndef _REGS_DPU_H_
#define _REGS_DPU_H_

#define SHADOW_DQE_OFFSET		0x0200

/* DQE_TOP */
#define DQE0_TOP_CORE_SECURITY          (0x0000)
#define DQE_CORE_SECURITY               (0x1 << 0)

#define DQE0_TOP_IMG_SIZE               (0x0004)
#define DQE_IMG_VSIZE(_v)               ((_v) << 16)
#define DQE_IMG_VSIZE_MASK              (0x3FFF << 16)
#define DQE_IMG_HSIZE(_v)               ((_v) << 0)
#define DQE_IMG_HSIZE_MASK              (0x3FFF << 0)

#define DQE0_TOP_FRM_SIZE               (0x0008)
#define DQE_FULL_IMG_VSIZE(_v)          ((_v) << 16)
#define DQE_FULL_IMG_VSIZE_MASK         (0x3FFF << 16)
#define DQE_FULL_IMG_HSIZE(_v)          ((_v) << 0)
#define DQE_FULL_IMG_HSIZE_MASK         (0x3FFF << 0)

#define DQE0_TOP_FRM_PXL_NUM            (0x000C)
#define DQE_FULL_PXL_NUM(_v)            ((_v) << 0)
#define DQE_FULL_PXL_NUM_MASK           (0xFFFFFFF << 0)

#define DQE0_TOP_PARTIAL_START          (0x0010)
#define DQE_PARTIAL_START_Y(_v)         ((_v) << 16)
#define DQE_PARTIAL_START_Y_MASK        (0x3FFF << 16)
#define DQE_PARTIAL_START_X(_v)         ((_v) << 0)
#define DQE_PARTIAL_START_X_MASK        (0x3FFF << 0)

#define DQE0_TOP_PARTIAL_CON            (0x0014)
#define DQE_PARTIAL_SAME(_v)            ((_v) << 2)
#define DQE_PARTIAL_SAME_MASK           (0x1 << 2)
#define DQE_PARTIAL_UPDATE_EN(_v)       ((_v) << 0)
#define DQE_PARTIAL_UPDATE_EN_MASK      (0x1 << 0)

/*----------------------[TOP_LPD_MODE]---------------------------------------*/

#define DQE0_TOP_LPD_MODE_CONTROL	(0x0018)
#define DQE_LPD_MODE_EXIT		(0x1 << 0)

#define DQE0_TOP_LPD_ATC_CON		(0x001C)
#define LPD_ATC_PU_ON			(0x1 << 5)
#define LPD_ATC_FRAME_CNT(_v)		((_v) << 3)
#define LPD_ATC_FRAME_CNT_MASK		(0x3 << 3)
#define LPD_ATC_DIMMING_PROG		(0x1 << 2)
#define LPD_ATC_FRAME_STATE(_v)		((_v) << 0)
#define LPD_ATC_FRAME_STATE_MASK	(0x3 << 0)

#define DQE0_TOP_LPD_ATC_TDR_0		(0x0020)
#define LPD_ATC_TDR_MIN(_v)		((_v) << 10)
#define LPD_ATC_TDR_MIN_MASK		(0x3FF << 10)
#define LPD_ATC_TDR_MAX(_v)		((_v) << 0)
#define LPD_ATC_TDR_MAX_MASK		(0x3FF << 0)

#define DQE0_TOP_LPD_ATC_TDR_1		(0x0024)
#define LPD_ATC_TDR_MIN_D1(_v)		((_v) << 10)
#define LPD_ATC_TDR_MIN_D1_MASK		(0x3FF << 10)
#define LPD_ATC_TDR_MAX_D1(_v)		((_v) << 0)
#define LPD_ATC_TDR_MAX_D1_MASK		(0x3FF << 0)

#define DQE0_TOP_LPD_SCALE(_n)		(0x0028 + ((_n) * 0x4))
#define DQE0_TOP_LPD_SCALE0		(0x0028)
#define DQE0_TOP_LPD_SCALE1(_n)		(0x002C + ((_n) * 0x4))
#define DQE0_TOP_LPD_SCALE2(_n)		(0x0040 + ((_n) * 0x4))
#define DQE0_TOP_LPD_SCALE0_D1		(0x007C)
#define DQE0_TOP_LPD_SCALE1_D1(_n)	(0x0080 + ((_n) * 0x4))
#define DQE0_TOP_LPD_SCALE2_D1(_n)	(0x0094 + ((_n) * 0x4))

#define DQE0_TOP_LPD_HSC		(0x00D0)
#define LPD_HSC_PU_ON			(0x1 << 22)
#define LPD_HSC_FRAME_STATE(_v)		((_v) << 20)
#define LPD_HSC_FRAME_STATE_MASK	(0x3 << 20)
#define LPD_HSC_AVG_UPDATE(_v)		((_v) << 10)
#define LPD_HSC_AVG_UPDATE_MASK		(0x3FF << 10)
#define LPD_HSC_AVG_NOUPDATE(_v)	((_v) << 0)
#define LPD_HSC_AVG_NOUPDATE_MASK	(0x3FF << 0)

/*----------------------[VERSION]---------------------------------------------*/

#define DQE0_TOP_VER			(0x00FC)
#define TOP_VER				(0x05000000)
#define TOP_VER_GET(_v)			(((_v) >> 0) & 0xFFFFFFFF)

/*----------------------[GAMMA_MATRIX]---------------------------------------*/

#define DQE0_GAMMA_MATRIX_CON           (0x1400)
#define GAMMA_MATRIX_EN(_v)             (((_v) & 0x1) << 0)
#define GAMMA_MATRIX_EN_MASK            (0x1 << 0)

#define DQE0_GAMMA_MATRIX_LUT(_n)       (0x1400 + ((_n) * 0x4))
#define DQE0_GAMMA_MATRIX_COEFF0        (0x1404)
#define DQE0_GAMMA_MATRIX_COEFF1        (0x1408)
#define DQE0_GAMMA_MATRIX_COEFF2        (0x140C)
#define DQE0_GAMMA_MATRIX_COEFF3        (0x1410)
#define DQE0_GAMMA_MATRIX_COEFF4        (0x1414)

#define GAMMA_MATRIX_COEFF_H(_v)        (((_v) & 0x3FFF) << 16)
#define GAMMA_MATRIX_COEFF_H_MASK       (0x3FFF << 16)
#define GAMMA_MATRIX_COEFF_L(_v)        (((_v) & 0x3FFF) << 0)
#define GAMMA_MATRIX_COEFF_L_MASK       (0x3FFF << 0)

#define DQE0_GAMMA_MATRIX_OFFSET0       (0x1418)
#define GAMMA_MATRIX_OFFSET_1(_v)       (((_v) & 0xFFF) << 16)
#define GAMMA_MATRIX_OFFSET_1_MASK      (0xFFF << 16)
#define GAMMA_MATRIX_OFFSET_0(_v)       (((_v) & 0xFFF) << 0)
#define GAMMA_MATRIX_OFFSET_0_MASK      (0xFFF << 0)

#define DQE0_GAMMA_MATRIX_OFFSET1       (0x141C)
#define GAMMA_MATRIX_OFFSET_2(_v)       (((_v) & 0xFFF) << 0)
#define GAMMA_MATRIX_OFFSET_2_MASK      (0xFFF << 0)

#define DQE_GAMMA_MATRIX_REG_MAX	(8)

/*----------------------[DEGAMMA_CON]----------------------------------------*/

#define DQE0_DEGAMMA_CON                (0x1800)
#define DEGAMMA_EN(_v)                  (((_v) & 0x1) << 0)
#define DEGAMMA_EN_MASK                 (0x1 << 0)

#define DQE0_DEGAMMALUT(_n)             (0x1800 + ((_n) * 0x4))

#define DEGAMMA_LUT_H(_v)               (((_v) & 0x1FFF) << 16)
#define DEGAMMA_LUT_H_MASK              (0x1FFF << 16)
#define DEGAMMA_LUT_L(_v)               (((_v) & 0x1FFF) << 0)
#define DEGAMMA_LUT_L_MASK              (0x1FFF << 0)
#define DEGAMMA_LUT(_x, _v)             ((_v) << (0 + (16 * ((_x) & 0x1))))
#define DEGAMMA_LUT_MASK(_x)            (0x1FFF << (0 + (16 * ((_x) & 0x1))))

#define DQE_DEGAMMA_REG_MAX		(34)
#define DQE_DEGAMMA_LUT_MAX		(66)

/*----------------------[CGC]------------------------------------------------*/

#define DQE0_CGC_CON			(0x2000)
#define CGC_LUT_READ_SHADOW(_v)		((_v) << 2)
#define CGC_LUT_READ_SHADOW_MASK	(0x1 << 2)
#define CGC_EN(_v)			(((_v) & 0x1) << 0)
#define CGC_EN_MASK			(0x1 << 0)

#define DQE0_CGC_MC_R(_n)		(0x2004 + ((_n) * 0x4))
#define DQE0_CGC_MC_R0			(0x2004)
#define DQE0_CGC_MC_R1			(0x2008)
#define DQE0_CGC_MC_R2			(0x200C)

#define CGC_MC_GAIN_R(_v)		((_v) << 16)
#define CGC_MC_GAIN_R_MASK		(0xFFF << 16)
#define CGC_MC_INVERSE_R(_v)		((_v) << 1)
#define CGC_MC_INVERSE_R_MASK		(0x1 << 1)
#define CGC_MC_ON_R(_v)			((_v) << 0)
#define CGC_MC_ON_R_MASK		(0x1 << 0)

/*----------------------[REGAMMA_CON]----------------------------------------*/

#define DQE0_REGAMMA_CON                (0x2400)
#define REGAMMA_EN(_v)                  (((_v) & 0x1) << 0)
#define REGAMMA_EN_MASK                 (0x1 << 0)

#define DQE0_REGAMMALUT(_n)             (0x2400 + ((_n) * 0x4))
#define DQE0_REGAMMALUT_R(_n)           (0x2404 + ((_n) * 0x4))
#define DQE0_REGAMMALUT_G(_n)           (0x2488 + ((_n) * 0x4))
#define DQE0_REGAMMALUT_B(_n)           (0x250C + ((_n) * 0x4))

#define REGAMMA_LUT_H(_v)               (((_v) & 0x1FFF) << 16)
#define REGAMMA_LUT_H_MASK              (0x1FFF << 16)
#define REGAMMA_LUT_L(_v)               (((_v) & 0x1FFF) << 0)
#define REGAMMA_LUT_L_MASK              (0x1FFF << 0)
#define REGAMMA_LUT(_x, _v)             ((_v) << (0 + (16 * ((_x) & 0x1))))
#define REGAMMA_LUT_MASK(_x)            (0x1FFF << (0 + (16 * ((_x) & 0x1))))

#define DQE_REGAMMA_LUT_MAX		(196)
#define DQE_REGAMMA_REG_MAX		(100)

/*----------------------[CGC_DITHER]-----------------------------------------*/

#define DQE0_CGC_DITHER                 (0x2800)
#define DQE0_DISP_DITHER                (0x2C00)
#define DITHER_TABLE_SEL_B              (0x1 << 7)
#define DITHER_TABLE_SEL_G              (0x1 << 6)
#define DITHER_TABLE_SEL_R              (0x1 << 5)
#define DITHER_TABLE_SEL(_v, _n)        ((_v) << (5 + (_n)))
#define DITHER_TABLE_SEL_MASK(_n)       (0x1 << (5 + (_n)))
#define DITHER_FRAME_OFFSET(_v)         ((_v) << 3)
#define DITHER_FRAME_OFFSET_MASK        (0x3 << 3)
#define DITHER_FRAME_CON(_v)            ((_v) << 2)
#define DITHER_FRAME_CON_MASK           (0x1 << 2)
#define DITHER_MODE(_v)                 ((_v) << 1)
#define DITHER_MODE_MASK                (0x1 << 1)
#define DITHER_EN(_v)                   ((_v) << 0)
#define DITHER_EN_MASK                  (0x1 << 0)

/*----------------------[CGC_LUT]-----------------------------------------*/

#define CGC_LUT(_n)			(0x4000 + ((_n) * 0x4))
#define CGC_LUT_R(_n)			(0x4000 + ((_n) * 0x4))
#define CGC_LUT_G(_n)			(0x8000 + ((_n) * 0x4))
#define CGC_LUT_B(_n)			(0xC000 + ((_n) * 0x4))

#define CGC_LUT_H(_v)			((_v) << 16)
#define CGC_LUT_H_MASK			(0x1FFF << 16)
#define CGC_LUT_L(_v)			((_v) << 0)
#define CGC_LUT_L_MASK			(0x1FFF << 0)
#define CGC_LUT_LH(_x, _v)		((_v) << (0 + (16 * ((_x) & 0x1))))
#define CGC_LUT_LH_MASK(_x)		(0x1FFF << (0 + (16 * ((_x) & 0x1))))

#define DQE_CGC_REG_MAX			(2457)
#define DQE_CGC_LUT_MAX			(4914)
#define DQE_CGC_CON_REG_MAX			(4)

#endif
