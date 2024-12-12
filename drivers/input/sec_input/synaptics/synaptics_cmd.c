#include "synaptics_dev.h"
#include "synaptics_reg.h"

static void synaptics_ts_print_channel(struct synaptics_ts_data *ts,
		struct synaptics_ts_buffer *tdata, int *min, int *max, u8 type)
{
	unsigned char *pStr = NULL;
	unsigned char pTmp[16] = { 0 };
	int i = 0, j = 0, k = 0;
	short *data_ptr = NULL;
	int *data_ptr_int = NULL;
	int lsize = PRINT_SIZE * (ts->cols + 1);

	if (!ts->cols)
		return;

	switch (type) {
	case TEST_PID18_HYBRID_ABS_RAW:  /* int */
		data_ptr_int = (int *)&tdata->buf[0];
		for (i = 0; i < ts->rows + ts->cols; i++) {
			ts->pFrame[i] = *data_ptr_int;
			data_ptr_int++;
		}
		break;
	default: /* short */
		data_ptr = (short *)&tdata->buf[0];
		for (i = 0; i < ts->rows + ts->cols; i++) {
			ts->pFrame[i] = *data_ptr;
			data_ptr++;
		}
		break;
	}

	*min = *max = ts->pFrame[0];

	pStr = vzalloc(lsize);
	if (!pStr)
		return;

	memset(pStr, 0x0, lsize);
	snprintf(pTmp, sizeof(pTmp), " RX");
	strlcat(pStr, pTmp, lsize);

	for (k = 0; k < ts->cols; k++) {
		snprintf(pTmp, sizeof(pTmp), "    %02d", k);
		strlcat(pStr, pTmp, lsize);
	}
	input_raw_info(true, &ts->client->dev, "%s\n", pStr);

	memset(pStr, 0x0, lsize);
	snprintf(pTmp, sizeof(pTmp), " +");
	strlcat(pStr, pTmp, lsize);

	for (k = 0; k < ts->cols; k++) {
		snprintf(pTmp, sizeof(pTmp), "------");
		strlcat(pStr, pTmp, lsize);
	}
	input_raw_info(true, &ts->client->dev, "%s\n", pStr);

	memset(pStr, 0x0, lsize);
	snprintf(pTmp, sizeof(pTmp), " | ");
	strlcat(pStr, pTmp, lsize);

	for (i = 0; i < ts->cols + ts->rows; i ++) {
		if (i == ts->cols) {
			input_raw_info(true, &ts->client->dev, "%s\n", pStr);
			input_raw_info(true, &ts->client->dev, "\n");
			memset(pStr, 0x0, lsize);
			snprintf(pTmp, sizeof(pTmp), " TX");
			strlcat(pStr, pTmp, lsize);

			for (k = 0; k < ts->rows; k++) {
				snprintf(pTmp, sizeof(pTmp), "    %02d", k);
				strlcat(pStr, pTmp, lsize);
			}

			input_raw_info(true, &ts->client->dev, "%s\n", pStr);

			memset(pStr, 0x0, lsize);
			snprintf(pTmp, sizeof(pTmp), " +");
			strlcat(pStr, pTmp, lsize);

			for (k = 0; k < ts->cols; k++) {
				snprintf(pTmp, sizeof(pTmp), "------");
				strlcat(pStr, pTmp, lsize);
			}
			input_raw_info(true, &ts->client->dev, "%s\n", pStr);

			memset(pStr, 0x0, lsize);
			snprintf(pTmp, sizeof(pTmp), " | ");
			strlcat(pStr, pTmp, lsize);
		} else if (i && !(i % ts->cols)) {
			input_raw_info(true, &ts->client->dev, "%s\n", pStr);
			memset(pStr, 0x0, lsize);
			snprintf(pTmp, sizeof(pTmp), " | ");
			strlcat(pStr, pTmp, lsize);
		}

		snprintf(pTmp, sizeof(pTmp), " %5d", ts->pFrame[i]);
		strlcat(pStr, pTmp, lsize);

		*min = min(*min, ts->pFrame[i]);
		*max = max(*max, ts->pFrame[i]);

		j++;
	}
	input_raw_info(true, &ts->client->dev, "%s\n", pStr);
	vfree(pStr);
}

static void synaptics_ts_print_frame(struct synaptics_ts_data *ts,
		struct synaptics_ts_buffer *tdata, int *min, int *max, u8 type)
{
	int i = 0;
	int j = 0;
	unsigned char *pStr = NULL;
	unsigned char pTmp[16] = { 0 };
	int lsize = PRINT_SIZE * (ts->rows + 1);
	short *data_ptr = NULL;
	int *data_ptr_int = NULL;

	input_raw_info(true, &ts->client->dev, "%s\n", __func__);

	switch (type) {
	case TEST_PID18_HYBRID_ABS_RAW:  /* int */
		data_ptr_int = (int *)&tdata->buf[0];
		for (i = 0; i < ts->rows; i++) {
			for (j = 0; j < ts->cols; j++) {
				ts->pFrame[i + (ts->rows * j)] = *data_ptr_int;
				data_ptr_int++;
			}
		}
		break;
	default:  /* short */
		data_ptr = (short *)&tdata->buf[0];
		for (i = 0; i < ts->rows; i++) {
			for (j = 0; j < ts->cols; j++) {
				ts->pFrame[i + (ts->rows * j)] = *data_ptr;
				data_ptr++;
			}
		}
		break;
	}

	*min = *max = ts->pFrame[0];

	pStr = kzalloc(lsize, GFP_KERNEL);
	if (pStr == NULL)
		return;

	memset(pStr, 0x0, lsize);
	snprintf(pTmp, sizeof(pTmp), "      TX");
	strlcat(pStr, pTmp, lsize);

	for (i = 0; i < ts->rows; i++) {
		snprintf(pTmp, sizeof(pTmp), " %02d ", i);
		strlcat(pStr, pTmp, lsize);
	}

	input_raw_info(true, &ts->client->dev, "%s\n", pStr);
	memset(pStr, 0x0, lsize);
	snprintf(pTmp, sizeof(pTmp), " +");
	strlcat(pStr, pTmp, lsize);

	for (i = 0; i < ts->rows; i++) {
		snprintf(pTmp, sizeof(pTmp), "----");
		strlcat(pStr, pTmp, lsize);
	}

	input_raw_info(true, &ts->client->dev, "%s\n", pStr);

	for (i = 0; i < ts->cols; i++) {
		memset(pStr, 0x0, lsize);
		snprintf(pTmp, sizeof(pTmp), "RX%02d | ", i);
		strlcat(pStr, pTmp, lsize);

		for (j = 0; j < ts->rows; j++) {
			snprintf(pTmp, sizeof(pTmp), " %5d", ts->pFrame[j + (i * ts->rows)]);

			if (ts->pFrame[j + (i * ts->rows)] < *min)
				*min = ts->pFrame[j + (i * ts->rows)];

			if (ts->pFrame[j + (i * ts->rows)] > *max)
				*max = ts->pFrame[j + (i * ts->rows)];

			strlcat(pStr, pTmp, lsize);
		}
		input_raw_info(true, &ts->client->dev, "%s\n", pStr);
	}
	kfree(pStr);
}

static int get_self_channel_data(void *device_data, u8 type)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[16] = { 0 };
	int ii;
	int tx_min = 0;
	int rx_min = 0;
	int tx_max = 0;
	int rx_max = 0;
	char *item_name = "NULL";
	char temp[SEC_CMD_STR_LEN] = { 0 };

	switch (type) {
	case TEST_PID05_FULL_RAW_CAP:
		item_name = "FULL_RAWCAP"; /* short * */
		break;
	case TEST_PID10_DELTA_NOISE:
		item_name = "DELTA_NOISE";	/* short * */
		break;
	case TEST_PID18_HYBRID_ABS_RAW:  /* int */
		item_name = "ABS_RAW";
		break;
	case TEST_PID22_TRANS_RAW_CAP:	/* short */
		item_name = "TRANS_RAW";
		break;
	case TEST_PID29_HYBRID_ABS_NOISE:	 /*  short */
		item_name = "ABS_NOISE";
		break;
	}

	for (ii = 0; ii < ts->cols; ii++) {
		if (ii == 0)
			tx_min = tx_max = ts->pFrame[ii];

		tx_min = min(tx_min, ts->pFrame[ii]);
		tx_max = max(tx_max, ts->pFrame[ii]);
	}
	snprintf(buff, sizeof(buff), "%d,%d", tx_min, tx_max);
	snprintf(temp, sizeof(temp), "%s%s", item_name, "_RX");
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, sizeof(buff), temp);

	memset(temp, 0x00, SEC_CMD_STR_LEN);

	for (ii = ts->cols; ii < ts->cols + ts->rows; ii++) {
		if (ii == ts->cols)
			rx_min = rx_max = ts->pFrame[ii];

		rx_min = min(rx_min, ts->pFrame[ii]);
		rx_max = max(rx_max, ts->pFrame[ii]);
	}
	snprintf(buff, sizeof(buff), "%d,%d", rx_min, rx_max);
	snprintf(temp, sizeof(temp), "%s%s", item_name, "_TX");
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, sizeof(buff), temp);

	return 0;
}

/**
 * syna_tcm_run_production_test()
 *
 * Implement the appplication fw command code to request the device to run
 * the production test.
 *
 * Production tests are listed at enum test_code (PID$).
 *
 * @param
 *    [ in] tcm_dev:    the device handle
 *    [ in] test_item:  the requested testing item
 *    [out] tdata:      testing data returned
 *
 * @return
 *    on success, 0 or positive value; otherwise, negative value on error.
 */
int synaptics_ts_run_production_test(struct synaptics_ts_data *ts,
		unsigned char test_item, struct synaptics_ts_buffer *tdata)
{
	int retval = 0;
	unsigned char resp_code;
	unsigned char test_code;

	if (IS_NOT_APP_FW_MODE(ts->dev_mode)) {
		input_err(true, &ts->client->dev, "%s: Device is not in application fw mode, mode: %x\n", __func__,
			ts->dev_mode);
		return -EINVAL;
	}

	test_code = (unsigned char)test_item;

	retval = ts->write_message(ts,
			SYNAPTICS_TS_CMD_PRODUCTION_TEST,
			&test_code,
			1,
			&resp_code,
			FORCE_ATTN_DRIVEN);
	if (retval < 0) {
		input_err(true, &ts->client->dev, "Fail to send command 0x%02x\n",
			SYNAPTICS_TS_CMD_PRODUCTION_TEST);
		goto exit;
	}

	if (tdata == NULL)
		goto exit;

	/* copy testing data to caller */
	retval = synaptics_ts_buf_copy(tdata, &ts->resp_buf);
	if (retval < 0) {
		input_err(true, &ts->client->dev, "Fail to copy testing data\n");
		goto exit;
	}

exit:
	return retval;
}

static void synaptics_ts_test_rawdata_read(struct synaptics_ts_data *ts,
		struct sec_cmd_data *sec, struct synaptics_ts_test_mode *mode)
{
	struct synaptics_ts_buffer test_data;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buff;
	char *item_name = "NULL";
	int ret;
	int ii;

	switch (mode->type) {
	case TEST_PID05_FULL_RAW_CAP:
		item_name = "FULL_RAWCAP"; /* short * */
		break;
	case TEST_PID10_DELTA_NOISE:
		item_name = "FULL_NOISE";	/* short * */
		break;
	case TEST_PID18_HYBRID_ABS_RAW:  /* int */
		item_name = "ABS_RAW";
		break;
	case TEST_PID22_TRANS_RAW_CAP:	/* short */
		item_name = "TRANS_RAW";
		break;
	case TEST_PID29_HYBRID_ABS_NOISE:	 /*  short */
		item_name = "ABS_NOISE";
		break;
	default:
		break;
	}

	input_err(true, &ts->client->dev, "%s: [%s] test\n", __func__, item_name);

	buff = kzalloc(ts->cols * ts->rows * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff)
		goto error_alloc_mem;

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		goto error_power_state;
	}

	synaptics_ts_buf_init(&test_data);
	memset(ts->pFrame, 0x00, ts->cols * ts->rows * sizeof(int));

	ret = synaptics_ts_run_production_test(ts, mode->type, &test_data);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to test\n",
			__func__);
		goto error_test_fail;
	}

	if (mode->frame_channel)
		synaptics_ts_print_channel(ts, &test_data, &mode->min, &mode->max, mode->type);
	else
		synaptics_ts_print_frame(ts, &test_data, &mode->min, &mode->max, mode->type);

	if (mode->allnode) {
		if (mode->frame_channel) {
			for (ii = 0; ii < (ts->cols + ts->rows); ii++) {
				snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", ts->pFrame[ii]);
				strlcat(buff, temp, ts->cols * ts->rows * CMD_RESULT_WORD_LEN);

				memset(temp, 0x00, SEC_CMD_STR_LEN);
			}
		} else {
			for (ii = 0; ii < (ts->cols * ts->rows); ii++) {
				snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", ts->pFrame[ii]);
				strlcat(buff, temp, ts->cols * ts->rows * CMD_RESULT_WORD_LEN);

				memset(temp, 0x00, SEC_CMD_STR_LEN);
			}
		}
	} else {
		snprintf(buff, SEC_CMD_STR_LEN, "%d,%d", mode->min, mode->max);
	}

	if (!sec)
		goto out_rawdata;

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, ts->cols * ts->rows * CMD_RESULT_WORD_LEN));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff,
				strnlen(buff, ts->cols * ts->rows * CMD_RESULT_WORD_LEN), item_name);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_raw_info(true, &ts->client->dev, "%s: %d, %s\n",
			__func__, mode->type, mode->allnode ? "ALL" : "");

out_rawdata:
	synaptics_ts_buf_release(&test_data);
	kfree(buff);

	synaptics_ts_locked_release_all_finger(ts);

	return;
error_test_fail:
	synaptics_ts_buf_release(&test_data);
error_power_state:
	kfree(buff);
error_alloc_mem:
	if (!sec)
		return;

	snprintf(temp, SEC_CMD_STR_LEN, "NG");
	sec_cmd_set_cmd_result(sec, temp, SEC_CMD_STR_LEN);
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, temp, SEC_CMD_STR_LEN, item_name);
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	synaptics_ts_locked_release_all_finger(ts);

	return;
}

/**************************************************************************************************/
static ssize_t scrub_position_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[256] = { 0 };

#if IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	input_info(true, &ts->client->dev,
			"%s: id: %d\n", __func__, ts->plat_data->gesture_id);
#else
	input_info(true, &ts->client->dev,
			"%s: id: %d, X:%d, Y:%d\n", __func__,
			ts->plat_data->gesture_id, ts->plat_data->gesture_x, ts->plat_data->gesture_y);
#endif
	snprintf(buff, sizeof(buff), "%d %d %d", ts->plat_data->gesture_id,
			ts->plat_data->gesture_x, ts->plat_data->gesture_y);

	ts->plat_data->gesture_x = 0;
	ts->plat_data->gesture_y = 0;

	return snprintf(buf, PAGE_SIZE, "%s", buff);
}

/* for bigdata */
/* read param */
static ssize_t hardware_param_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_INPUT_HW_PARAM_SIZE];
	char tbuff[SEC_CMD_STR_LEN];
	char temp[SEC_CMD_STR_LEN];

	memset(buff, 0x00, sizeof(buff));

	sec_input_get_common_hw_param(ts->plat_data, buff);

	/* module_id */
	memset(tbuff, 0x00, sizeof(tbuff));
	snprintf(tbuff, sizeof(tbuff), ",\"TMOD\":\"SY%02X%02X%02X%c%01X\"",
			ts->plat_data->img_version_of_bin[1], ts->plat_data->img_version_of_bin[2],
			ts->plat_data->img_version_of_bin[3],
#ifdef TCLM_CONCEPT
			ts->tdata->tclm_string[ts->tdata->nvdata.cal_position].s_name,
			ts->tdata->nvdata.cal_count & 0xF);
#else
			'0', 0);
#endif
	strlcat(buff, tbuff, sizeof(buff));

	/* vendor_id */
	memset(tbuff, 0x00, sizeof(tbuff));
	if (ts->plat_data->firmware_name) {
		memset(temp, 0x00, sizeof(temp));
		snprintf(temp, 5, "%s", ts->plat_data->firmware_name);

		snprintf(tbuff, sizeof(tbuff), ",\"TVEN\":\"SYNA_%s\"", temp);
	} else {
		snprintf(tbuff, sizeof(tbuff), ",\"TVEN\":\"SYNA\"");
	}
	strlcat(buff, tbuff, sizeof(buff));

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buff);
}

/* clear param */
static ssize_t hardware_param_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);

	sec_input_clear_common_hw_param(ts->plat_data);

	return count;
}

#define SENSITIVITY_POINT_CNT	9	/* ~ davinci : 5 ea => 9 ea */
static ssize_t sensitivity_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	int value[SENSITIVITY_POINT_CNT] = { 0 };
	int ret, i;
	char tempv[10] = { 0 };
	char buff[SENSITIVITY_POINT_CNT * 10] = { 0 };
	struct synaptics_ts_buffer test_data;
	unsigned short *data_ptr = NULL;

	synaptics_ts_buf_init(&test_data);

	ret = synaptics_ts_run_production_test(ts,
			TEST_PID60_SENSITIVITY,
			&test_data);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "Fail to run test %d\n",
		TEST_PID54_BSC_DIFF_TEST);
		synaptics_ts_buf_release(&test_data);
		return ret;
	}

	data_ptr = (short *)&test_data.buf[0];
	for (i = 0; i < SENSITIVITY_POINT_CNT; i++) {
		value[i] = *data_ptr;
		if (i != 0)
			strlcat(buff, ",", sizeof(buff));
		snprintf(tempv, 10, "%d", value[i]);
		strlcat(buff, tempv, sizeof(buff));
		data_ptr++;
	}

	input_info(true, &ts->client->dev, "%s: sensitivity mode : %s\n", __func__, buff);
	synaptics_ts_buf_release(&test_data);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buff);
}

static ssize_t sensitivity_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	int ret;
	unsigned long value = 0;

	if (count > 2)
		return -EINVAL;

	ret = kstrtoul(buf, 10, &value);
	if (ret != 0)
		return ret;

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: power off in IC\n", __func__);
		return 0;
	}

	input_err(true, &ts->client->dev, "%s: enable:%ld\n", __func__, value);

	input_info(true, &ts->client->dev, "%s: done\n", __func__);

	return count;
}

static ssize_t prox_power_off_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);

	input_info(true, &ts->client->dev, "%s: %d\n", __func__,
			ts->plat_data->prox_power_off);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", ts->plat_data->prox_power_off);
}

static ssize_t prox_power_off_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	long data;
	int ret;

	ret = kstrtol(buf, 10, &data);
	if (ret < 0)
		return ret;

	input_info(true, &ts->client->dev, "%s: %ld\n", __func__, data);

	ts->plat_data->prox_power_off = data;

	return count;
}

static ssize_t read_support_feature(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	u32 feature = 0;

	if (ts->plat_data->enable_settings_aot)
		feature |= INPUT_FEATURE_ENABLE_SETTINGS_AOT;

	if (ts->plat_data->sync_reportrate_120)
		feature |= INPUT_FEATURE_ENABLE_SYNC_RR120;

	if (ts->plat_data->support_vrr)
		feature |= INPUT_FEATURE_ENABLE_VRR;

	if (ts->plat_data->support_open_short_test)
		feature |= INPUT_FEATURE_SUPPORT_OPEN_SHORT_TEST;

	if (ts->plat_data->support_mis_calibration_test)
		feature |= INPUT_FEATURE_SUPPORT_MIS_CALIBRATION_TEST;

	if (ts->plat_data->support_wireless_tx)
		feature |= INPUT_FEATURE_SUPPORT_WIRELESS_TX;

	if (ts->plat_data->enable_sysinput_enabled)
		feature |= INPUT_FEATURE_ENABLE_SYSINPUT_ENABLED;

	input_info(true, &ts->client->dev, "%s: %d%s%s%s%s%s%s%s%s\n",
			__func__, feature,
			feature & INPUT_FEATURE_ENABLE_SETTINGS_AOT ? " aot" : "",
			feature & INPUT_FEATURE_ENABLE_PRESSURE ? " pressure" : "",
			feature & INPUT_FEATURE_ENABLE_SYNC_RR120 ? " RR120hz" : "",
			feature & INPUT_FEATURE_ENABLE_VRR ? " vrr" : "",
			feature & INPUT_FEATURE_SUPPORT_OPEN_SHORT_TEST ? " openshort" : "",
			feature & INPUT_FEATURE_SUPPORT_MIS_CALIBRATION_TEST ? " miscal" : "",
			feature & INPUT_FEATURE_SUPPORT_WIRELESS_TX ? " wirelesstx" : "",
			feature & INPUT_FEATURE_ENABLE_SYSINPUT_ENABLED ? " SE" : "");

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", feature);
}

static ssize_t get_lp_dump(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	u8 string_data[10] = {0, };
	u16 current_index;
	u16 dump_start, dump_end, dump_cnt;
	int i, ret, dump_area, dump_gain;
	unsigned char *sec_spg_dat;
	u8 dump_clear_packet[2] = {0x00,0x01};

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: Touch is stopped!\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "TSP turned off");
	}

	if (ts->reset_is_on_going) {
		input_err(true, &ts->client->dev, "%s: Reset is ongoing!\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "Reset is ongoing");
	}

	/* preparing dump buffer */
	sec_spg_dat = vmalloc(SEC_TS_MAX_SPONGE_DUMP_BUFFER);
	if (!sec_spg_dat) {
		input_err(true, &ts->client->dev, "%s : Failed!!\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "vmalloc failed");
	}
	memset(sec_spg_dat, 0, SEC_TS_MAX_SPONGE_DUMP_BUFFER);

	ret = ts->synaptics_ts_read_sponge(ts, string_data, 2, SEC_TS_CMD_SPONGE_LP_DUMP_CUR_IDX, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to read rect\n", __func__);
		snprintf(buf, SEC_CMD_BUF_SIZE, "NG, Failed to read rect");
		goto out;
	}

	if (ts->sponge_inf_dump)
		dump_gain = 2;
	else
		dump_gain = 1;

	current_index = (string_data[1] & 0xFF) << 8 | (string_data[0] & 0xFF);
	dump_start = SEC_TS_CMD_SPONGE_LP_DUMP_EVENT;
	dump_end = dump_start + (ts->sponge_dump_format * ((ts->sponge_dump_event * dump_gain) - 1));

	if (current_index > dump_end || current_index < dump_start) {
		input_err(true, &ts->client->dev,
				"Failed to Sponge LP log %d\n", current_index);
		snprintf(buf, SEC_CMD_BUF_SIZE,
				"NG, Failed to Sponge LP log, current_index=%d",
				current_index);
		goto out;
	}

	/* legacy get_lp_dump */
	input_info(true, &ts->client->dev, "%s: DEBUG format=%d, num=%d, start=%d, end=%d, current_index=%d\n",
			__func__, ts->sponge_dump_format, ts->sponge_dump_event, dump_start, dump_end, current_index);

	for (i = (ts->sponge_dump_event * dump_gain) - 1 ; i >= 0 ; i--) {
		u16 data0, data1, data2, data3, data4;
		char buff[30] = {0, };
		u16 string_addr;

		if (current_index < (ts->sponge_dump_format * i))
			string_addr = (ts->sponge_dump_format * ts->sponge_dump_event * dump_gain) + current_index - (ts->sponge_dump_format * i);
		else
			string_addr = current_index - (ts->sponge_dump_format * i);

		if (string_addr < dump_start)
			string_addr += (ts->sponge_dump_format * ts->sponge_dump_event * dump_gain);

		string_data[0] = string_addr & 0xFF;
		string_data[1] = (string_addr & 0xFF00) >> 8;

		ret = ts->synaptics_ts_read_sponge(ts, string_data, ts->sponge_dump_format, string_addr, ts->sponge_dump_format);
		if (ret < 0) {
			input_err(true, &ts->client->dev,
					"%s: Failed to read sponge\n", __func__);
			snprintf(buf, SEC_CMD_BUF_SIZE,
					"NG, Failed to read sponge, addr=%d",
					string_addr);
			goto out;
		}

		data0 = (string_data[1] & 0xFF) << 8 | (string_data[0] & 0xFF);
		data1 = (string_data[3] & 0xFF) << 8 | (string_data[2] & 0xFF);
		data2 = (string_data[5] & 0xFF) << 8 | (string_data[4] & 0xFF);
		data3 = (string_data[7] & 0xFF) << 8 | (string_data[6] & 0xFF);
		data4 = (string_data[9] & 0xFF) << 8 | (string_data[8] & 0xFF);

		if (data0 || data1 || data2 || data3 || data4) {
			if (ts->sponge_dump_format == 10) {
				snprintf(buff, sizeof(buff),
						"%d: %04x%04x%04x%04x%04x\n",
						string_addr, data0, data1, data2, data3, data4);
			} else {
				snprintf(buff, sizeof(buff),
						"%d: %04x%04x%04x%04x\n",
						string_addr, data0, data1, data2, data3);
			}
			strlcat(buf, buff, PAGE_SIZE);
		}
	}

	if (ts->sponge_inf_dump) {
		if (current_index >= ts->sponge_dump_border) {
			dump_cnt = ((current_index - (ts->sponge_dump_border)) / ts->sponge_dump_format) + 1;
			dump_area = 1;
			sec_spg_dat[0] = ts->sponge_dump_border_lsb;
			sec_spg_dat[1] = ts->sponge_dump_border_msb;
		} else {
			dump_cnt = ((current_index - SEC_TS_CMD_SPONGE_LP_DUMP_EVENT) / ts->sponge_dump_format) + 1;
			dump_area = 0;
			sec_spg_dat[0] = SEC_TS_CMD_SPONGE_LP_DUMP_EVENT;
			sec_spg_dat[1] = 0;
		}

		ret = ts->synaptics_ts_read_sponge(ts, sec_spg_dat, dump_cnt * ts->sponge_dump_format,
					(sec_spg_dat[1] & 0xFF) << 8 | (sec_spg_dat[0] & 0xFF), dump_cnt * ts->sponge_dump_format);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: Failed to read sponge\n", __func__);
			goto out;
		}

		for (i = 0 ; i <= dump_cnt ; i++) {
			int e_offset = i * ts->sponge_dump_format;
			char ibuff[30] = {0, };
			u16 edata[5];

			edata[0] = (sec_spg_dat[1 + e_offset] & 0xFF) << 8 | (sec_spg_dat[0 + e_offset] & 0xFF);
			edata[1] = (sec_spg_dat[3 + e_offset] & 0xFF) << 8 | (sec_spg_dat[2 + e_offset] & 0xFF);
			edata[2] = (sec_spg_dat[5 + e_offset] & 0xFF) << 8 | (sec_spg_dat[4 + e_offset] & 0xFF);
			edata[3] = (sec_spg_dat[7 + e_offset] & 0xFF) << 8 | (sec_spg_dat[6 + e_offset] & 0xFF);
			edata[4] = (sec_spg_dat[9 + e_offset] & 0xFF) << 8 | (sec_spg_dat[8 + e_offset] & 0xFF);

			if (edata[0] || edata[1] || edata[2] || edata[3] || edata[4]) {
				snprintf(ibuff, sizeof(ibuff), "%03d: %04x%04x%04x%04x%04x\n",
						i + (ts->sponge_dump_event * dump_area),
						edata[0], edata[1], edata[2], edata[3], edata[4]);
#if IS_ENABLED(CONFIG_SEC_DEBUG_TSP_LOG)
				sec_tsp_sponge_log(ibuff);
#endif
			}
		}

		ts->sponge_dump_delayed_flag = false;
		ret = ts->synaptics_ts_write_sponge(ts, dump_clear_packet, 2, 0x01, 2);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: Failed to clear sponge dump\n", __func__);
		}
	}
out:
	vfree(sec_spg_dat);
	return strlen(buf);
}

static ssize_t synaptics_ts_fod_position_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[3] = { 0 };
	int i, ret;

	if (!ts->plat_data->support_fod) {
		input_err(true, &ts->client->dev, "%s: fod is not supported\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "NA");
	}

	if (!ts->plat_data->fod_data.vi_size) {
		input_err(true, &ts->client->dev, "%s: not read fod_info yet\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "NG");
	}

	if (ts->plat_data->fod_data.vi_event == CMD_SYSFS) {
		ret = ts->synaptics_ts_read_sponge(ts, ts->plat_data->fod_data.vi_data, ts->plat_data->fod_data.vi_size,
				SEC_TS_CMD_SPONGE_FOD_POSITION, ts->plat_data->fod_data.vi_size);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: Failed to read\n", __func__);
			return snprintf(buf, SEC_CMD_BUF_SIZE, "NG");
		}
	}

	for (i = 0; i < ts->plat_data->fod_data.vi_size; i++) {
		snprintf(buff, 3, "%02X", ts->plat_data->fod_data.vi_data[i]);
		strlcat(buf, buff, SEC_CMD_BUF_SIZE);
	}

	return strlen(buf);
}

static ssize_t synaptics_ts_fod_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);

	return sec_input_get_fod_info(&ts->client->dev, buf);
}

static ssize_t synaptics_aod_active_area(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);

	input_info(true, &ts->client->dev, "%s: top:%d, edge:%d, bottom:%d\n",
		 __func__, ts->plat_data->aod_data.active_area[0],
		 ts->plat_data->aod_data.active_area[1], ts->plat_data->aod_data.active_area[2]);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d,%d,%d",
		 ts->plat_data->aod_data.active_area[0], ts->plat_data->aod_data.active_area[1],
		 ts->plat_data->aod_data.active_area[2]);
}

static ssize_t virtual_prox_show(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	int retval;

	retval = (ts->hover_event != 3) ? 0 : 3;

	input_info(true, &ts->client->dev, "%s: hover_event: %d, retval: %d\n", __func__, ts->hover_event, retval);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", retval);
}

static ssize_t virtual_prox_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	int ret;
	u8 data;

	if (!ts->plat_data->support_ear_detect) {
		input_err(true, &ts->client->dev, "%s: ear detection is not supported\n", __func__);
		return -ENODEV;
	}

	ret = kstrtou8(buf, 8, &data);
	if (ret < 0)
		return ret;

	if (data != 0 && data != 1 && data != 3) {
		input_err(true, &ts->client->dev, "%s: %d is wrong param\n", __func__, data);
		return -EINVAL;
	}

	ts->plat_data->ed_enable = data;
	synaptics_ts_ear_detect_enable(ts, ts->plat_data->ed_enable);
	input_info(true, &ts->client->dev, "%s: %d\n", __func__, data);

	return count;
}

static ssize_t debug_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[516];
	char tbuff[128];

	memset(buff, 0x00, sizeof(buff));

	memset(tbuff, 0x00, sizeof(tbuff));
	snprintf(tbuff, sizeof(tbuff), "SystemInputDebugInfo++\n");
	strlcat(buff, tbuff, sizeof(buff));

	memset(tbuff, 0x00, sizeof(tbuff));
	snprintf(tbuff, sizeof(tbuff), "FW: (I)%02X%02X%02X%02X / (B)%02X%02X%02X%02X\n",
			ts->plat_data->img_version_of_ic[0], ts->plat_data->img_version_of_ic[1],
			ts->plat_data->img_version_of_ic[2], ts->plat_data->img_version_of_ic[3],
			ts->plat_data->img_version_of_bin[0], ts->plat_data->img_version_of_bin[1],
			ts->plat_data->img_version_of_bin[2], ts->plat_data->img_version_of_bin[3]);
	strlcat(buff, tbuff, sizeof(buff));

#if IS_ENABLED(CONFIG_INPUT_TOUCHSCREEN_TCLMV2)
	if (ts->tdata->tclm_level != TCLM_LEVEL_NOT_SUPPORT) {
		memset(tbuff, 0x00, sizeof(tbuff));
		snprintf(tbuff, sizeof(tbuff), "C%02XT%04X.%4s%s Cal_flag:%d fail_cnt:%d\n",
			ts->tdata->nvdata.cal_count, ts->tdata->nvdata.tune_fix_ver,
			ts->tdata->tclm_string[ts->tdata->nvdata.cal_position].f_name,
			(ts->tdata->tclm_level == TCLM_LEVEL_LOCKDOWN) ? ".L" : " ",
			ts->tdata->nvdata.cal_fail_falg, ts->tdata->nvdata.cal_fail_cnt);
		strlcat(buff, tbuff, sizeof(buff));
	}
#endif

	memset(tbuff, 0x00, sizeof(tbuff));
	snprintf(tbuff, sizeof(tbuff), "ic_reset_count %d\n", ts->plat_data->hw_param.ic_reset_count);
	strlcat(buff, tbuff, sizeof(buff));

	memset(tbuff, 0x00, sizeof(tbuff));
	snprintf(tbuff, sizeof(tbuff), "SystemInputDebugInfo--\n");
	strlcat(buff, tbuff, sizeof(buff));

	input_info(true, &ts->client->dev, "%s: size: %ld\n", __func__, strlen(buff));

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buff);
}

static ssize_t enabled_show(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);

	if (!ts->plat_data->enable_sysinput_enabled)
		return -EINVAL;

	input_info(true, &ts->client->dev, "%s: enabled %d\n", __func__, ts->plat_data->enabled);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", ts->plat_data->enabled);
}

static ssize_t enabled_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	struct input_dev *input_dev = ts->plat_data->input_dev;
	int buff[2];
	int ret;

	if (!ts->plat_data->enable_sysinput_enabled)
		return -EINVAL;

	ret = sscanf(buf, "%d,%d", &buff[0], &buff[1]);
	if (ret != 2) {
		input_err(true, &ts->client->dev,
				"%s: failed read params [%d]\n", __func__, ret);
		return -EINVAL;
	}

	if (buff[0] == DISPLAY_STATE_ON && buff[1] == DISPLAY_EVENT_LATE) {
		if (ts->plat_data->enabled) {
			input_err(true, &ts->client->dev, "%s: device already enabled\n", __func__);
			goto out;
		}

		ret = sec_input_enable_device(input_dev);
	} else if (buff[0] == DISPLAY_STATE_OFF && buff[1] == DISPLAY_EVENT_EARLY) {
		if (!ts->plat_data->enabled) {
			input_err(true, &ts->client->dev, "%s: device already disabled\n", __func__);
			goto out;
		}

		ret = sec_input_disable_device(input_dev);
	} else if (buff[0] == DISPLAY_STATE_FORCE_ON) {
		if (ts->plat_data->enabled) {
			input_err(true, &ts->client->dev, "%s: device already enabled\n", __func__);
			goto out;
		}

		ret = sec_input_enable_device(input_dev);
		input_info(true, &ts->client->dev, "%s: DISPLAY_STATE_FORCE_ON(%d)\n", __func__, ret);
	} else if (buff[0] == DISPLAY_STATE_FORCE_OFF) {
		if (!ts->plat_data->enabled) {
			input_err(true, &ts->client->dev, "%s: device already disabled\n", __func__);
			goto out;
		}

		ret = sec_input_disable_device(input_dev);
		input_info(true, &ts->client->dev, "%s: DISPLAY_STATE_FORCE_OFF(%d)\n", __func__, ret);
	}

	if (ret)
		return ret;

out:
	return count;
}

static DEVICE_ATTR(scrub_pos, 0444, scrub_position_show, NULL);
static DEVICE_ATTR(hw_param, 0664, hardware_param_show, hardware_param_store); /* for bigdata */
static DEVICE_ATTR(get_lp_dump, 0444, get_lp_dump, NULL);
static DEVICE_ATTR(sensitivity_mode, 0664, sensitivity_mode_show, sensitivity_mode_store);
static DEVICE_ATTR(prox_power_off, 0664, prox_power_off_show, prox_power_off_store);
static DEVICE_ATTR(support_feature, 0444, read_support_feature, NULL);
static DEVICE_ATTR(fod_pos, 0444, synaptics_ts_fod_position_show, NULL);
static DEVICE_ATTR(fod_info, 0444, synaptics_ts_fod_info_show, NULL);
static DEVICE_ATTR(aod_active_area, 0444, synaptics_aod_active_area, NULL);
static DEVICE_ATTR(virtual_prox, 0664, virtual_prox_show, virtual_prox_store);
static DEVICE_ATTR(debug_info, 0444, debug_info_show, NULL);
static DEVICE_ATTR(enabled, 0664, enabled_show, enabled_store);

static struct attribute *cmd_attributes[] = {
	&dev_attr_scrub_pos.attr,
	&dev_attr_hw_param.attr,
	&dev_attr_get_lp_dump.attr,
	&dev_attr_sensitivity_mode.attr,
	&dev_attr_prox_power_off.attr,
	&dev_attr_support_feature.attr,
	&dev_attr_fod_pos.attr,
	&dev_attr_fod_info.attr,
	&dev_attr_aod_active_area.attr,
	&dev_attr_virtual_prox.attr,
	&dev_attr_debug_info.attr,
	&dev_attr_enabled.attr,
	NULL,
};

static struct attribute_group cmd_attr_group = {
	.attrs = cmd_attributes,
};

static void fw_update(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[64] = { 0 };
	int retval = 0;

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	mutex_lock(&ts->modechange);
	retval = synaptics_ts_fw_update_on_hidden_menu(ts, sec->cmd_param[0]);
	if (retval < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_err(true, &ts->client->dev, "%s: failed [%d]\n", __func__, retval);
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_OK;
		input_info(true, &ts->client->dev, "%s: success [%d]\n", __func__, retval);
	}

	mutex_unlock(&ts->modechange);
}

static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "SY%02X%02X%02X%02X",
			ts->plat_data->img_version_of_bin[0], ts->plat_data->img_version_of_bin[1],
			ts->plat_data->img_version_of_bin[2], ts->plat_data->img_version_of_bin[3]);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_BIN");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[16] = { 0 };
	char model[16] = { 0 };
	int ret;
	int idx;

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	ret = synaptics_ts_get_app_info(ts, &ts->app_info);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Fail to get application info\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	for (idx = 0; idx < 4; idx++)
		ts->plat_data->img_version_of_ic[idx] = ts->config_id[idx];

	snprintf(buff, sizeof(buff), "SY%02X%02X%02X%02X",
			ts->plat_data->img_version_of_ic[0], ts->plat_data->img_version_of_ic[1],
			ts->plat_data->img_version_of_ic[2], ts->plat_data->img_version_of_ic[3]);
	snprintf(model, sizeof(model), "SY%02X%02X",
			ts->plat_data->img_version_of_ic[0], ts->plat_data->img_version_of_ic[1]);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_IC");
		sec_cmd_set_cmd_result_all(sec, model, strnlen(model, sizeof(model)), "FW_MODEL");
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_chip_vendor(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[16] = { 0 };

	strncpy(buff, "SYNAPTICS", sizeof(buff));
	sec_cmd_set_default_result(sec);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "IC_VENDOR");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_chip_name(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[16] = { 0 };

	if (ts->plat_data->img_version_of_ic[0] == 0x31)
		strncpy(buff, "S3908", sizeof(buff));
	else if (ts->plat_data->img_version_of_ic[0] == 0x32)
		strncpy(buff, "S3907", sizeof(buff));
	else
		strncpy(buff, "N/A", sizeof(buff));

	sec_cmd_set_default_result(sec);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "IC_NAME");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_x_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%d", ts->rows);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_y_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%d", ts->cols);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void module_off_master(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[3] = { 0 };
	int ret = 0;

	ret = ts->plat_data->stop_device(ts);

	if (ret == 0)
		snprintf(buff, sizeof(buff), "OK");
	else
		snprintf(buff, sizeof(buff), "NG");

	sec_cmd_set_default_result(sec);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (strncmp(buff, "OK", 2) == 0)
		sec->cmd_state = SEC_CMD_STATUS_OK;
	else
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void module_on_master(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[3] = { 0 };
	int ret = 0;

	ret = ts->plat_data->start_device(ts);

	if (!ts->plat_data->enabled) {
		ts->plat_data->lpmode(ts, TO_LOWPOWER_MODE);
		ts->plat_data->power_state = SEC_INPUT_STATE_LPM;
	}

	if (ret == 0)
		snprintf(buff, sizeof(buff), "OK");
	else
		snprintf(buff, sizeof(buff), "NG");

	sec_cmd_set_default_result(sec);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (strncmp(buff, "OK", 2) == 0)
		sec->cmd_state = SEC_CMD_STATUS_OK;
	else
		sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_crc_check(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	struct synaptics_ts_message_data_blob *tcm_msg = NULL;
	char buff[16] = { 0 };
	unsigned char resp_code;
	int ret;

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		goto NG;
	}

	ret = ts->write_message(ts,
			SYNAPTICS_TS_CMD_IDENTIFY,
			NULL,
			0,
			&resp_code,
			0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to write identify cmd\n", __func__);
		goto NG;
	}

	tcm_msg = &ts->msg_data;

	synaptics_ts_buf_lock(&tcm_msg->in);

	ret = synaptics_ts_v1_parse_idinfo(ts,
			&tcm_msg->in.buf[SYNAPTICS_TS_MESSAGE_HEADER_SIZE],
			tcm_msg->in.buf_size - SYNAPTICS_TS_MESSAGE_HEADER_SIZE,
			tcm_msg->payload_length);
	if (ret < 0) {
		input_info(true, &ts->client->dev, "%s:Fail to identify device\n", __func__);
		synaptics_ts_buf_unlock(&tcm_msg->in);
		goto NG;
	}

	synaptics_ts_buf_unlock(&tcm_msg->in);

	if (IS_NOT_APP_FW_MODE(ts->dev_mode)) {
		input_info(true, &ts->client->dev, "%s: in 0x%02X mode\n", __func__, ts->dev_mode);
		goto NG;
	} else {
		input_info(true, &ts->client->dev, "%s: in application mode\n", __func__);
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	return;
NG:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

static void run_full_rawcap_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	struct synaptics_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct synaptics_ts_test_mode));
	mode.type = TEST_PID05_FULL_RAW_CAP;

	synaptics_ts_test_rawdata_read(ts, sec, &mode);
}

static void run_full_rawcap_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	struct synaptics_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct synaptics_ts_test_mode));
	mode.type = TEST_PID05_FULL_RAW_CAP;
	mode.allnode = TEST_MODE_ALL_NODE;

	synaptics_ts_test_rawdata_read(ts, sec, &mode);
}

static void run_trans_rawcap_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	struct synaptics_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct synaptics_ts_test_mode));
	mode.type = TEST_PID22_TRANS_RAW_CAP;

	synaptics_ts_test_rawdata_read(ts, sec, &mode);
}

static void run_trans_rawcap_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	struct synaptics_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct synaptics_ts_test_mode));
	mode.type = TEST_PID22_TRANS_RAW_CAP;
	mode.allnode = TEST_MODE_ALL_NODE;

	synaptics_ts_test_rawdata_read(ts, sec, &mode);
}

static void run_delta_noise_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	struct synaptics_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct synaptics_ts_test_mode));
	mode.type = TEST_PID10_DELTA_NOISE;

	synaptics_ts_test_rawdata_read(ts, sec, &mode);
}

static void run_delta_noise_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	struct synaptics_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct synaptics_ts_test_mode));
	mode.type = TEST_PID10_DELTA_NOISE;
	mode.allnode = TEST_MODE_ALL_NODE;

	synaptics_ts_test_rawdata_read(ts, sec, &mode);
}

static void run_abs_rawcap_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	struct synaptics_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct synaptics_ts_test_mode));
	mode.type = TEST_PID18_HYBRID_ABS_RAW;
	mode.frame_channel = TEST_MODE_READ_CHANNEL;

	synaptics_ts_test_rawdata_read(ts, sec, &mode);
}

static void run_abs_rawcap_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	struct synaptics_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct synaptics_ts_test_mode));
	mode.type = TEST_PID18_HYBRID_ABS_RAW;
	mode.allnode = TEST_MODE_ALL_NODE;
	mode.frame_channel = TEST_MODE_READ_CHANNEL;

	synaptics_ts_test_rawdata_read(ts, sec, &mode);
}

static void run_abs_noise_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	struct synaptics_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct synaptics_ts_test_mode));
	mode.type = TEST_PID29_HYBRID_ABS_NOISE;
	mode.frame_channel = TEST_MODE_READ_CHANNEL;

	synaptics_ts_test_rawdata_read(ts, sec, &mode);
}

static void run_abs_noise_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	struct synaptics_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct synaptics_ts_test_mode));
	mode.type = TEST_PID29_HYBRID_ABS_NOISE;
	mode.frame_channel = TEST_MODE_READ_CHANNEL;
	mode.allnode = TEST_MODE_ALL_NODE;

	synaptics_ts_test_rawdata_read(ts, sec, &mode);
}

static int get_gap_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[16] = { 0 };
	int ii;
	int node_gap_tx = 0;
	int node_gap_rx = 0;
	int tx_max = 0;
	int rx_max = 0;

	for (ii = 0; ii < (ts->rows * ts->cols); ii++) {
		if ((ii + 1) % (ts->rows) != 0) {
			node_gap_tx = (abs(ts->pFrame[ii + 1] - ts->pFrame[ii])) * 100 / max(ts->pFrame[ii], ts->pFrame[ii + 1]);
			tx_max = max(tx_max, node_gap_tx);
		}

		if (ii < (ts->cols - 1) * ts->rows) {
			node_gap_rx = (abs(ts->pFrame[ii + ts->rows] - ts->pFrame[ii])) * 100 / max(ts->pFrame[ii], ts->pFrame[ii + 1]);
			rx_max = max(rx_max, node_gap_rx);
		}
	}

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		snprintf(buff, sizeof(buff), "%d,%d", 0, tx_max);
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "GAP_DATA_TX");
		snprintf(buff, sizeof(buff), "%d,%d", 0, rx_max);
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "GAP_DATA_RX");
		snprintf(buff, sizeof(buff), "%d,%d", 0, max(tx_max, rx_max));
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "GAP_DATA");
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	return 0;
}

static void get_gap_data_x_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char *buff = NULL;
	int ii;
	int node_gap = 0;
	char temp[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	buff = kzalloc(ts->cols * ts->rows * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff)
		return;

	for (ii = 0; ii < (ts->rows * ts->cols); ii++) {
		if ((ii + 1) % (ts->rows) != 0) {
			node_gap = (abs(ts->pFrame[ii + 1] - ts->pFrame[ii])) * 100 / max(ts->pFrame[ii], ts->pFrame[ii + 1]);
			snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", node_gap);
			strlcat(buff, temp, ts->cols * ts->rows * CMD_RESULT_WORD_LEN);
			memset(temp, 0x00, SEC_CMD_STR_LEN);
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, ts->cols * ts->rows * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);
}

static void get_gap_data_y_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char *buff = NULL;
	int ii;
	int node_gap = 0;
	char temp[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	buff = kzalloc(ts->cols * ts->rows * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff)
		return;

	for (ii = 0; ii < (ts->rows * ts->cols); ii++) {
		if (ii < (ts->cols - 1) * ts->rows) {
			node_gap = (abs(ts->pFrame[ii + ts->rows] - ts->pFrame[ii])) * 100 / max(ts->pFrame[ii], ts->pFrame[ii + 1]);

			snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", node_gap);
			strlcat(buff, temp, ts->cols * ts->rows * CMD_RESULT_WORD_LEN);
			memset(temp, 0x00, SEC_CMD_STR_LEN);
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, ts->cols * ts->rows * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);
}

static void run_sram_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = {0};
	unsigned char *buf = NULL;
	int ret;
	u8 result = 0;
	unsigned char command[4] = {SYNAPTICS_TS_CMD_PRODUCTION_TEST,
		0x01, 0x00, TEST_PID53_SRAM_TEST};

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF)
		goto err_power_state;

	buf = synaptics_ts_pal_mem_alloc(2, sizeof(struct synaptics_ts_identification_info));
	if (!buf) {
		input_err(true, &ts->client->dev, "Fail to create a buffer for test\n");
		goto err_power_state;
	}

	disable_irq(ts->client->irq);

	ret = ts->synaptics_ts_i2c_write(ts, command, sizeof(command), NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "Fail to write cmd\n");
		enable_irq(ts->client->irq);
		synaptics_ts_pal_mem_free(buf);
		goto err_power_state;
	}

	msleep(500);

	ret = synaptics_ts_i2c_only_read(ts, buf,
			sizeof(struct synaptics_ts_identification_info) + SYNAPTICS_TS_MESSAGE_HEADER_SIZE);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "Fail to read cmd\n");
		enable_irq(ts->client->irq);
		synaptics_ts_pal_mem_free(buf);
		goto err_power_state;
	}

	if (buf[0] != SYNAPTICS_TS_V1_MESSAGE_MARKER)
		result = 1; /* FAIL*/
	else
		result = 0;  /* PASS */

	msleep(200);

	enable_irq(ts->client->irq);
	snprintf(buff, sizeof(buff), "%d", result);
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "SRAM");

	synaptics_ts_pal_mem_free(buf);

	return;

err_power_state:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "SRAM");

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void run_interrupt_gpio_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = {0};
	unsigned char *buf = NULL;
	int ret, status1, status2;
	unsigned char command[3] = {SYNAPTICS_TS_CMD_IDENTIFY, 0x00, 0x00};

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: IC power is off\n", __func__);
		goto err_power_state;
	}

	buf = synaptics_ts_pal_mem_alloc(2, sizeof(struct synaptics_ts_identification_info));
	if (!buf) {
		input_err(true, &ts->client->dev, "%s: Fail to create a buffer for test\n", __func__);
		goto err_power_state;
	}

	disable_irq(ts->client->irq);

	ret = ts->synaptics_ts_i2c_write(ts, command, sizeof(command), NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Fail to write cmd\n", __func__);
		goto err;
	}

	msleep(200);

	status1 = gpio_get_value(ts->plat_data->irq_gpio);
	input_info(true, &ts->client->dev, "%s: gpio value %d (should be 0)\n", __func__, status1);

	ret = synaptics_ts_i2c_only_read(ts, buf,
			sizeof(struct synaptics_ts_identification_info) + SYNAPTICS_TS_MESSAGE_HEADER_SIZE);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Fail to read cmd\n", __func__);
		goto err;
	}

	status2 = gpio_get_value(ts->plat_data->irq_gpio);
	input_info(true, &ts->client->dev, "%s: gpio value %d (should be 1)\n", __func__, status2);

	if ((status1 == 0) && (status2 == 1)) {
		snprintf(buff, sizeof(buff), "0");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		if (status1 != 0)
			snprintf(buff, sizeof(buff), "1:HIGH");
		else if (status2 != 1)
			snprintf(buff, sizeof(buff), "1:LOW");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	msleep(50);

	enable_irq(ts->client->irq);
	synaptics_ts_pal_mem_free(buf);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "INT_GPIO");
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
	return;

err:
	enable_irq(ts->client->irq);
	synaptics_ts_pal_mem_free(buf);
err_power_state:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "INT_GPIO");

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

enum prox_intensity_test_item {
	PROX_INTENSITY_TEST_THD_X = 0,
	PROX_INTENSITY_TEST_THD_Y,
	PROX_INTENSITY_TEST_SUM_X,
	PROX_INTENSITY_TEST_SUM_Y,
	PROX_INTENSITY_TEST_MAX,
};
static void run_prox_intensity_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	struct synaptics_ts_buffer test_data;
	short *data_ptr = NULL;
	short value[PROX_INTENSITY_TEST_MAX] = { 0 };
	int ret, i;

	sec_cmd_set_default_result(sec);

	if (!ts->plat_data->support_ear_detect) {
		input_info(true, &ts->client->dev, "%s: ear detection is not supported\n", __func__);
		snprintf(buff, sizeof(buff), "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		return;
	}
	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: IC power is off\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		return;
	}

	synaptics_ts_buf_init(&test_data);

	ret = synaptics_ts_run_production_test(ts,
			TEST_PID198_PROX_INTENSITY,
			&test_data);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Fail to run test %d\n",
				__func__, TEST_PID198_PROX_INTENSITY);
		synaptics_ts_buf_release(&test_data);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		return;
	}

	data_ptr = (short *)&test_data.buf[0];
	for (i = 0; i < PROX_INTENSITY_TEST_MAX; i++) {
		value[i] = *data_ptr;
		data_ptr++;
	}

	synaptics_ts_buf_release(&test_data);

	snprintf(buff, sizeof(buff), "SUM_X:%d SUM_Y:%d THD_X:%d THD_Y:%d",
			value[PROX_INTENSITY_TEST_SUM_X], value[PROX_INTENSITY_TEST_SUM_Y],
			value[PROX_INTENSITY_TEST_THD_X], value[PROX_INTENSITY_TEST_THD_Y]);
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void run_factory_miscalibration(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = {0};
	struct synaptics_ts_buffer test_data;
	unsigned short *data_ptr = NULL;
	int ret, i;
	short result[3] = {0};

	sec_cmd_set_default_result(sec);

	if (ts->tdata->tclm_level == TCLM_LEVEL_NOT_SUPPORT) {
		snprintf(buff, sizeof(buff), "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		input_info(true, &ts->client->dev, "%s: tclm not supported\n", __func__);
		return;
	}

	synaptics_ts_buf_init(&test_data);

	ret = synaptics_ts_run_production_test(ts,
			TEST_PID54_BSC_DIFF_TEST,
			&test_data);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "Fail to run test %d\n",
		TEST_PID54_BSC_DIFF_TEST);
		synaptics_ts_buf_release(&test_data);
		goto exit;
	}

	data_ptr = (unsigned short *)&test_data.buf[0];
	for (i = 0; i < test_data.data_length / 2; i++) {
		result[i] = *data_ptr;
		data_ptr++;
	}

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		snprintf(buff, sizeof(buff), "%d,%d", result[1], result[2]);
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "MIS_CAL");
	} else {
		if (result[0] == 1) {
			snprintf(buff, sizeof(buff), "OK,%d,%d", result[1], result[2]);
		} else {
			snprintf(buff, sizeof(buff), "NG,%d,%d", result[1], result[2]);
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	synaptics_ts_buf_release(&test_data);

	return;

exit:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "MIS_CAL");

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void run_miscalibration(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = {0};
	struct synaptics_ts_buffer test_data;
	unsigned short *data_ptr = NULL;
	int ret, i;
	short result[3] = {0};

	sec_cmd_set_default_result(sec);

	if (ts->tdata->tclm_level == TCLM_LEVEL_NOT_SUPPORT) {
		snprintf(buff, sizeof(buff), "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		input_info(true, &ts->client->dev, "%s: tclm not supported\n", __func__);
		return;
	}

	synaptics_ts_buf_init(&test_data);

	ret = synaptics_ts_run_production_test(ts,
			TEST_PID61_MISCAL,
			&test_data);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "Fail to run test %d\n",
		TEST_PID61_MISCAL);
		synaptics_ts_buf_release(&test_data);
		goto exit;
	}

	data_ptr = (unsigned short *)&test_data.buf[0];
	for (i = 0; i < test_data.data_length / 2; i++) {
		result[i] = *data_ptr;
		data_ptr++;
	}

	if (result[0] == 1) {
		snprintf(buff, sizeof(buff), "OK,%d,%d", result[1], result[2]);
	} else {
		snprintf(buff, sizeof(buff), "NG,%d,%d", result[1], result[2]);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	synaptics_ts_buf_release(&test_data);

	return;

exit:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void run_miscal_data_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	struct synaptics_ts_buffer test_data;
	char buff[SEC_CMD_STR_LEN] = {0};
	int min, max;
	int i;
	int ret;
	u8 test_type[3] = {TEST_PID65_MISCALDATA_NORMAL, TEST_PID66_MISCALDATA_NOISE, TEST_PID67_MISCALDATA_WET};

	sec_cmd_set_default_result(sec);

	if (ts->tdata->tclm_level == TCLM_LEVEL_NOT_SUPPORT) {
		snprintf(buff, sizeof(buff), "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		input_info(true, &ts->client->dev, "%s: tclm not supported\n", __func__);
		return;
	}

	/* only print log */
	for (i = 0; i < 3; i++) {
		input_raw_info(true, &ts->client->dev, "%s: %d\n", __func__, test_type[i]);
		synaptics_ts_buf_init(&test_data);
		ret = synaptics_ts_run_production_test(ts, test_type[i], &test_data);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "Fail to run test %d\n", test_type[i]);
			synaptics_ts_buf_release(&test_data);
			snprintf(buff, sizeof(buff), "NG");
			sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			return;
		}
		synaptics_ts_print_frame(ts, &test_data, &min, &max, test_type[i]);
		synaptics_ts_buf_release(&test_data);
	}

	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

void synaptics_ts_rawdata_read_all(struct synaptics_ts_data *ts)
{
	struct synaptics_ts_buffer test_data;
	int min, max;
	int i, test_num;
	int ret;
	u8 test_type[7] = {TEST_PID10_DELTA_NOISE,
			TEST_PID18_HYBRID_ABS_RAW, TEST_PID22_TRANS_RAW_CAP, TEST_PID29_HYBRID_ABS_NOISE,
			TEST_PID65_MISCALDATA_NORMAL, TEST_PID66_MISCALDATA_NOISE, TEST_PID67_MISCALDATA_WET};

	if (ts->tdata->tclm_level == TCLM_LEVEL_NOT_SUPPORT)
		test_num = 4;
	else
		test_num = 7;

	ts->tsp_dump_lock = 1;
	input_raw_data_clear();

	for (i = 0; i < test_num; i++) {
		input_raw_info(true, &ts->client->dev, "%s: %d\n", __func__, test_type[i]);
		synaptics_ts_buf_init(&test_data);
		ret = synaptics_ts_run_production_test(ts, test_type[i], &test_data);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "Fail to run test %d\n", test_type[i]);
			synaptics_ts_buf_release(&test_data);
			goto out;
		}
		if (test_type[i] == TEST_PID18_HYBRID_ABS_RAW || test_type[i] == TEST_PID29_HYBRID_ABS_NOISE)
			synaptics_ts_print_channel(ts, &test_data, &min, &max, test_type[i]);
		else
			synaptics_ts_print_frame(ts, &test_data, &min, &max, test_type[i]);
		synaptics_ts_buf_release(&test_data);
	}

	synaptics_ts_clear_buffer(ts);
	synaptics_ts_locked_release_all_finger(ts);

out:
	ts->tsp_dump_lock = 0;
}

static void factory_cmd_result_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);

	sec->item_count = 0;
	memset(sec->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);

	if (ts->tsp_dump_lock == 1) {
		input_err(true, &ts->client->dev, "%s: already checking now\n", __func__);
		sec->cmd_all_factory_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}
	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: IC is power off\n", __func__);
		sec->cmd_all_factory_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;

	get_chip_vendor(sec);
	get_chip_name(sec);
	get_fw_ver_bin(sec);
	get_fw_ver_ic(sec);

	run_trans_rawcap_read(sec);
	get_gap_data(sec);

	run_full_rawcap_read(sec);
	run_delta_noise_read(sec);
	run_abs_rawcap_read(sec);
	get_self_channel_data(sec, TEST_PID18_HYBRID_ABS_RAW);
	run_abs_noise_read(sec);

#ifdef TCLM_CONCEPT
	if (ts->tdata->tclm_level != TCLM_LEVEL_NOT_SUPPORT)
		run_factory_miscalibration(sec);
#endif

	run_sram_test(sec);
	run_interrupt_gpio_test(sec);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;

out:
	input_info(true, &ts->client->dev, "%s: %d%s\n", __func__, sec->item_count, sec->cmd_result_all);
}

static void run_force_calibration(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = {0};
	int rc;

	sec_cmd_set_default_result(sec);

	if (ts->tdata->tclm_level == TCLM_LEVEL_NOT_SUPPORT) {
		snprintf(buff, sizeof(buff), "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		goto out_force_cal_before_irq_ctrl;
	}

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out_force_cal_before_irq_ctrl;
	}

	if (ts->plat_data->touch_count > 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out_force_cal_before_irq_ctrl;
	}

	disable_irq(ts->client->irq);

	rc = synaptics_ts_calibration(ts);
	if (rc < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out_force_cal;
	}

#ifdef TCLM_CONCEPT
	/* devide tclm case */
	sec_tclm_case(ts->tdata, sec->cmd_param[0]);

	input_info(true, &ts->client->dev, "%s: param, %d, %c, %d\n", __func__,
		sec->cmd_param[0], sec->cmd_param[0], ts->tdata->root_of_calibration);

	rc = sec_execute_tclm_package(ts->tdata, 1);
	if (rc < 0) {
		input_err(true, &ts->client->dev,
					"%s: sec_execute_tclm_package\n", __func__);
	}
	sec_tclm_root_of_cal(ts->tdata, CALPOSITION_NONE);
#endif

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;

out_force_cal:
	enable_irq(ts->client->irq);

out_force_cal_before_irq_ctrl:
#ifdef TCLM_CONCEPT
	/* not to enter external factory mode without setting everytime */
	ts->tdata->external_factory = false;
#endif
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}


#ifdef TCLM_CONCEPT
static void set_tsp_test_result(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	struct sec_ts_test_result *result;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	input_info(true, &ts->client->dev, "%s\n", __func__);

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	ts->tdata->tclm_read(ts->client, SEC_TCLM_NVM_ALL_DATA);
	if (ts->fac_nv == 0xFF)
		ts->fac_nv = 0;

	result = (struct sec_ts_test_result *)&ts->fac_nv;

	if (sec->cmd_param[0] == TEST_OCTA_ASSAY) {
		result->assy_result = sec->cmd_param[1];
		if (result->assy_count < 3)
			result->assy_count++;
	}

	if (sec->cmd_param[0] == TEST_OCTA_MODULE) {
		result->module_result = sec->cmd_param[1];
		if (result->module_count < 3)
			result->module_count++;
	}

	input_info(true, &ts->client->dev, "%s: %d, %d, %d, %d, 0x%X\n", __func__,
			result->module_result, result->module_count,
			result->assy_result, result->assy_count, result->data[0]);

	ts->fac_nv = *result->data;

	ts->tdata->tclm_write(ts->client, SEC_TCLM_NVM_ALL_DATA);
	ts->tdata->tclm_read(ts->client, SEC_TCLM_NVM_ALL_DATA);

	input_info(true, &ts->client->dev, "%s: command (1)%X, (2)%X: %X\n",
			__func__, sec->cmd_param[0], sec->cmd_param[1], ts->fac_nv);

	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void get_tsp_test_result(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	struct sec_ts_test_result *result;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	ts->tdata->tclm_read(ts->client, SEC_TCLM_NVM_ALL_DATA);
  	if (ts->fac_nv == 0xFF) {
		ts->fac_nv = 0;
		ts->tdata->tclm_write(ts->client, SEC_TCLM_NVM_ALL_DATA);
  	}

	result = (struct sec_ts_test_result *)&ts->fac_nv;

	input_info(true, &ts->client->dev, "%s: [0x%X][0x%X] M:%d, M:%d, A:%d, A:%d\n",
			__func__, *result->data, ts->fac_nv,
			result->module_result, result->module_count,
			result->assy_result, result->assy_count);

	snprintf(buff, sizeof(buff), "M:%s, M:%d, A:%s, A:%d",
			result->module_result == 0 ? "NONE" :
			result->module_result == 1 ? "FAIL" :
			result->module_result == 2 ? "PASS" : "A",
			result->module_count,
			result->assy_result == 0 ? "NONE" :
			result->assy_result == 1 ? "FAIL" :
			result->assy_result == 2 ? "PASS" : "A",
			result->assy_count);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void clear_tsp_test_result(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	input_info(true, &ts->client->dev, "%s\n", __func__);

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	ts->fac_nv = 0;
	ts->tdata->tclm_write(ts->client, SEC_TCLM_NVM_ALL_DATA);
	ts->tdata->tclm_read(ts->client, SEC_TCLM_NVM_ALL_DATA);

	input_info(true, &ts->client->dev, "%s: fac_nv:0x%02X\n", __func__, ts->fac_nv);

	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void increase_disassemble_count(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[3] = { 0 };

	input_info(true, &ts->client->dev, "%s\n", __func__);

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	ts->tdata->tclm_read(ts->client, SEC_TCLM_NVM_ALL_DATA);

	input_info(true, &ts->client->dev, "%s: disassemble count is #1 %d\n", __func__, ts->disassemble_count);

	if (ts->disassemble_count == 0xFF)
		ts->disassemble_count = 0;

	if (ts->disassemble_count < 0xFE)
		ts->disassemble_count++;

	/* Use TSP NV area : in this model, use only one byte
	 * buff[0] : offset from user NVM storage
	 * buff[1] : length of stroed data - 1 (ex. using 1byte, value is  1 - 1 = 0)
	 * buff[2] : write data
	 */
	ts->tdata->tclm_write(ts->client, SEC_TCLM_NVM_ALL_DATA);
	ts->tdata->tclm_read(ts->client, SEC_TCLM_NVM_ALL_DATA);

	input_info(true, &ts->client->dev, "%s: check disassemble count: %d\n", __func__, ts->disassemble_count);

	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void get_disassemble_count(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	input_info(true, &ts->client->dev, "%s\n", __func__);

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	memset(buff, 0x00, SEC_CMD_STR_LEN);
	ts->tdata->tclm_read(ts->client, SEC_TCLM_NVM_ALL_DATA);

	input_info(true, &ts->client->dev, "%s: read disassemble count: %d\n", __func__, ts->disassemble_count);

	snprintf(buff, sizeof(buff), "%d", ts->disassemble_count);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void get_pat_information(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[50] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ts->tdata->tclm_level == TCLM_LEVEL_NOT_SUPPORT) {
		snprintf(buff, sizeof(buff), "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		input_info(true, &ts->client->dev, "%s: tclm not supported\n", __func__);
		return;
	}

#ifdef CONFIG_SEC_FACTORY
	if (ts->factory_position == 1) {
		sec_tclm_initialize(ts->tdata);
		ts->tdata->tclm_read(ts->client, SEC_TCLM_NVM_ALL_DATA);
	}
#endif
	/* fixed tune version will be saved at excute autotune */
	snprintf(buff, sizeof(buff), "C%02XT%04X.%4s%s%c%d%c%d%c%d",
		ts->tdata->nvdata.cal_count, ts->tdata->nvdata.tune_fix_ver,
		ts->tdata->tclm_string[ts->tdata->nvdata.cal_position].f_name,
		(ts->tdata->tclm_level == TCLM_LEVEL_LOCKDOWN) ? ".L " : " ",
		ts->tdata->cal_pos_hist_last3[0], ts->tdata->cal_pos_hist_last3[1],
		ts->tdata->cal_pos_hist_last3[2], ts->tdata->cal_pos_hist_last3[3],
		ts->tdata->cal_pos_hist_last3[4], ts->tdata->cal_pos_hist_last3[5]);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void set_external_factory(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	ts->tdata->external_factory = true;
	snprintf(buff, sizeof(buff), "OK");

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}
#endif

static void run_trx_short_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	struct synaptics_ts_buffer test_data;
	char buff[SEC_CMD_STR_LEN];
	u8 type = 0;
	int ret;
	char result_uevent[32];
	char test[32];
	char tempn[40] = {0};
	char result[32];
	int i, j;
	unsigned char result_data = 0;
	u8 *test_result_buff;
	unsigned char tmp = 0;
	unsigned char p = 0;

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	if (ts->plat_data->power_state == SEC_INPUT_STATE_LPM) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is lp mode\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	if (sec->cmd_param[0] == 1 && sec->cmd_param[1] == 0) {
		input_err(true, &ts->client->dev,
				"%s: %s: seperate cm1 test open / short test result\n", __func__, buff);

		snprintf(buff, sizeof(buff), "%s", "CONT");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_OK;
		return;
	}

	if (sec->cmd_param[0] == 1 && sec->cmd_param[1] == 1) {
		type = TEST_PID58_TRX_OPEN;
	} else if (sec->cmd_param[0] == 1 && sec->cmd_param[1] == 2) {
		type = TEST_PID01_TRX_SHORTS;
	} else if (sec->cmd_param[0] == 3) {
		type = TEST_PID59_PATTERN_SHORT;
	} else if (sec->cmd_param[0] > 1) {
		u8 result_buff[10];

		snprintf(result_buff, sizeof(result_buff), "NA");
		sec_cmd_set_cmd_result(sec, result_buff, strnlen(result_buff, sizeof(result_buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;

		input_info(true, &ts->client->dev, "%s : not supported test case\n", __func__);
		return;
	}

	test_result_buff = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!test_result_buff) {
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	if (sec->cmd_param[1])
		snprintf(test, sizeof(test), "TEST=%d,%d", sec->cmd_param[0], sec->cmd_param[1]);
	else
		snprintf(test, sizeof(test), "TEST=%d", sec->cmd_param[0]);

	synaptics_ts_buf_init(&test_data);

	ret = synaptics_ts_run_production_test(ts, type, &test_data);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "Fail to run test %d\n", type);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;

		snprintf(result_uevent, sizeof(result_uevent), "RESULT=FAIL");
		sec_cmd_send_event_to_user(&ts->sec, test, result_uevent);

		synaptics_ts_buf_release(&test_data);
		kfree(test_result_buff);
		return;
	}

	memset(tempn, 0x00, 40);
	/* print log */
	if (type == TEST_PID58_TRX_OPEN)
		snprintf(tempn, 40, " TX/RX_OPEN:");
	else if (type == TEST_PID01_TRX_SHORTS)
		snprintf(tempn, 40, " TX/RX_SHORT:");
	else if (type == TEST_PID59_PATTERN_SHORT)
		snprintf(tempn, 40, " PATTERN_SHORT:");

	strlcat(test_result_buff, tempn, PAGE_SIZE);

	if (type == TEST_PID01_TRX_SHORTS) {
		for (i = 0; i < test_data.data_length; i++) {
			memset(tempn, 0x00, 40);
			tmp = test_data.buf[i];
			result_data += tmp;
			for (j = 0; j < 8; j++) {
				p = GET_BIT_LSB(tmp, j);
				if (p == 0x1) {
					snprintf(tempn, 20, "TRX%d,", i * 8 + j);
					strlcat(test_result_buff, tempn, PAGE_SIZE);
				}
			}
		}
	} else {
		for (i = 0; i < test_data.data_length; i++) {
			memset(tempn, 0x00, 40);
			tmp = test_data.buf[i];
			result_data += tmp;
			for (j = 0; j < 8; j++) {
				p = GET_BIT_MSB(tmp, j);
				if (p == 0x1) {
					if (i * 8 + j < ts->cols)
						snprintf(tempn, 20, "RX%d,", i * 8 + j);
					else
						snprintf(tempn, 20, "TX%d,", (i * 8 + j) - ts->cols);
					strlcat(test_result_buff, tempn, PAGE_SIZE);
				}
			}
		}
	}

	if (result_data == 0)
		ret = SEC_SUCCESS;
	else
		ret = SEC_ERROR;

	if (ret < 0) {
		sec_cmd_set_cmd_result(sec, test_result_buff, strnlen(buff, PAGE_SIZE));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;

		input_info(true, &ts->client->dev, "%s: %s\n", __func__, test_result_buff);

		snprintf(result, sizeof(result), "RESULT=FAIL");
		sec_cmd_send_event_to_user(&ts->sec, test, result);

		synaptics_ts_buf_release(&test_data);
		kfree(test_result_buff);

		return;

	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_OK;
		input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

		snprintf(result_uevent, sizeof(result_uevent), "RESULT=PASS");
		sec_cmd_send_event_to_user(&ts->sec, test, result_uevent);

		synaptics_ts_buf_release(&test_data);
		kfree(test_result_buff);

		return;

	}
}

static void run_jitter_delta_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	int result = -1;
	const int IDX_MIN = 1;
	const int IDX_MAX = 0;
	short min[2] = { 0 };
	short max[2] = { 0 };
	short avg[2] = { 0 };
	unsigned char buf[32] = { 0 };
	unsigned int time = 0;
	unsigned int timeout = 30;
	unsigned char command[4] = {SYNAPTICS_TS_CMD_PRODUCTION_TEST,
		0x01, 0x00, TEST_PID52_JITTER_DELTA_TEST};

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF ||
			ts->plat_data->power_state == SEC_INPUT_STATE_LPM) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		goto OUT_JITTER_DELTA;
	}

	synaptics_ts_release_all_finger(ts);

	disable_irq(ts->client->irq);

	ret = ts->synaptics_ts_i2c_write(ts, command, sizeof(command), NULL, 0);
	if (ret < 0) {
		input_info(true, &ts->client->dev, "%s: failed to set active mode\n", __func__);
		goto OUT_JITTER_DELTA;
	}

	/* jitter delta + 1000 frame*/
	for (time = 0; time < timeout; time++) {
		sec_delay(500);

		ret = synaptics_ts_i2c_only_read(ts,
			buf,
			sizeof(buf));
		if (ret < 0) {
			input_info(true, &ts->client->dev, "Fail to read the test result\n");
			goto OUT_JITTER_DELTA;
		}

		if (buf[1] == STATUS_OK) {
			result = 0;
			break;
		}
	}

	if (time >= timeout) {
		input_info(true, &ts->client->dev, "Timed out waiting for result of PT34\n");
		goto OUT_JITTER_DELTA;
	}

	min[IDX_MIN] = (short)synaptics_ts_pal_le2_to_uint(&buf[SYNAPTICS_TS_MESSAGE_HEADER_SIZE + 0]);
	min[IDX_MAX] = (short)synaptics_ts_pal_le2_to_uint(&buf[SYNAPTICS_TS_MESSAGE_HEADER_SIZE + 2]);

	max[IDX_MIN] = (short)synaptics_ts_pal_le2_to_uint(&buf[SYNAPTICS_TS_MESSAGE_HEADER_SIZE + 4]);
	max[IDX_MAX] = (short)synaptics_ts_pal_le2_to_uint(&buf[SYNAPTICS_TS_MESSAGE_HEADER_SIZE + 6]);

	avg[IDX_MIN] = (short)synaptics_ts_pal_le2_to_uint(&buf[SYNAPTICS_TS_MESSAGE_HEADER_SIZE + 8]);
	avg[IDX_MAX] = (short)synaptics_ts_pal_le2_to_uint(&buf[SYNAPTICS_TS_MESSAGE_HEADER_SIZE + 10]);

OUT_JITTER_DELTA:

	enable_irq(ts->client->irq);

	if (result < 0)
		snprintf(buff, sizeof(buff), "NG");
	else
		snprintf(buff, sizeof(buff), "%d,%d,%d,%d,%d,%d", min[IDX_MIN], min[IDX_MAX],
						max[IDX_MIN], max[IDX_MAX], avg[IDX_MIN], avg[IDX_MAX]);

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		char buffer[SEC_CMD_STR_LEN] = { 0 };

		snprintf(buffer, sizeof(buffer), "%d,%d", min[IDX_MIN], min[IDX_MAX]);
		sec_cmd_set_cmd_result_all(sec, buffer, strnlen(buffer, sizeof(buffer)), "JITTER_DELTA_MIN");

		memset(buffer, 0x00, sizeof(buffer));
		snprintf(buffer, sizeof(buffer), "%d,%d", max[IDX_MIN], max[IDX_MAX]);
		sec_cmd_set_cmd_result_all(sec, buffer, strnlen(buffer, sizeof(buffer)), "JITTER_DELTA_MAX");

		memset(buffer, 0x00, sizeof(buffer));
		snprintf(buffer, sizeof(buffer), "%d,%d", avg[IDX_MIN], avg[IDX_MAX]);
		sec_cmd_set_cmd_result_all(sec, buffer, strnlen(buffer, sizeof(buffer)), "JITTER_DELTA_AVG");
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void run_snr_non_touched(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned char buf[2 * 7 * (3 * 3) + 4] = { 0 };
	int ret = 0;
	int wait_time = 0;
	unsigned time = 0;
	unsigned timeout = 30;
	unsigned int input = 0;
	unsigned char command[4] = {SYNAPTICS_TS_CMD_PRODUCTION_TEST,
			0x01, 0x00, TEST_PID56_TSP_SNR_NONTOUCH};

	input_info(true, &ts->client->dev, "%s\n", __func__);

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		goto out_init;
	}

	if (sec->cmd_param[0] < 1 || sec->cmd_param[0] > 1000) {
		input_err(true, &ts->client->dev, "%s: strange value frame:%d\n",
				__func__, sec->cmd_param[0]);
		goto out_init;
	}

	input = sec->cmd_param[0];

	/* set up snr frame */
	ret = synaptics_ts_set_dynamic_config(ts,
			DC_TSP_SNR_TEST_FRAMES,
			(unsigned short)input);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "Fail to set %d with dynamic command 0x%x\n",
			input, DC_TSP_SNR_TEST_FRAMES);
		goto out;
	}

	disable_irq(ts->client->irq);
	/* enter SNR mode */
	ret = ts->synaptics_ts_i2c_write(ts, command, sizeof(command), NULL, 0);
	if (ret < 0) {
		input_info(true, &ts->client->dev, "%s: failed to set active mode\n", __func__);
		goto out;
	}

	wait_time = (sec->cmd_param[0] * 1000) / 120 + (sec->cmd_param[0] * 1000) % 120;

	sec_delay(wait_time);

	for (time = 0; time < timeout; time++) {
		sec_delay(20);

		ret = synaptics_ts_i2c_only_read(ts,
			buf,
			sizeof(buf));
		if (ret < 0) {
			input_info(true, &ts->client->dev, "Fail to read the test result\n");
			goto out;
		}

		if (buf[1] == STATUS_OK)
			break;
	}
	if (buf[1] == 0) {
		input_err(true, &ts->client->dev, "%s: failed non-touched status:%d\n", __func__, buf[1]);
		goto out;
	}

	enable_irq(ts->client->irq);
	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return;

out:
	enable_irq(ts->client->irq);
out_init:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void run_snr_touched(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char tbuff[SEC_CMD_STR_LEN] = { 0 };
	unsigned char buf[2 * 7 * (3 * 3) + 4] = { 0 };
	int ret = 0;
	int wait_time = 0;
	int i = 0;
	unsigned time = 0;
	unsigned timeout = 30;
	unsigned int input = 0;
	unsigned char command[4] = {SYNAPTICS_TS_CMD_PRODUCTION_TEST,
			0x01, 0x00, TEST_PID57_TSP_SNR_TOUCH};
	struct tsp_snr_result_of_point result[9];

	memset(result, 0x00, sizeof(struct tsp_snr_result_of_point) * 9);

	input_info(true, &ts->client->dev, "%s\n", __func__);

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		goto out_init;
	}

	if (sec->cmd_param[0] < 1 || sec->cmd_param[0] > 1000) {
		input_err(true, &ts->client->dev, "%s: strange value frame:%d\n",
				__func__, sec->cmd_param[0]);
		goto out_init;
	}


	input = sec->cmd_param[0];

	/* set up snr frame */
	ret = synaptics_ts_set_dynamic_config(ts,
			DC_TSP_SNR_TEST_FRAMES,
			(unsigned short)input);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "Fail to set %d with dynamic command 0x%x\n",
			input, DC_TSP_SNR_TEST_FRAMES);
		goto out;
	}

	disable_irq(ts->client->irq);
	/* enter SNR mode */
	ret = ts->synaptics_ts_i2c_write(ts, command, sizeof(command), NULL, 0);
	if (ret < 0) {
		input_info(true, &ts->client->dev, "%s: failed to set active mode\n", __func__);
		goto out;
	}

	wait_time = (sec->cmd_param[0] * 1000) / 120 + (sec->cmd_param[0] * 1000) % 120;

	sec_delay(wait_time);

	for (time = 0; time < timeout; time++) {
		sec_delay(20);

		ret = synaptics_ts_i2c_only_read(ts,
			buf,
			sizeof(buf));
		if (ret < 0) {
			input_info(true, &ts->client->dev, "Fail to read the test result\n");
			goto out;
		}

		if (buf[1] == STATUS_OK)
			break;
	}

	if (buf[1] == 0) {
		input_err(true, &ts->client->dev, "%s: failed non-touched status:%d\n", __func__, buf[1]);
		goto out;
	}

	enable_irq(ts->client->irq);

	//memcpy(result, &buf[SYNAPTICS_TS_MESSAGE_HEADER_SIZE], sizeof(struct tsp_snr_result_of_point) * 9);

	for (i = 0; i < 9; i++) {

		result[i].max = (short)synaptics_ts_pal_le2_to_uint(&buf[i * 14 + SYNAPTICS_TS_MESSAGE_HEADER_SIZE]);
		result[i].min = (short)synaptics_ts_pal_le2_to_uint(&buf[i * 14 + SYNAPTICS_TS_MESSAGE_HEADER_SIZE + 2]);
		result[i].average = (short)synaptics_ts_pal_le2_to_uint(&buf[i * 14 + SYNAPTICS_TS_MESSAGE_HEADER_SIZE + 4]);
		result[i].nontouch_peak_noise = (short)synaptics_ts_pal_le2_to_uint(&buf[i * 14 + SYNAPTICS_TS_MESSAGE_HEADER_SIZE + 6]);
		result[i].touch_peak_noise = (short)synaptics_ts_pal_le2_to_uint(&buf[i * 14 + SYNAPTICS_TS_MESSAGE_HEADER_SIZE + 8]);
		result[i].snr1 = (short)synaptics_ts_pal_le2_to_uint(&buf[i * 14 + SYNAPTICS_TS_MESSAGE_HEADER_SIZE + 10]);
		result[i].snr2 = (short)synaptics_ts_pal_le2_to_uint(&buf[i * 14 + SYNAPTICS_TS_MESSAGE_HEADER_SIZE + 12]);

		input_info(true, &ts->client->dev, "%s: average:%d, snr1:%d, snr2:%d\n", __func__,
			result[i].average, result[i].snr1, result[i].snr2);
		snprintf(tbuff, sizeof(tbuff), "%d,%d,%d,", result[i].average, result[i].snr1, result[i].snr2);
		strlcat(buff, tbuff, sizeof(buff));
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return;

out:
	enable_irq(ts->client->irq);
out_init:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

}

static void factory_cmd_result_all_imagetest(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);

	sec->item_count = 0;
	memset(sec->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		sec->cmd_all_factory_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;

	run_jitter_delta_test(sec);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;

out:
	input_info(true, &ts->client->dev, "%s: %d%s\n", __func__, sec->item_count, sec->cmd_result_all);
}

static void glove_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	char mode;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		mode = sec->cmd_param[0];

		ret = synaptics_ts_set_dynamic_config(ts, DC_ENABLE_HIGHSENSMODE, mode);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: failed, retval = %d\n", __func__, ret);
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
		} else {
			snprintf(buff, sizeof(buff), "OK");
			sec->cmd_state = SEC_CMD_STATUS_OK;
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s cmd_param: %d\n", __func__, buff, sec->cmd_param[0]);
}

static void dead_zone_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	char mode;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		mode = sec->cmd_param[0];

		ret = synaptics_ts_set_dynamic_config(ts, DC_ENABLE_DEADZONE, mode);
		if (ret < 0) {
			input_err(true, &ts->client->dev,
					"%s: failed to set deadzone\n", __func__);
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto err_set_dead_zone;
		}

		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

err_set_dead_zone:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
};

static void clear_cover_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	input_info(true, &ts->client->dev, "%s: start clear_cover_mode %s\n", __func__, buff);
	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 3) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0] > 1) {
			ts->plat_data->cover_type = sec->cmd_param[1];
			ts->cover_closed = true;
		} else {
			ts->cover_closed = false;
		}
		synaptics_ts_set_cover_type(ts, ts->cover_closed);

		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
};

static void run_rawdata_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ts->tsp_dump_lock == 1) {
		input_err(true, &ts->client->dev, "%s: already checking now\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}
	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: IC is power off\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	synaptics_ts_rawdata_read_all(ts);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void spay_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	if (sec->cmd_param[0])
		ts->plat_data->lowpower_mode |= SEC_TS_MODE_SPONGE_SWIPE;
	else
		ts->plat_data->lowpower_mode &= ~SEC_TS_MODE_SPONGE_SWIPE;

	input_info(true, &ts->client->dev, "%s: %s, %02X\n",
			__func__, sec->cmd_param[0] ? "on" : "off", ts->plat_data->lowpower_mode);

	synaptics_ts_set_custom_library(ts);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;
}

static void set_aod_rect(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret, i;

	sec_cmd_set_default_result(sec);

	input_info(true, &ts->client->dev, "%s: w:%d, h:%d, x:%d, y:%d, lowpower_mode:0x%02X\n",
			__func__, sec->cmd_param[0], sec->cmd_param[1],
			sec->cmd_param[2], sec->cmd_param[3], ts->plat_data->lowpower_mode);

	for (i = 0; i < 4; i++)
		ts->plat_data->aod_data.rect_data[i] = sec->cmd_param[i];

	ret = synaptics_ts_set_aod_rect(ts);
	if (ret < 0)
		goto NG;

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

NG:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void get_aod_rect(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned char data[8] = {0};
	u16 rect_data[4] = {0, };
	int ret, i;
	unsigned short offset;

	sec_cmd_set_default_result(sec);

	offset = 2;

	ret = ts->synaptics_ts_read_sponge(ts,
			data, sizeof(data), offset, sizeof(data));
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to read rect\n", __func__);
		goto NG;
	}

	for (i = 0; i < 4; i++)
		rect_data[i] = (data[i * 2 + 1] & 0xFF) << 8 | (data[i * 2] & 0xFF);

	input_info(true, &ts->client->dev, "%s: w:%d, h:%d, x:%d, y:%d\n",
			__func__, rect_data[0], rect_data[1], rect_data[2], rect_data[3]);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

NG:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void aod_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	if (sec->cmd_param[0])
		ts->plat_data->lowpower_mode |= SEC_TS_MODE_SPONGE_AOD;
	else
		ts->plat_data->lowpower_mode &= ~SEC_TS_MODE_SPONGE_AOD;

	input_info(true, &ts->client->dev, "%s: %s, %02X\n",
			__func__, sec->cmd_param[0] ? "on" : "off", ts->plat_data->lowpower_mode);

	synaptics_ts_set_custom_library(ts);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;
}

static void aot_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	if (sec->cmd_param[0])
		ts->plat_data->lowpower_mode |= SEC_TS_MODE_SPONGE_DOUBLETAP_TO_WAKEUP;
	else
		ts->plat_data->lowpower_mode &= ~SEC_TS_MODE_SPONGE_DOUBLETAP_TO_WAKEUP;

	input_info(true, &ts->client->dev, "%s: %s, %02X\n",
			__func__, sec->cmd_param[0] ? "on" : "off", ts->plat_data->lowpower_mode);

	synaptics_ts_set_custom_library(ts);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;
}

static void singletap_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	if (sec->cmd_param[0])
		ts->plat_data->lowpower_mode |= SEC_TS_MODE_SPONGE_SINGLE_TAP;
	else
		ts->plat_data->lowpower_mode &= ~SEC_TS_MODE_SPONGE_SINGLE_TAP;

	input_info(true, &ts->client->dev, "%s: %s, %02X\n",
			__func__, sec->cmd_param[0] ? "on" : "off", ts->plat_data->lowpower_mode);

	synaptics_ts_set_custom_library(ts);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;
}

static void ear_detect_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] != 0 && sec->cmd_param[0] != 1 && sec->cmd_param[0] != 3) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	} else {
		ts->plat_data->ed_enable = sec->cmd_param[0];
		synaptics_ts_ear_detect_enable(ts, ts->plat_data->ed_enable);
	}

	input_info(true, &ts->client->dev, "%s: %s\n",
			__func__, sec->cmd_param[0] ? "on" : "off");

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;
}

static void pocket_mode_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] != 0 && sec->cmd_param[0] != 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	} else if (!ts->plat_data->support_ear_detect) {
		snprintf(buff, sizeof(buff), "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	ts->plat_data->pocket_mode = sec->cmd_param[0];
	synaptics_ts_pocket_mode_enable(ts, ts->plat_data->pocket_mode);

	input_info(true, &ts->client->dev, "%s: %s\n",
			__func__, ts->plat_data->pocket_mode ? "on" : "off");

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void low_sensitivity_mode_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 3) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	mutex_lock(&ts->modechange);
	ts->plat_data->low_sensitivity_mode = sec->cmd_param[0];
	synaptics_ts_low_sensitivity_mode_enable(ts, ts->plat_data->low_sensitivity_mode);
	mutex_unlock(&ts->modechange);

	input_info(true, &ts->client->dev, "%s: %d\n",
			__func__, ts->plat_data->low_sensitivity_mode);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void fod_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	} else if (!ts->plat_data->support_fod) {
		snprintf(buff, sizeof(buff), "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	if (sec->cmd_param[0])
		ts->plat_data->lowpower_mode |= SEC_TS_MODE_SPONGE_PRESS;
	else
		ts->plat_data->lowpower_mode &= ~SEC_TS_MODE_SPONGE_PRESS;

	ts->plat_data->fod_data.press_prop = (sec->cmd_param[1] & 0x01) | ((sec->cmd_param[2] & 0x01) << 1);

	input_info(true, &ts->client->dev, "%s: %s, fast:%s, strict:%s, %02X\n",
			__func__, sec->cmd_param[0] ? "on" : "off",
			ts->plat_data->fod_data.press_prop & 1 ? "on" : "off",
			ts->plat_data->fod_data.press_prop & 2 ? "on" : "off",
			ts->plat_data->lowpower_mode);

	mutex_lock(&ts->modechange);

	if (!ts->plat_data->enabled && !ts->plat_data->lowpower_mode && !ts->plat_data->pocket_mode
			&& !ts->plat_data->ed_enable && !ts->plat_data->support_fod_lp_mode) {
		if (device_may_wakeup(&ts->client->dev) && ts->plat_data->power_state == SEC_INPUT_STATE_LPM)
			disable_irq_wake(ts->client->irq);
		ts->plat_data->stop_device(ts);
	} else {
		synaptics_ts_set_custom_library(ts);
		synaptics_ts_set_press_property(ts);
	}

	mutex_unlock(&ts->modechange);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;
}

static void fod_lp_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (!!sec->cmd_param[0])
		ts->plat_data->fod_lp_mode = 1;
	else
		ts->plat_data->fod_lp_mode = 0;

	input_info(true, &ts->client->dev, "%s: %s\n",
				__func__, sec->cmd_param[0] ? "on" : "off");

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;
}

static void set_fod_rect(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;

	sec_cmd_set_default_result(sec);

	input_info(true, &ts->client->dev, "%s: l:%d, t:%d, r:%d, b:%d\n",
			__func__, sec->cmd_param[0], sec->cmd_param[1],
			sec->cmd_param[2], sec->cmd_param[3]);

	if (!ts->plat_data->support_fod) {
		snprintf(buff, sizeof(buff), "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	if (!sec_input_set_fod_rect(&ts->client->dev, sec->cmd_param))
		goto NG;

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: Touch is stopped! Set data at reinit()\n", __func__);
		goto OK;
	}

	ret = synaptics_ts_set_fod_rect(ts);
	if (ret < 0)
		goto NG;

OK:
	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

NG:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void set_game_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	char mode;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		mode = sec->cmd_param[0];

		ret = synaptics_ts_set_dynamic_config(ts, DC_ENABLE_GAMEMODE, mode);
		if (ret < 0) {
			input_err(true, &ts->client->dev,
					"%s: failed to set deadzone\n", __func__);
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto err_set_gamemode;
		}

		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

err_set_gamemode:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;
}

/*
 *	index  0 :  set edge handler
 *		1 :  portrait (normal) mode
 *		2 :  landscape mode
 *
 *	data
 *		0, X (direction), X (y start), X (y end)
 *		direction : 0 (off), 1 (left), 2 (right)
 *			ex) echo set_grip_data,0,2,600,900 > cmd
 *
 *		1, X (edge zone), X (dead zone up x), X (dead zone down x), X (dead zone up y), X (dead zone bottom x), X (dead zone down y)
 *			ex) echo set_grip_data,1,60,10,32,926,32,3088 > cmd
 *
 *		2, 1 (landscape mode), X (edge zone), X (dead zone x), X (dead zone top y), X (dead zone bottom y), X (edge zone top y), X (edge zone bottom y)
 *			ex) echo set_grip_data,2,1,200,100,120,0 > cmd
 *
 *		2, 0 (portrait mode)
 *			ex) echo set_grip_data,2,0  > cmd
 */

static void set_grip_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 mode = G_NONE;

	sec_cmd_set_default_result(sec);

	memset(buff, 0, sizeof(buff));

	mutex_lock(&ts->modechange);

	if (sec->cmd_param[0] == 0) {	// edge handler
		if (sec->cmd_param[1] >= 0 && sec->cmd_param[1] < ts->edgehandler_direction_max) {
			ts->plat_data->grip_data.edgehandler_direction = sec->cmd_param[1];
			ts->plat_data->grip_data.edgehandler_start_y = sec->cmd_param[2];
			ts->plat_data->grip_data.edgehandler_end_y = sec->cmd_param[3];
		} else {
			input_err(true, &ts->client->dev, "%s: cmd1 is abnormal, %d (%d)\n",
					__func__, sec->cmd_param[1], __LINE__);
			goto err_grip_data;
		}
		mode = G_SET_EDGE_HANDLER;
	} else if (sec->cmd_param[0] == 1) {	// normal mode
		ts->plat_data->grip_data.edge_range = sec->cmd_param[1];
		ts->plat_data->grip_data.deadzone_up_x = sec->cmd_param[2];
		ts->plat_data->grip_data.deadzone_dn_x = sec->cmd_param[3];
		ts->plat_data->grip_data.deadzone_y = sec->cmd_param[4];
		mode = G_SET_NORMAL_MODE;
	} else if (sec->cmd_param[0] == 2) {	// landscape mode
		if (sec->cmd_param[1] == 0) {	// normal mode
			ts->plat_data->grip_data.landscape_mode = 0;
			mode = G_CLR_LANDSCAPE_MODE;
		} else if (sec->cmd_param[1] == 1) {
			ts->plat_data->grip_data.landscape_mode = 1;
			ts->plat_data->grip_data.landscape_edge = sec->cmd_param[2];
			ts->plat_data->grip_data.landscape_deadzone = sec->cmd_param[3];
			ts->plat_data->grip_data.landscape_top_deadzone = sec->cmd_param[4];
			ts->plat_data->grip_data.landscape_bottom_deadzone = sec->cmd_param[5];
			ts->plat_data->grip_data.landscape_top_gripzone = sec->cmd_param[6];
			ts->plat_data->grip_data.landscape_bottom_gripzone = sec->cmd_param[7];
			mode = G_SET_LANDSCAPE_MODE;
		} else {
			input_err(true, &ts->client->dev, "%s: cmd1 is abnormal, %d (%d)\n",
					__func__, sec->cmd_param[1], __LINE__);
			goto err_grip_data;
		}
	} else {
		input_err(true, &ts->client->dev, "%s: cmd0 is abnormal, %d", __func__, sec->cmd_param[0]);
		goto err_grip_data;
	}

	synaptics_set_grip_data_to_ic(&ts->client->dev, mode);

	mutex_unlock(&ts->modechange);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

err_grip_data:
	mutex_unlock(&ts->modechange);

	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void fix_active_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	char mode;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		mode = sec->cmd_param[0];

		ret = synaptics_ts_set_dynamic_config(ts, DC_DISABLE_DOZE, mode);
		if (ret < 0) {
			input_err(true, &ts->client->dev,
					"%s: failed to set fix_active_mode\n", __func__);
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto err_set_activemode;
		}

		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

err_set_activemode:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
};

/*	for game mode
	byte[0]: Setting for the Game Mode with 240Hz scan rate
		- 0: Disable
		- 1: Enable

	byte[1]: Vsycn mode
		- 0: Normal 60
		- 1: HS60
		- 2: HS120
		- 3: VSYNC 48
		- 4: VSYNC 96
*/
static void set_scan_rate(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	short mode;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1 ||
		sec->cmd_param[1] < 0 || sec->cmd_param[1] > 4) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		/* R9 model not use vsycn mode */
		mode = sec->cmd_param[0];

		ret = synaptics_ts_set_dynamic_config(ts, DC_SET_SCANRATE, mode);
		if (ret < 0) {
			input_err(true, &ts->client->dev,
					"%s: failed to set fix_active_mode\n", __func__);
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto err_set_activemode;
		}

		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

err_set_activemode:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

/*
 * cmd_param[0] means,
 * 0 : normal (60hz)
 * 1 : adaptive
 * 2 : always (120hz)
 */
static void refresh_rate_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	short mode;

	sec_cmd_set_default_result(sec);

	if (!ts->plat_data->support_refresh_rate_mode) {
		snprintf(buff, sizeof(buff), "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 2) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0] > 0) {
			/* 120hz */
			mode = 1 | (2 << 8);
		} else {
			/* 60hz */
			mode = 0 | (1 << 8);
		}

		ret = synaptics_ts_set_dynamic_config(ts, DC_SET_SCANRATE, mode);
		if (ret < 0) {
			input_err(true, &ts->client->dev,
					"%s: failed to set refresh_rate_mode\n", __func__);
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto out;
		}

		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void set_sip_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	short mode;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		mode = sec->cmd_param[0];

		ret = synaptics_ts_set_dynamic_config(ts, DC_ENABLE_SIPMODE, mode);
		if (ret < 0) {
			input_err(true, &ts->client->dev,
					"%s: failed to set sip mode\n", __func__);
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto err_set_activemode;
		}

		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

err_set_activemode:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void set_wirelesscharger_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	short mode;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 3) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		mode = sec->cmd_param[0];

		ret = synaptics_ts_set_dynamic_config(ts, DC_ENABLE_WIRELESS_CHARGER, mode);
		if (ret < 0) {
			input_err(true, &ts->client->dev,
					"%s: failed to set wireless charge mode\n", __func__);
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto err_set_activemode;
		}

		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

err_set_activemode:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void set_note_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	short mode;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		mode = sec->cmd_param[0];

		ret = synaptics_ts_set_dynamic_config(ts, DC_ENABLE_NOTEMODE, mode);
		if (ret < 0) {
			input_err(true, &ts->client->dev,
					"%s: failed to set note mode\n", __func__);
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto err_set_activemode;
		}

		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

err_set_activemode:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void debug(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct synaptics_ts_data *ts = container_of(sec, struct synaptics_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	ts->debug_flag = sec->cmd_param[0];

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "NA");

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);
}

static struct sec_cmd sec_cmds[] = {
	{SEC_CMD("fw_update", fw_update),},
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
//	{SEC_CMD("get_config_ver", get_config_ver),},
//	{SEC_CMD("get_threshold", get_threshold),},
	{SEC_CMD("module_off_master", module_off_master),},
	{SEC_CMD("module_on_master", module_on_master),},
	{SEC_CMD("get_chip_vendor", get_chip_vendor),},
	{SEC_CMD("get_chip_name", get_chip_name),},
	{SEC_CMD("get_x_num", get_x_num),},
	{SEC_CMD("get_y_num", get_y_num),},
	{SEC_CMD("run_full_rawcap_read", run_full_rawcap_read),},
	{SEC_CMD("run_full_rawcap_read_all", run_full_rawcap_read_all),},
	{SEC_CMD("run_trans_rawcap_read", run_trans_rawcap_read),},
	{SEC_CMD("run_trans_rawcap_read_all", run_trans_rawcap_read_all),},
	{SEC_CMD("run_delta_noise_read", run_delta_noise_read),},
	{SEC_CMD("run_delta_noise_read_all", run_delta_noise_read_all),},
	{SEC_CMD("run_abs_rawcap_read", run_abs_rawcap_read),},
	{SEC_CMD("run_abs_rawcap_read_all", run_abs_rawcap_read_all),},
	{SEC_CMD("run_abs_noise_read", run_abs_noise_read),},
	{SEC_CMD("run_abs_noise_read_all", run_abs_noise_read_all),},
	{SEC_CMD("get_gap_data_x_all", get_gap_data_x_all),},
	{SEC_CMD("get_gap_data_y_all", get_gap_data_y_all),},
	{SEC_CMD("run_sram_test", run_sram_test),},
	{SEC_CMD("run_interrupt_gpio_test", run_interrupt_gpio_test),},
	{SEC_CMD("run_factory_miscalibration", run_factory_miscalibration),},
	{SEC_CMD("run_miscalibration", run_miscalibration),},
	{SEC_CMD("run_force_calibration", run_force_calibration),},
	{SEC_CMD("run_miscal_data_read_all", run_miscal_data_read_all),},
/*	{SEC_CMD("get_wet_mode", get_wet_mode),},
	{SEC_CMD("get_cmoffset_set_proximity", get_cmoffset_set_proximity),},
	{SEC_CMD("run_cmoffset_set_proximity_read_all", run_cmoffset_set_proximity_read_all),},
	{SEC_CMD("get_checksum_data", get_checksum_data),},*/
	{SEC_CMD("run_cs_raw_read_all", run_trans_rawcap_read_all),},
	{SEC_CMD("run_cs_delta_read_all", run_delta_noise_read_all),},
	{SEC_CMD("run_rawdata_read_all_for_ghost", run_rawdata_read_all),},
	{SEC_CMD("run_trx_short_test", run_trx_short_test),},
	{SEC_CMD("run_jitter_delta_test", run_jitter_delta_test),},
/*	{SEC_CMD("run_elvss_test", run_elvss_test),},*/
	{SEC_CMD("get_crc_check", get_crc_check),},
	{SEC_CMD("factory_cmd_result_all", factory_cmd_result_all),},
	{SEC_CMD("factory_cmd_result_all_imagetest", factory_cmd_result_all_imagetest),},
	{SEC_CMD("run_snr_non_touched", run_snr_non_touched),},
	{SEC_CMD("run_snr_touched", run_snr_touched),},
/*	{SEC_CMD("set_factory_level", set_factory_level),},
	{SEC_CMD("check_connection", check_connection),},*/
	{SEC_CMD_H("fix_active_mode", fix_active_mode),},
/*	{SEC_CMD_H("touch_aging_mode", touch_aging_mode),}, */
	{SEC_CMD("run_prox_intensity_read_all", run_prox_intensity_read_all),},
#ifdef TCLM_CONCEPT
	{SEC_CMD("set_tsp_test_result", set_tsp_test_result),},
	{SEC_CMD("get_tsp_test_result", get_tsp_test_result),},
	{SEC_CMD("clear_tsp_test_result", clear_tsp_test_result),},
	{SEC_CMD("increase_disassemble_count", increase_disassemble_count),},
	{SEC_CMD("get_disassemble_count", get_disassemble_count),},
	{SEC_CMD("get_pat_information", get_pat_information),},
	{SEC_CMD("set_external_factory", set_external_factory),},
#endif
	{SEC_CMD_H("glove_mode", glove_mode),},
	{SEC_CMD("dead_zone_enable", dead_zone_enable),},
	{SEC_CMD_H("clear_cover_mode", clear_cover_mode),},
	{SEC_CMD_H("set_wirelesscharger_mode", set_wirelesscharger_mode),},
/*	{SEC_CMD("set_temperature", set_temperature),},*/
	{SEC_CMD_H("spay_enable", spay_enable),},
	{SEC_CMD("set_aod_rect", set_aod_rect),},
	{SEC_CMD("get_aod_rect", get_aod_rect),},
	{SEC_CMD_H("aod_enable", aod_enable),},
	{SEC_CMD_H("aot_enable", aot_enable),},
	{SEC_CMD("fod_enable", fod_enable),},
	{SEC_CMD_H("fod_lp_mode", fod_lp_mode),},
	{SEC_CMD("set_fod_rect", set_fod_rect),},
	{SEC_CMD_H("singletap_enable", singletap_enable),},
	{SEC_CMD_H("ear_detect_enable", ear_detect_enable),},
	{SEC_CMD_H("pocket_mode_enable", pocket_mode_enable),},
	{SEC_CMD("low_sensitivity_mode_enable", low_sensitivity_mode_enable),},
	{SEC_CMD("set_grip_data", set_grip_data),},
/*	{SEC_CMD_H("external_noise_mode", external_noise_mode),},*/
	{SEC_CMD_H("set_scan_rate", set_scan_rate),},
	{SEC_CMD_H("refresh_rate_mode", refresh_rate_mode),},
/*	{SEC_CMD_H("set_touchable_area", set_touchable_area),},*/
	{SEC_CMD("set_sip_mode", set_sip_mode),},
	{SEC_CMD_H("set_note_mode", set_note_mode),},
	{SEC_CMD_H("set_game_mode", set_game_mode),},
	{SEC_CMD("debug", debug),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

int synaptics_ts_fn_init(struct synaptics_ts_data *ts)
{
	int retval = 0;

	retval = sec_cmd_init(&ts->sec, sec_cmds,
			ARRAY_SIZE(sec_cmds), SEC_CLASS_DEVT_TSP);
	if (retval < 0) {
		input_err(true, &ts->client->dev,
				"%s: Failed to sec_cmd_init\n", __func__);
		goto exit;
	}

	retval = sysfs_create_group(&ts->sec.fac_dev->kobj,
			&cmd_attr_group);
	if (retval < 0) {
		input_err(true, &ts->client->dev,
				"%s: Failed to create sysfs attributes\n", __func__);
		sec_cmd_exit(&ts->sec, SEC_CLASS_DEVT_TSP);
		goto exit;
	}

	retval = sysfs_create_link(&ts->sec.fac_dev->kobj,
			&ts->plat_data->input_dev->dev.kobj, "input");
	if (retval < 0) {
		input_err(true, &ts->client->dev,
				"%s: Failed to create input symbolic link\n",
				__func__);
		sysfs_remove_group(&ts->sec.fac_dev->kobj, &cmd_attr_group);
		sec_cmd_exit(&ts->sec, SEC_CLASS_DEVT_TSP);
		goto exit;
	}

	retval = sec_input_sysfs_create(&ts->plat_data->input_dev->dev.kobj);
	if (retval < 0) {
		sysfs_remove_link(&ts->sec.fac_dev->kobj, "input");
		sysfs_remove_group(&ts->sec.fac_dev->kobj, &cmd_attr_group);
		sec_cmd_exit(&ts->sec, SEC_CLASS_DEVT_TSP);
		input_err(true, &ts->client->dev,
				"%s: Failed to create sec_input_sysfs attributes\n", __func__);
		goto exit;
	}

	return 0;

exit:
	return retval;

}

void synaptics_ts_fn_remove(struct synaptics_ts_data *ts)
{
	input_err(true, &ts->client->dev, "%s\n", __func__);

	sec_input_sysfs_remove(&ts->plat_data->input_dev->dev.kobj);

	sysfs_remove_link(&ts->sec.fac_dev->kobj, "input");

	sysfs_remove_group(&ts->sec.fac_dev->kobj,
			&cmd_attr_group);

	sec_cmd_exit(&ts->sec, SEC_CLASS_DEVT_TSP);
}

MODULE_LICENSE("GPL");
