// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * JiHoon Kim <jihoonn.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/ctype.h>
#include <linux/lcd.h>

#include "panel.h"
#include "panel_drv.h"
#include "panel_vrr.h"
#include "panel_debug.h"
#include "panel_bl.h"
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
#include "copr.h"
#endif
#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
#include "mdnie.h"
#endif
#ifdef CONFIG_PANEL_AID_DIMMING
#include "dimming.h"
#endif
#ifdef CONFIG_SUPPORT_DDI_FLASH
#include "panel_poc.h"
#endif
#ifdef CONFIG_EXTEND_LIVE_CLOCK
#include "./aod/aod_drv.h"
#endif
#ifdef CONFIG_SUPPORT_POC_SPI
#include "panel_spi.h"
#endif
#ifdef CONFIG_DYNAMIC_MIPI
#include "./dynamic_mipi/dynamic_mipi.h"
#endif

static DEFINE_MUTEX(sysfs_lock);

char *mcd_rs_name[MAX_MCD_RS] = {
	"MCD1_R", "MCD1_L", "MCD2_R", "MCD2_L",
};

#ifdef CONFIG_EXYNOS_LCD_ENG
#ifdef CONFIG_SUPPORT_ISC_TUNE_TEST
static const char *str_stm_fied[STM_FIELD_MAX] = {
	"stm_ctrl_en=",
	"stm_max_opt=",
	"stm_default_opt=",
	"stm_dim_step=",
	"stm_frame_period=",
	"stm_min_sect=",
	"stm_pixel_period=",
	"stm_line_period=",
	"stm_min_move=",
	"stm_m_thres=",
	"stm_v_thres="
};

static ssize_t isc_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	snprintf(buf, PAGE_SIZE, "%u\n",
			panel_data->props.isc_threshold);

	return strlen(buf);
}

static ssize_t isc_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int value, rc, ret;
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (panel_data->props.isc_threshold == value)
		return size;

	mutex_lock(&panel->op_lock);
	panel_data->props.isc_threshold = value;
	mutex_unlock(&panel->op_lock);

	ret = panel_do_seqtbl_by_index(panel, PANEL_ISC_THRESHOLD_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write isc threshold seq\n");
		return ret;
	}
	panel_info("isc N %d\n", panel_data->props.isc_threshold);

	return size;
}

int print_stm_info(u8 *stm_field, char *buf)
{
	snprintf(buf, PAGE_SIZE, "CTRL EN=%d, MAX_OPT=%d, DEFAULT_OPT=%d, DIM_STEP=%d, FRAME_PERIOD=%d, MIN_SECT=%d, PIXEL_PERIOD=%d, LINE_PERIOD=%d, MIN_MOVE=%d, M_THRES=%d, V_THRES=%d\n",
	stm_field[STM_CTRL_EN], stm_field[STM_MAX_OPT], stm_field[STM_DEFAULT_OPT],
	stm_field[STM_DIM_STEP], stm_field[STM_FRAME_PERIOD], stm_field[STM_MIN_SECT], stm_field[STM_PIXEL_PERIOD],
	stm_field[STM_LINE_PERIOD], stm_field[STM_MIN_MOVE], stm_field[STM_M_THRES], stm_field[STM_V_THRES]);

	return strlen(buf);
}
static ssize_t stm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	return print_stm_info(panel_data->props.stm_field_info, buf);
}

int set_stm_info(char *user_set, u8 *stm_field)
{
	int i;
	int val = 0, ret;

	for (i = STM_CTRL_EN; i < STM_FIELD_MAX; i++) {
		if (strncmp(user_set, str_stm_fied[i], strlen(str_stm_fied[i])) == 0) {
			ret = sscanf(user_set + strlen(str_stm_fied[i]), "%d", &val);
			stm_field[i] = val;
			return 0;
		}
	}
	return -EINVAL;
}

static ssize_t stm_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);
	char *recv_buf;
	char *ptr = NULL;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;
	recv_buf = (char *)buf;
	while ((ptr = strsep(&recv_buf, " \t")) != NULL) {
		if (*ptr) {
			ret = set_stm_info(ptr, panel_data->props.stm_field_info);
			if (ret < 0)
				panel_info("invalid input %s\n", ptr);
		}
	}
	ret = panel_do_seqtbl_by_index(panel, PANEL_STM_TUNE_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write stm_tune\n");
		return ret;
	}
	panel_info("n");

	return size;
}
#endif

unsigned char readbuf[256] = { 0xff, };
unsigned int readreg, readpos, readlen;

static ssize_t read_mtp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i, len;

	mutex_lock(&sysfs_lock);
	if (readreg <= 0 || readreg > 0xFF || readlen <= 0 || readlen > 0xFF ||
			readpos > 0xFFFF) {
		mutex_unlock(&sysfs_lock);
		return -EINVAL;
	}

	len = snprintf(buf, PAGE_SIZE,
			"addr:0x%02X pos:%d size:%d\n",
			readreg, readpos, readlen);
	for (i = 0; i < readlen; i++)
		len += snprintf(buf + len, PAGE_SIZE - len, "0x%02X%s", readbuf[i],
				(((i + 1) % 16) == 0) || (i == readlen - 1) ? "\n" : " ");

	readreg = 0;
	readpos = 0;
	readlen = 0;
	mutex_unlock(&sysfs_lock);

	return len;
}

static ssize_t read_mtp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct panel_device *panel = dev_get_drvdata(dev);
	int ret, i;

	if (!IS_PANEL_ACTIVE(panel))
		return -EIO;

	mutex_lock(&sysfs_lock);
	ret = sscanf(buf, "%x %d %d", &readreg, &readpos, &readlen);
	if (ret != 3 || readreg <= 0 || readreg > 0xFF ||
			readlen <= 0 || readlen > 0xFF || readpos > 0xFFFF) {
		ret = -EINVAL;
		panel_info("%x %d %d\n", readreg, readpos, readlen);
		goto store_err;
	}

	mutex_lock(&panel->op_lock);
	panel_set_key(panel, 3, true);
	ret = panel_rx_nbytes(panel, DSI_PKT_TYPE_RD, readbuf, readreg, readpos, readlen);
	panel_set_key(panel, 3, false);
	mutex_unlock(&panel->op_lock);

	if (unlikely(ret != readlen)) {
		panel_err("failed to read reg %02Xh pos %d len %d\n",
				readreg, readpos, readlen);
		ret = -EIO;
		goto store_err;
	}

	panel_info("READ_Reg addr: %02x, pos : %d len : %d\n",
			readreg, readpos, readlen);
	for (i = 0; i < readlen; i++)
		panel_info("READ_Reg %dth : %02x\n", i + 1, readbuf[i]);
	mutex_unlock(&sysfs_lock);

	return size;

store_err:
	readreg = 0;
	readpos = 0;
	readlen = 0;
	mutex_unlock(&sysfs_lock);

	return ret;
}

static u8 WRITE_MTP_TX_DATA[512];
static DEFINE_STATIC_PACKET(write_mtp_tx_data, DSI_PKT_TYPE_WR, WRITE_MTP_TX_DATA, 0);
static void *write_mtp_cmdtbl[] = {
	&PKTINFO(write_mtp_tx_data),
};
struct seqinfo write_mtp_seqtbl[] = {
	SEQINFO_INIT("write-mtp-seq", write_mtp_cmdtbl),
};

static ssize_t write_mtp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i, len = 0, size, sz_buf = PAGE_SIZE;
	char *data = WRITE_MTP_TX_DATA;
	bool newline = true;

	mutex_lock(&sysfs_lock);
	size = PKTINFO(write_mtp_tx_data).dlen;
	for (i = 0; i < size; i++) {
		if (newline)
			len += snprintf(buf + len, sz_buf - len,
					"[%02Xh]  ", i);
		len += snprintf(buf + len, sz_buf - len,
				"%02X", data[i] & 0xFF);
		if (!((i + 1) % 32) || (i + 1 == size)) {
			len += snprintf(buf + len, sz_buf - len, "\n");
			newline = true;
		} else {
			len += snprintf(buf + len, sz_buf - len, " ");
			newline = false;
		}
	}
	mutex_unlock(&sysfs_lock);

	return len;
}

static ssize_t write_mtp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct panel_device *panel = dev_get_drvdata(dev);
	int tx_len = 0, ret = 0;
	u32 val;
	char *p, *arg = (char *)buf;

	if (!IS_PANEL_ACTIVE(panel))
		return -EIO;

	mutex_lock(&sysfs_lock);
	while ((p = strsep(&arg, " \t\n=")) != NULL) {
		if (!*p)
			continue;

		ret = sscanf(p, "%02x", &val);
		if (ret != 1) {
			panel_err("failed to scan payload %d\n", tx_len);
			ret = -EINVAL;
			goto err_write_mtp_store;
		}

		WRITE_MTP_TX_DATA[tx_len++] = val & 0xFF;
		if (tx_len >= ARRAY_SIZE(WRITE_MTP_TX_DATA))
			break;
	}

	PKTINFO(write_mtp_tx_data).dlen = tx_len;
	mutex_lock(&panel->op_lock);
	ret = excute_seqtbl_nolock(panel, write_mtp_seqtbl, 0);
	mutex_unlock(&panel->op_lock);
	if (ret < 0) {
		panel_err("failed to excute write-mtp-seq(ret:%d)\n", ret);
		goto err_write_mtp_store;
	}
	panel_info("%d byte(s) sent.\n", tx_len);
	mutex_unlock(&sysfs_lock);

	return size;

err_write_mtp_store:
	PKTINFO(write_mtp_tx_data).dlen = 0;
	memset(WRITE_MTP_TX_DATA, 0, sizeof(WRITE_MTP_TX_DATA));
	mutex_unlock(&sysfs_lock);
	panel_info("failed to write_mtp\n");

	return ret;
}

static ssize_t gamma_interpolation_test_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;
	if (panel_data->props.gamma_control_buf != NULL) {
		snprintf(buf, PAGE_SIZE, "%x %x %x %x %x %x\n",
			panel_data->props.gamma_control_buf[0], panel_data->props.gamma_control_buf[1],
			panel_data->props.gamma_control_buf[2], panel_data->props.gamma_control_buf[3],
			panel_data->props.gamma_control_buf[4], panel_data->props.gamma_control_buf[5]);
	}
	return strlen(buf);
}

static ssize_t gamma_interpolation_test_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct panel_device *panel = dev_get_drvdata(dev);
	struct panel_info *panel_data;
	int ret;
	u8 write_buf[6] = { 0x00, };
	int i;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	ret = sscanf(buf, "%02hhx %02hhx %02hhx %02hhx %02hhx %02hhx",
						&write_buf[0], &write_buf[1], &write_buf[2],
						&write_buf[3], &write_buf[4], &write_buf[5]);
	if (ret != 6) {
		panel_err("invalid input %d\n", ret);
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	kfree(panel_data->props.gamma_control_buf);

	panel_data->props.gamma_control_buf = kzalloc(sizeof(write_buf), GFP_KERNEL);
	mutex_lock(&panel->op_lock);
	memcpy(panel_data->props.gamma_control_buf, write_buf, sizeof(write_buf));
	mutex_unlock(&panel->op_lock);
	for (i = 0; i < 6; i++)
		panel_info("%x %x\n", write_buf[i], panel_data->props.gamma_control_buf[i]);

	ret = panel_do_seqtbl_by_index(panel, PANEL_GAMMA_INTER_CONTROL_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write gamma interpolation control seq\n");
		return ret;
	}
	panel_info("n");
	return size;
}
#endif

//void g_tracing_mark_write( char id, char* str1, int value );
int fingerprint_value = -1;
static ssize_t fingerprint_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	snprintf(buf, PAGE_SIZE, "%u\n", fingerprint_value);

	return strlen(buf);
}

static ssize_t fingerprint_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int rc;

	rc = kstrtouint(buf, 0, &fingerprint_value);
	if (rc < 0)
		return rc;

	//g_tracing_mark_write( 'C', "BCDS_hbm", fingerprint_value & 4);
	//g_tracing_mark_write( 'C', "BCDS_alpha", fingerprint_value & 2);

	panel_info("%d\n", fingerprint_value);

	return size;
}

static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	snprintf(buf, PAGE_SIZE, "SDC_%02X%02X%02X\n",
			panel_data->id[0], panel_data->id[1], panel_data->id[2]);

	return strlen(buf);
}

#ifdef CONFIG_SUPPORT_MAFPC
static ssize_t mafpc_time_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	snprintf(buf, PAGE_SIZE, "MAC : %lld usec", panel->mafpc_write_time);

	return strlen(buf);
}
static int mafpc_get_target_crc(struct panel_device *panel, u8 *crc)
{
	struct mafpc_device *mafpc = NULL;

	v4l2_subdev_call(panel->mafpc_sd, core, ioctl,
		V4L2_IOCTL_MAFPC_GET_INFO, NULL);

	mafpc = (struct mafpc_device *)v4l2_get_subdev_hostdata(panel->mafpc_sd);
	if (mafpc == NULL) {
		panel_err("failed to get mafpc info\n");
		return -EINVAL;
	}

	if (mafpc->comp_crc_len < MAFPC_CRC_LEN) {
		panel_err("crc len must be %d\n", MAFPC_CRC_LEN);
		return -EINVAL;
	}
	memcpy(crc, mafpc->comp_crc_buf, MAFPC_CRC_LEN);

	return 0;
}

#define MAFPC_CRC_LEN 2


static void prepare_mafpc_check_mode(struct panel_device *panel)
{
	int ret;

	panel_set_bypass(panel, true);
	usleep_range(90000, 100000);
	ret = panel_set_gpio_irq(&panel->gpio[PANEL_GPIO_DISP_DET], false);
	if (ret < 0)
		panel_warn("do not support irq\n");

	ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_EXIT_SEQ);
	if (ret < 0)
		panel_err("failed exit-seq\n");

	ret = __set_panel_power(panel, PANEL_POWER_OFF);
	if (ret < 0)
		panel_err("failed to set power off\n");

	ret = __set_panel_power(panel, PANEL_POWER_ON);
	if (ret < 0)
		panel_err("failed to set power on\n");

	ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_INIT_SEQ);
	if (ret < 0)
		panel_err("failed init-seq\n");

#ifdef CONFIG_EXTEND_LIVE_CLOCK
	ret = panel_aod_init_panel(panel, INIT_WITHOUT_LOCK);
	if (ret)
		panel_err("failed to aod init_panel\n");
#endif

}

static void clear_mafpc_check_mode(struct panel_device *panel)
{
	int ret;
	//struct panel_info *panel_data = &panel->panel_data;
	panel->state.cur_state = PANEL_STATE_NORMAL;
	panel->state.disp_on = PANEL_DISPLAY_OFF;
	panel_set_bypass(panel, false);

	clear_pending_bit(panel->gpio[PANEL_GPIO_DISP_DET].irq);
	ret = panel_set_gpio_irq(&panel->gpio[PANEL_GPIO_DISP_DET], true);
	if (ret < 0)
		panel_warn("do not support irq\n");

	msleep(20);
}


static ssize_t mafpc_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int size = 0;
	int ret = 0;
	u8 target_crc[MAFPC_CRC_LEN] = {0, };
	u8 origin_crc[MAFPC_CRC_LEN] = {1, };
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	panel_info("+\n");

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;

	mutex_lock(&panel->io_lock);
	if (!IS_PANEL_ACTIVE(panel)) {
		panel_err("panel is not active\n");
		goto exit;
	}

	if (panel->state.cur_state == PANEL_STATE_ALPM) {
		panel_err("gct not supported on LPM\n");
		goto exit;
	}

#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	copr_disable(&panel->copr);
#endif
	mdnie_disable(&panel->mdnie);

	mutex_lock(&panel->mdnie.lock);
	mutex_lock(&panel->op_lock);
	prepare_mafpc_check_mode(panel);

	ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_MAFPC_CHECKSUM_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write panel_mafpc_crc seq\n");
		goto out;
	}

	ret = resource_copy_n_clear_by_name(panel_data,	target_crc, "mafpc_crc");
	if (unlikely(ret < 0)) {
		panel_err("failed to read mafpc crc\n");
		goto out;
	}

	ret  = mafpc_get_target_crc(panel, origin_crc);
	if (ret)
		panel_err("failed to get target mAFPC crc value\n");

	panel_info("target crc : %x :%x\n", target_crc[0], target_crc[1]);
	panel_info("origin crc : %x :%x\n", origin_crc[0], origin_crc[1]);

out:
	clear_mafpc_check_mode(panel);
	mutex_unlock(&panel->op_lock);
	mutex_unlock(&panel->mdnie.lock);
exit:
	size = snprintf(buf, PAGE_SIZE, "%01d %02x %02x\n",
				memcmp(target_crc, origin_crc, MAFPC_CRC_LEN) == 0 ? 1 : 0,
				target_crc[0], target_crc[1]);

	mutex_unlock(&panel->io_lock);

	return size;
}

#endif

static ssize_t window_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	snprintf(buf, PAGE_SIZE, "%02x %02x %02x\n",
			panel_data->id[0], panel_data->id[1], panel_data->id[2]);
	panel_info("%02x %02x %02x\n", panel_data->id[0], panel_data->id[1], panel_data->id[2]);
	return strlen(buf);
}

static ssize_t manufacture_code_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u8 code[5] = { 0, };
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	resource_copy_by_name(panel_data, code, "code");

	snprintf(buf, PAGE_SIZE, "%02X%02X%02X%02X%02X\n",
		code[0], code[1], code[2], code[3], code[4]);

	return strlen(buf);
}

static ssize_t SVC_OCTA_DDI_CHIPID_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return manufacture_code_show(dev, attr, buf);
}

static ssize_t cell_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u8 date[PANEL_DATE_LEN] = { 0, }, coordinate[4] = { 0, };
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	resource_copy_by_name(panel_data, date, "date");
	resource_copy_by_name(panel_data, coordinate, "coordinate");

	snprintf(buf, PAGE_SIZE, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
		date[0], date[1], date[2], date[3], date[4], date[5], date[6],
		coordinate[0], coordinate[1], coordinate[2], coordinate[3]);

	return strlen(buf);
}

static ssize_t SVC_OCTA_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return cell_id_show(dev, attr, buf);
}

static ssize_t octa_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i, site, rework, poc;
	u8 cell_id[16], octa_id[PANEL_OCTA_ID_LEN] = { 0, };
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);
	int len = 0;
	bool cell_id_exist = true;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;
	resource_copy_by_name(panel_data, octa_id, "octa_id");

	site = (octa_id[0] >> 4) & 0x0F;
	rework = octa_id[0] & 0x0F;
	poc = octa_id[1] & 0x0F;

	panel_dbg("site (%d), rework (%d), poc (%d)\n",
			site, rework, poc);

	panel_dbg("<CELL ID>\n");
	for (i = 0; i < 16; i++) {
		cell_id[i] = isalnum(octa_id[i + 4]) ? octa_id[i + 4] : '\0';
		panel_dbg("%x -> %c\n", octa_id[i + 4], cell_id[i]);
		if (cell_id[i] == '\0') {
			cell_id_exist = false;
			break;
		}
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "%d%d%d%02x%02x",
			site, rework, poc, octa_id[2], octa_id[3]);
	if (cell_id_exist) {
		for (i = 0; i < 16; i++)
			len += snprintf(buf + len, PAGE_SIZE - len, "%c", cell_id[i]);
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

	return strlen(buf);
}

static ssize_t SVC_OCTA_CHIPID_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return octa_id_show(dev, attr, buf);
}

static ssize_t color_coordinate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u8 coordinate[4] = { 0, };
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	resource_copy_by_name(panel_data, coordinate, "coordinate");

	snprintf(buf, PAGE_SIZE, "%u, %u\n", /* X, Y */
			coordinate[0] << 8 | coordinate[1],
			coordinate[2] << 8 | coordinate[3]);
	return strlen(buf);
}

static ssize_t manufacture_date_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u16 year;
	u8 month, day, hour, min, date[PANEL_DATE_LEN] = { 0, };
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	resource_copy_by_name(panel_data, date, "date");

	year = ((date[0] & 0xF0) >> 4) + 2011;
	month = date[0] & 0xF;
	day = date[1] & 0x1F;
	hour = date[2] & 0x1F;
	min = date[3] & 0x3F;

	snprintf(buf, PAGE_SIZE, "%d, %d, %d, %d:%d\n",
			year, month, day, hour, min);
	return strlen(buf);
}

static ssize_t brightness_table_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_device *panel = dev_get_drvdata(dev);
	struct panel_bl_device *panel_bl;
	int br, len = 0, recv_len = 0, prev_br = 0, temp = 0;
	int actual_brightness = 0, prev_actual_brightness = 0;
	char recv_buf[50] = {0, };
	int recv_buf_len = ARRAY_SIZE(recv_buf);
	int max_brightness = 0;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_bl = &panel->panel_bl;
	max_brightness = get_max_brightness(panel_bl);

	mutex_lock(&panel_bl->lock);
	for (br = 0; br <= max_brightness; br++) {
		actual_brightness = get_actual_brightness(panel_bl, br);
		if (recv_len == 0) {
			recv_len += snprintf(recv_buf, recv_buf_len, "%5d", prev_br);
			prev_actual_brightness = actual_brightness;
		}
		if ((prev_actual_brightness != actual_brightness) || (br == max_brightness))  {
			if (recv_len < recv_buf_len) {
				temp += snprintf(recv_buf + recv_len, recv_buf_len - recv_len,
					"~%5d %3d\n", prev_br, prev_actual_brightness);
				len += snprintf(buf + len, PAGE_SIZE - len, "%s", recv_buf);
			}
			recv_len = 0;
			memset(recv_buf, 0x00, sizeof(recv_buf));
		}
		prev_actual_brightness = actual_brightness;
		prev_br = br;
		if (len >= PAGE_SIZE) {
			panel_info("print buffer overflow %d\n", len);
			len = PAGE_SIZE - 1;
			goto exit;
		}
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "%s", recv_buf);
exit:
	mutex_unlock(&panel_bl->lock);

	return len;
}

static ssize_t adaptive_control_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	snprintf(buf, PAGE_SIZE, "%d\n",
			panel_data->props.adaptive_control);

	return strlen(buf);
}

static ssize_t adaptive_control_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);
	struct panel_bl_device *panel_bl;
	int rc;
	u32 value;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;
	panel_bl = &panel->panel_bl;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (value > 100) {
		panel_err("invalid adaptive_control %d\n", value);
		return -EINVAL;
	}

	if (panel_data->props.adaptive_control == value)
		return size;

	mutex_lock(&panel_bl->lock);
	panel_data->props.adaptive_control = value;
	mutex_unlock(&panel_bl->lock);
	panel_update_brightness(panel);

	panel_info("adaptive_control %d\n", panel_data->props.adaptive_control);

	return size;
}

static ssize_t siop_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	snprintf(buf, PAGE_SIZE, "%u\n",
			panel_data->props.siop_enable);

	return strlen(buf);
}

static ssize_t siop_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);
	int value, rc;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (panel_data->props.siop_enable == value)
		return size;

	mutex_lock(&panel->op_lock);
	panel_data->props.siop_enable = value;
	mutex_unlock(&panel->op_lock);
	panel_update_brightness(panel);

	panel_info("siop_enable %d\n",
			panel_data->props.siop_enable);
	return size;
}

static ssize_t temperature_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char temp[] = "-15, -14, 0, 1\n";

	strcat(buf, temp);
	return strlen(buf);
}

static ssize_t temperature_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);
	int value, rc;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	rc = kstrtoint(buf, 10, &value);
	if (rc < 0)
		return rc;

	mutex_lock(&panel->op_lock);
	panel_data->props.temperature = value;
	mutex_unlock(&panel->op_lock);
	panel_update_brightness(panel);

	panel_info("temperature %d\n",
			panel_data->props.temperature);

	return size;
}

static ssize_t mcd_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	snprintf(buf, PAGE_SIZE, "%u\n", panel_data->props.mcd_on);

	return strlen(buf);
}

static ssize_t mcd_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int value, rc, ret;
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (panel_data->props.mcd_on == value)
		return size;

	mutex_lock(&panel->io_lock);
#if 0		// temporary
#ifdef CONFIG_PANEL_VRR_BRIDGE
	if ((value) && ((panel_data->props.vrr_fps != 60) ||
		(panel_data->props.vrr_mode != VRR_NORMAL_MODE))) {
		// "mcd on" is only 60 Normal
		panel_info("request mcd on, but current %d %s mode\n",
				panel_data->props.vrr_fps, panel_data->props.vrr_mode ? "HS" : "Normal");
		mutex_unlock(&panel->io_lock);
		return size;
	}
#endif
#endif
	mutex_lock(&panel->op_lock);
	panel_data->props.mcd_on = value;
	mutex_unlock(&panel->op_lock);

	ret = panel_do_seqtbl_by_index(panel,
			value ? PANEL_MCD_ON_SEQ : PANEL_MCD_OFF_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write mcd seq\n");
		mutex_unlock(&panel->io_lock);
		return ret;
	}
	panel_info("mcd %s (%d %s mode)\n",
			panel_data->props.mcd_on ? "on" : "off",
			panel_data->props.vrr_fps, panel_data->props.vrr_mode ? "HS" : "Normal");
	mutex_unlock(&panel->io_lock);

	return size;
}

static void print_mcd_resistance(u8 *mcd_nok, int size)
{
	int code, len;
	char buf[1024];

	len = snprintf(buf, sizeof(buf),
			"MCD CHECK [b7:MCD1_R, b6:MCD2_R, b3:MCD1_L, b2:MCD2_L]\n");
	for (code = 0; code < size; code++) {
		if (!(code % 0x10))
			len += snprintf(buf + len, sizeof(buf) - len, "[%02X] ", code);
		len += snprintf(buf + len, sizeof(buf) - len, "%02X%s",
				mcd_nok[code], (!((code + 1) % 0x10)) ? "\n" : " ");
	}
	panel_info("%s\n", buf);
}

static int read_mcd_resistance(struct panel_device *panel)
{
	int i, ret, code;
	u8 mcd_nok[128];
	struct panel_info *panel_data = &panel->panel_data;
	int stt, end;
	u8 mcd_rs_mask[MAX_MCD_RS] = {
		(1U << 7),
		(1U << 3),
		(1U << 6),
		(1U << 2),
	};
	s64 elapsed_usec;
	struct timespec64 cur_ts, last_ts, delta_ts;

	ktime_get_ts64(&last_ts);

	ret = panel_set_gpio_irq(&panel->gpio[PANEL_GPIO_DISP_DET], false);
	if (ret < 0)
		panel_warn("do not support irq\n");

	ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_MCD_RS_ON_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write mcd_3_0_on seq\n");
		goto out;
	}

	memset(mcd_nok, 0, sizeof(mcd_nok));
	for (code = 0; code < 0x80; code++) {
		panel_data->props.mcd_resistance = code;
		ret = panel_do_seqtbl_by_index_nolock(panel,
				PANEL_MCD_RS_READ_SEQ);
		if (unlikely(ret < 0)) {
			panel_err("failed to write mcd_rs_read seq\n");
			goto out;
		}

		ret = resource_copy_n_clear_by_name(panel_data,
				&mcd_nok[code], "mcd_resistance");
		if (unlikely(ret < 0)) {
			panel_err("failed to copy resource(mcd_resistance) (ret %d)\n", ret);
			goto out;
		}
		panel_dbg("%02X : %02X\n", code, mcd_nok[code]);
	}
	print_mcd_resistance(mcd_nok, ARRAY_SIZE(mcd_nok));

	for (i = 0; i < MAX_MCD_RS; i++) {
		for (code = 0, stt = -1, end = -1; code < 0x80; code++) {
			if (mcd_nok[code] & mcd_rs_mask[i]) {
				if (stt == -1)
					stt = code;
				end = code;
			}
		}
		panel_data->props.mcd_rs_range[i][0] = stt;
		panel_data->props.mcd_rs_range[i][1] = end;
	}

	ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_MCD_RS_OFF_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write mcd_3_0_off seq\n");
		goto out;
	}

out:
	ret = panel_set_gpio_irq(&panel->gpio[PANEL_GPIO_DISP_DET], true);
	if (ret < 0)
		panel_warn("do not support irq\n");
	ktime_get_ts64(&cur_ts);
	delta_ts = timespec64_sub(cur_ts, last_ts);
	elapsed_usec = timespec64_to_ns(&delta_ts) / 1000;
	panel_info("done (elapsed %2lld.%03lld msec)\n",
			elapsed_usec / 1000, elapsed_usec % 1000);

	return ret;
}

static ssize_t mcd_resistance_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);
	int i, len = 0;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;
	mutex_lock(&panel->op_lock);
	for (i = 0; i < MAX_MCD_RS; i++)
		len += snprintf(buf + len, PAGE_SIZE - len,
				"SDC_%s:%d%s", mcd_rs_name[i],
				panel_data->props.mcd_rs_flash_range[i][1],
				(i != MAX_MCD_RS - 1) ? " " : "\n");

	for (i = 0; i < MAX_MCD_RS; i++)
		len += snprintf(buf + len, PAGE_SIZE - len,
				"%s:%d%s", mcd_rs_name[i],
				panel_data->props.mcd_rs_range[i][1],
				(i != MAX_MCD_RS - 1) ? " " : "\n");
	mutex_unlock(&panel->op_lock);

	return len;
}

static ssize_t mcd_resistance_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int i, value, rc, ret;
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);
#ifdef CONFIG_SUPPORT_DDI_FLASH
	u8 flash_mcd[8];
#endif

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;
	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (!!value) {
		mutex_lock(&panel->op_lock);
		/* clear variable */
		memset(panel_data->props.mcd_rs_range, -1,
				sizeof(panel_data->props.mcd_rs_range));

		ret = read_mcd_resistance(panel);
		mutex_unlock(&panel->op_lock);
		if (unlikely(ret < 0)) {
			panel_err("failed to check mcd resistance\n");
			return ret;
		}

		for (i = 0; i < MAX_MCD_RS; i++)
			panel_info("%s:(%d, %d)\n", mcd_rs_name[i],
					panel_data->props.mcd_rs_range[i][0],
					panel_data->props.mcd_rs_range[i][1]);

#ifdef CONFIG_SUPPORT_DDI_FLASH
		ret = set_panel_poc(&panel->poc_dev, POC_OP_MCD_READ, NULL);
		if (unlikely(ret)) {
			panel_err("failed to read mcd(ret %d)\n", ret);
			return ret;
		}
		ret = panel_resource_update_by_name(panel, "flash_mcd");
		if (unlikely(ret < 0)) {
			panel_err("failed to update flash_mcd res (ret %d)\n", ret);
			return ret;
		}

		ret = resource_copy_by_name(&panel->panel_data, flash_mcd, "flash_mcd");
		if (unlikely(ret < 0)) {
			panel_err("failed to copy flash_mcd res (ret %d)\n", ret);
			return ret;
		}

		panel_data->props.mcd_rs_flash_range[MCD_RS_1_RIGHT][1] = flash_mcd[0];
		panel_data->props.mcd_rs_flash_range[MCD_RS_2_RIGHT][1] = flash_mcd[1];
		panel_data->props.mcd_rs_flash_range[MCD_RS_1_LEFT][1] = flash_mcd[4];
		panel_data->props.mcd_rs_flash_range[MCD_RS_2_LEFT][1] = flash_mcd[5];
		for (i = 0; i < MAX_MCD_RS; i++)
			panel_info("SDC_%s:(%d, %d)\n", mcd_rs_name[i],
					panel_data->props.mcd_rs_flash_range[i][0],
					panel_data->props.mcd_rs_flash_range[i][1]);
#endif
	}

	return size;
}

static ssize_t irc_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	snprintf(buf, PAGE_SIZE, "%u\n", panel_data->props.irc_mode);

	return strlen(buf);
}

static ssize_t irc_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int value, rc;
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	mutex_lock(&panel->op_lock);
	panel_data->props.irc_mode = !!value;
	mutex_unlock(&panel->op_lock);
	panel_update_brightness(panel);
	panel_info("irc_mode %s\n",
			panel_data->props.irc_mode ? "on" : "off");

	return size;
}

static ssize_t dia_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	snprintf(buf, PAGE_SIZE, "%u\n", panel_data->props.dia_mode);

	return strlen(buf);
}

static ssize_t dia_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int value, rc, ret;
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	mutex_lock(&panel->op_lock);
	panel_data->props.dia_mode = value;
	mutex_unlock(&panel->op_lock);

	ret = panel_do_seqtbl_by_index(panel, PANEL_DIA_ONOFF_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write mcd seq\n");
		return ret;
	}
	panel_info("set %s\n",
			panel_data->props.dia_mode ? "on" : "off");
	return size;
}

static ssize_t partial_disp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	snprintf(buf, PAGE_SIZE, "%u\n", panel_data->props.panel_partial_disp);

	return strlen(buf);
}

static ssize_t partial_disp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int value, rc, ret;
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (value != panel_data->props.panel_partial_disp) {
		mutex_lock(&panel->op_lock);
		panel_data->props.panel_partial_disp = value;
		mutex_unlock(&panel->op_lock);

		ret = panel_do_seqtbl_by_index(panel,
				value ? PANEL_PARTIAL_DISP_ON_SEQ : PANEL_PARTIAL_DISP_OFF_SEQ);
		if (unlikely(ret < 0)) {
			panel_err("failed to write mcd seq\n");
			return ret;
		}
		panel_info("set %s\n",
				panel_data->props.panel_partial_disp ? "on" : "off");
	} else {
		panel_info("already set %s, so skip\n",
				panel_data->props.panel_partial_disp ? "on" : "off");
	}

	return size;
}

#ifdef CONFIG_SUPPORT_MST
static ssize_t mst_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	snprintf(buf, PAGE_SIZE, "%u\n",
			panel_data->props.mst_on);

	return strlen(buf);
}

static ssize_t mst_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int value, rc, ret;
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (panel_data->props.mst_on == value)
		return size;

	mutex_lock(&panel->op_lock);
	panel_data->props.mst_on = value;
	mutex_unlock(&panel->op_lock);

	ret = panel_do_seqtbl_by_index(panel,
			value ? PANEL_MST_ON_SEQ : PANEL_MST_OFF_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write mst seq\n");
		return ret;
	}
	panel_info("mst %s\n",
			panel_data->props.mst_on ? "on" : "off");

	return size;
}
#endif

#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
u8 checksum[4] = { 0x12, 0x34, 0x56, 0x78 };
static bool gct_chksum_is_valid(struct panel_device *panel)
{
	int i;
	struct panel_info *panel_data = &panel->panel_data;

	for (i = 0; i < 4; i++)
		if (checksum[i] != panel_data->props.gct_valid_chksum[i])
			return false;
	return true;
}

static void prepare_gct_mode(struct panel_device *panel)
{
	int ret;

	panel_set_bypass(panel, true);
	usleep_range(90000, 100000);
	ret = panel_set_gpio_irq(&panel->gpio[PANEL_GPIO_DISP_DET], false);
	if (ret < 0)
		panel_warn("do not support irq\n");
}

static void clear_gct_mode(struct panel_device *panel)
{
	struct panel_info *panel_data = &panel->panel_data;
	int ret;

	ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_EXIT_SEQ);
	if (ret < 0)
		panel_err("failed exit-seq\n");

	ret = __set_panel_power(panel, PANEL_POWER_OFF);
	if (ret < 0)
		panel_err("failed to set power off\n");

	ret = __set_panel_power(panel, PANEL_POWER_ON);
	if (ret < 0)
		panel_err("failed to set power on\n");

	ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_INIT_SEQ);
	if (ret < 0)
		panel_err("failed init-seq\n");

#ifdef CONFIG_EXTEND_LIVE_CLOCK
	ret = panel_aod_init_panel(panel, INIT_WITHOUT_LOCK);
	if (ret)
		panel_err("failed to aod init_panel\n");
#endif

	ret = panel_set_gpio_irq(&panel->gpio[PANEL_GPIO_DISP_DET], true);
	if (ret < 0)
		panel_warn("do not support irq\n");

	panel_data->props.gct_on = GRAM_TEST_OFF;
	panel->state.cur_state = PANEL_STATE_NORMAL;
	panel->state.disp_on = PANEL_DISPLAY_OFF;
	panel_set_bypass(panel, false);
	msleep(20);
}

static ssize_t gct_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	snprintf(buf, PAGE_SIZE, "%u 0x%08x\n",
		panel_data->props.gct_on == GRAM_TEST_SKIPPED ||
		gct_chksum_is_valid(panel) ? 1 : 0, ntohl(*(u32 *)checksum));
	panel_info("%s", buf);

	return strlen(buf);
}

static ssize_t gct_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int value, rc, ret, result = 0;
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);
	struct seqinfo *seqtbl;
	int i, index = 0, vddm = 0, pattern = 0;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (value != GRAM_TEST_ON)
		return -EINVAL;

	if (!check_seqtbl_exist(panel_data, PANEL_GCT_ENTER_SEQ)) {
		panel_warn("cannot found gct seq. skip\n");
		panel_data->props.gct_on = GRAM_TEST_SKIPPED;
		return -EINVAL;
	}

	/* clear checksum buffer */
	checksum[0] = 0x12;
	checksum[1] = 0x34;
	checksum[2] = 0x56;
	checksum[3] = 0x78;

	mutex_lock(&panel->io_lock);
	if (!IS_PANEL_ACTIVE(panel)) {
		panel_err("panel is not active\n");
		mutex_unlock(&panel->io_lock);
		return -EAGAIN;
	}

	if (panel->state.cur_state == PANEL_STATE_ALPM) {
		panel_warn("gct not supported on LPM\n");
		mutex_unlock(&panel->io_lock);
		return -EINVAL;
	}

#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	copr_disable(&panel->copr);
#endif
#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
	mdnie_disable(&panel->mdnie);

	mutex_lock(&panel->mdnie.lock);
#endif
	mutex_lock(&panel->op_lock);
	prepare_gct_mode(panel);
	panel_data->props.gct_on = value;

#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
#ifdef CONFIG_SUPPORT_AFC
	if (panel->mdnie.props.afc_on &&
			panel->mdnie.nr_seqtbl > MDNIE_AFC_OFF_SEQ) {
		panel_info("afc off\n");
		ret = panel_do_seqtbl(panel, &panel->mdnie.seqtbl[MDNIE_AFC_OFF_SEQ]);
		if (unlikely(ret < 0))
			panel_err("failed to write afc off seqtbl\n");
	}
#endif
#endif

	ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_GCT_ENTER_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write gram-checksum-test-enter seq\n");
		result = ret;
		goto out;
	}

	for (vddm = VDDM_LV; vddm < MAX_VDDM; vddm++) {
		panel_data->props.gct_vddm = vddm;
		ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_GCT_VDDM_SEQ);
		if (unlikely(ret < 0)) {
			panel_err("failed to write gram-checksum-on seq\n");
			result = ret;
			goto out;
		}

		for (pattern = GCT_PATTERN_1; pattern < MAX_GCT_PATTERN; pattern++) {
			panel_data->props.gct_pattern = pattern;
			seqtbl = find_index_seqtbl(&panel->panel_data,
					PANEL_GCT_IMG_UPDATE_SEQ);
			ret = panel_do_seqtbl_by_index_nolock(panel,
					(seqtbl && seqtbl->cmdtbl) ? PANEL_GCT_IMG_UPDATE_SEQ :
					((pattern == GCT_PATTERN_1) ?
					 PANEL_GCT_IMG_0_UPDATE_SEQ : PANEL_GCT_IMG_1_UPDATE_SEQ));
			if (unlikely(ret < 0)) {
				panel_err("failed to write gram-img-update seq\n");
				result = ret;
				goto out;
			}

			ret = resource_copy_n_clear_by_name(panel_data,
					&checksum[index], "gram_checksum");
			if (unlikely(ret < 0)) {
				panel_err("failed to copy gram_checksum[%d] (ret %d)\n", index, ret);
				result = ret;
				goto out;
			}
			panel_info("gram_checksum[%d] 0x%x\n", index, checksum[index]);
			index++;
		}
	}

	ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_GCT_EXIT_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write gram-checksum-off seq\n");
		result = ret;
	}
out:
	clear_gct_mode(panel);
	mutex_unlock(&panel->op_lock);
#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
	mutex_unlock(&panel->mdnie.lock);
#endif
	mutex_unlock(&panel->io_lock);

	for (i = 0; i < 20; i++) {
		if (panel->state.disp_on == PANEL_DISPLAY_ON)
			break;
		msleep(50);
	}

	if (i == 20) {
		panel_info("display on\n");
		ret = panel_display_on(panel);
		if (ret < 0)
			panel_err("failed to display on\n");
	}

	if (result < 0)
		return result;

	panel_info("chksum %s 0x%08x\n",
			gct_chksum_is_valid(panel) ? "ok" : "nok", ntohl(*(u32 *)checksum));

	return size;
}
#endif

#ifdef CONFIG_SUPPORT_XTALK_MODE
static ssize_t xtalk_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	snprintf(buf, PAGE_SIZE, "%u\n",
			panel_data->props.xtalk_mode);

	return strlen(buf);
}

static ssize_t xtalk_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int value, rc;
	struct panel_info *panel_data;
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;
	panel_bl = &panel->panel_bl;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (panel_data->props.xtalk_mode == value)
		return size;

	mutex_lock(&panel_bl->lock);
	panel_data->props.xtalk_mode = value;
	mutex_unlock(&panel_bl->lock);
	panel_update_brightness(panel);

	panel_info("xtalk_mode %d\n",
			panel_data->props.xtalk_mode);

	return size;
}
#endif

#ifdef CONFIG_SUPPORT_POC_FLASH
static ssize_t poc_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_device *panel = dev_get_drvdata(dev);
	struct panel_poc_device *poc_dev;
	struct panel_poc_info *poc_info;
	int ret;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	if (!IS_PANEL_ACTIVE(panel)) {
		panel_err("panel is not active\n");
		return -EAGAIN;
	}

	poc_dev = &panel->poc_dev;
	poc_info = &poc_dev->poc_info;

	ret = set_panel_poc(poc_dev, POC_OP_CHECKPOC, NULL);
	if (unlikely(ret < 0)) {
		panel_err("failed to chkpoc (ret %d)\n", ret);
		return ret;
	}

	ret = set_panel_poc(poc_dev, POC_OP_CHECKSUM, NULL);
	if (unlikely(ret < 0)) {
		panel_err("failed to chksum (ret %d)\n", ret);
		return ret;
	}

	snprintf(buf, PAGE_SIZE, "%d %d %02x\n", poc_info->poc,
			poc_info->poc_chksum[4], poc_info->poc_ctrl[3]);

	panel_info("poc:%d chk:%d gray:%02x\n", poc_info->poc,
			poc_info->poc_chksum[4], poc_info->poc_ctrl[3]);

	return strlen(buf);
}

static ssize_t poc_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct panel_device *panel = dev_get_drvdata(dev);
	struct panel_info *panel_data;
	struct panel_poc_device *poc_dev;
	struct panel_poc_info *poc_info;
#ifdef CONFIG_SUPPORT_POC_SPI
	struct panel_spi_dev *spi_dev = &panel->panel_spi_dev;
#endif
	int rc, ret;
	unsigned int value;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;
	poc_dev = &panel->poc_dev;
	poc_info = &poc_dev->poc_info;

	rc = sscanf(buf, "%u", &value);
	if (rc < 1) {
		panel_err("poc_op required\n");
		return -EINVAL;
	}

	if (!IS_VALID_POC_OP(value)) {
		panel_warn("invalid poc_op %d\n", value);
		return -EINVAL;
	}

	if (value == POC_OP_WRITE || value == POC_OP_READ) {
		panel_warn("unsupported poc_op %d\n", value);
		return size;
	}

#ifdef CONFIG_SUPPORT_POC_SPI
	if (value == POC_OP_SET_SPI_SPEED) {
		rc = sscanf(buf, "%*u %u", &value);
		if (rc < 1) {
			panel_warn("SET_SPI_SPEED need 2 params\n");
			return -EINVAL;
		}
		spi_dev->spi_info.speed_hz = value;
		value = POC_OP_SET_SPI_SPEED;
		return size;
	}
#endif

	if (value == POC_OP_CANCEL) {
		atomic_set(&poc_dev->cancel, 1);
	} else {
		mutex_lock(&panel->io_lock);
		ret = set_panel_poc(poc_dev, value, (void *)buf);
		if (unlikely(ret < 0)) {
			panel_err("failed to poc_op %d(ret %d)\n", value, ret);
			mutex_unlock(&panel->io_lock);
			return -EINVAL;
		}
		mutex_unlock(&panel->io_lock);
	}

	mutex_lock(&panel->op_lock);
	panel_data->props.poc_op = value;
	mutex_unlock(&panel->op_lock);

	panel_info("poc_op %d\n",
			panel_data->props.poc_op);

	return size;
}

static ssize_t poc_mca_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_device *panel = dev_get_drvdata(dev);
	int ret;
	u8 chksum_data[256];
	int i, len, ofs = 0;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	if (!IS_PANEL_ACTIVE(panel)) {
		panel_err("panel is not active\n");
		return -EAGAIN;
	}

	panel_set_key(panel, 2, true);
	ret = panel_resource_update_by_name(panel, "poc_mca_chksum");
	if (unlikely(ret < 0)) {
		panel_err("failed to update poc_mca_chksum res (ret %d)\n", ret);
		return ret;
	}
	panel_set_key(panel, 2, false);

	ret = resource_copy_by_name(&panel->panel_data, chksum_data, "poc_mca_chksum");
	if (unlikely(ret < 0)) {
		panel_err("failed to copy poc_mca_chksum res (ret %d)\n", ret);
		return ret;
	}

	len = get_resource_size_by_name(&panel->panel_data, "poc_mca_chksum");
	for (i = 0; i < len; i++)
		ofs += snprintf(buf + ofs,
				PAGE_SIZE - ofs, "%02X ", chksum_data[i]);
	ofs += snprintf(buf + ofs, PAGE_SIZE - ofs, "\n");

	panel_info("poc_mca_checksum: %s", buf);

	return ofs;
}

static ssize_t poc_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_device *panel = dev_get_drvdata(dev);
	struct panel_poc_device *poc_dev;
	struct panel_poc_info *poc_info;
	int ret;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	poc_dev = &panel->poc_dev;
	poc_info = &poc_dev->poc_info;

	ret = get_poc_partition_size(poc_dev, POC_IMG_PARTITION);
	if (unlikely(ret < 0)) {
		panel_err("failed to get poc partition size (ret %d)\n", ret);
		return ret;
	}

	snprintf(buf, PAGE_SIZE, "poc_mca_image_size %d\n", ret);

	panel_info("%s\n", buf);

	return strlen(buf);
}
#endif

static ssize_t gamma_check_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct panel_device *panel = dev_get_drvdata(dev);
	int ret;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	ret = panel_mtp_gamma_check(panel);
	panel_info("ret %d\n", ret);
	snprintf(buf, PAGE_SIZE, "%d\n", ret);

	return strlen(buf);
}

/*
 * gamma_flash check function for gamma-mode2 data
 */
static ssize_t gamma_flash_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct panel_device *panel = dev_get_drvdata(dev);
	struct dim_flash_result *result;
	int size = 0;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	result = &panel->flash_checksum_result;
	panel_info("%s\n", result->result);
	size += snprintf(buf + size, PAGE_SIZE - size, "%s\n", result->result);
	return size;
}

static ssize_t gamma_flash_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct panel_device *panel = dev_get_drvdata(dev);
	int rc;
	unsigned int value;

	if (!IS_PANEL_ACTIVE(panel))
		return -ENODEV;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	panel_info("+ %u\n", value);
	if (value == 1) {
		rc = panel_flash_checksum_calc(panel);
		if (rc < 0) {
			panel_info("failed %d\n", rc);
			return rc;
		}
	}

	return size;
}

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static ssize_t grayspot_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	snprintf(buf, PAGE_SIZE, "%u\n",
			panel_data->props.grayspot);

	return strlen(buf);
}

static ssize_t grayspot_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int value, rc, ret;
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (panel_data->props.grayspot == value)
		return size;

	mutex_lock(&panel->op_lock);
	panel_data->props.grayspot = value;
	mutex_unlock(&panel->op_lock);

	ret = panel_do_seqtbl_by_index(panel,
			value ? PANEL_GRAYSPOT_ON_SEQ : PANEL_GRAYSPOT_OFF_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write grayspot on/off seq\n");
		return ret;
	}
	panel_info("grayspot %s\n",
			panel_data->props.grayspot ? "on" : "off");

	return size;
}
#endif

#ifdef CONFIG_SUPPORT_HMD
static ssize_t hmt_bright_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = dev_get_drvdata(dev);
	int size;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_bl = &panel->panel_bl;
	mutex_lock(&panel_bl->lock);
	if (panel_bl->props.id == PANEL_BL_SUBDEV_TYPE_DISP) {
		size = snprintf(buf, 30, "HMD off state\n");
	} else {
		size = snprintf(buf, PAGE_SIZE, "index : %d, brightenss : %d\n",
				panel_bl->props.actual_brightness_index,
				BRT_USER(panel_bl->props.brightness));
	}
	mutex_unlock(&panel_bl->lock);
	return size;
}

static ssize_t hmt_bright_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	int value, rc;
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_bl = &panel->panel_bl;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	panel_info("brightness : %d\n", value);

	mutex_lock(&panel_bl->lock);
	mutex_lock(&panel->op_lock);

	if (panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_HMD].brightness != BRT(value))
		panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_HMD].brightness = BRT(value);

	if (panel->state.hmd_on != PANEL_HMD_ON) {
		panel_info("hmd off\n");
		goto exit_store;
	}

	ret = panel_bl_set_brightness(panel_bl, PANEL_BL_SUBDEV_TYPE_HMD, SEND_CMD);
	if (ret) {
		panel_err("fail to set brightness\n");
		goto exit_store;
	}

exit_store:
	mutex_unlock(&panel->op_lock);
	mutex_unlock(&panel_bl->lock);
	return size;
}

static ssize_t hmt_on_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_state *state;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	state = &panel->state;

	snprintf(buf, PAGE_SIZE, "%u\n", state->hmd_on);

	return strlen(buf);
}

static ssize_t hmt_on_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	int value, rc;
	struct backlight_device *bd;
	struct panel_state *state;
	struct panel_device *panel = dev_get_drvdata(dev);
	struct panel_bl_device *panel_bl;
	struct panel_vrr *vrr;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_bl = &panel->panel_bl;
	bd = panel_bl->bd;
	state = &panel->state;

	rc = kstrtoint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (value != PANEL_HMD_OFF && value != PANEL_HMD_ON) {
		panel_err("invalid parameter %d\n", value);
		return -EINVAL;
	}

	panel_info("hmd set to: %s\n", (value == PANEL_HMD_ON) ? "on" : "off");

	panel_flush_image(panel);
	mutex_lock(&panel_bl->lock);
	mutex_lock(&panel->op_lock);

	if (value == state->hmd_on) {
		panel_warn("already set : %d\n", value);
		ret = -EBUSY;
		goto exit;
	}

	vrr = get_panel_vrr(panel);
	if (vrr != NULL && value == PANEL_HMD_ON) {
		if (vrr->fps != 60 || vrr->mode != 0) {
			panel_err("failed to set hmd %s: fps %d mode %d\n",
					(value == PANEL_HMD_ON) ? "on" : "off",
					vrr->fps, vrr->mode);
			ret = -EINVAL;
			goto exit;
		}
	}

	ret = panel_do_seqtbl_by_index_nolock(panel,
			(value == PANEL_HMD_ON) ? PANEL_HMD_ON_SEQ : PANEL_HMD_OFF_SEQ);
	if (ret < 0)
		panel_err("failed to set hmd %s seq\n",
				(value == PANEL_HMD_ON) ? "on" : "off");


	ret = panel_bl_set_brightness(panel_bl, (value == PANEL_HMD_ON) ?
			PANEL_BL_SUBDEV_TYPE_HMD : PANEL_BL_SUBDEV_TYPE_DISP, SEND_CMD);
	if (ret < 0)
		panel_err("failed to set %s brightness\n",
				(value == PANEL_HMD_ON) ? "hmd" : "normal");

	panel_info("hmd_on %d -> %d\n", state->hmd_on, value);
	state->hmd_on = value;
exit:
	mutex_unlock(&panel->op_lock);
	mutex_unlock(&panel_bl->lock);
	if (ret < 0)
		return ret;
	return size;
}
#endif /* CONFIG_SUPPORT_HMD */

#ifdef CONFIG_EXYNOS_DOZE
static int set_alpm_mode(struct panel_device *panel, int mode)
{
	int ret = 0;
#ifdef CONFIG_SUPPORT_AOD_BL
	int lpm_ver = (mode & 0x00FF0000) >> 16;
#endif
	int lpm_mode = (mode & 0xFF);
#ifdef CONFIG_SEC_FACTORY
	static int backup_br;
#endif
	struct panel_info *panel_data = &panel->panel_data;
	struct panel_bl_device *panel_bl = &panel->panel_bl;
#if defined(CONFIG_SEC_FACTORY) || defined(CONFIG_SUPPORT_AOD_BL)
	struct backlight_device *bd = panel_bl->bd;
#endif
	switch (lpm_mode) {
	case ALPM_OFF:
#ifdef CONFIG_SEC_FACTORY
		ret = panel_seq_exit_alpm(panel);
		if (ret)
			panel_err("failed to write set_alpm\n");
		if (backup_br)
			bd->props.brightness = backup_br;
#endif
		panel_data->props.alpm_mode = lpm_mode;
		panel_update_brightness(panel);
#ifdef CONFIG_SEC_FACTORY
		msleep(34);
#endif
		break;
	case ALPM_LOW_BR:
	case HLPM_LOW_BR:
	case ALPM_HIGH_BR:
	case HLPM_HIGH_BR:
		panel_data->props.alpm_mode = lpm_mode;

#ifndef CONFIG_SEC_FACTORY
		if (panel->state.cur_state != PANEL_STATE_ALPM) {
			panel_info("panel state(%d) is not lpm mode\n",
					panel->state.cur_state);
			return ret;
		}
#endif
#ifdef CONFIG_SEC_FACTORY
		backup_br = bd->props.brightness;
#endif
#ifdef CONFIG_SUPPORT_AOD_BL
		if (lpm_ver == 0) {
			bd->props.brightness =
				(lpm_mode <= HLPM_LOW_BR) ? BRT(0) : BRT(94);
			panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness =
				(lpm_mode <= HLPM_LOW_BR) ? BRT(0) : BRT(94);
		}
#endif
#ifdef CONFIG_SEC_FACTORY
		ret = panel_seq_set_alpm(panel);
		if (ret)
			panel_err("failed to set_alpm\n");
#endif
		break;
	default:
		panel_err("invalid alpm_mode: %d\n", lpm_mode);
		break;
	}

	return ret;
}
#endif

static ssize_t alpm_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int value, rc;
	struct panel_device *panel = dev_get_drvdata(dev);
	struct panel_info *panel_data = &panel->panel_data;

	panel_info("++\n");
	mutex_lock(&panel->io_lock);
	rc = kstrtoint(buf, 0, &value);
	if (rc < 0) {
		panel_warn("invalid param (ret %d)\n", rc);
		mutex_unlock(&panel->io_lock);
		return rc;
	}

#ifdef CONFIG_EXYNOS_DOZE
	rc = set_alpm_mode(panel, value);
	if (rc)
		panel_err("failed to set alpm (value %d, ret %d)\n", value, rc);
#endif

	mutex_unlock(&panel->io_lock);
	panel_info("value %d, alpm_mode %d\n", value, panel_data->props.alpm_mode);
	return size;
}

static ssize_t alpm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	snprintf(buf, PAGE_SIZE, "%d\n", panel_data->props.alpm_mode);
	panel_dbg("%d\n", panel_data->props.alpm_mode);

	return strlen(buf);
}

static ssize_t lpm_opr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int value, rc;
	struct panel_device *panel = dev_get_drvdata(dev);
	struct panel_info *panel_data = &panel->panel_data;

	mutex_lock(&panel->io_lock);
	rc = kstrtoint(buf, 0, &value);
	if (rc < 0) {
		panel_warn("invalid param (ret %d)\n", rc);
		mutex_unlock(&panel->io_lock);
		return rc;
	}

	mutex_lock(&panel->op_lock);
	panel_data->props.lpm_opr = value;
	mutex_unlock(&panel->op_lock);
	panel_update_brightness(panel);

	panel_info("value %d, lpm_opr %d\n",
			value, panel_data->props.lpm_opr);

	mutex_unlock(&panel->io_lock);
	return size;
}

static ssize_t lpm_opr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	snprintf(buf, PAGE_SIZE, "%d\n", panel_data->props.lpm_opr);

	return strlen(buf);
}

static ssize_t conn_det_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int value, rc;
	struct panel_device *panel = dev_get_drvdata(dev);

	rc = kstrtoint(buf, 0, &value);
	if (rc < 0) {
		panel_warn("invalid param (ret %d)\n", rc);
		return rc;
	}

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	if (!panel_gpio_valid(&panel->gpio[PANEL_GPIO_CONN_DET])) {
		panel_err("conn det is unsupported\n");
		return -EINVAL;
	}

	if (panel->panel_data.props.conn_det_enable != value) {
		panel->panel_data.props.conn_det_enable = value;
		panel_info("set %d %s\n",
				panel->panel_data.props.conn_det_enable,
				panel->panel_data.props.conn_det_enable ? "enable" : "disable");
		if (panel->panel_data.props.conn_det_enable) {
			if (ub_con_disconnected(panel))
				panel_send_ubconn_uevent(panel);
		}
	} else {
		panel_info("already set %d %s\n",
				panel->panel_data.props.conn_det_enable,
				panel->panel_data.props.conn_det_enable ? "enable" : "disable");
	}
	return size;
}

static ssize_t conn_det_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	if (panel_gpio_valid(&panel->gpio[PANEL_GPIO_CONN_DET]))
		snprintf(buf, PAGE_SIZE, "%s\n",
				ub_con_disconnected(panel) ? "disconnected" : "connected");
	else
		snprintf(buf, PAGE_SIZE, "%d\n", -1);
	panel_info("%s", buf);
	return strlen(buf);
}

#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
static ssize_t lux_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	snprintf(buf, PAGE_SIZE, "%d\n", panel_data->props.lux);

	return strlen(buf);
}

static ssize_t lux_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int rc;
	int value;
	struct mdnie_info *mdnie;
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	mdnie = &panel->mdnie;
	panel_data = &panel->panel_data;

	rc = kstrtoint(buf, 0, &value);

	if (rc < 0)
		return rc;

	if (panel_data->props.lux != value) {
		mutex_lock(&panel->op_lock);
		panel_data->props.lux = value;
		mutex_unlock(&panel->op_lock);
		attr_store_for_each(mdnie->class, attr->attr.name, buf, size);
	}

	return size;
}
#endif /* CONFIG_EXYNOS_DECON_MDNIE_LITE */

#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
static ssize_t copr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_device *panel = dev_get_drvdata(dev);
	struct copr_info *copr = &panel->copr;

	return copr_reg_show(copr, buf);
}

static ssize_t copr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct panel_device *panel = dev_get_drvdata(dev);
	struct copr_info *copr = &panel->copr;
	char *p, *arg = (char *)buf;
	const char *name;
	int index, rc;
	u32 value;

	mutex_lock(&copr->lock);
	while ((p = strsep(&arg, " \t")) != NULL) {
		if (!*p)
			continue;
		index = find_copr_reg_by_name(copr->props.version, p);
		if (index < 0) {
			panel_err("arg(%s) not found\n", p);
			continue;
		}

		name = get_copr_reg_name(copr->props.version, index);
		if (name == NULL) {
			panel_err("arg(%s) not found\n", p);
			continue;
		}

		rc = sscanf(p + strlen(name), "%i", &value);
		if (rc != 1) {
			panel_err("invalid argument name:%s ret:%d\n", name, rc);
			continue;
		}

		rc = copr_reg_store(copr, index, value);
		if (rc < 0) {
			panel_err("failed to store to copr_reg (index %d, value %d)\n",
					index, value);
			continue;
		}
	}

	copr->props.state = COPR_UNINITIALIZED;
	copr_update_average(copr);
	panel_info("copr %s\n", get_copr_reg_copr_en(copr) ? "enable" : "disable");
	mutex_unlock(&copr->lock);
	copr_update_start(&panel->copr, 1);

	return size;
}

static ssize_t read_copr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct panel_device *panel = dev_get_drvdata(dev);
	struct copr_info *copr = &panel->copr;
	int cur_copr;

	if (!IS_PANEL_ACTIVE(panel)) {
		panel_err("panel is not active\n");
		return snprintf(buf, PAGE_SIZE, "-1\n");
	}

	if (!copr_is_enabled(copr)) {
		panel_err("copr is off state\n");
		return snprintf(buf, PAGE_SIZE, "-1\n");
	}

	cur_copr = copr_get_value(copr);
	if (cur_copr < 0) {
		panel_err("failed to get copr\n");
		return snprintf(buf, PAGE_SIZE, "-1\n");
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", cur_copr);
}

static ssize_t copr_roi_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct panel_device *panel = dev_get_drvdata(dev);
	struct copr_info *copr = &panel->copr;
	struct copr_roi roi[6];
	int nr_roi, rc = 0, i;

	memset(roi, -1, sizeof(roi));
	if (copr->props.version == COPR_VER_1 ||
			copr->props.version == COPR_VER_2) {
		rc = sscanf(buf, "%i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i",
				&roi[0].roi_xs, &roi[0].roi_ys, &roi[0].roi_xe, &roi[0].roi_ye,
				&roi[1].roi_xs, &roi[1].roi_ys, &roi[1].roi_xe, &roi[1].roi_ye,
				&roi[2].roi_xs, &roi[2].roi_ys, &roi[2].roi_xe, &roi[2].roi_ye,
				&roi[3].roi_xs, &roi[3].roi_ys, &roi[3].roi_xe, &roi[3].roi_ye);
		if (rc < 0) {
			panel_err("invalid roi input(rc %d)\n", rc);
			return -EINVAL;
		}
		nr_roi = rc / 4;
	} else if (copr->props.version == COPR_VER_3) {
		rc = sscanf(buf, "%i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i",
				&roi[0].roi_xs, &roi[0].roi_ys, &roi[0].roi_xe, &roi[0].roi_ye,
				&roi[1].roi_xs, &roi[1].roi_ys, &roi[1].roi_xe, &roi[1].roi_ye,
				&roi[2].roi_xs, &roi[2].roi_ys, &roi[2].roi_xe, &roi[2].roi_ye,
				&roi[3].roi_xs, &roi[3].roi_ys, &roi[3].roi_xe, &roi[3].roi_ye,
				&roi[4].roi_xs, &roi[4].roi_ys, &roi[4].roi_xe, &roi[4].roi_ye);
		if (rc < 0) {
			panel_err("invalid roi input(rc %d)\n", rc);
			return -EINVAL;
		}
		if (rc == 20) {
			/* roi5&6 must be same in copr ver3.0 */
			memcpy(&roi[5], &roi[4], sizeof(struct copr_roi));
		}
		nr_roi = rc / 4;
	} else if (copr->props.version == COPR_VER_5) {
		rc = sscanf(buf, "%i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i",
				&roi[0].roi_xs, &roi[0].roi_ys, &roi[0].roi_xe, &roi[0].roi_ye,
				&roi[1].roi_xs, &roi[1].roi_ys, &roi[1].roi_xe, &roi[1].roi_ye,
				&roi[2].roi_xs, &roi[2].roi_ys, &roi[2].roi_xe, &roi[2].roi_ye,
				&roi[3].roi_xs, &roi[3].roi_ys, &roi[3].roi_xe, &roi[3].roi_ye,
				&roi[4].roi_xs, &roi[4].roi_ys, &roi[4].roi_xe, &roi[4].roi_ye);
		if (rc < 0) {
			panel_err("invalid roi input(rc %d)\n", rc);
			return -EINVAL;
		}
		nr_roi = rc / 4;
	} else if (copr->props.version == COPR_VER_6) {
		rc = sscanf(buf, "%i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i",
				&roi[0].roi_er, &roi[0].roi_eg, &roi[0].roi_eb, &roi[0].roi_xs, &roi[0].roi_ys, &roi[0].roi_xe, &roi[0].roi_ye,
				&roi[1].roi_er, &roi[1].roi_eg, &roi[1].roi_eb, &roi[1].roi_xs, &roi[1].roi_ys, &roi[1].roi_xe, &roi[1].roi_ye,
				&roi[2].roi_er, &roi[2].roi_eg, &roi[2].roi_eb, &roi[2].roi_xs, &roi[2].roi_ys, &roi[2].roi_xe, &roi[2].roi_ye,
				&roi[3].roi_er, &roi[3].roi_eg, &roi[3].roi_eb, &roi[3].roi_xs, &roi[3].roi_ys, &roi[3].roi_xe, &roi[3].roi_ye,
				&roi[4].roi_er, &roi[4].roi_eg, &roi[4].roi_eb, &roi[4].roi_xs, &roi[4].roi_ys, &roi[4].roi_xe, &roi[4].roi_ye);
		if (rc < 0) {
			panel_err("invalid roi input(rc %d)\n", rc);
			return -EINVAL;
		}
		nr_roi = rc / 7;
	} else {
		panel_err("roi is unsupported in copr ver%d\n", copr->props.version);
		return -EINVAL;
	}

	mutex_lock(&copr->lock);
	for (i = 0; i < nr_roi; i++) {
		if ((int)roi[i].roi_xs == -1 || (int)roi[i].roi_ys == -1 ||
			(int)roi[i].roi_xe == -1 || (int)roi[i].roi_ye == -1)
			continue;
		else
			memcpy(&copr->props.roi[i], &roi[i], sizeof(struct copr_roi));
	}
	if (copr->props.version == COPR_VER_2 ||
			copr->props.version == COPR_VER_1)
		copr->props.nr_roi = nr_roi;
	mutex_unlock(&copr->lock);

	if (copr->props.version == COPR_VER_3 ||
		copr->props.version == COPR_VER_5 ||
		copr->props.version == COPR_VER_6) {
		/* apply roi at once in copr ver3.0 ~ ver6.0 */
		copr_roi_set_value(copr, copr->props.roi,
				copr->props.nr_roi);
	}

	if (copr->props.version == COPR_VER_6) {
		for (i = 0; i < nr_roi; i++)
			panel_info("set roi[%d] %d %d %d %d %d %d %d\n",
					i, roi[i].roi_er, roi[i].roi_eg, roi[i].roi_eb,
					roi[i].roi_xs, roi[i].roi_ys,
					roi[i].roi_xe, roi[i].roi_ye);

		for (i = 0; i < copr->props.nr_roi; i++)
			panel_info("cur roi[%d] %d %d %d %d %d %d %d\n",
					i, copr->props.roi[i].roi_er, copr->props.roi[i].roi_eg,
					copr->props.roi[i].roi_eb,
					copr->props.roi[i].roi_xs, copr->props.roi[i].roi_ys,
					copr->props.roi[i].roi_xe, copr->props.roi[i].roi_ye);
	} else {
		for (i = 0; i < nr_roi; i++)
			panel_info("set roi[%d] %d %d %d %d\n",
					i, roi[i].roi_xs, roi[i].roi_ys,
					roi[i].roi_xe, roi[i].roi_ye);

		for (i = 0; i < copr->props.nr_roi; i++)
			panel_info("cur roi[%d] %d %d %d %d\n",
					i, copr->props.roi[i].roi_xs, copr->props.roi[i].roi_ys,
					copr->props.roi[i].roi_xe, copr->props.roi[i].roi_ye);
	}

	return size;
}

static ssize_t copr_roi_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_device *panel = dev_get_drvdata(dev);
	struct copr_info *copr = &panel->copr;
	int i, c, ret, len = 0;
	int max_color = (copr->props.version == COPR_VER_6) ?
		MAX_RGBW_COLOR : MAX_COLOR;
	u32 out[5 * 4] = { 0, };

	if (copr->props.nr_roi == 0) {
		panel_warn("copr roi disabled\n");
		return -ENODEV;
	}

	if (!copr_is_enabled(copr)) {
		panel_err("copr is off state\n");
		return snprintf(buf, PAGE_SIZE, "-1\n");
	}

	if (!IS_PANEL_ACTIVE(panel)) {
		panel_err("panel is not active\n");
		return snprintf(buf, PAGE_SIZE, "-1\n");
	}

	ret = copr_roi_get_value(copr, copr->props.roi,
			copr->props.nr_roi, out);
	if (ret < 0) {
		panel_err("failed to get copr\n");
		return snprintf(buf, PAGE_SIZE, "-1\n");
	}

	for (i = 0; i < copr->props.nr_roi; i++) {
		for (c = 0; c < max_color; c++) {
			len += snprintf(buf + len, PAGE_SIZE - len,
					"%d%s", out[i * max_color + c],
					((i == copr->props.nr_roi - 1) && c == max_color - 1) ? "\n" : " ");
		}
	}

	return len;
}

static ssize_t brt_avg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_device *panel = dev_get_drvdata(dev);
	struct panel_bl_device *panel_bl = &panel->panel_bl;
	int brt_avg;

	if (!IS_PANEL_ACTIVE(panel)) {
		panel_err("panel is not active\n");
		return snprintf(buf, PAGE_SIZE, "-1\n");
	}

	brt_avg = panel_bl_get_average_and_clear(panel_bl, 1);
	if (brt_avg < 0) {
		panel_err("failed to get average brt1\n");
		return snprintf(buf, PAGE_SIZE, "-1\n");
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", brt_avg);
}
#endif

#ifdef CONFIG_DISPLAY_USE_INFO
/*
 * HW PARAM LOGGING SYSFS NODE
 */
static ssize_t dpui_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;

	update_dpui_log(DPUI_LOG_LEVEL_INFO, DPUI_TYPE_PANEL);
	ret = get_dpui_log(buf, DPUI_LOG_LEVEL_INFO, DPUI_TYPE_PANEL);
	if (ret < 0) {
		panel_err("failed to get log %d\n", ret);
		return ret;
	}

	panel_info("%s\n", buf);
	return ret;
}

static ssize_t dpui_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	if (buf[0] == 'C' || buf[0] == 'c')
		clear_dpui_log(DPUI_LOG_LEVEL_INFO, DPUI_TYPE_PANEL);

	return size;
}

/*
 * [DEV ONLY]
 * HW PARAM LOGGING SYSFS NODE
 */
static ssize_t dpui_dbg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;

	update_dpui_log(DPUI_LOG_LEVEL_DEBUG, DPUI_TYPE_PANEL);
	ret = get_dpui_log(buf, DPUI_LOG_LEVEL_DEBUG, DPUI_TYPE_PANEL);
	if (ret < 0) {
		panel_err("failed to get log %d\n", ret);
		return ret;
	}

	panel_info("%s\n", buf);
	return ret;
}

static ssize_t dpui_dbg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	if (buf[0] == 'C' || buf[0] == 'c')
		clear_dpui_log(DPUI_LOG_LEVEL_DEBUG, DPUI_TYPE_PANEL);

	return size;
}

/*
 * [AP DEPENDENT ONLY]
 * HW PARAM LOGGING SYSFS NODE
 */
static ssize_t dpci_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;

	update_dpui_log(DPUI_LOG_LEVEL_INFO, DPUI_TYPE_CTRL);
	ret = get_dpui_log(buf, DPUI_LOG_LEVEL_INFO, DPUI_TYPE_CTRL);
	if (ret < 0) {
		panel_err("failed to get log %d\n", ret);
		return ret;
	}

	panel_info("%s\n", buf);
	return ret;
}

static ssize_t dpci_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	if (buf[0] == 'C' || buf[0] == 'c')
		clear_dpui_log(DPUI_LOG_LEVEL_INFO, DPUI_TYPE_CTRL);

	return size;
}

/*
 * [AP DEPENDENT DEV ONLY]
 * HW PARAM LOGGING SYSFS NODE
 */
static ssize_t dpci_dbg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;

	update_dpui_log(DPUI_LOG_LEVEL_DEBUG, DPUI_TYPE_CTRL);
	ret = get_dpui_log(buf, DPUI_LOG_LEVEL_DEBUG, DPUI_TYPE_CTRL);
	if (ret < 0) {
		panel_err("failed to get log %d\n", ret);
		return ret;
	}

	panel_info("%s\n", buf);
	return ret;
}

static ssize_t dpci_dbg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	if (buf[0] == 'C' || buf[0] == 'c')
		clear_dpui_log(DPUI_LOG_LEVEL_DEBUG, DPUI_TYPE_CTRL);

	return size;
}
#endif

static ssize_t poc_onoff_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	snprintf(buf, PAGE_SIZE, "%d\n", panel_data->props.poc_onoff);

	return strlen(buf);
}

static ssize_t poc_onoff_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int rc;
	int value;
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	rc = kstrtoint(buf, 0, &value);

	if (rc < 0)
		return rc;

	panel_info("%d -> %d\n", panel_data->props.poc_onoff, value);

	if (panel_data->props.poc_onoff != value) {
		mutex_lock(&panel->panel_bl.lock);
		panel_data->props.poc_onoff = value;
		mutex_unlock(&panel->panel_bl.lock);
		panel_update_brightness(panel);
	}

	return size;
}

#ifdef CONFIG_EXTEND_LIVE_CLOCK

static ssize_t self_mask_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct aod_dev_info *aod;
	struct aod_ioctl_props *props;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	aod = &panel->aod;
	props = &aod->props;

	snprintf(buf, PAGE_SIZE, "%d\n", props->self_mask_en);

	return strlen(buf);
}

static ssize_t self_mask_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int rc, ret;
	int value;
	struct aod_dev_info *aod;
	struct aod_ioctl_props *props;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	aod = &panel->aod;
	props = &aod->props;
	if ((aod == NULL) || (props == NULL)) {
		panel_err("aod is null\n");
		return -EINVAL;
	}

	rc = kstrtoint(buf, 0, &value);

	if (rc < 0)
		return rc;

	panel_info("%d -> %d\n", props->self_mask_en, value);

	if (props->self_mask_en != value) {
		if (value == 0) {
			ret = panel_do_aod_seqtbl_by_index(aod, SELF_MASK_DIS_SEQ);
			if (unlikely(ret < 0))
				panel_err("failed to disable self mask\n");
		} else {
			ret = panel_do_aod_seqtbl_by_index(aod, SELF_MASK_IMG_SEQ);
			if (unlikely(ret < 0))
				panel_err("failed to write self mask image\n");

			ret = panel_do_aod_seqtbl_by_index(aod, SELF_MASK_ENA_SEQ);
			if (unlikely(ret < 0))
				panel_err("failed to enable self mask\n");
		}
		props->self_mask_en = value;
	}

	return size;
}

static void prepare_self_mask_check(struct panel_device *panel)
{
	int ret = 0;

	panel_set_bypass(panel, true);
	ret = panel_set_gpio_irq(&panel->gpio[PANEL_GPIO_DISP_DET], false);
	if (ret < 0)
		panel_warn("do not support irq\n");
}

static void clear_self_mask_check(struct panel_device *panel)
{
	int ret = 0;

	ret = panel_set_gpio_irq(&panel->gpio[PANEL_GPIO_DISP_DET], true);
	if (ret < 0)
		panel_warn("do not support irq\n");
	panel_set_bypass(panel, false);
}

static ssize_t self_mask_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_device *panel = dev_get_drvdata(dev);
	struct aod_dev_info *aod;
	struct panel_info *panel_data;
	u8 success_check = 1;
	u8 *recv_checksum = NULL;
	int ret = 0, i = 0;
	int len = 0;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	aod = &panel->aod;
	panel_data = &panel->panel_data;

	if (aod->props.self_mask_checksum_len) {
		recv_checksum = kmalloc_array(aod->props.self_mask_checksum_len, sizeof(u8), GFP_KERNEL);
		if (!recv_checksum)
			return -ENOMEM;
		prepare_self_mask_check(panel);

		ret = panel_do_aod_seqtbl_by_index(aod, SELF_MASK_CHECKSUM_SEQ);
		if (unlikely(ret < 0)) {
			panel_err("failed to send cmd selfmask checksum\n");
			kfree(recv_checksum);
			return ret;
		}

		ret = resource_copy_n_clear_by_name(panel_data,	recv_checksum, "self_mask_checksum");
		if (unlikely(ret < 0)) {
			panel_err("failed to get selfmask checksum\n");
			kfree(recv_checksum);
			return ret;
		}
		clear_self_mask_check(panel);

		for (i = 0; i < aod->props.self_mask_checksum_len; i++) {
			if (aod->props.self_mask_checksum[i] != recv_checksum[i]) {
				success_check = 0;
				break;
			}
		}
		len = snprintf(buf, PAGE_SIZE, "%d", success_check);
		for (i = 0; i < aod->props.self_mask_checksum_len; i++)
			len += snprintf(buf + len, PAGE_SIZE - len, " %02x", recv_checksum[i]);
		len += snprintf(buf + len, PAGE_SIZE - len, "\n");
		kfree(recv_checksum);
	} else {
		snprintf(buf, PAGE_SIZE, "-1\n");
	}
	return strlen(buf);
}

#endif

#ifdef SUPPORT_NORMAL_SELF_MOVE
static ssize_t self_move_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct aod_dev_info *aod;
	struct aod_ioctl_props *props;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	aod = &panel->aod;
	props = &aod->props;

	snprintf(buf, PAGE_SIZE, "%d\n", props->self_move_pattern);

	return strlen(buf);
}

static ssize_t self_move_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	int pattern;
	struct aod_dev_info *aod;
	struct aod_ioctl_props *props;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	aod = &panel->aod;
	props = &aod->props;
	if ((aod == NULL) || (props == NULL)) {
		panel_err("aod is null\n");
		return -EINVAL;
	}

	ret = kstrtoint(buf, 0, &pattern);
	if (ret < 0)
		return ret;

	if (pattern < NORMAL_SELF_MOVE_PATTERN_OFF ||
		pattern >= MAX_NORMAL_SELF_MOVE_PATTERN) {
		panel_err("out of range(%d)\n", pattern);
		return -EINVAL;
	}

	panel_info("pattern : %d\n", pattern);
	mutex_lock(&panel->io_lock);
	props->self_move_pattern = pattern;
	ret = panel_self_move_pattern_update(panel);
	if (ret < 0)
		panel_info("failed to set self move pattern\n");

	mutex_unlock(&panel->io_lock);
	return size;
}
#endif

#ifdef CONFIG_SUPPORT_ISC_DEFECT
static ssize_t isc_defect_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	snprintf(buf, PAGE_SIZE, "support isc defect checkd\n");

	return 0;
}

static ssize_t isc_defect_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int rc, ret;
	int value;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	rc = kstrtoint(buf, 0, &value);

	if (rc < 0)
		return rc;

	panel_info("%d\n", value);

	mutex_lock(&panel->op_lock);

	if (value) {
		ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_CHECK_ISC_DEFECT_SEQ);
		if (unlikely(ret < 0))
			panel_err("failed to write ics defect seq\n");
	}

	mutex_unlock(&panel->op_lock);
	return size;
}
#endif

#ifdef CONFIG_SUPPORT_BRIGHTDOT_TEST
static ssize_t brightdot_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_device *panel = dev_get_drvdata(dev);
	struct panel_info *panel_data = &panel->panel_data;

	snprintf(buf, PAGE_SIZE, "%u\n", panel_data->props.brightdot_test_enable);

	return strlen(buf);
}

static ssize_t brightdot_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int rc, ret;
	u32 value = 0;
	struct panel_device *panel = dev_get_drvdata(dev);
	struct panel_info *panel_data;

	panel_data = &panel->panel_data;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	rc = kstrtouint(buf, 0, &value);

	if (rc < 0)
		return rc;

	mutex_lock(&panel->op_lock);
	panel_info("%u -> %u\n", panel_data->props.brightdot_test_enable, value);
	panel_data->props.brightdot_test_enable = value;

	ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_BRIGHTDOT_TEST_SEQ);
	if (unlikely(ret < 0))
		panel_err("failed to write brightdot seq\n");

	mutex_unlock(&panel->op_lock);

	return size;
}
#endif

#ifdef CONFIG_SUPPORT_SPI_IF_SEL
static ssize_t spi_if_sel_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t spi_if_sel_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int rc, ret;
	int value;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	rc = kstrtoint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (check_seqtbl_exist(&panel->panel_data,
			value ? PANEL_SPI_IF_ON_SEQ : PANEL_SPI_IF_OFF_SEQ) <= 0) {
		panel_info("spi if on/off unsupported\n");
		return size;
	}

	panel_info("%d\n", value);
	mutex_lock(&panel->op_lock);
	ret = panel_do_seqtbl_by_index_nolock(panel,
			value ? PANEL_SPI_IF_ON_SEQ : PANEL_SPI_IF_OFF_SEQ);
	if (unlikely(ret < 0))
		panel_err("failed to write spi-if-%s seq\n", value ? "on" : "off");

	mutex_unlock(&panel->op_lock);
	return size;
}
#endif

#ifdef CONFIG_SUPPORT_CCD_TEST
#define CCD_STATE_SIZE 4
static ssize_t ccd_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);
	struct resinfo *info;
	int ret = 0, retVal = 0, ccd_size, i;
	size_t ires;
	enum {
		CCD_CHKSUM_PASS,
		CCD_CHKSUM_FAIL,
	};
	static const char * const ccd_resource_name[] = {
		[CCD_CHKSUM_PASS] = "ccd_chksum_pass",
		[CCD_CHKSUM_FAIL] = "ccd_chksum_fail",
	};
	u8 ccd_state[CCD_STATE_SIZE] = { 0x12, 0x34, 0x56, 0x78 };
	u8 ccd_compare[CCD_STATE_SIZE] = { 0x87, 0x65, 0x43, 0x21 };

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	info = find_panel_resource(panel_data, "ccd_state");
	if (unlikely(info == NULL)) {
		panel_err("failed to get ccd resource\n");
		return -EINVAL;
	}

	ccd_size = info->dlen;
	if (ccd_size < 1 || ccd_size > CCD_STATE_SIZE) {
		panel_err("invalid ccd size %d %d\n", ccd_size, CCD_STATE_SIZE);
		return -EINVAL;
	}

	ret = panel_do_seqtbl_by_index(panel, PANEL_CCD_TEST_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write ccd seq\n");
		return ret;
	}
	ret = resource_copy_n_clear_by_name(panel_data, ccd_state, "ccd_state");
	if (unlikely(ret < 0)) {
		panel_err("failed to read ccd_state \n");
		return ret;
	}

	for (ires = 0; ires < ARRAY_SIZE(ccd_resource_name); ires++) {
		info = find_panel_resource(panel_data, (char *)ccd_resource_name[ires]);
		if (info)
			break;
	}

	if (ires != ARRAY_SIZE(ccd_resource_name)) {
		if (info->dlen != ccd_size) {
			panel_err("%s: size mismatch %d %d\n",
					ccd_resource_name[ires], info->dlen, ccd_size);
			return -EINVAL;
		}
		if (rescpy(ccd_compare, info, 0, ccd_size) < 0)
			return -EINVAL;

		if (ires == CCD_CHKSUM_PASS) {
			retVal = memcmp(ccd_state, ccd_compare, ccd_size) == 0 ? 1 : 0;
			panel_info("p_comp %s\n", (retVal == 1) ? "Pass" : "Fail");
		} else {
			retVal = memcmp(ccd_state, ccd_compare, ccd_size) == 0 ? 0 : 1;
			panel_info("f_comp %s\n", (retVal == 1) ? "Pass" : "Fail");
		}
		for (i = 0; i < ccd_size; i++)
			panel_info("[%d] 0x%02x 0x%02x\n", i, ccd_state[i], ccd_compare[i]);
	} else {
		/* support previous panel, compare with first 1byte: 0x00(pass) */
		retVal = (ccd_state[0] == 0x00) ? 1 : 0;
		panel_info("comp %s 0x%02x %d\n", (retVal == 1) ? "Pass" : "Fail", ccd_state[0], retVal);
	}

	snprintf(buf, PAGE_SIZE, "%d\n", retVal);
	return strlen(buf);
}
#endif

#ifdef CONFIG_SUPPORT_DYNAMIC_HLPM
static ssize_t dynamic_hlpm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	snprintf(buf, PAGE_SIZE, "%u\n",
			panel_data->props.dynamic_hlpm);

	return strlen(buf);
}

static ssize_t dynamic_hlpm_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int value, rc, ret;
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;
	if ((panel_data->props.alpm_mode != HLPM_HIGH_BR) && (panel_data->props.alpm_mode != HLPM_LOW_BR)) {
		panel_info("please set HLPM mode %d\n", panel_data->props.alpm_mode);
		return size;
	}

	if (panel_data->props.dynamic_hlpm == value)
		return size;

	mutex_lock(&panel->op_lock);
	panel_data->props.dynamic_hlpm = value;
	mutex_unlock(&panel->op_lock);

	ret = panel_do_seqtbl_by_index(panel,
			value ? PANEL_DYNAMIC_HLPM_ON_SEQ : PANEL_DYNAMIC_HLPM_OFF_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write dynamic_hlpm on/off seq\n");
		return ret;
	}
	panel_info("dynamic hlpm %s\n",
			panel_data->props.dynamic_hlpm ? "on" : "off");

	return size;
}
#endif

#ifdef CONFIG_DYNAMIC_MIPI
static ssize_t dynamic_mipi_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_device *panel = dev_get_drvdata(dev);
	struct dm_status_info *dm_status;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	dm_status = &panel->dynamic_mipi.dm_status;

	snprintf(buf, PAGE_SIZE, "req: %d cur: %d, req_osc: %d, cur_osc: %d\n",
		dm_status->request_df, dm_status->current_df,
		dm_status->request_ddi_osc, dm_status->current_ddi_osc);

	return strlen(buf);
}

static ssize_t dynamic_mipi_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int osc;
	int value, rc;
	struct panel_device *panel = dev_get_drvdata(dev);
	struct dm_status_info *dm_status;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	dm_status = &panel->dynamic_mipi.dm_status;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (value < 0) {
		panel_err("value is negative : %d\n", value);
		return -EINVAL;
	}

	osc = (value & 0x4) >> 2;
	value = value & 0x3;

	panel_info("osc: %d, value: %d\n", osc, value);
	dm_status->request_ddi_osc = osc;

	update_dynamic_mipi(panel, value);

	return size;
}
#endif

static ssize_t vrr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);
	struct panel_properties *props;
	struct panel_vrr *vrr;
	int vrr_fps, vrr_mode;
	int div_count;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;
	props = &panel->panel_data.props;

	if (!panel_vrr_is_supported(panel))
		return snprintf(buf, PAGE_SIZE, "60 0\n");

	vrr = get_panel_vrr(panel);
	if (vrr == NULL)
		return -EINVAL;

	div_count = max(TE_SKIP_TO_DIV(vrr->te_sw_skip_count,
				vrr->te_hw_skip_count), MIN_VRR_DIV_COUNT);
	vrr_fps = vrr->fps;
	vrr_mode = vrr->mode;
	panel_info("display(%d%s) panel(%d%s)\n",
			vrr_fps / div_count, REFRESH_MODE_STR(vrr_mode),
			vrr_fps, REFRESH_MODE_STR(vrr_mode));
	snprintf(buf, PAGE_SIZE, "%d %d\n",
			vrr_fps / div_count, vrr_mode);

	return strlen(buf);
}

#if defined(CONFIG_PANEL_DISPLAY_MODE)
static ssize_t display_mode_show(struct device *dev,
		    struct device_attribute *attr, char *buf)
{
	struct panel_device *panel = dev_get_drvdata(dev);
	struct panel_display_modes *panel_modes;
	struct common_panel_display_modes *common_panel_modes;
	struct panel_properties *props;
	int i, len = 0;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	props = &panel->panel_data.props;
	panel_modes = panel->panel_modes;
	if (!panel->panel_modes) {
		len += snprintf(buf + len, PAGE_SIZE - len,
				"panel_display_modes empty!!\n");
	} else {
		for (i = 0; i < panel_modes->num_modes; i++) {
			if (!panel_modes->modes[i])
				continue;
			len += snprintf(buf + len, PAGE_SIZE - len, "pdm:%d %s\n",
					i, panel_modes->modes[i]->name);
		}
	}

	common_panel_modes = panel->panel_data.common_panel_modes;
	if (!common_panel_modes) {
		len += snprintf(buf + len, PAGE_SIZE - len,
				"common_panel_display_modes empty!!\n");
	} else {
		for (i = 0; i < common_panel_modes->num_modes; i++) {
			if (!common_panel_modes->modes[i])
				continue;
			len += snprintf(buf + len, PAGE_SIZE - len, "cpdm:%d %s\n",
					i, common_panel_modes->modes[i]->name);
		}
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "panel_mode:%d\n",
			props->panel_mode);


	return len;
}

static ssize_t display_mode_store(struct device *dev,
		    struct device_attribute *attr, const char *buf, size_t size)
{
	struct panel_device *panel = dev_get_drvdata(dev);
	struct common_panel_display_modes *common_panel_modes;
	struct panel_properties *props;
	int rc, panel_mode = 0;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	props = &panel->panel_data.props;
	common_panel_modes = panel->panel_data.common_panel_modes;
	rc = kstrtoint(buf, 0, &panel_mode);
	if (rc < 0)
		return -EINVAL;

	mutex_lock(&panel->op_lock);
	if (panel_mode < 0 ||
			panel_mode >= common_panel_modes->num_modes) {
		panel_err("panel_mode(%d) exceeded num_modes(%d)\n",
				panel_mode, common_panel_modes->num_modes);
		mutex_unlock(&panel->op_lock);
		return -EINVAL;
	}

	props->panel_mode = panel_mode;
	mutex_unlock(&panel->op_lock);
	rc = panel_update_display_mode(panel);
	if (rc < 0)
		panel_err("failed to panel_update_display_mode\n");

	return size;
}
#endif

#ifdef CONFIG_PANEL_VRR_BRIDGE
static ssize_t vrr_bridge_show(struct device *dev,
		    struct device_attribute *attr, char *buf)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	return snprintf(buf, PAGE_SIZE, "%d\n",
			panel_data->props.vrr_bridge_enable);
}

static ssize_t vrr_bridge_store(struct device *dev,
		    struct device_attribute *attr, const char *buf, size_t size)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);
	int rc, enable = 0;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	rc = kstrtoint(buf, 0, &enable);
	if (rc < 0)
		return -EINVAL;

	mutex_lock(&panel->io_lock);
	panel_data->props.vrr_bridge_enable = !!enable;
	panel_info("vrr_bridge_enable %d\n", enable);
	mutex_unlock(&panel->io_lock);

	return size;
}
#endif

ssize_t snprint_vrr_lfd(struct panel_device *panel, char *buf, ssize_t size)
{
	struct panel_properties *props = &panel->panel_data.props;
	ssize_t len = 0;
	int i = 0, scope = 0;
	bool updated;
	const char *client_name = NULL;
	const char *scope_name = NULL;

	len += snprintf(buf + len, size - len, "[req]\n");
	updated = false;
	for (i = 0; i < MAX_VRR_LFD_CLIENT; i++) {
		for (scope = 0; scope < MAX_VRR_LFD_SCOPE; scope++) {
			client_name = get_vrr_lfd_client_name(i);
			scope_name = get_vrr_lfd_scope_name(scope);
			if (client_name == NULL || scope_name == NULL)
				continue;

			if (props->vrr_lfd_info.req[i][scope].fix == VRR_LFD_FREQ_NONE &&
				props->vrr_lfd_info.req[i][scope].scalability == VRR_LFD_SCALABILITY_NONE &&
				props->vrr_lfd_info.req[i][scope].min == 0 &&
				props->vrr_lfd_info.req[i][scope].max == 0)
				continue;

			len += snprintf(buf + len, size - len,
					"client=%s scope=%s fix=%d scalability=%d min=%d max=%d\n",
					client_name, scope_name,
					props->vrr_lfd_info.req[i][scope].fix,
					props->vrr_lfd_info.req[i][scope].scalability,
					props->vrr_lfd_info.req[i][scope].min,
					props->vrr_lfd_info.req[i][scope].max);
			updated = true;
		}
	}

	if (updated == false)
		len += snprintf(buf + len, size - len, "none\n");

	len += snprintf(buf + len, size - len, "[cur]\n");
	for (scope = 0; scope < MAX_VRR_LFD_SCOPE; scope++) {
		scope_name = get_vrr_lfd_scope_name(scope);
		if (scope_name == NULL)
			continue;

		len += snprintf(buf + len, size - len,
				"scope=%s lfd:%d~%dHz div:%d~%d(fix=%d scalability=%d min=%d max=%d)\n",
				scope_name,
				props->vrr_lfd_info.status[scope].lfd_min_freq,
				props->vrr_lfd_info.status[scope].lfd_max_freq,
				props->vrr_lfd_info.status[scope].lfd_min_freq_div,
				props->vrr_lfd_info.status[scope].lfd_max_freq_div,
				props->vrr_lfd_info.cur[scope].fix,
				props->vrr_lfd_info.cur[scope].scalability,
				props->vrr_lfd_info.cur[scope].min,
				props->vrr_lfd_info.cur[scope].max);
	}

	return len;
}

static ssize_t vrr_lfd_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	if (!panel_vrr_is_supported(panel)) {
		panel_warn("vrr is not supported\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;
	return snprint_vrr_lfd(panel, buf, PAGE_SIZE);
}

static ssize_t vrr_lfd_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct panel_info *panel_data;
	struct panel_properties *props;
	struct panel_device *panel = dev_get_drvdata(dev);
	int index, scope;
	int argc = 0, client_index = -1, scope_mask = 0;
	struct sysfs_arg vrr_lfd_arglist[] = {
		/* old argument : to be removed */
		{ .name = "0", .nargs = 0, .type = SYSFS_ARG_TYPE_NONE, },
		{ .name = "1", .nargs = 0, .type = SYSFS_ARG_TYPE_NONE, },
		{ .name = "2", .nargs = 0, .type = SYSFS_ARG_TYPE_NONE, },
		{ .name = "tsp_lpm", .nargs = 1, .type = SYSFS_ARG_TYPE_S32, },
		{ .name = "FIX", .nargs = 1, .type = SYSFS_ARG_TYPE_S32, },
		{ .name = "SCAN", .nargs = 1, .type = SYSFS_ARG_TYPE_S32, },
		/* new argument */
		{ .name = VRR_LFD_ARG_KEY_CLIENT, .nargs = 1, .type = SYSFS_ARG_TYPE_STR, },
		{ .name = VRR_LFD_ARG_KEY_SCOPE, .nargs = 1, .type = SYSFS_ARG_TYPE_STR, },
		{ .name = VRR_LFD_ARG_KEY_FIX, .nargs = 1, .type = SYSFS_ARG_TYPE_S32, },
		{ .name = VRR_LFD_ARG_KEY_SCALABILITY, .nargs = 1, .type = SYSFS_ARG_TYPE_S32, },
		{ .name = VRR_LFD_ARG_KEY_MIN, .nargs = 1, .type = SYSFS_ARG_TYPE_S32, },
		{ .name = VRR_LFD_ARG_KEY_MAX, .nargs = 1, .type = SYSFS_ARG_TYPE_S32, },
	};
	struct sysfs_arg_out out;
	char *p, *arg = (char *)buf, *arg1;
	int len = 0;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	if (!panel_vrr_is_supported(panel)) {
		panel_warn("vrr is not supported\n");
		return -EINVAL;
	}

	memset(&out, 0, sizeof(struct sysfs_arg_out));
	panel_data = &panel->panel_data;
	props = &panel_data->props;

	mutex_lock(&panel->panel_bl.lock);
	while ((p = strsep(&arg, " \t\n=")) != NULL) {
		if (!*p)
			continue;

		index = find_sysfs_arg_by_name(vrr_lfd_arglist,
				ARRAY_SIZE(vrr_lfd_arglist), p);
		if (index < 0) {
			panel_err("arg(%s) not found\n", p);
			mutex_unlock(&panel->panel_bl.lock);
			return -EINVAL;
		}

		if (vrr_lfd_arglist[index].nargs > 0) {
			len = parse_sysfs_arg(vrr_lfd_arglist[index].nargs,
					vrr_lfd_arglist[index].type, arg, &out);
			if (len < 0) {
				panel_err("failed to parse sysfs arg(%s)\n", arg);
				mutex_unlock(&panel->panel_bl.lock);
				return -EINVAL;
			}
			arg += len;
		}

		if (argc == 0) {
			/* madatory: 1st argument should start with "client=" */
			if (!strcmp(vrr_lfd_arglist[index].name, VRR_LFD_ARG_KEY_CLIENT)) {
				client_index = find_vrr_lfd_client_name(out.d[0].val_str);
				if (client_index < 0) {
					panel_err("client(%s) not found\n", out.d[0].val_str);
					mutex_unlock(&panel->panel_bl.lock);
					return -EINVAL;
				}
				argc++;
				continue;
			}
		}

		/* old argument : to be removed */
		if (client_index == -1) {
			if (!strcmp("0", vrr_lfd_arglist[index].name)) {
				for (scope = 0; scope < MAX_VRR_LFD_SCOPE; scope++)
					props->vrr_lfd_info.req[VRR_LFD_CLIENT_FAC][scope].fix = VRR_LFD_FREQ_NONE;
			} else if (!strcmp("1", vrr_lfd_arglist[index].name)) {
				for (scope = 0; scope < MAX_VRR_LFD_SCOPE; scope++)
					props->vrr_lfd_info.req[VRR_LFD_CLIENT_FAC][scope].fix = VRR_LFD_FREQ_HIGH;
			} else if (!strcmp("2", vrr_lfd_arglist[index].name)) {
				for (scope = 0; scope < MAX_VRR_LFD_SCOPE; scope++)
					props->vrr_lfd_info.req[VRR_LFD_CLIENT_FAC][scope].fix = VRR_LFD_FREQ_LOW;
			} else if (!strcmp("tsp_lpm", vrr_lfd_arglist[index].name)) {
				props->vrr_lfd_info.req[VRR_LFD_CLIENT_AOD][VRR_LFD_SCOPE_LPM].fix = out.d[0].val_s32;
			} else if (!strcmp("FIX", vrr_lfd_arglist[index].name)) {
				for (scope = 0; scope < MAX_VRR_LFD_SCOPE; scope++)
					props->vrr_lfd_info.req[VRR_LFD_CLIENT_DISP][scope].fix = out.d[0].val_s32;
			} else if (!strcmp("SCAN", vrr_lfd_arglist[index].name)) {
				props->vrr_lfd_info.req[VRR_LFD_CLIENT_VID][VRR_LFD_SCOPE_NORMAL].max = out.d[0].val_s32;
			} else {
				panel_err("undefined argument(%s)\n",
						vrr_lfd_arglist[index].name);
			}
			break;
		}

		/* madatory: 2nd argument should start with "scope=" */
		if (!strcmp(vrr_lfd_arglist[index].name, VRR_LFD_ARG_KEY_SCOPE)) {
			arg1 = out.d[0].val_str;
			while ((p = strsep(&arg1, " ,|")) != NULL) {
				if (!*p)
					continue;

				scope = find_vrr_lfd_scope_name(p);
				if (scope < 0) {
					panel_err("scope(%s) not found\n", p);
					mutex_unlock(&panel->panel_bl.lock);
					return -EINVAL;
				}
				scope_mask |= VRR_LFD_SCOPE_MASK(scope);
			}
			argc++;
			continue;
		}

		/*
		 * this is temporay w/a code for old argument.
		 * it will be removed.
		 */
		if (scope_mask == 0) {
			if (client_index == VRR_LFD_CLIENT_AOD)
				scope_mask = VRR_LFD_SCOPE_LPM_MASK;
			else
				scope_mask = VRR_LFD_SCOPE_NORMAL_MASK;
		}

		if (scope_mask == 0) {
			panel_err("argument(scope=) not found\n");
			mutex_unlock(&panel->panel_bl.lock);
			return -EINVAL;
		}

		if (!strcmp(VRR_LFD_ARG_KEY_FIX, vrr_lfd_arglist[index].name)) {
			for (scope = 0; scope < MAX_VRR_LFD_SCOPE; scope++)
				if (scope_mask & VRR_LFD_SCOPE_MASK(scope))
					props->vrr_lfd_info.req[client_index][scope].fix = out.d[0].val_s32;
		} else if (!strcmp(VRR_LFD_ARG_KEY_SCALABILITY, vrr_lfd_arglist[index].name)) {
			for (scope = 0; scope < MAX_VRR_LFD_SCOPE; scope++)
				if (scope_mask & VRR_LFD_SCOPE_MASK(scope))
					props->vrr_lfd_info.req[client_index][scope].scalability = out.d[0].val_s32;
		} else if (!strcmp(VRR_LFD_ARG_KEY_MIN, vrr_lfd_arglist[index].name)) {
			for (scope = 0; scope < MAX_VRR_LFD_SCOPE; scope++)
				if (scope_mask & VRR_LFD_SCOPE_MASK(scope))
					props->vrr_lfd_info.req[client_index][scope].min = out.d[0].val_s32;
		} else if (!strcmp(VRR_LFD_ARG_KEY_MAX, vrr_lfd_arglist[index].name)) {
			for (scope = 0; scope < MAX_VRR_LFD_SCOPE; scope++)
				if (scope_mask & VRR_LFD_SCOPE_MASK(scope))
					props->vrr_lfd_info.req[client_index][scope].max = out.d[0].val_s32;
		} else {
			panel_err("undefined argument(%s)\n",
					vrr_lfd_arglist[index].name);
		}
		argc++;
	}

	queue_delayed_work(panel->work[PANEL_WORK_UPDATE].wq,
			&panel->work[PANEL_WORK_UPDATE].dwork, msecs_to_jiffies(0));
	mutex_unlock(&panel->panel_bl.lock);
	panel_info("done %d\n", argc);

	return size;
}

#ifdef CONFIG_SUPPORT_POC_SPI
#define SPI_BUF_LEN 2048
u8 spi_flash_readbuf[SPI_BUF_LEN];
u32 spi_flash_readlen;
u8 spi_flash_writebuf[SPI_BUF_LEN];
u32 spi_flash_writelen;
static ssize_t spi_flash_ctrl_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i, len;

	mutex_lock(&sysfs_lock);
	if (spi_flash_writelen <= 0) {
		mutex_unlock(&sysfs_lock);
		return -EINVAL;
	}

	len = snprintf(buf, PAGE_SIZE, "send %d byte(s):\n", spi_flash_writelen);
	for (i = 0; i < spi_flash_writelen; i++)
		len += snprintf(buf + len, PAGE_SIZE - len, "0x%02X%s", spi_flash_writebuf[i],
				(((i + 1) % 16) == 0) || (i == spi_flash_writelen - 1) ? "\n" : " ");

	len += snprintf(buf + len, PAGE_SIZE - len, "receive %d byte(s):\n", spi_flash_readlen);
	for (i = 0; i < spi_flash_readlen; i++)
		len += snprintf(buf + len, PAGE_SIZE - len, "0x%02X%s", spi_flash_readbuf[i],
				(((i + 1) % 16) == 0) || (i == spi_flash_readlen - 1) ? "\n" : " ");

	spi_flash_writelen = 0;
	spi_flash_readlen = 0;
	mutex_unlock(&sysfs_lock);

	return len;
}

static ssize_t spi_flash_ctrl_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct panel_device *panel = dev_get_drvdata(dev);
	struct panel_spi_dev *spi_dev = &panel->panel_spi_dev;
	int ret, cmd_scanned, parse, cmd_input;

	mutex_lock(&sysfs_lock);
	mutex_lock(&panel->op_lock);

	memset(spi_flash_readbuf, 0, SPI_BUF_LEN);
	memset(spi_flash_writebuf, 0, SPI_BUF_LEN);
	spi_flash_readlen = spi_flash_writelen = 0;

	ret = sscanf(buf, "%d%n", &spi_flash_readlen, &parse);
	if (ret != 1 || parse <= 0 || spi_flash_readlen > SPI_BUF_LEN) {
		ret = -EINVAL;
		goto store_err;
	}
	cmd_scanned = parse;
	while (cmd_scanned < size && spi_flash_writelen < SPI_BUF_LEN) {
		ret = sscanf(buf + cmd_scanned, " %x%n", &cmd_input, &parse);
		panel_dbg("readed %d %d ret %d target str %s\n", cmd_input, parse, ret, buf + cmd_scanned);
		if (parse <= 0 || ret <= 0)
			break;

		spi_flash_writebuf[spi_flash_writelen++] = cmd_input & 0xFF;
		cmd_scanned += parse;
	}

	panel_info("send %d byte(s), receive %d byte(s)\n", spi_flash_writelen, spi_flash_readlen);
	print_hex_dump(KERN_ERR, __func__, DUMP_PREFIX_OFFSET, 16, 1, spi_flash_writebuf, spi_flash_writelen, false);

	ret = spi_dev->ops->cmd(spi_dev, spi_flash_writebuf, spi_flash_writelen, spi_flash_readbuf, spi_flash_readlen);
	if (ret < 0) {
		panel_err("failed to spi cmd 0x%0x, ret %d\n", spi_flash_writebuf[0], ret);
		ret = -EIO;
		goto store_err;
	}

	panel_info("received %d byte(s)\n", ret);
	print_hex_dump(KERN_ERR, __func__, DUMP_PREFIX_OFFSET, 16, 1, spi_flash_readbuf, spi_flash_readlen, false);

	mutex_unlock(&panel->op_lock);
	mutex_unlock(&sysfs_lock);

	return size;

store_err:
	spi_flash_readlen = spi_flash_writelen = 0;
	mutex_unlock(&panel->op_lock);
	mutex_unlock(&sysfs_lock);

	return ret;
}
#endif

#if defined(CONFIG_SUPPORT_FAST_DISCHARGE)
static ssize_t enable_fd_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	snprintf(buf, PAGE_SIZE, "%u\n", panel_data->props.enable_fd);

	return strlen(buf);
}

static ssize_t enable_fd_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int rc, ret;
	u32 prev_value, value;
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (panel_data->props.enable_fd == value) {
		panel_info("fast discharge is already %s\n",
			panel_data->props.enable_fd ? "on" : "off");
		return size;
	}

	mutex_lock(&panel->op_lock);
	prev_value = panel_data->props.enable_fd;
	panel_data->props.enable_fd = value;
	mutex_unlock(&panel->op_lock);

#ifdef CONFIG_EVASION_DISP_DET
	ret = panel_set_gpio_irq(&panel->gpio[PANEL_GPIO_DISP_DET], false);
	if (ret < 0)
		panel_warn("do not support irq\n");
#endif
	ret = panel_fast_discharge_set(panel);
	if (unlikely(ret < 0)) {
		panel_err("failed to write fast discharge set\n");
		mutex_lock(&panel->op_lock);
		panel_data->props.enable_fd = prev_value;
		mutex_unlock(&panel->op_lock);
		return ret;
	}

#ifdef CONFIG_EVASION_DISP_DET
	queue_delayed_work(panel->work[PANEL_WORK_EVASION_DISP_DET].wq,
		&panel->work[PANEL_WORK_EVASION_DISP_DET].dwork, msecs_to_jiffies(34));
#endif
	panel_info("fast discharge set to %s\n", panel_data->props.enable_fd ? "on" : "off");

	return size;
}
#endif

#ifdef CONFIG_SUPPORT_MASK_LAYER
static ssize_t mask_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);
	struct panel_bl_device *panel_bl;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;
	panel_bl = &panel->panel_bl;

	sprintf(buf, "%d\n", panel_bl->props.mask_layer_br_target);

	return strlen(buf);
}

static ssize_t mask_brightness_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);
	struct panel_bl_device *panel_bl;
	int value, rc;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;
	panel_bl = &panel->panel_bl;

	if (value > panel_bl->bd->props.max_brightness) {
		panel_err("input:%d bd max:%d\n", value, panel_bl->bd->props.max_brightness);
		return -EINVAL;
	}

	panel_info("%d->%d target br updated.\n",
			panel_bl->props.mask_layer_br_target, value);

	panel_bl->props.mask_layer_br_target = value;

	return size;
}

static ssize_t actual_mask_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_info *panel_data;
	struct panel_device *panel = dev_get_drvdata(dev);
	struct panel_bl_device *panel_bl;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;
	panel_bl = &panel->panel_bl;

	sprintf(buf, "%d\n", panel_bl->props.mask_layer_br_actual);

	return strlen(buf);
}
#endif
struct device_attribute panel_attrs[] = {
	__PANEL_ATTR_RO(lcd_type, 0444),
	__PANEL_ATTR_RO(window_type, 0444),
	__PANEL_ATTR_RO(manufacture_code, 0444),
	__PANEL_ATTR_RO(cell_id, 0444),
	__PANEL_ATTR_RO(octa_id, 0444),
	__PANEL_ATTR_RO(SVC_OCTA, 0444),
	__PANEL_ATTR_RO(SVC_OCTA_CHIPID, 0444),
	__PANEL_ATTR_RO(SVC_OCTA_DDI_CHIPID, 0444),
#ifdef CONFIG_SUPPORT_XTALK_MODE
	__PANEL_ATTR_RW(xtalk_mode, 0664),
#endif
#ifdef CONFIG_SUPPORT_MST
	__PANEL_ATTR_RW(mst, 0664),
#endif
#ifdef CONFIG_SUPPORT_POC_FLASH
	__PANEL_ATTR_RW(poc, 0660),
	__PANEL_ATTR_RO(poc_mca, 0440),
	__PANEL_ATTR_RO(poc_info, 0440),
#endif
	__PANEL_ATTR_RW(gamma_flash, 0660),
	__PANEL_ATTR_RO(gamma_check, 0440),
#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
	__PANEL_ATTR_RW(gct, 0664),
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	__PANEL_ATTR_RW(grayspot, 0664),
#endif
	__PANEL_ATTR_RW(irc_mode, 0664),
	__PANEL_ATTR_RW(dia, 0664),
	__PANEL_ATTR_RO(color_coordinate, 0444),
	__PANEL_ATTR_RO(manufacture_date, 0444),
	__PANEL_ATTR_RO(brightness_table, 0444),
	__PANEL_ATTR_RW(adaptive_control, 0664),
	__PANEL_ATTR_RW(siop_enable, 0664),
	__PANEL_ATTR_RW(temperature, 0664),
#ifdef CONFIG_EXYNOS_LCD_ENG
	__PANEL_ATTR_RW(read_mtp, 0644),
	__PANEL_ATTR_RW(write_mtp, 0644),
	__PANEL_ATTR_RW(gamma_interpolation_test, 0664),
#ifdef CONFIG_SUPPORT_ISC_TUNE_TEST
	__PANEL_ATTR_RW(stm, 0664),
	__PANEL_ATTR_RW(isc, 0664),
#endif
#endif
	__PANEL_ATTR_RW(mcd_mode, 0664),
	__PANEL_ATTR_RW(partial_disp, 0664),
	__PANEL_ATTR_RW(mcd_resistance, 0664),
#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
	__PANEL_ATTR_RW(lux, 0644),
#endif
#if defined(CONFIG_EXYNOS_DECON_LCD_COPR)
	__PANEL_ATTR_RW(copr, 0600),
	__PANEL_ATTR_RO(read_copr, 0440),
	__PANEL_ATTR_RW(copr_roi, 0600),
	__PANEL_ATTR_RO(brt_avg, 0440),
#endif
	__PANEL_ATTR_RW(alpm, 0664),
	__PANEL_ATTR_RW(lpm_opr, 0664),
	__PANEL_ATTR_RW(fingerprint, 0644),
#ifdef CONFIG_SUPPORT_HMD
	__PANEL_ATTR_RW(hmt_bright, 0664),
	__PANEL_ATTR_RW(hmt_on, 0664),
#endif
#ifdef CONFIG_DISPLAY_USE_INFO
	__PANEL_ATTR_RW(dpui, 0660),
	__PANEL_ATTR_RW(dpui_dbg, 0660),
	__PANEL_ATTR_RW(dpci, 0660),
	__PANEL_ATTR_RW(dpci_dbg, 0660),
#endif
	__PANEL_ATTR_RW(poc_onoff, 0664),
#ifdef CONFIG_EXTEND_LIVE_CLOCK
	__PANEL_ATTR_RW(self_mask, 0664),
	__PANEL_ATTR_RO(self_mask_check, 0444),
#endif
#ifdef SUPPORT_NORMAL_SELF_MOVE
	__PANEL_ATTR_RW(self_move, 0664),
#endif
#ifdef CONFIG_SUPPORT_ISC_DEFECT
	__PANEL_ATTR_RW(isc_defect, 0664),
#endif
#ifdef CONFIG_SUPPORT_SPI_IF_SEL
	__PANEL_ATTR_RW(spi_if_sel, 0664),
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
	__PANEL_ATTR_RO(ccd_state, 0444),
#endif
#ifdef CONFIG_SUPPORT_DYNAMIC_HLPM
	__PANEL_ATTR_RW(dynamic_hlpm, 0664),
#endif
#ifdef CONFIG_DYNAMIC_MIPI
	__PANEL_ATTR_RW(dynamic_mipi, 0664),
#endif
#ifdef CONFIG_SUPPORT_POC_SPI
	__PANEL_ATTR_RW(spi_flash_ctrl, 0660),
#endif
	__PANEL_ATTR_RO(vrr, 0444),
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	__PANEL_ATTR_RW(display_mode, 0664),
#endif
#ifdef CONFIG_PANEL_VRR_BRIDGE
	__PANEL_ATTR_RW(vrr_bridge, 0664),
#endif
	__PANEL_ATTR_RW(conn_det, 0664),
#ifdef CONFIG_SUPPORT_MAFPC
	__PANEL_ATTR_RO(mafpc_time, 0444),
	__PANEL_ATTR_RO(mafpc_check, 0440),
#endif
	__PANEL_ATTR_RW(vrr_lfd, 0664),
#if defined(CONFIG_SUPPORT_FAST_DISCHARGE)
	__PANEL_ATTR_RW(enable_fd, 0664),
#endif
#ifdef CONFIG_SUPPORT_MASK_LAYER
	__PANEL_ATTR_RW(mask_brightness, 0664),
	__PANEL_ATTR_RO(actual_mask_brightness, 0444),
#endif
#ifdef CONFIG_SUPPORT_BRIGHTDOT_TEST
	__PANEL_ATTR_RW(brightdot, 0664),
#endif
};

int panel_sysfs_probe(struct panel_device *panel)
{
	size_t i;
	int ret = 0, retVal = 0;
	struct device *lcd_dev;
	struct kobject *top_kobj = NULL;
	struct kernfs_node *kn = NULL;
	struct kobject *svc_kobj = NULL;

	lcd_dev = panel->lcd_dev;
	if (unlikely(!lcd_dev)) {
		panel_err("lcd device not exist\n");
		retVal = -ENODEV;
		goto exit;
	}

	if (panel->panel_bl.bd) {
		top_kobj = &panel->panel_bl.bd->dev.kobj.kset->kobj;
		kn = kernfs_find_and_get(top_kobj->sd, "svc");
	}

	svc_kobj = kn ? kn->priv : kobject_create_and_add("svc", top_kobj);
	if (!svc_kobj) {
		panel_err("%s: fail to create svc\n", __func__);
		retVal = -ENODEV;
		goto exit;
	}

	if (!IS_ERR_OR_NULL(svc_kobj)) {
		ret = sysfs_create_link(svc_kobj, &lcd_dev->kobj, "OCTA");
		if (ret)
			panel_err("failed to create svc/OCTA/\n");
		else
			panel_info("success to create svc/OCTA/\n");
	} else {
		panel_err("failed to find svc kobject\n");
	}

	for (i = 0; i < ARRAY_SIZE(panel_attrs); i++) {
		ret = device_create_file(lcd_dev, &panel_attrs[i]);
		if (ret < 0) {
			panel_err("failed to add sysfs(%s) entries, %d\n",
					panel_attrs[i].attr.name, ret);
			retVal = -ENODEV;
			goto exit;
		}
	}

exit:
	if (kn)
		kernfs_put(kn);

	return retVal;
}

int panel_sysfs_remove(struct panel_device *panel)
{
	size_t i;
	int retVal = 0;
	struct device *lcd_dev;
	struct kobject *top_kobj = NULL;
	struct kernfs_node *kn = NULL;
	struct kobject *svc_kobj = NULL;

	// remove original sysfs
	lcd_dev = panel->lcd_dev;
	if (unlikely(!lcd_dev)) {
		panel_err("lcd device not exist\n");
		retVal = -ENODEV;
		goto exit;
	}
	for (i = 0; i < ARRAY_SIZE(panel_attrs); i++)
		device_remove_file(lcd_dev, &panel_attrs[i]);
	panel_info("sysfs is removed\n");


	if (panel->panel_bl.bd) {
		top_kobj = &panel->panel_bl.bd->dev.kobj.kset->kobj;
		kn = kernfs_find_and_get(top_kobj->sd, "svc");
	}

	// remove sysfs link(svc/OCTA)
	if (kn) {
		svc_kobj = kn->priv;
		if (!IS_ERR_OR_NULL(svc_kobj)) {
			sysfs_remove_link(svc_kobj, "OCTA");
			kobject_put(svc_kobj);
		} else {
			panel_err("%s: fail to find svc\n", __func__);
			retVal = -ENODEV;
			goto exit;
		}
		panel_info("sysfs link is removed\n");
	}

exit:
	if (kn)
		kernfs_put(kn);

	return retVal;
}

