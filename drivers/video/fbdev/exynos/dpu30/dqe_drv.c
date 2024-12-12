/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung EXYNOS SoC series DQE driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/console.h>

#include "dqe.h"
#include "decon.h"
#if defined(CONFIG_SOC_EXYNOS2100)
#include "./cal_2100/regs-dqe.h"
#endif

struct dqe_device *dqe_drvdata;
struct class *dqe_class;

int gamma_matrix[17];
u32 degamma_lut[DQE_DEGAMMA_LUT_MAX];
u32 cgc17_lut[3][DQE_CGC_LUT_MAX];
u32 regamma_lut[DQE_REGAMMA_LUT_MAX];

u32 cgc17_encoded[3][17][17][5];
int cgc17_enc_rgb;
int cgc17_enc_idx;
u32 cgc17_con[17];

char dqe_str_result[1024];

int dqe_log_level = 6;
module_param(dqe_log_level, int, 0644);

static void dqe_dump(void)
{
	struct decon_device *decon = get_decon_drvdata(0);

	if (IS_DQE_OFF_STATE(decon)) {
		decon_info("%s: DECON is disabled, state(%d)\n",
				__func__, (decon) ? (decon->state) : -1);
		return;
	}

	__dqe_dump(decon->id, decon->res.dqe_regs);
}

static void dqe_load_context(void)
{
	int i;
	struct dqe_device *dqe = dqe_drvdata;

	dqe_info("%s\n", __func__);

	for (i = 0; i < DQE_GAMMA_MATRIX_REG_MAX; i++) {
		dqe->ctx.gamma_matrix[i].addr = DQE0_GAMMA_MATRIX_LUT(i);
		dqe->ctx.gamma_matrix[i].val = dqe_read(dqe->ctx.gamma_matrix[i].addr);
	}

	for (i = 0; i < DQE_DEGAMMA_REG_MAX; i++) {
		dqe->ctx.degamma_lut[i].addr = DQE0_DEGAMMALUT(i);
		dqe->ctx.degamma_lut[i].val = dqe_read(dqe->ctx.degamma_lut[i].addr);
	}

	for (i = 0; i < (DQE_CGC_CON_REG_MAX - 1); i++) {
		dqe->ctx.cgc_con[i].addr = DQE0_CGC_MC_R(i);
		dqe->ctx.cgc_con[i].val = dqe_read(dqe->ctx.cgc_con[i].addr);
	}
	dqe->ctx.cgc_con[DQE_CGC_CON_REG_MAX - 1].addr = DQE0_CGC_DITHER;
	dqe->ctx.cgc_con[DQE_CGC_CON_REG_MAX - 1].val = dqe_read(DQE0_CGC_DITHER);

	for (i = 0; i < DQE_CGC_REG_MAX; i++) {
		/* RED */
		dqe->ctx.cgc_lut[0][i].addr = CGC_LUT_R(i);
		dqe->ctx.cgc_lut[0][i].val = dqe_read(dqe->ctx.cgc_lut[0][i].addr);

		/* GREEN */
		dqe->ctx.cgc_lut[1][i].addr = CGC_LUT_G(i);
		dqe->ctx.cgc_lut[1][i].val = dqe_read(dqe->ctx.cgc_lut[1][i].addr);

		/* BLUE */
		dqe->ctx.cgc_lut[2][i].addr = CGC_LUT_B(i);
		dqe->ctx.cgc_lut[2][i].val = dqe_read(dqe->ctx.cgc_lut[2][i].addr);
	}

	for (i = 0; i < DQE_REGAMMA_REG_MAX; i++) {
		dqe->ctx.regamma_lut[i].addr = DQE0_REGAMMALUT(i);
		dqe->ctx.regamma_lut[i].val = dqe_read(dqe->ctx.regamma_lut[i].addr);
	}

	for (i = 0; i < DQE_REGAMMA_REG_MAX; i++) {
		dqe_dbg("0x%04x %08x",
			dqe->ctx.regamma_lut[i].addr, dqe->ctx.regamma_lut[i].val);
	}

	for (i = 0; i < DQE_DEGAMMA_REG_MAX; i++) {
		dqe_dbg("0x%04x %08x",
			dqe->ctx.degamma_lut[i].addr, dqe->ctx.degamma_lut[i].val);
	}
}

static void dqe_init_context(void)
{
	int i, j, k;
	struct dqe_device *dqe = dqe_drvdata;

	dqe_info("%s\n", __func__);

	dqe->ctx.gamma_matrix_on = 0;
	dqe->ctx.degamma_on = 0;
	dqe->ctx.cgc_on = 0;
	dqe->ctx.regamma_on = 0;

	dqe->ctx.need_udpate = true;

	degamma_lut[0] = 0;
	for (i = 1; i < DQE_DEGAMMA_LUT_MAX; i++)
		degamma_lut[i] = (i - 1) * 16;

	regamma_lut[0] = 0;
	for (i = 1; i < DQE_REGAMMA_LUT_MAX; i++)
		regamma_lut[i] = ((i - 1) % 65) * 16;

	for (i = 0; i < 17; i++)
		for (j = 0; j < 17; j++)
			for (k = 0; k < 17; k++) {
				int pos = i * 17 * 17 + j * 17 + k;

				cgc17_lut[0][pos] = i * 256;
				cgc17_lut[1][pos] = j * 256;
				cgc17_lut[2][pos] = k * 256;
	}

	for (i = 0; i < 17; i++)
		cgc17_con[i] = 0;
}

int dqe_save_context(void)
{
	return 0;
}

static int dqe_restore_context(void)
{
	int i;
	struct dqe_device *dqe = dqe_drvdata;
	struct decon_device *decon = get_decon_drvdata(0);

	dqe_dbg("%s\n", __func__);

	mutex_lock(&decon->up.lock);

	/* DQE_GAMMA_MATRIX */
	if (dqe->ctx.gamma_matrix_on) {
		for (i = 1; i < DQE_GAMMA_MATRIX_REG_MAX; i++)
			dqe_write(dqe->ctx.gamma_matrix[i].addr,
					dqe->ctx.gamma_matrix[i].val);
		dqe_reg_set_gamma_matrix_on(1);
	} else
		dqe_reg_set_gamma_matrix_on(0);

	/* DQE_DEGAMMA */
	if (dqe->ctx.degamma_on) {
		for (i = 1; i < DQE_DEGAMMA_REG_MAX; i++)
			dqe_write(dqe->ctx.degamma_lut[i].addr,
					dqe->ctx.degamma_lut[i].val);
		dqe_reg_set_degamma_on(1);
	} else
		dqe_reg_set_degamma_on(0);

	/* DQE_CGC & DQE_CGC_LUT */
	if (dqe->ctx.cgc_on) {
		if (!dqe->ctx.cgc_update[decon->dqe_cgc_idx]) {
			for (i = 0; i < DQE_CGC_REG_MAX; i++) {
				/* RED */
				writel_relaxed(dqe->ctx.cgc_lut[0][i].val,
						decon->res.dqe_regs + dqe->ctx.cgc_lut[0][i].addr);
				/* GREEN */
				writel_relaxed(dqe->ctx.cgc_lut[1][i].val,
						decon->res.dqe_regs + dqe->ctx.cgc_lut[1][i].addr);
				/* BLUE */
				writel_relaxed(dqe->ctx.cgc_lut[2][i].val,
						decon->res.dqe_regs + dqe->ctx.cgc_lut[2][i].addr);
			}

			for (i = 0; i < DQE_CGC_CON_REG_MAX; i++)
				dqe_write(dqe->ctx.cgc_con[i].addr,
						dqe->ctx.cgc_con[i].val);

			dqe_reg_set_cgc_on(1);

			dqe->ctx.cgc_update[decon->dqe_cgc_idx] = true;
			dqe_dbg("[%s] CGC SRAM Area %d is set.\n", __func__,
					decon->dqe_cgc_idx);
			dqe_dbg("[%s] CGC SRAM Area info - A : %d, B : %d\n", __func__,
					dqe->ctx.cgc_update[0], dqe->ctx.cgc_update[1]);
		} else {
			dqe_dbg("[%s] The CGC SRAM Area is already occupied\n", __func__);
		}
	} else
		dqe_reg_set_cgc_on(0);

	/* DQE_REGAMMA */
	if (dqe->ctx.regamma_on) {
		for (i = 1; i < DQE_REGAMMA_REG_MAX; i++)
			dqe_write(dqe->ctx.regamma_lut[i].addr,
					dqe->ctx.regamma_lut[i].val);
		dqe_reg_set_regamma_on(1);
	} else
		dqe_reg_set_regamma_on(0);

	dqe->ctx.need_udpate = false;

	mutex_unlock(&decon->up.lock);

	return 0;
}

static void decon_dqe_update_context(struct decon_device *decon)
{
	struct dqe_device *dqe = dqe_drvdata;

	if (IS_DQE_OFF_STATE(decon)) {
		dqe_err("decon is not enabled!(%d)\n", (decon) ? (decon->state) : -1);
		return;
	}

	if (IS_DQE_DISABLE(dqe)) {
		dqe_err("dqe is not enabled!(%d)\n", (dqe) ? (dqe->state) : -1);
		return;
	}

	decon_hiber_block_exit(decon);

	dqe_restore_context();

	if (decon->lcd_info->mode == DECON_VIDEO_MODE)
		decon_reg_update_req_dqe(decon->id);

	decon_hiber_unblock(decon);
}

static void dqe_reset_cgc_update(void)
{
	struct dqe_device *dqe = dqe_drvdata;

	dqe_dbg("%s +\n", __func__);

	dqe->ctx.cgc_update[0] = dqe->ctx.cgc_update[1] = false;
}

/*
 * If Gamma Matrix is transferred with this pattern,
 *
 * |A B C D|
 * |E F G H|
 * |I J K L|
 * |M N O P|
 *
 * Coeff and Offset will be applied as follows.
 *
 * |Rout|   |A E I||Rin|   |M|
 * |Gout| = |B F J||Gin| + |N|
 * |Bout|   |C G K||Bin|   |O|
 *
 */
static void dqe_gamma_matrix_set(void)
{
	struct dqe_device *dqe = dqe_drvdata;

	dqe_dbg("%s\n", __func__);

	dqe->ctx.gamma_matrix[0].val = GAMMA_MATRIX_EN(gamma_matrix[0]);

	/* GAMMA_MATRIX_COEFF */
	dqe->ctx.gamma_matrix[1].val = (
		GAMMA_MATRIX_COEFF_H(gamma_matrix[5]) | GAMMA_MATRIX_COEFF_L(gamma_matrix[1])
	);

	dqe->ctx.gamma_matrix[2].val = (
		GAMMA_MATRIX_COEFF_H(gamma_matrix[2]) | GAMMA_MATRIX_COEFF_L(gamma_matrix[9])
	);

	dqe->ctx.gamma_matrix[3].val = (
		GAMMA_MATRIX_COEFF_H(gamma_matrix[10]) | GAMMA_MATRIX_COEFF_L(gamma_matrix[6])
	);

	dqe->ctx.gamma_matrix[4].val = (
		GAMMA_MATRIX_COEFF_H(gamma_matrix[7]) | GAMMA_MATRIX_COEFF_L(gamma_matrix[3])
	);

	dqe->ctx.gamma_matrix[5].val = GAMMA_MATRIX_COEFF_L(gamma_matrix[11]);

	/* GAMMA_MATRIX_OFFSET */
	dqe->ctx.gamma_matrix[6].val = (
		GAMMA_MATRIX_OFFSET_1(gamma_matrix[14]) | GAMMA_MATRIX_OFFSET_0(gamma_matrix[13])
	);

	dqe->ctx.gamma_matrix[7].val = GAMMA_MATRIX_OFFSET_2(gamma_matrix[15]);
}

static void dqe_degamma_lut_set(void)
{
	int i, j;
	struct dqe_device *dqe = dqe_drvdata;

	dqe->ctx.degamma_lut[0].val = DEGAMMA_EN(degamma_lut[0]);

	for (i = 1, j = 1; i < DQE_DEGAMMA_REG_MAX; i++) {
		if (j % 65) {
			dqe->ctx.degamma_lut[i].val = (
				DEGAMMA_LUT_H(degamma_lut[j + 1]) | DEGAMMA_LUT_L(degamma_lut[j])
			);
			j += 2;
		} else {
			dqe->ctx.degamma_lut[i].val = (
				DEGAMMA_LUT_L(degamma_lut[j])
			);
			j += 1;
		}
	}
}

static void dqe_regamma_lut_set(void)
{
	int i, j;
	struct dqe_device *dqe = dqe_drvdata;

	dqe->ctx.regamma_lut[0].val = REGAMMA_EN(regamma_lut[0]);

	for (i = 1, j = 1; i < DQE_REGAMMA_REG_MAX; i++) {
		if (j % 65) {
			dqe->ctx.regamma_lut[i].val = (
				REGAMMA_LUT_H(regamma_lut[j + 1]) | REGAMMA_LUT_L(regamma_lut[j])
			);
			j += 2;
		} else {
			dqe->ctx.regamma_lut[i].val = (
				REGAMMA_LUT_L(regamma_lut[j])
			);
			j += 1;
		}
	}
}

static void dqe_cgc_lut_set(void)
{
	int i;
	u32 val;
	struct dqe_device *dqe = dqe_drvdata;

	dqe_dbg("%s\n",__func__);

	dqe_reset_cgc_update();

	for (i = 0; i < DQE_CGC_REG_MAX; i++) {
		/* RED */
		val = CGC_LUT_H(cgc17_lut[0][2 * i + 1]) |
				CGC_LUT_L(cgc17_lut[0][2 * i + 0]);
		dqe->ctx.cgc_lut[0][i].val = val;

		/* GREEN */
		val = CGC_LUT_H(cgc17_lut[1][2 * i + 1]) |
				CGC_LUT_L(cgc17_lut[1][2 * i + 0]);
		dqe->ctx.cgc_lut[1][i].val = val;

		/* BLUE */
		val = CGC_LUT_H(cgc17_lut[2][2 * i + 1]) |
				CGC_LUT_L(cgc17_lut[2][2 * i + 0]);
		dqe->ctx.cgc_lut[2][i].val = val;
	}

	/* DQE0_CGC_MC_R0*/
	dqe->ctx.cgc_con[0].val = (
		CGC_MC_ON_R(cgc17_con[1]) | CGC_MC_INVERSE_R(cgc17_con[2]) |
		CGC_MC_GAIN_R(cgc17_con[3])
	);

	/* DQE0_CGC_MC_R1*/
	dqe->ctx.cgc_con[1].val = (
		CGC_MC_ON_R(cgc17_con[4]) | CGC_MC_INVERSE_R(cgc17_con[5]) |
		CGC_MC_GAIN_R(cgc17_con[6])
	);

	/* DQE0_CGC_MC_R2*/
	dqe->ctx.cgc_con[2].val = (
		CGC_MC_ON_R(cgc17_con[7]) | CGC_MC_INVERSE_R(cgc17_con[8]) |
		CGC_MC_GAIN_R(cgc17_con[9])
	);

	/* DQE0_CGC_DITHER*/
	dqe->ctx.cgc_con[3].val = (
		DITHER_EN(cgc17_con[10]) | DITHER_MODE(cgc17_con[11]) |
		DITHER_FRAME_CON(cgc17_con[12]) | DITHER_FRAME_OFFSET(cgc17_con[13]) |
		DITHER_TABLE_SEL(cgc17_con[14], 0) | DITHER_TABLE_SEL(cgc17_con[15], 1) |
		DITHER_TABLE_SEL(cgc17_con[16], 2)
	);
}

static void dqe_cgc17_dpcm_dec(int *encoded, int *ret)
{
	int ii, idx;
	int offset, maxPos;
	int dpcm[16] = {0,};
	int pos = 0;
	int mode = (encoded[0] >> 30) & 0x3;
	int shift = 0;
	int min_pos = 0;

	for (ii = 0; ii < 17; ii++)
		ret[ii] = -1;

	if (mode == 0) {
		ret[0] = (encoded[0] >> 17) & 0x1fff;
		maxPos = encoded[0] & 0xf;
		ret[maxPos + 1] = (encoded[0] >> 4) & 0x1fff;

		for (ii = 1; ii < 5; ii++) {
			dpcm[pos++] = ((encoded[ii] >> 24) & 0xff);
			dpcm[pos++] = ((encoded[ii] >> 16) & 0xff);
			dpcm[pos++] = ((encoded[ii] >> 8) & 0xff);
			dpcm[pos++] = ((encoded[ii]) & 0xff);
		}

		offset = dpcm[maxPos];
		if (offset > 128)
			offset -= 256;

		for (idx = 1, pos = 0; idx < 17; idx++, pos++) {
			if (ret[idx] == -1)
				ret[idx] = ret[idx - 1] + dpcm[pos] + offset;
		}
	} else {
		shift = (mode == 2) ? 0 : 1;
		min_pos = (encoded[0] >> 12) & 0x1f;
		ret[min_pos] = (encoded[0] >> 17) & 0x1fff;

		for (ii = 1; ii < 5; ii++) {
			dpcm[pos++] = ((encoded[ii] >> 24) & 0xff);
			dpcm[pos++] = ((encoded[ii] >> 16) & 0xff);
			dpcm[pos++] = ((encoded[ii] >> 8) & 0xff);
			dpcm[pos++] = ((encoded[ii]) & 0xff);
		}

		pos = 0;
		for (ii = 0; ii < 17; ii++) {
			if (ii != min_pos)
				ret[ii] = ret[min_pos] + (dpcm[pos++] << shift);
		}
	}
}

static void dqe_cgc17_dpcm_enc(char *enc, int rgb, int idx)
{
	int i, j;
	char str[41] = {0,};
	char hex[9] = {0,};
	char *head = NULL;
	char *ptr = NULL;

	head = enc;

	for (i = 0 ; i < 17; i++) {
		strncpy(str, head, 40);

		ptr = str;

		for (j = 0; j < 5; j++) {
			strncpy(hex, ptr, 8);
			cgc17_encoded[rgb][idx][i][j] = (u32)simple_strtoul(hex, NULL, 16);
			ptr += 8;
		}

		head += 40;
	}
}

static void dqe_cgc17_dec_cat(int *decoded)
{
	int i;
	char num[7] = {0,};

	dqe_str_result[0] = '\0';
	for (i = 0; i < 17; i++) {
		snprintf(num, 6, "%4d,", decoded[i]);
		strcat(dqe_str_result, num);
	}
	dqe_str_result[strlen(dqe_str_result)] = '\0';
	dqe_info("%s\n", dqe_str_result);
}

static void dqe_cgc17_dec(int mask, int wr_mode)
{
	int ii;
	int r_pos, g_pos, b_pos;
	int decoded[17] = {0,};

	if (mask & 0x1) {
		for (g_pos = 0; g_pos < 17; g_pos++) {
			for (r_pos = 0; r_pos < 17; r_pos++) {
				dqe_cgc17_dpcm_dec((int *)cgc17_encoded[0][g_pos][r_pos], decoded);

				if (!wr_mode)
					dqe_cgc17_dec_cat(decoded);
				else {
					for (ii = 0; ii < 17; ii++)
						cgc17_lut[0][r_pos * 17 * 17 + g_pos * 17 + ii] = (u32)decoded[ii];
				}
			}
		}
	}

	if (mask & 0x2) {
		for (r_pos = 0; r_pos < 17; r_pos++) {
			for (g_pos = 0; g_pos < 17; g_pos++) {
				dqe_cgc17_dpcm_dec((int *)cgc17_encoded[1][r_pos][g_pos], decoded);

				if (!wr_mode)
					dqe_cgc17_dec_cat(decoded);
				else {
					for (ii = 0; ii < 17; ii++)
						cgc17_lut[1][r_pos * 17 * 17 + g_pos * 17 + ii] = (u32)decoded[ii];
				}
			}
		}
	}

	if (mask & 0x4) {
		for (g_pos = 0; g_pos < 17; g_pos++) {
			for (b_pos = 0; b_pos < 17; b_pos++) {
				dqe_cgc17_dpcm_dec((int *)cgc17_encoded[2][g_pos][b_pos], decoded);

				if (!wr_mode)
					dqe_cgc17_dec_cat(decoded);
				else {
					for (ii = 0; ii < 17; ii++)
						cgc17_lut[2][ii * 17 * 17 + g_pos * 17 + b_pos] = (u32)decoded[ii];
				}
			}
		}
	}
}

static ssize_t decon_dqe_cgc17_idx_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	dqe_info("%s\n", __func__);

	count = snprintf(buf, PAGE_SIZE, "%d %d\n", cgc17_enc_rgb, cgc17_enc_idx);

	return count;
}

static ssize_t decon_dqe_cgc17_idx_store(struct device *dev, struct device_attribute *attr,
		const char *buffer, size_t count)
{
	int ret = 0;
	int val1, val2;
	struct dqe_device *dqe = dev_get_drvdata(dev);

	mutex_lock(&dqe->lock);

	if (count <= 0) {
		dqe_err("cgc17_idx write count error\n");
		ret = -1;
		goto err;
	}

	ret = sscanf(buffer, "%d %d", &val1, &val2);
	if (ret != 2) {
		dqe_err("Failed at sscanf function: %d\n", ret);
		ret = -EINVAL;
		goto err;
	}

	if (val1 < 0 || val1 > 2 || val2 < 0 || val2 > 16) {
		dqe_err("index range error: %d %d\n", val1, val2);
			ret = -EINVAL;
		goto err;
	}

	cgc17_enc_rgb = val1;
	cgc17_enc_idx = val2;

	mutex_unlock(&dqe->lock);

	dqe_dbg("%s: %d %d\n", __func__, cgc17_enc_rgb, cgc17_enc_idx);

	return count;
err:
	mutex_unlock(&dqe->lock);

	dqe_info("%s : err(%d)\n", __func__, ret);

	return ret;
}

static DEVICE_ATTR(cgc17_idx, 0660,
	decon_dqe_cgc17_idx_show,
	decon_dqe_cgc17_idx_store);

static ssize_t decon_dqe_cgc17_enc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i, j;
	ssize_t count = 0;
	char hex[10] = {0,};
	struct dqe_device *dqe = dev_get_drvdata(dev);

	dqe_info("%s\n", __func__);

	mutex_lock(&dqe->lock);

	dqe_str_result[0] = '\0';
	for (i = 0 ; i < 17; i++)
		for (j = 0; j < 5; j++) {
			snprintf(hex, 9, "%08x", cgc17_encoded[cgc17_enc_rgb][cgc17_enc_idx][i][j]);
			strcat(dqe_str_result, hex);
		}
	dqe_str_result[strlen(dqe_str_result)] = '\0';

	mutex_unlock(&dqe->lock);

	count = snprintf(buf, PAGE_SIZE, "%s\n", dqe_str_result);

	return count;
}

static ssize_t decon_dqe_cgc17_enc_store(struct device *dev, struct device_attribute *attr,
		const char *buffer, size_t count)
{
	int ret = 0;
	int len  = 0;
	char *head = NULL;
	struct dqe_device *dqe = dev_get_drvdata(dev);

	dqe_dbg("%s +\n", __func__);

	mutex_lock(&dqe->lock);

	if (count <= 0) {
		dqe_err("cgc17_enc write count error\n");
		ret = -1;
		goto err;
	}

	head = (char *)buffer;
	if  (*head != 0) {
		len = strlen(head);
		dqe_dbg("%s\n", head);

		if (len < 680 || len > 681) {
			dqe_err("strlen error. (%d)\n", len);
			ret = -EINVAL;
			goto err;
		}

		dqe_cgc17_dpcm_enc(head, cgc17_enc_rgb, cgc17_enc_idx);

	} else {
		dqe_err("buffer is null.\n");
		goto err;
	}

	mutex_unlock(&dqe->lock);

	if (cgc17_enc_idx <= 0 || cgc17_enc_idx >= 16)
		dqe_info("%s: %d %d\n", __func__, cgc17_enc_rgb, cgc17_enc_idx);

	return count;
err:
	mutex_unlock(&dqe->lock);

	dqe_info("%s : err(%d) %d %d\n", __func__, ret, cgc17_enc_rgb, cgc17_enc_idx);

	return ret;
}

static DEVICE_ATTR(cgc17_enc, 0660,
	decon_dqe_cgc17_enc_show,
	decon_dqe_cgc17_enc_store);

static ssize_t decon_dqe_cgc17_dec_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int val = 0;
	ssize_t count = 0;

	dqe_info("%s\n", __func__);

	dqe_cgc17_dec(0x7, false);

	count = snprintf(buf, PAGE_SIZE, "dump=%d\n", val);

	return count;
}

static ssize_t decon_dqe_cgc17_dec_store(struct device *dev, struct device_attribute *attr,
		const char *buffer, size_t count)
{
	int ret = 0;
	unsigned int value = 0;
	struct dqe_device *dqe = dev_get_drvdata(dev);

	dqe_info("%s +\n", __func__);

	mutex_lock(&dqe->lock);

	if (count <= 0) {
		dqe_err("cgc17_dec write count error\n");
		ret = -1;
		goto err;
	}

	ret = kstrtouint(buffer, 0, &value);
	if (ret < 0)
		goto err;

	dqe_info("%s: %d\n", __func__, value);

	if (value >= 0x1 && value <= 0x7)
		dqe_cgc17_dec(value, true);
	else {
		ret = -EINVAL;
		goto err;
	}

	mutex_unlock(&dqe->lock);

	dqe_info("%s -\n", __func__);

	return count;
err:
	mutex_unlock(&dqe->lock);

	dqe_info("%s : err(%d)\n", __func__, ret);

		return ret;
}

static DEVICE_ATTR(cgc17_dec, 0660,
	decon_dqe_cgc17_dec_show,
	decon_dqe_cgc17_dec_store);

static ssize_t decon_dqe_cgc17_con_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i;
	ssize_t count = 0;
	int str_len = 0;
	char num[6] = {0,};
	struct dqe_device *dqe = dev_get_drvdata(dev);

	dqe_info("%s\n", __func__);

	mutex_lock(&dqe->lock);

	dqe_str_result[0] = '\0';
	for (i = 0; i < 17; i++) {
		snprintf(num, 6, "%d,", cgc17_con[i]);
		strcat(dqe_str_result, num);
	}
	str_len = strlen(dqe_str_result);
	dqe_str_result[str_len - 1] = '\0';

	mutex_unlock(&dqe->lock);

	count = snprintf(buf, PAGE_SIZE, "%s\n", dqe_str_result);

	return count;
}

static ssize_t decon_dqe_cgc17_con_store(struct device *dev, struct device_attribute *attr,
		const char *buffer, size_t count)
{
	int i, j, k;
	int ret = 0;
	char *head = NULL;
	char *ptr = NULL;
	struct dqe_device *dqe = dev_get_drvdata(dev);
	struct decon_device *decon = get_decon_drvdata(0);
	s32 cgc17_con_signed[17] = {0,};

	dqe_info("%s +\n", __func__);

	mutex_lock(&dqe->lock);

	if (count <= 0) {
		dqe_err("cgc17_con write count error\n");
		ret = -1;
		goto err;
	}

	head = (char *)buffer;
	if  (*head != 0) {
		dqe_dbg("%s\n", head);
		for (i = 0; i < 1; i++) {
			k = 16;
			for (j = 0; j < k; j++) {
				ptr = strchr(head, ',');
				if (ptr == NULL) {
					dqe_err("not found comma.(%d, %d)\n", i, j);
					ret = -EINVAL;
					goto err;
			}
				*ptr = 0;
				ret = kstrtos32(head, 0, &cgc17_con_signed[j]);
				if (ret) {
					dqe_err("strtos32(%d, %d) error.\n", i, j);
					ret = -EINVAL;
					goto err;
				}
				head = ptr + 1;
			}
		}
		k = 0;
		if (*(head + k) == '-')
			k++;
		while (*(head + k) >= '0' && *(head + k) <= '9')
			k++;
		*(head + k) = 0;
		ret = kstrtos32(head, 0, &cgc17_con_signed[j]);
		if (ret) {
			dqe_err("strtos32(%d, %d) error.\n", i-1, j);
			ret = -EINVAL;
			goto err;
		}
	} else {
		dqe_err("buffer is null.\n");
		goto err;
	}

	for (j = 0; j < 17; j++)
		dqe_dbg("%d ", cgc17_con_signed[j]);

	/* signed -> unsigned */
	for (j = 0; j < 17; j++) {
		cgc17_con[j] = (u32)cgc17_con_signed[j];
		dqe_dbg("%04x %d", cgc17_con[j], cgc17_con_signed[j]);
	}

	dqe_cgc_lut_set();

	if (!cgc17_con[0])
		dqe->ctx.cgc_on = 0;
	else {
		dqe->ctx.cgc_on = 1;
		dqe->ctx.degamma_on = 1;
		dqe->ctx.regamma_on = 1;
	}

	dqe->ctx.need_udpate = true;

	decon_dqe_update_context(decon);

	mutex_unlock(&dqe->lock);

	dqe_info("%s -\n", __func__);

	return count;
err:
	mutex_unlock(&dqe->lock);

	dqe_info("%s : err(%d)\n", __func__, ret);

	return ret;
}

static DEVICE_ATTR(cgc17_con, 0660,
	decon_dqe_cgc17_con_show,
	decon_dqe_cgc17_con_store);

static ssize_t decon_dqe_gamma_matrix_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	struct dqe_device *dqe = dev_get_drvdata(dev);
	int i, j, str_len = 0;

	dqe_info("%s\n", __func__);

	mutex_lock(&dqe->lock);

	dqe_str_result[0] = '\0';
	for (i = 0; i < 1; i++) {
		for (j = 0; j < 17; j++) {
			char num[6] = {0,};

			snprintf(num, 6, "%d,", gamma_matrix[j]);
			strcat(dqe_str_result, num);
		}
	}
	str_len = strlen(dqe_str_result);
	dqe_str_result[str_len - 1] = '\0';

	mutex_unlock(&dqe->lock);

	count = snprintf(buf, PAGE_SIZE, "%s\n", dqe_str_result);

	return count;
}

static ssize_t decon_dqe_gamma_matrix_store(struct device *dev, struct device_attribute *attr,
		const char *buffer, size_t count)
{
	int i, j, k;
	int ret = 0;
	char *head = NULL;
	char *ptr = NULL;
	struct dqe_device *dqe = dev_get_drvdata(dev);
	struct decon_device *decon = get_decon_drvdata(0);
	s32 gamma_matrix_signed[17] = {0,};

	dqe_info("%s +\n", __func__);

	mutex_lock(&dqe->lock);

	if (count <= 0) {
		dqe_err("gamma matrix write count error\n");
		ret = -1;
		goto err;
	}

	head = (char *)buffer;
	if  (*head != 0) {
		dqe_dbg("%s\n", head);
		for (i = 0; i < 1; i++) {
			k = 16;
			for (j = 0; j < k; j++) {
				ptr = strchr(head, ',');
				if (ptr == NULL) {
					dqe_err("not found comma.(%d, %d)\n", i, j);
					ret = -EINVAL;
					goto err;
				}
				*ptr = 0;
				ret = kstrtos32(head, 0, &gamma_matrix_signed[j]);
				if (ret) {
					dqe_err("strtou32(%d, %d) error.\n", i, j);
					ret = -EINVAL;
					goto err;
				}
				head = ptr + 1;
			}
		}
		k = 0;
		if (*(head + k) == '-')
			k++;
		while (*(head + k) >= '0' && *(head + k) <= '9')
			k++;
		*(head + k) = 0;
		ret = kstrtou32(head, 0, &gamma_matrix_signed[j]);
		if (ret) {
			dqe_err("strtou32(%d, %d) error.\n", i-1, j);
			ret = -EINVAL;
			goto err;
		}
	} else {
		dqe_err("buffer is null.\n");
		goto err;
	}

	/* signed -> unsigned */
	for (i = 0; i < 17; i++) {
		gamma_matrix[i] = (u32)gamma_matrix_signed[i];
		dqe_dbg("%04x %d", gamma_matrix[i], gamma_matrix_signed[i]);
	}

	/* Coefficient */
	for (i = 0; i < 12; i++)
		gamma_matrix[i + 1] = (gamma_matrix[i + 1] >> 5);
	/* Offeset */
	for (i = 12; i < 16; i++)
		gamma_matrix[i + 1] = (gamma_matrix[i + 1] >> 8);

	for (j = 0; j < 17; j++)
		dqe_dbg("%d ", gamma_matrix[j]);

	dqe_gamma_matrix_set();

	if (!gamma_matrix[0])
		dqe->ctx.gamma_matrix_on = 0;
	else
		dqe->ctx.gamma_matrix_on = 1;

	dqe->ctx.need_udpate = true;

	decon_dqe_update_context(decon);

	mutex_unlock(&dqe->lock);

	dqe_info("%s -\n", __func__);

	return count;
err:
	mutex_unlock(&dqe->lock);

	dqe_info("%s : err(%d)\n", __func__, ret);

	return ret;
}

static DEVICE_ATTR(gamma_matrix, 0660,
		decon_dqe_gamma_matrix_show,
		decon_dqe_gamma_matrix_store);

static ssize_t decon_dqe_gamma_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	struct dqe_device *dqe = dev_get_drvdata(dev);
	int i, str_len = 0;

	dqe_info("%s\n", __func__);

	mutex_lock(&dqe->lock);

	dqe_str_result[0] = '\0';
	for (i = 0; i < 196; i++) {
		char num[6] = {0,};

		snprintf(num, 6, "%d,", regamma_lut[i]);
		strcat(dqe_str_result, num);
	}
	str_len = strlen(dqe_str_result);
	dqe_str_result[str_len - 1] = '\0';

	mutex_unlock(&dqe->lock);

	count = snprintf(buf, PAGE_SIZE, "%s\n", dqe_str_result);

	return count;
}

static ssize_t decon_dqe_gamma_store(struct device *dev, struct device_attribute *attr,
		const char *buffer, size_t count)
{
	int i, j, k;
	int ret = 0;
	char *head = NULL;
	char *ptr = NULL;
	struct dqe_device *dqe = dev_get_drvdata(dev);
	struct decon_device *decon = get_decon_drvdata(0);
	u32 regamma_unsigned[196] = {0,};

	dqe_info("%s +\n", __func__);

	mutex_lock(&dqe->lock);

	if (count <= 0) {
		dqe_err("gamma write count error\n");
		ret = -1;
		goto err;
	}

	head = (char *)buffer;
	if  (*head != 0) {
		dqe_dbg("%s\n", head);
		for (i = 0; i < 1; i++) {
			k = 195;
			for (j = 0; j < k; j++) {
				ptr = strchr(head, ',');
				if (ptr == NULL) {
					dqe_err("not found comma.(%d, %d)\n", i, j);
					ret = -EINVAL;
					goto err;
				}
				*ptr = 0;
				ret = kstrtou32(head, 0, &regamma_unsigned[j]);
				if (ret) {
					dqe_err("strtou32(%d, %d) error.\n", i, j);
					ret = -EINVAL;
					goto err;
				}
				head = ptr + 1;
			}
		}
		k = 0;
		if (*(head + k) == '-')
			k++;
		while (*(head + k) >= '0' && *(head + k) <= '9')
			k++;
		*(head + k) = 0;
		ret = kstrtou32(head, 0, &regamma_unsigned[j]);
		if (ret) {
			dqe_err("strtou32(%d, %d) error.\n", i-1, j);
			ret = -EINVAL;
			goto err;
		}
	} else {
		dqe_err("buffer is null.\n");
		goto err;
	}

	for (i = 0; i < 196; i++) {
		regamma_lut[i] = regamma_unsigned[i];
		dqe_dbg("%d ", regamma_lut[i]);
	}

	dqe_regamma_lut_set();

	if (!regamma_lut[0])
		dqe->ctx.regamma_on = 0;
	else
		dqe->ctx.regamma_on = 1;

	dqe->ctx.need_udpate = true;

	decon_dqe_update_context(decon);

	mutex_unlock(&dqe->lock);

	dqe_info("%s -\n", __func__);

	return count;
err:
	mutex_unlock(&dqe->lock);

	dqe_info("%s : err(%d)\n", __func__, ret);

	return ret;
}

static DEVICE_ATTR(gamma, 0660,
	decon_dqe_gamma_show,
	decon_dqe_gamma_store);

static ssize_t decon_dqe_degamma_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	struct dqe_device *dqe = dev_get_drvdata(dev);
	int i, str_len = 0;

	dqe_info("%s\n", __func__);

	mutex_lock(&dqe->lock);

	dqe_str_result[0] = '\0';
	for (i = 0; i < 66; i++) {
		char num[6] = {0,};

		snprintf(num, 6, "%d,", degamma_lut[i]);
		strcat(dqe_str_result, num);
	}
	str_len = strlen(dqe_str_result);
	dqe_str_result[str_len - 1] = '\0';

	mutex_unlock(&dqe->lock);

	count = snprintf(buf, PAGE_SIZE, "%s\n", dqe_str_result);

	return count;
}

static ssize_t decon_dqe_degamma_store(struct device *dev, struct device_attribute *attr,
		const char *buffer, size_t count)
{
	int i, j, k;
	int ret = 0;
	char *head = NULL;
	char *ptr = NULL;
	struct dqe_device *dqe = dev_get_drvdata(dev);
	struct decon_device *decon = get_decon_drvdata(0);
	u32 degamma_unsigned[66] = {0,};

	dqe_info("%s +\n", __func__);

	mutex_lock(&dqe->lock);

	if (count <= 0) {
		dqe_err("degamma write count error\n");
		ret = -1;
		goto err;
	}

	head = (char *)buffer;
	if  (*head != 0) {
		dqe_dbg("%s\n", head);
		for (i = 0; i < 1; i++) {
			k = 65;
			for (j = 0; j < k; j++) {
				ptr = strchr(head, ',');
				if (ptr == NULL) {
					dqe_err("not found comma.(%d, %d)\n", i, j);
					ret = -EINVAL;
					goto err;
				}
				*ptr = 0;
				ret = kstrtou32(head, 0, &degamma_unsigned[j]);
				if (ret) {
					dqe_err("strtou32(%d, %d) error.\n", i, j);
					ret = -EINVAL;
					goto err;
				}
				head = ptr + 1;
			}
		}
		k = 0;
		if (*(head + k) == '-')
			k++;
		while (*(head + k) >= '0' && *(head + k) <= '9')
			k++;
		*(head + k) = 0;
		ret = kstrtou32(head, 0, &degamma_unsigned[j]);
		if (ret) {
			dqe_err("strtou32(%d, %d) error.\n", i-1, j);
			ret = -EINVAL;
			goto err;
		}
	} else {
		dqe_err("buffer is null.\n");
		goto err;
	}

	for (i = 0; i < 66; i++) {
		degamma_lut[i] = degamma_unsigned[i];
		dqe_dbg("%d ", degamma_lut[i]);
	}

	dqe_degamma_lut_set();

	if (!degamma_lut[0])
		dqe->ctx.degamma_on = 0;
	else
		dqe->ctx.degamma_on = 1;

	dqe->ctx.need_udpate = true;

	decon_dqe_update_context(decon);

	mutex_unlock(&dqe->lock);

	dqe_info("%s -\n", __func__);

	return count;
err:
	mutex_unlock(&dqe->lock);

	dqe_info("%s : err(%d)\n", __func__, ret);

	return ret;
}

static DEVICE_ATTR(degamma, 0660,
	decon_dqe_degamma_show,
	decon_dqe_degamma_store);

static ssize_t decon_dqe_dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int val = 0;
	ssize_t count = 0;
	struct decon_device *decon = get_decon_drvdata(0);

	dqe_info("%s\n", __func__);

	decon_hiber_block_exit(decon);

	dqe_dump();

	decon_hiber_unblock(decon);

	count = snprintf(buf, PAGE_SIZE, "dump = %d\n", val);

	return count;
}

static DEVICE_ATTR(dump, 0440,
	decon_dqe_dump_show,
	NULL);

static struct attribute *dqe_attrs[] = {
	&dev_attr_cgc17_idx.attr,
	&dev_attr_cgc17_enc.attr,
	&dev_attr_cgc17_dec.attr,
	&dev_attr_cgc17_con.attr,
	&dev_attr_gamma_matrix.attr,
	&dev_attr_degamma.attr,
	&dev_attr_gamma.attr,
	&dev_attr_dump.attr,
	NULL,
};
ATTRIBUTE_GROUPS(dqe);

int decon_dqe_get_e_path(struct decon_device *decon, enum dqe_state *state,
					enum decon_enhance_path *e_path)
{
	struct dqe_device *dqe = dqe_drvdata;
	enum decon_data_path d_path = DPATH_DSCENC0_OUTFIFO0_DSIMIF0;

	if (decon->id || !dqe) {
		dqe_err("decon%d doesn't support DQE!\n", decon->id);
		return -EINVAL;
	}

	decon_reg_get_data_path(decon->id, &d_path, e_path);

	*state = dqe->state;

	return 0;
}

int decon_dqe_set_e_path(struct decon_device *decon, enum dqe_state state,
					enum decon_enhance_path val)
{
	struct dqe_device *dqe = dqe_drvdata;
	enum decon_data_path d_path = DPATH_DSCENC0_OUTFIFO0_DSIMIF0;
	enum decon_enhance_path e_path = ENHANCEPATH_ENHANCE_ALL_OFF;

	if (decon->id || !dqe) {
		dqe_err("decon%d doesn't support DQE!\n", decon->id);
		return -EINVAL;
	}

	dqe->state = state;
	decon_reg_get_data_path(decon->id, &d_path, &e_path);
	decon_reg_set_data_path(decon->id, d_path, val);

	return 0;
}

int decon_dqe_set_color_mode(struct decon_color_mode_with_render_intent_info *mode)
{
	int ret = 0;
	struct dqe_device *dqe = dqe_drvdata;
	struct decon_device *decon = get_decon_drvdata(0);

	mutex_lock(&dqe->lock);

	if (IS_DQE_OFF_STATE(decon)) {
		dqe_err("decon is not enabled!(%d)\n", (decon) ? (decon->state) : -1);
		ret = -1;
	}

	dqe_info("%s\n", __func__);

	mutex_unlock(&dqe->lock);

	return ret;
}

int decon_dqe_set_color_transform(struct decon_color_transform_info *transform)
{
	int ret = 0;
	int i;
	struct dqe_device *dqe = dqe_drvdata;
	struct decon_device *decon = get_decon_drvdata(0);

	mutex_lock(&dqe->lock);

	if (IS_DQE_OFF_STATE(decon)) {
		dqe_err("decon is not enabled!(%d)\n", (decon) ? (decon->state) : -1);
		goto err;
	}

	for (i = 0; i < 16; i+=4) {
		dqe_dbg("%6d %6d %6d %6d\n", transform->matrix[i], transform->matrix[i + 1],
				transform->matrix[i + 2], transform->matrix[i + 3]);
	}

	gamma_matrix[0] = 1;

	/* Coefficient */
	for (i = 0; i < 12; i++)
		gamma_matrix[i + 1] = ((transform->matrix[i]) >> 5);
	/* Offset */
	for (i = 12; i < 16; i++)
		gamma_matrix[i + 1] = ((transform->matrix[i]) >> 8);

	dqe_gamma_matrix_set();

	dqe->ctx.gamma_matrix_on = 1;
	dqe->ctx.need_udpate = true;

	decon_dqe_update_context(decon);
err:
	mutex_unlock(&dqe->lock);

	dqe_info("%s : ret=%d n", __func__, ret);

	return ret;
}

void decon_dqe_restore_context(struct decon_device *decon)
{
	if (decon->id)
		return;

	dqe_reset_cgc_update();

#if IS_ENABLED(CONFIG_EXYNOS_BTS) || IS_ENABLED(CONFIG_EXYNOS_BTS_MODULE)
	decon->bts.ops->bts_acquire_dqe_bw(decon);
#endif
	dqe_restore_context();
#if IS_ENABLED(CONFIG_EXYNOS_BTS) || IS_ENABLED(CONFIG_EXYNOS_BTS_MODULE)
	decon->bts.ops->bts_release_dqe_bw(decon);
#endif
}

void decon_dqe_restore_cgc_context(struct decon_device *decon)
{
	struct dqe_device *dqe = dqe_drvdata;

	dqe_dbg("%s +\n", __func__);

	if (decon->id)
		return;

	if (!dqe->ctx.cgc_on)
		return;

	if (dqe->ctx.cgc_update[decon->dqe_cgc_idx])
		return;

	dqe_restore_context();
}

void decon_dqe_start(struct decon_device *decon, struct exynos_panel_info *lcd_info)
{
	if (decon->id)
		return;

	dqe_reg_start(decon->id, lcd_info);
}

void decon_dqe_enable(struct decon_device *decon)
{
	struct dqe_device *dqe = dqe_drvdata;

	if (decon->id)
		return;

	dqe_dbg("%s\n", __func__);

	dqe_reset_cgc_update();

	dqe_restore_context();
	dqe_reg_start(decon->id, decon->lcd_info);

	if (dqe)
		dqe->state = DQE_STATE_ENABLE;
}

void decon_dqe_disable(struct decon_device *decon)
{
	struct dqe_device *dqe = dqe_drvdata;

	if (decon->id)
		return;

	dqe_dbg("%s\n", __func__);

	if (dqe)
		dqe->state = DQE_STATE_DISABLE;
#if 0
	dqe_save_context();
	dqe_reg_stop(decon->id);
#endif
}

int decon_dqe_create_interface(struct decon_device *decon)
{
	int ret = 0;
	struct dqe_device *dqe;

	if (decon->id || (decon->dt.out_type != DECON_OUT_DSI)) {
		dqe_info("decon%d doesn't need dqe interface\n", decon->id);
		return 0;
	}

	dqe = kzalloc(sizeof(struct dqe_device), GFP_KERNEL);
	if (!dqe) {
		ret = -ENOMEM;
		goto exit0;
	}

	dqe_drvdata = dqe;
	dqe->decon = decon;

	if (IS_ERR_OR_NULL(dqe_class)) {
		dqe_class = class_create(THIS_MODULE, "dqe");
		if (IS_ERR_OR_NULL(dqe_class)) {
			pr_err("failed to create dqe class\n");
			ret = -EINVAL;
			goto exit1;
		}

		dqe_class->dev_groups = dqe_groups;
	}

	dqe->dev = device_create(dqe_class, decon->dev, 0,
			&dqe, "dqe", 0);
	if (IS_ERR_OR_NULL(dqe->dev)) {
		pr_err("failed to create dqe device\n");
		ret = -EINVAL;
		goto exit2;
	}

	mutex_init(&dqe->lock);
	mutex_init(&dqe->restore_lock);
	dev_set_drvdata(dqe->dev, dqe);

	dqe_load_context();

	dqe_init_context();

	dqe_info("decon_dqe_create_interface done.\n");

	return ret;
exit2:
	class_destroy(dqe_class);
exit1:
	kfree(dqe);
exit0:
	return ret;
}
