/*
 *  Copyright (C) 2015, Samsung Electronics Co. Ltd. All Rights Reserved.
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
#include "../ssp.h"
#include "sensors.h"

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/
#define GM_DATA_SPEC_MIN	-16666
#define GM_DATA_SPEC_MAX	16666

#define GM_DATA_SUM_SPEC	26666

#define GM_SELFTEST_X_SPEC_MIN	-200
#define GM_SELFTEST_X_SPEC_MAX	200
#define GM_SELFTEST_Y_SPEC_MIN	-200
#define GM_SELFTEST_Y_SPEC_MAX	200
#define GM_SELFTEST_Z_SPEC_MIN	-1000
#define GM_SELFTEST_Z_SPEC_MAX	-200

#define MAG_CAL_PARAM_FILE_PATH	"/efs/FactoryApp/gyro_cal_data"
#define MAC_CAL_PARAM_SIZE_AKM  13
//#define MAC_CAL_PARAM_SIZE_YAS  7

static int check_spec_in(struct ssp_data *data, int sensortype)
{
	if ((data->buf[sensortype].x == 0) &&
		(data->buf[sensortype].y == 0) &&
		(data->buf[sensortype].z == 0))
		return FAIL;
	else if ((data->buf[sensortype].x > GM_DATA_SPEC_MAX)
		|| (data->buf[sensortype].x < GM_DATA_SPEC_MIN)
		|| (data->buf[sensortype].y > GM_DATA_SPEC_MAX)
		|| (data->buf[sensortype].y < GM_DATA_SPEC_MIN)
		|| (data->buf[sensortype].z > GM_DATA_SPEC_MAX)
		|| (data->buf[sensortype].z < GM_DATA_SPEC_MIN))
		return FAIL;
	else if ((int)abs(data->buf[sensortype].x) + (int)abs(data->buf[sensortype].y)
		+ (int)abs(data->buf[sensortype].z) >= GM_DATA_SUM_SPEC)
		return FAIL;
	else 
		return SUCCESS;
}

int get_fuserom_data(struct ssp_data *data)
{
	int ret = 0;
	char buffer[3] = { 0, };
	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	msg->cmd = MSG2SSP_AP_FUSEROM;
	msg->length = 3;
	msg->options = AP2HUB_READ;
	msg->buffer = buffer;
	msg->free_buffer = 0;

	ret = ssp_spi_sync(data, msg, 1000);

	if (ret) {
		data->uFuseRomData[0] = buffer[0];
		data->uFuseRomData[1] = buffer[1];
		data->uFuseRomData[2] = buffer[2];
	} else {
		data->uFuseRomData[0] = 0;
		data->uFuseRomData[1] = 0;
		data->uFuseRomData[2] = 0;
		return FAIL;
	}

	pr_info("[SSP] FUSE ROM Data %d , %d, %d\n", data->uFuseRomData[0],
			data->uFuseRomData[1], data->uFuseRomData[2]);

	return SUCCESS;
}

static int set_pdc_matrix(struct ssp_data *data)
{
	int ret = 0;
	struct ssp_msg *msg;

	if (!(data->uSensorState & 0x04)) {
		pr_info("[SSP] %s - Skip this function!!!, magnetic sensor is not connected(0x%llx)\n",
			__func__, data->uSensorState);
		return ret;
	}

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_SET_MAGNETIC_STATIC_MATRIX;
	msg->length = sizeof(data->pdc_matrix);
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(sizeof(data->pdc_matrix), GFP_KERNEL);
	msg->free_buffer = 1;
	memcpy(msg->buffer, data->pdc_matrix, sizeof(data->pdc_matrix));

	ret = ssp_spi_async(data, msg);
	if (ret != SUCCESS) {
		pr_err("[SSP] %s - i2c fail %d\n", __func__, ret);
		ret = ERROR;
	}

	pr_info("[SSP] %s: finished\n", __func__);

	return ret;
}

void init(struct ssp_data *data) {
	int ret = get_fuserom_data(data);
	if (ret < 0)
		pr_err("[SSP] %s - get_fuserom_data failed %d\n",
			__func__, ret);

	ret = set_pdc_matrix(data);
	if (ret < 0)
		pr_err("[SSP] %s - set_magnetic_pdc_matrix failed %d\n",
			__func__, ret);
}

/* AKM Functions */
static ssize_t magnetic_asa_show(struct device *dev, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d,%d,%d\n", (s16)data->uFuseRomData[0],
		(s16)data->uFuseRomData[1], (s16)data->uFuseRomData[2]);
}

static ssize_t matrix_show(struct device *dev, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return sprintf(buf,
		"%u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u\n",
		data->pdc_matrix[0], data->pdc_matrix[1], data->pdc_matrix[2], data->pdc_matrix[3],
		data->pdc_matrix[4], data->pdc_matrix[5], data->pdc_matrix[6], data->pdc_matrix[7],
		data->pdc_matrix[8], data->pdc_matrix[9], data->pdc_matrix[10], data->pdc_matrix[11],
		data->pdc_matrix[12], data->pdc_matrix[13], data->pdc_matrix[14], data->pdc_matrix[15],
		data->pdc_matrix[16], data->pdc_matrix[17], data->pdc_matrix[18], data->pdc_matrix[19],
		data->pdc_matrix[20], data->pdc_matrix[21], data->pdc_matrix[22], data->pdc_matrix[23],
		data->pdc_matrix[24], data->pdc_matrix[25], data->pdc_matrix[26]);
}

static ssize_t matrix_store(struct device *dev, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	u8 val[PDC_SIZE] = {0, };
	int iRet;
	int i;
	char *token;
	char *str;

	str = (char *)buf;


	for (i = 0; i < PDC_SIZE; i++) {
		token = strsep(&str, " \n");
		if (token == NULL) {
			pr_err("[SSP] %s : too few arguments (%d needed)", __func__, PDC_SIZE);
			return -EINVAL;
		}

		iRet = kstrtou8(token, 10, &val[i]);
		if (iRet < 0) {
			pr_err("[SSP] %s : kstros16 error %d", __func__, iRet);
			return iRet;
		}
	}

	for (i = 0; i < PDC_SIZE; i++)
		data->pdc_matrix[i] = val[i];

	pr_info("[SSP] %s : %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u\n",
		__func__, data->pdc_matrix[0], data->pdc_matrix[1], data->pdc_matrix[2], data->pdc_matrix[3],
		data->pdc_matrix[4], data->pdc_matrix[5], data->pdc_matrix[6], data->pdc_matrix[7],
		data->pdc_matrix[8], data->pdc_matrix[9], data->pdc_matrix[10], data->pdc_matrix[11],
		data->pdc_matrix[12], data->pdc_matrix[13], data->pdc_matrix[14], data->pdc_matrix[15],
		data->pdc_matrix[16], data->pdc_matrix[17], data->pdc_matrix[18], data->pdc_matrix[19],
		data->pdc_matrix[20], data->pdc_matrix[21], data->pdc_matrix[22], data->pdc_matrix[23],
		data->pdc_matrix[24], data->pdc_matrix[25], data->pdc_matrix[26]);
	set_pdc_matrix(data);
	

	return size;
}

/* common functions */

static ssize_t magnetic_get_selftest(struct device *dev, char *buf)
{
	s8 iResult[4] = {-1, -1, -1, -1};
	char bufSelftest[28] = {0, };
	char bufAdc[4] = {0, };
	s16 iSF_X = 0, iSF_Y = 0, iSF_Z = 0;
	s16 iADC_X = 0, iADC_Y = 0, iADC_Z = 0;
	s32 dMsDelay = 20;
	int ret = 0, iSpecOutRetries = 0, i = 0;
	struct ssp_data *data = dev_get_drvdata(dev);
	struct ssp_msg *msg;

	pr_info("[SSP] %s in\n", __func__);

	/* STATUS */
	if ((data->uFuseRomData[0] == 0) ||
		(data->uFuseRomData[0] == 0xff) ||
		(data->uFuseRomData[1] == 0) ||
		(data->uFuseRomData[1] == 0xff) ||
		(data->uFuseRomData[2] == 0) ||
		(data->uFuseRomData[2] == 0xff))
		iResult[0] = -1;
	else
		iResult[0] = 0;

Retry_selftest:
	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = GEOMAGNETIC_FACTORY;
	msg->length = 28;
	msg->options = AP2HUB_READ;
	msg->buffer = bufSelftest;
	msg->free_buffer = 0;

	ret = ssp_spi_sync(data, msg, 1000);
	if (ret != SUCCESS) {
		pr_err("[SSP] %s - Magnetic Selftest Timeout!! %d\n",
			__func__, ret);
		goto exit;
	}

	/* read 6bytes data registers */
	iSF_X = (s16)((bufSelftest[13] << 8) + bufSelftest[14]);
	iSF_Y = (s16)((bufSelftest[15] << 8) + bufSelftest[16]);
	iSF_Z = (s16)((bufSelftest[17] << 8) + bufSelftest[18]);

	/* DAC (store Cntl Register value to check power down) */
	iResult[2] = bufSelftest[21];

	iSF_X = (s16)(((iSF_X * data->uFuseRomData[0]) >> 7) + iSF_X);
	iSF_Y = (s16)(((iSF_Y * data->uFuseRomData[1]) >> 7) + iSF_Y);
	iSF_Z = (s16)(((iSF_Z * data->uFuseRomData[2]) >> 7) + iSF_Z);

	pr_info("[SSP] %s: self test x = %d, y = %d, z = %d\n",
		__func__, iSF_X, iSF_Y, iSF_Z);

	if ((iSF_X >= GM_SELFTEST_X_SPEC_MIN)
		&& (iSF_X <= GM_SELFTEST_X_SPEC_MAX))
		pr_info("[SSP] x passed self test, expect -200<=x<=200\n");
	else
		pr_info("[SSP] x failed self test, expect -200<=x<=200\n");
	if ((iSF_Y >= GM_SELFTEST_Y_SPEC_MIN)
		&& (iSF_Y <= GM_SELFTEST_Y_SPEC_MAX))
		pr_info("[SSP] y passed self test, expect -200<=y<=200\n");
	else
		pr_info("[SSP] y failed self test, expect -200<=y<=200\n");
	if ((iSF_Z >= GM_SELFTEST_Z_SPEC_MIN)
		&& (iSF_Z <= GM_SELFTEST_Z_SPEC_MAX))
		pr_info("[SSP] z passed self test, expect -1000<=z<=-200\n");
	else
		pr_info("[SSP] z failed self test, expect -1000<=z<=-200\n");

	/* SELFTEST */
	if ((iSF_X >= GM_SELFTEST_X_SPEC_MIN)
		&& (iSF_X <= GM_SELFTEST_X_SPEC_MAX)
		&& (iSF_Y >= GM_SELFTEST_Y_SPEC_MIN)
		&& (iSF_Y <= GM_SELFTEST_Y_SPEC_MAX)
		&& (iSF_Z >= GM_SELFTEST_Z_SPEC_MIN)
		&& (iSF_Z <= GM_SELFTEST_Z_SPEC_MAX))
		iResult[1] = 0;

	if ((iResult[1] == -1) && (iSpecOutRetries++ < 5)) {
		pr_err("[SSP] %s, selftest spec out. Retry = %d", __func__,
			iSpecOutRetries);
		goto Retry_selftest;
	}

	for(i = 0; i < 3; i++) {
		if(bufSelftest[22 + (i * 2)] == 1 && bufSelftest[23 + (i * 2)] == 0)
			continue;
		iResult[1] = -1;
		pr_info("[SSP] continuos selftest fail #%d:%d #%d:%d", 
				22 + (i * 2), bufSelftest[22 + (i * 2)],
				23 + (i * 2), bufSelftest[23 + (i * 2)]);
	}
	iSpecOutRetries = 10;

	/* ADC */
	memcpy(&bufAdc[0], &dMsDelay, 4);

	data->buf[GEOMAGNETIC_RAW].x = 0;
	data->buf[GEOMAGNETIC_RAW].y = 0;
	data->buf[GEOMAGNETIC_RAW].z = 0;

	if (!(atomic64_read(&data->aSensorEnable) & (1 << GEOMAGNETIC_RAW)))
		send_instruction(data, ADD_SENSOR, GEOMAGNETIC_RAW,
			bufAdc, 4);

	do {
		msleep(60);
		if (check_spec_in(data, GEOMAGNETIC_RAW) == SUCCESS)
			break;
	} while (--iSpecOutRetries);

	if (iSpecOutRetries > 0)
		iResult[3] = 0;

	iADC_X = data->buf[GEOMAGNETIC_RAW].x;
	iADC_Y = data->buf[GEOMAGNETIC_RAW].y;
	iADC_Z = data->buf[GEOMAGNETIC_RAW].z;

	if (!(atomic64_read(&data->aSensorEnable) & (1 << GEOMAGNETIC_RAW)))
		send_instruction(data, REMOVE_SENSOR, GEOMAGNETIC_RAW,
			bufAdc, 4);

	pr_info("[SSP] %s -adc, x = %d, y = %d, z = %d, retry = %d\n",
		__func__, iADC_X, iADC_Y, iADC_Z, iSpecOutRetries);

exit:
	pr_info("[SSP] %s out. Result = %d %d %d %d\n",
		__func__, iResult[0], iResult[1], iResult[2], iResult[3]);

	return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		iResult[0], iResult[1], iSF_X, iSF_Y, iSF_Z,
		iResult[2], iResult[3], iADC_X, iADC_Y, iADC_Z);
}

static ssize_t si_param_show(struct device *dev, char *buf) {
	struct ssp_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "\"SI_PARAMETER\":\"%u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u\"\n",
			data->pdc_matrix[0], data->pdc_matrix[1], data->pdc_matrix[2], data->pdc_matrix[3],
			data->pdc_matrix[4], data->pdc_matrix[5], data->pdc_matrix[6], data->pdc_matrix[7],
			data->pdc_matrix[8], data->pdc_matrix[9], data->pdc_matrix[10], data->pdc_matrix[11],
			data->pdc_matrix[12], data->pdc_matrix[13], data->pdc_matrix[14], data->pdc_matrix[15],
			data->pdc_matrix[16], data->pdc_matrix[17], data->pdc_matrix[18], data->pdc_matrix[19],
			data->pdc_matrix[20], data->pdc_matrix[21], data->pdc_matrix[22], data->pdc_matrix[23],
			data->pdc_matrix[24], data->pdc_matrix[25], data->pdc_matrix[26]);
}

struct magnetometer_t mag_ak09918c = {
    .name = "AK09918C",
	.vendor = "AKM",
	.initialize = init,
	.check_data_spec = check_spec_in,
	.get_magnetic_asa = magnetic_asa_show,
	.get_magnetic_matrix = matrix_show,
	.set_magnetic_matrix = matrix_store,
	.get_magnetic_selftest = magnetic_get_selftest,
	.get_magnetic_si_param = si_param_show,
};

struct magnetometer_t* get_mag_ak09918c(){
	return &mag_ak09918c;
}
