/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include "../ssp.h"
#include "sensors.h"
/*************************************************************************/
/* factory Sysfs							 */
/*************************************************************************/

#define SELFTEST_DPS_LIMIT  15

#define VERBOSE_OUT 1

//STMICRO
#define DEF_GYRO_SENS	(70) // 0.07 * 1000
#define DEF_BIAS_LSB_THRESH_SELF	(40000 / DEF_GYRO_SENS)
#define DEF_SCALE_FOR_FLOAT (1000)
#define GYRO_LIB_DL_FAIL	9990

#ifndef ABS
#define ABS(a) ((a) > 0 ? (a) : -(a))
#endif

static ssize_t gyro_power_off_show(struct device *dev, char *buf)
{
	ssp_dbg("[SSP]: %s\n", __func__);

	return sprintf(buf, "%d\n", 1);
}

static ssize_t gyro_power_on_show(struct device *dev, char *buf)
{
	ssp_dbg("[SSP]: %s\n", __func__);

	return sprintf(buf, "%d\n", 1);
}

static ssize_t gyro_temperature_show(struct device *dev, char *buf)
{
	short temperature = 0;
	struct ssp_data *data = dev_get_drvdata(dev);

	temperature = get_gyro_temperature(data);
	return sprintf(buf, "%d\n", temperature);
}

static ssize_t gyro_selftest_show(struct device *dev, char *buf)
{
	char chTempBuf[39] = { 0,};
    int fifo_test_result = 0, fifo_zro_result = 0, fifo_hwst_delta_result = 0;
	u8 initialized = 0;
	s8 hw_result = 0;
	int j = 0, total_count = 0, ret_val = 0, gyro_lib_dl_fail = 0;
	long avg[3] = {0,}, rms[3] = {0,};
	int gyro_bias[3] = {0,};
	s16 shift_ratio[3] = {0,}; //self_diff value
	s32 iCalData[3] = {0,};
	char a_name[3][2] = { "X", "Y", "Z" };
	int iRet = 0;
	int bias_thresh = DEF_BIAS_LSB_THRESH_SELF;
	int fifo_ret = 0;
	int cal_ret = 0;
	int self_test_ret = 0;
	int self_test_zro_ret = 0;
	struct ssp_data *data = dev_get_drvdata(dev);
	s16 st_zro[3] = {0, };
	s16 st_bias[3] = {0, };
	int gyro_fifo_avg[3] = {0,}, gyro_self_zro[3] = {0,};
	int gyro_self_bias[3] = {0,}, gyro_self_diff[3] = {0,};
	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	msg->cmd = GYROSCOPE_FACTORY;
	msg->length = ARRAY_SIZE(chTempBuf);
	msg->options = AP2HUB_READ;
	msg->buffer = chTempBuf;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 7000);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - Gyro Selftest Timeout!!\n", __func__);
		ret_val = 1;
		goto exit;
	}

	data->uTimeOutCnt = 0;

	pr_err("[SSP]%d %d %d %d %d %d %d %d %d %d %d %d\n", chTempBuf[0],
		chTempBuf[1], chTempBuf[2], chTempBuf[3], chTempBuf[4],
		chTempBuf[5], chTempBuf[6], chTempBuf[7], chTempBuf[8],
		chTempBuf[9], chTempBuf[10], chTempBuf[11]);

	initialized = chTempBuf[0];
	shift_ratio[0] = (s16)((chTempBuf[2] << 8) +
				chTempBuf[1]);
	shift_ratio[1] = (s16)((chTempBuf[4] << 8) +
				chTempBuf[3]);
	shift_ratio[2] = (s16)((chTempBuf[6] << 8) +
				chTempBuf[5]);
	hw_result = (s8)chTempBuf[7];
	total_count = (int)((chTempBuf[11] << 24) +
				(chTempBuf[10] << 16) +
				(chTempBuf[9] << 8) +
				chTempBuf[8]);
	avg[0] = (long)((chTempBuf[15] << 24) +
				(chTempBuf[14] << 16) +
				(chTempBuf[13] << 8) +
				chTempBuf[12]);
	avg[1] = (long)((chTempBuf[19] << 24) +
				(chTempBuf[18] << 16) +
				(chTempBuf[17] << 8) +
				chTempBuf[16]);
	avg[2] = (long)((chTempBuf[23] << 24) +
				(chTempBuf[22] << 16) +
				(chTempBuf[21] << 8) +
				chTempBuf[20]);
	rms[0] = (long)((chTempBuf[27] << 24) +
				(chTempBuf[26] << 16) +
				(chTempBuf[25] << 8) +
				chTempBuf[24]);
	rms[1] = (long)((chTempBuf[31] << 24) +
				(chTempBuf[30] << 16) +
				(chTempBuf[29] << 8) +
				chTempBuf[28]);
	rms[2] = (long)((chTempBuf[35] << 24) +
				(chTempBuf[34] << 16) +
				(chTempBuf[33] << 8) +
				chTempBuf[32]);

	st_zro[0] = (s16)((chTempBuf[25] << 8) +
				chTempBuf[24]);
	st_zro[1] = (s16)((chTempBuf[27] << 8) +
				chTempBuf[26]);
	st_zro[2] = (s16)((chTempBuf[29] << 8) +
				chTempBuf[28]);

	st_bias[0] = (s16)((chTempBuf[31] << 8) +
				chTempBuf[30]);
	st_bias[1] = (s16)((chTempBuf[33] << 8) +
				chTempBuf[32]);
	st_bias[2] = (s16)((chTempBuf[35] << 8) +
				chTempBuf[34]);
    
	pr_info("[SSP] init: %d, total cnt: %d\n", initialized, total_count);
	pr_info("[SSP] hw_result: %d, %d, %d, %d\n", hw_result,
		shift_ratio[0], shift_ratio[1],	shift_ratio[2]);
	pr_info("[SSP] avg %+8ld %+8ld %+8ld (LSB)\n", avg[0], avg[1], avg[2]);
	pr_info("[SSP] st_zro %+8d %+8d %+8d (LSB)\n", st_zro[0], st_zro[1], st_zro[2]);
	pr_info("[SSP] rms %+8ld %+8ld %+8ld (LSB)\n", rms[0], rms[1], rms[2]);

	fifo_test_result = chTempBuf[36];
	fifo_zro_result = chTempBuf[37];
	fifo_hwst_delta_result = chTempBuf[38];
	pr_info("[SSP] test :%d zro :%d hwst_delta :%d", fifo_test_result, fifo_zro_result, fifo_hwst_delta_result);

	if(!fifo_test_result || !fifo_zro_result || !fifo_hwst_delta_result)        
			return sprintf(buf, "%d,%d,%d\n", fifo_test_result, fifo_zro_result, fifo_hwst_delta_result);

	//FIFO ZRO check pass / fail
	gyro_fifo_avg[0] = avg[0] * DEF_GYRO_SENS / DEF_SCALE_FOR_FLOAT;
	gyro_fifo_avg[1] = avg[1] * DEF_GYRO_SENS / DEF_SCALE_FOR_FLOAT;
	gyro_fifo_avg[2] = avg[2] * DEF_GYRO_SENS / DEF_SCALE_FOR_FLOAT;
	// ZRO self test
	gyro_self_zro[0] = st_zro[0] * DEF_GYRO_SENS / DEF_SCALE_FOR_FLOAT;
	gyro_self_zro[1] = st_zro[1] * DEF_GYRO_SENS / DEF_SCALE_FOR_FLOAT;
	gyro_self_zro[2] = st_zro[2] * DEF_GYRO_SENS / DEF_SCALE_FOR_FLOAT;
	//bias
	gyro_self_bias[0] = st_bias[0] * DEF_GYRO_SENS / DEF_SCALE_FOR_FLOAT;
	gyro_self_bias[1] = st_bias[1] * DEF_GYRO_SENS / DEF_SCALE_FOR_FLOAT;
	gyro_self_bias[2] = st_bias[2] * DEF_GYRO_SENS / DEF_SCALE_FOR_FLOAT;
	//diff = bias - ZRO
	gyro_self_diff[0] = shift_ratio[0] * DEF_GYRO_SENS / DEF_SCALE_FOR_FLOAT;
	gyro_self_diff[1] = shift_ratio[1] * DEF_GYRO_SENS / DEF_SCALE_FOR_FLOAT;
	gyro_self_diff[2] = shift_ratio[2] * DEF_GYRO_SENS / DEF_SCALE_FOR_FLOAT;

	if (total_count != 128) {
		pr_err("[SSP] %s, total_count is not 128. goto exit\n", __func__);
		ret_val = 2;
		goto exit;
	} else
		cal_ret = fifo_ret = 1;

	if ((gyro_self_diff[0] >= 150 && gyro_self_diff[0] <= 700)
			&& (gyro_self_diff[1] >= 150 && gyro_self_diff[1] <= 700)
			&& (gyro_self_diff[2] >= 150 && gyro_self_diff[2] <= 700))
		self_test_ret = 1;

	if (ABS(gyro_self_zro[0]) <= SELFTEST_DPS_LIMIT 
            && ABS(gyro_self_zro[1]) <= SELFTEST_DPS_LIMIT 
            && ABS(gyro_self_zro[2]) <= SELFTEST_DPS_LIMIT)
		self_test_zro_ret = 1;

	if (hw_result < 1) {
		pr_err("[SSP] %s - hw selftest fail(%d), sw selftest skip\n",
			__func__, hw_result);
		if (shift_ratio[0] == GYRO_LIB_DL_FAIL &&
			shift_ratio[1] == GYRO_LIB_DL_FAIL &&
			shift_ratio[2] == GYRO_LIB_DL_FAIL) {
			pr_err("[SSP] %s - gyro lib download fail\n", __func__);
			gyro_lib_dl_fail = 1;
		} else {
			/*
			 *ssp_dbg("[SSP]: %s - %d,%d,%d fail.\n",
			 *        __func__,
			 *        shift_ratio[0] / 10,
			 *        shift_ratio[1] / 10,
			 *        shift_ratio[2] / 10);
			 *return sprintf(buf, "%d,%d,%d\n",
			 *        shift_ratio[0] / 10,
			 *        shift_ratio[1] / 10,
			 *        shift_ratio[2] / 10);
			 */
			ssp_dbg("[SSP]: %s - %d,%d,%d fail.\n",	__func__, gyro_self_diff[0],
					gyro_self_diff[1], gyro_self_diff[2]);
			//return sprintf(buf, "%d,%d,%d\n", gyro_self_diff[0], gyro_self_diff[1], gyro_self_diff[2]);
		}
	}

	// AVG value range test +/- 40
	if ((ABS(gyro_fifo_avg[0]) > SELFTEST_DPS_LIMIT) 
            || (ABS(gyro_fifo_avg[1]) > SELFTEST_DPS_LIMIT) 
            || (ABS(gyro_fifo_avg[2]) > SELFTEST_DPS_LIMIT)) {
		ssp_dbg("[SSP]: %s - %d,%d,%d fail.\n",	__func__, gyro_fifo_avg[0], gyro_fifo_avg[1], gyro_fifo_avg[2]);
		return sprintf(buf, "%d,%d,%d\n", gyro_fifo_avg[0], gyro_fifo_avg[1], gyro_fifo_avg[2]);
	}

	// STMICRO
	gyro_bias[0] = avg[0] * DEF_GYRO_SENS;
	gyro_bias[1] = avg[1] * DEF_GYRO_SENS;
	gyro_bias[2] = avg[2] * DEF_GYRO_SENS;
	iCalData[0] = (s16)avg[0];
	iCalData[1] = (s16)avg[1];
	iCalData[2] = (s16)avg[2];

	if (VERBOSE_OUT) {
		pr_info("[SSP] abs bias : %+8d.%03d %+8d.%03d %+8d.%03d (dps)\n",
			(int)abs(gyro_bias[0]) / DEF_SCALE_FOR_FLOAT,
			(int)abs(gyro_bias[0]) % DEF_SCALE_FOR_FLOAT,
			(int)abs(gyro_bias[1]) / DEF_SCALE_FOR_FLOAT,
			(int)abs(gyro_bias[1]) % DEF_SCALE_FOR_FLOAT,
			(int)abs(gyro_bias[2]) / DEF_SCALE_FOR_FLOAT,
			(int)abs(gyro_bias[2]) % DEF_SCALE_FOR_FLOAT);
	}

	for (j = 0; j < 3; j++) {
		if (unlikely(abs(avg[j]) > bias_thresh)) {
			pr_err("[SSP] %s-Gyro bias (%ld) exceeded threshold (threshold = %d LSB)\n",
				a_name[j],
				avg[j], bias_thresh);
			ret_val |= 1 << (3 + j);
		}
	}
// STMICRO
/*
 *3rd, check RMS for dead gyros
 *If any of the RMS noise value returns zero,
 *then we might have dead gyro or FIFO/register failure,
 *the part is sleeping, or the part is not responsive
 */
	//if (rms[0] == 0 || rms[1] == 0 || rms[2] == 0)
	//ret_val |= 1 << 6;

	if (gyro_lib_dl_fail) {
		pr_err("[SSP] gyro_lib_dl_fail, Don't save cal data\n");
		ret_val = -1;
		goto exit;
	}

	if (likely(!ret_val)) {
		save_gyro_caldata(data, iCalData);
	} else {
		pr_err("[SSP] ret_val != 0, gyrocal is 0 at all axis\n");
		data->gyrocal.x = 0;
		data->gyrocal.y = 0;
		data->gyrocal.z = 0;
	}
exit:
	ssp_dbg("[SSP]: %s - %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		__func__,
		gyro_fifo_avg[0], gyro_fifo_avg[1], gyro_fifo_avg[2],
		gyro_self_zro[0], gyro_self_zro[1], gyro_self_zro[2],
		gyro_self_bias[0], gyro_self_bias[1], gyro_self_bias[2],
		gyro_self_diff[0], gyro_self_diff[1], gyro_self_diff[2],
		self_test_ret,
		self_test_zro_ret);

	// Gyro Calibration pass / fail, buffer 1~6 values.
	if ((fifo_ret == 0) || (cal_ret == 0))
		return sprintf(buf, "%d,%d,%d\n", gyro_self_diff[0], gyro_self_diff[1], gyro_self_diff[2]);

	return sprintf(buf,
		"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		gyro_fifo_avg[0], gyro_fifo_avg[1], gyro_fifo_avg[2],
		gyro_self_zro[0], gyro_self_zro[1], gyro_self_zro[2],
		gyro_self_bias[0], gyro_self_bias[1], gyro_self_bias[2],
		gyro_self_diff[0], gyro_self_diff[1], gyro_self_diff[2],
		self_test_ret,
		self_test_zro_ret);

}

static ssize_t gyro_selftest_dps_store(struct device *dev, const char *buf, size_t count)
{
	int64_t iNewDps = 0;
	int iRet = 0;
	char chTempBuf = 0;
	struct ssp_data *data = dev_get_drvdata(dev);
	struct ssp_msg *msg;

	if (!(data->uSensorState & (1 << GYROSCOPE_SENSOR)))
		goto exit;

	data->IsGyroselftest = true;
	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = GYROSCOPE_DPS_FACTORY;
	msg->length = 1;
	msg->options = AP2HUB_READ;
	msg->buffer = &chTempBuf;
	msg->free_buffer = 0;

	iRet = kstrtoll(buf, 10, &iNewDps);
	if (iRet < 0) {
		kfree(msg);
		return iRet;
	}

	if (iNewDps == GYROSCOPE_DPS250)
		msg->options |= 0 << SSP_GYRO_DPS;
	else if (iNewDps == GYROSCOPE_DPS500)
		msg->options |= 1 << SSP_GYRO_DPS;
	else if (iNewDps == GYROSCOPE_DPS2000)
		msg->options |= 2 << SSP_GYRO_DPS;
	else {
		msg->options |= 1 << SSP_GYRO_DPS;
		iNewDps = GYROSCOPE_DPS500;
	}

	iRet = ssp_spi_sync(data, msg, 3000);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - Gyro Selftest DPS Timeout!!\n", __func__);
		goto exit;
	}

	if (chTempBuf != SUCCESS) {
		pr_err("[SSP]: %s - Gyro Selftest DPS Error!!\n", __func__);
		goto exit;
	}

	data->uGyroDps = (unsigned int)iNewDps;
	pr_err("[SSP]: %s - %u dps stored\n", __func__, data->uGyroDps);
exit:
	data->IsGyroselftest = false;
	return count;
}

static ssize_t gyro_selftest_dps_show(struct device *dev, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", data->uGyroDps);
}

struct gyroscope_t gyro_lsm6dso = {
	.name = "LSM6DSO",
	.vendor = "STM",	
	.get_gyro_power_off = gyro_power_off_show,
	.get_gyro_power_on = gyro_power_on_show,
	.get_gyro_temperature = gyro_temperature_show,
	.get_gyro_selftest = gyro_selftest_show,
	.get_gyro_selftest_dps = gyro_selftest_dps_show,
	.set_gyro_selftest_dps = gyro_selftest_dps_store,
};

struct gyroscope_t* get_gyro_lsm6dso(){
	return &gyro_lsm6dso;
}
