#include "../ssp.h"
#include "sensors.h"

#define PROX_ADC_BITS_NUM		14
#define THRESHOLD_HIGH			0
#define THRESHOLD_LOW			1
#define THRESHOLD_DETECT_HIGH	2
#define THRESHOLD_DETECT_LOW	3

#define PROX_CAL_FILE_PATH	"/efs/FactoryApp/gyro_cal_data"

struct sensor_manager prox_manager;
#define get_prox(dev) ((prox *)get_sensor_ptr(dev_get_drvdata(dev), &prox_manager, PROXIMITY_SENSOR))

struct proximity_t prox_default = {
	.name = " ",
	.vendor = " ",
	.get_prox_position = sensor_default_show
};

/*************************************************************************/
/* Functions                                                             */
/*************************************************************************/
static s16 get_proximity_rawdata(struct ssp_data *data)
{
	s16 uRowdata = 0;
	char chTempbuf[4] = { 0 };

	s32 dMsDelay = 20;

	memcpy(&chTempbuf[0], &dMsDelay, 4);

	if (data->bProximityRawEnabled == false) {
		send_instruction(data, ADD_SENSOR, PROXIMITY_RAW, chTempbuf, 4);
		msleep(200);
		uRowdata = data->buf[PROXIMITY_RAW].prox_raw[0];
		send_instruction(data, REMOVE_SENSOR, PROXIMITY_RAW,
			chTempbuf, 4);
	} else {
		uRowdata = data->buf[PROXIMITY_RAW].prox_raw[0];
	}

	return uRowdata;
}

static u16 get_proximity_all_threshold(struct ssp_data *data, int threshold_type)
{
	int iRet = 0;
	struct ssp_msg *msg;
	u8 buffer[8] = {0, };
	u16 threshold_high = 0;
	u16 threshold_low = 0;
	u16 threshold_detect_high = 0;
	u16 threshold_detect_low = 0;
	u16 threshold = 0;

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_PROX_GET_THRESHOLD;
	msg->length = 8;
	msg->options = AP2HUB_READ;
	msg->buffer = buffer;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 1000);
	if (iRet != SUCCESS) {
		pr_err("[SSP] %s fail %d\n", __func__, iRet);
		return FAIL;
	}

	threshold_high = ((((u16) buffer[1]) << 8) & 0xff00) | ((((u16) buffer[0])) & 0x00ff);
	threshold_low = ((((u16) buffer[3]) << 8) & 0xff00) | ((((u16) buffer[2])) & 0x00ff);
	threshold_detect_high = ((((u16) buffer[5]) << 8) & 0xff00) | ((((u16) buffer[4])) & 0x00ff);
	threshold_detect_low = ((((u16) buffer[7]) << 8) & 0xff00) | ((((u16) buffer[6])) & 0x00ff);

	pr_info("[SSP] %s - %d, %d, %d, %d\n", __func__,
		threshold_high, threshold_low, threshold_detect_high, threshold_detect_low);

	if(threshold_type == THRESHOLD_HIGH) {
		threshold = threshold_high;
	}
	else if(threshold_type == THRESHOLD_LOW) {
		threshold = threshold_low;
	}
	else if(threshold_type == THRESHOLD_DETECT_HIGH) {
		threshold = threshold_detect_high;
	}
	else {
		threshold = threshold_detect_low;
	}

	return threshold;
	
}

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

static ssize_t proximity_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n",  get_prox(dev)->vendor);
}

static ssize_t proximity_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n",  get_prox(dev)->name);
}

static ssize_t proximity_probe_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	bool probe_pass_fail = FAIL;

	if (data->uSensorState & (1 << PROXIMITY_SENSOR))
		probe_pass_fail = SUCCESS;
	else
		probe_pass_fail = FAIL;

	pr_info("[SSP]: %s - All sensor 0x%llx, prox_sensor %d\n",
		__func__, data->uSensorState, probe_pass_fail);

	return snprintf(buf, PAGE_SIZE, "%d\n", probe_pass_fail);
}

static ssize_t proximity_thresh_high_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	u16 threshold_high = get_proximity_all_threshold(data, THRESHOLD_HIGH);

	return snprintf(buf, PAGE_SIZE, "%d\n", threshold_high);
}

static ssize_t proximity_thresh_high_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	u16 uNewThresh;
	int iRet, i = 0;
	u16 prox_bits_mask = 0, prox_non_bits_mask = 0;
	struct ssp_data *data = dev_get_drvdata(dev);

	while (i < PROX_ADC_BITS_NUM)
		prox_bits_mask += (1 << i++);

	while (i < 16)
		prox_non_bits_mask += (1 << i++);

	iRet = kstrtou16(buf, 10, &uNewThresh);
	if (iRet < 0)
		pr_err("[SSP]: %s - kstrto16 failed.(%d)\n", __func__, iRet);
	else {
		if (uNewThresh & prox_non_bits_mask)
			pr_err("[SSP]: %s - allow %ubits.(%d)\n", __func__, PROX_ADC_BITS_NUM, uNewThresh);
		else {
			uNewThresh &= prox_bits_mask;
			data->uProxHiThresh = uNewThresh;
		}
	}

	ssp_dbg("[SSP]: %s - new prox threshold : hi - %u\n", __func__, data->uProxHiThresh);

	return size;
}

static ssize_t proximity_thresh_low_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	u16 threshold_low = get_proximity_all_threshold(data, THRESHOLD_LOW);

	return snprintf(buf, PAGE_SIZE, "%d\n", threshold_low);
}

static ssize_t proximity_thresh_low_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	u16 uNewThresh;
	int iRet, i = 0;
	u16 prox_bits_mask = 0, prox_non_bits_mask = 0;
	struct ssp_data *data = dev_get_drvdata(dev);

	while (i < PROX_ADC_BITS_NUM)
		prox_bits_mask += (1 << i++);

	while (i < 16)
		prox_non_bits_mask += (1 << i++);

	iRet = kstrtou16(buf, 10, &uNewThresh);
	if (iRet < 0)
		pr_err("[SSP]: %s - kstrto16 failed.(%d)\n", __func__, iRet);
	else {
		if (uNewThresh & prox_non_bits_mask)
			pr_err("[SSP]: %s - allow %ubits.(%d)\n", __func__, PROX_ADC_BITS_NUM, uNewThresh);
		else {
			uNewThresh &= prox_bits_mask;
			data->uProxLoThresh = uNewThresh;
		}
	}

	ssp_dbg("[SSP]: %s - new prox threshold : lo - %u\n", __func__, data->uProxLoThresh);

	return size;
}

static ssize_t proximity_thresh_detect_high_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	u16 threshold_detect_high = get_proximity_all_threshold(data, THRESHOLD_DETECT_HIGH);

	return snprintf(buf, PAGE_SIZE, "%d\n", threshold_detect_high);
}

static ssize_t proximity_thresh_detect_high_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	u16 uNewThresh;
	int iRet, i = 0;
	u16 prox_bits_mask = 0, prox_non_bits_mask = 0;
	struct ssp_data *data = dev_get_drvdata(dev);

	while (i < PROX_ADC_BITS_NUM)
		prox_bits_mask += (1 << i++);

	while (i < 16)
		prox_non_bits_mask += (1 << i++);

	iRet = kstrtou16(buf, 10, &uNewThresh);
	if (iRet < 0)
		pr_err("[SSP]: %s - kstrto16 failed.(%d)\n", __func__, iRet);
	else {
		if (uNewThresh & prox_non_bits_mask)
			pr_err("[SSP]: %s - allow %ubits.(%d)\n", __func__, PROX_ADC_BITS_NUM, uNewThresh);
		else {
			uNewThresh &= prox_bits_mask;
			data->uProxHiThresh_detect = uNewThresh;
		}
	}

	ssp_dbg("[SSP]: %s - new prox threshold : hidetect - %u\n", __func__, data->uProxHiThresh_detect);

	return size;
}

static ssize_t proximity_thresh_detect_low_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	u16 threshold_detect_low = get_proximity_all_threshold(data, THRESHOLD_DETECT_LOW);

	return snprintf(buf, PAGE_SIZE, "%d\n", threshold_detect_low);
}

static ssize_t proximity_thresh_detect_low_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	u16 uNewThresh;
	int iRet, i = 0;
	u16 prox_bits_mask = 0, prox_non_bits_mask = 0;
	struct ssp_data *data = dev_get_drvdata(dev);

	while (i < PROX_ADC_BITS_NUM)
		prox_bits_mask += (1 << i++);

	while (i < 16)
		prox_non_bits_mask += (1 << i++);

	iRet = kstrtou16(buf, 10, &uNewThresh);
	if (iRet < 0)
		pr_err("[SSP]: %s - kstrto16 failed.(%d)\n", __func__, iRet);
	else {
		if (uNewThresh & prox_non_bits_mask)
			pr_err("[SSP]: %s - allow %ubits.(%d)\n", __func__, PROX_ADC_BITS_NUM, uNewThresh);
		else {
			uNewThresh &= prox_bits_mask;
			data->uProxLoThresh_detect = uNewThresh;
		}
	}

	ssp_dbg("[SSP]: %s - new prox threshold : lodetect - %u\n", __func__, data->uProxLoThresh_detect);

	return size;
}


static ssize_t proximity_raw_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", get_proximity_rawdata(data));
}

int load_prox_cal_from_nvm(struct ssp_data *data, int *cal_data, int size)
{
	int ret = 0;

	pr_info("[SSP] %s ", __func__);

	ret = file_manager_read(data, PROX_CAL_FILE_PATH, (u8 *)cal_data, size, PROX_CAL_FILE_INDEX);

	if (ret != size) {
		memset(cal_data, 0, size);
		pr_err("[SSP]: %s - Can't read the cancel data\n", __func__);
		ret = -EIO;
	}	

	pr_info("[SSP] %s: flag %d, gain %d\n", __func__, cal_data[0], cal_data[1]);

	return ret;
}

static ssize_t proximity_default_trim_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int prox_cal[2] = {0, };
	struct ssp_data *data = dev_get_drvdata(dev);

	pr_info("[SSP] %s ", __func__);

	ret = load_prox_cal_from_nvm(data, prox_cal, sizeof(prox_cal));

	// prox_cal[1] : moving sum offset
	// 1: no offset
	// 2: moving sum type2 
	// 3: moving sum type3
	if (ret != sizeof(prox_cal)) {
#if defined(CONFIG_SENSORS_SSP_R9S)
		ret = 2;
#else
		ret = 1;
#endif
	} else {
		ret = prox_cal[1];
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", ret);
}

static ssize_t proximity_avg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		data->buf[PROXIMITY_RAW].prox_raw[1],
		data->buf[PROXIMITY_RAW].prox_raw[2],
		data->buf[PROXIMITY_RAW].prox_raw[3]);
}

/*
 *	Proximity Raw Sensor Register/Unregister
 */
static ssize_t proximity_avg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	char chTempbuf[4] = { 0 };
	int iRet;
	int64_t dEnable;
	struct ssp_data *data = dev_get_drvdata(dev);

	s32 dMsDelay = 20;

	memcpy(&chTempbuf[0], &dMsDelay, 4);

	iRet = kstrtoll(buf, 10, &dEnable);
	if (iRet < 0)
		return iRet;
	if (atomic64_read(&data->aSensorEnable) & (1 << PROXIMITY_SENSOR)) {
		if (dEnable) {
			data->buf[PROXIMITY_RAW].prox_raw[0] = -1;
			data->buf[PROXIMITY_RAW].prox_raw[1] = 0;
			data->buf[PROXIMITY_RAW].prox_raw[2] = 0;
			data->buf[PROXIMITY_RAW].prox_raw[3] = 0;
			data->uFactoryProxAvg[0] = 0;
			data->uFactoryProxAvg[1] = 0;
			data->uFactoryProxAvg[2] = 0;
			data->uFactoryProxAvg[3] = 0;
			send_instruction(data, ADD_SENSOR, PROXIMITY_RAW, chTempbuf, 4);
			data->bProximityRawEnabled = true;
		} else {
			send_instruction(data, REMOVE_SENSOR, PROXIMITY_RAW,
				chTempbuf, 4);
			data->bProximityRawEnabled = false;
		}
	}

	return size;
}

static ssize_t barcode_emul_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->bBarcodeEnabled);
}

static ssize_t barcode_emul_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int iRet;
	int64_t dEnable;
	struct ssp_data *data = dev_get_drvdata(dev);

	iRet = kstrtoll(buf, 10, &dEnable);
	if (iRet < 0)
		return iRet;

	if (dEnable)
		set_proximity_barcode_enable(data, true);
	else
		set_proximity_barcode_enable(data, false);

	return size;
}

#ifdef CONFIG_SENSORS_SSP_PROX_SETTING
static ssize_t proximity_setting_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 1);
}

static ssize_t proximity_setting_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int iRet;
	u8 val[2];
	char *token;
	char *str;
	struct ssp_msg *msg;
	struct ssp_data *data = dev_get_drvdata(dev);

	pr_info("[SSP] %s - %s\n", __func__, buf);

	//parsing
	str = (char *)buf;
	token = strsep(&str, "\n");
	if (token == NULL) {
		pr_err("[SSP] %s : too few arguments (2 needed)", __func__);
			return -EINVAL;
	}

	iRet = kstrtou8(token, 10, &val[0]);
	if (iRet < 0) {
		pr_err("[SSP] %s : kstrtou8 error %d", __func__, iRet);
		return iRet;
	}

	token = strsep(&str, "\n");
	if (token == NULL) {
		pr_err("[SSP] %s : too few arguments (2 needed)", __func__);
			return -EINVAL;
	}

	iRet = kstrtou8(token, 16, &val[1]);
	if (iRet < 0) {
		pr_err("[SSP] %s : kstrtou8 error %d", __func__, iRet);
		return iRet;
	}

	pr_info("[SSP] %s - index = %d value = 0x%x\n", __func__, val[0], val[1]);

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_SET_PROX_SETTING;
	msg->length = 2;
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(2, GFP_KERNEL);

	msg->free_buffer = 1;
	memcpy(msg->buffer, val, 2);

	iRet = ssp_spi_async(data, msg);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - fail %d\n", __func__, iRet);
		iRet = ERROR;
	}

	return size;
}
#endif

static ssize_t proximity_alert_thresh_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->uProxAlertHiThresh);
}

static ssize_t prox_light_get_dhr_sensor_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int iRet = 0;
	struct ssp_msg *msg;
	u8 buffer[18] = {0, };
	u8 chipId = 0;
	u16 threshold_high = 0;
	u16 threshold_low = 0;
	u16 threshold_detect_high = 0;
	u16 threshold_detect_low = 0;
	u8 p_drive_current = 0;
	u8 persistent_time = 0;
	u8 p_pulse = 0;
	u8 p_gain = 0;
	u8 p_time = 0;
	u8 p_pulse_length = 0;
	u8 l_atime = 0;
	int offset = 0;

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_GET_PROXIMITY_LIGHT_DHR_SENSOR_INFO;
	msg->length = 18;
	msg->options = AP2HUB_READ;
	msg->buffer = buffer;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 1000);
	if (iRet != SUCCESS) {
		pr_err("[SSP] %s fail %d\n", __func__, iRet);
		return FAIL;
	}

	chipId = buffer[0];
	threshold_high = ((((u16) buffer[2]) << 8) & 0xff00) | ((((u16) buffer[1])) & 0x00ff);
	threshold_low = ((((u16) buffer[4]) << 8) & 0xff00) | ((((u16) buffer[3])) & 0x00ff);
	threshold_detect_high = ((((u16) buffer[6]) << 8) & 0xff00) | ((((u16) buffer[5])) & 0x00ff);
	threshold_detect_low = ((((u16) buffer[8]) << 8) & 0xff00) | ((((u16) buffer[7])) & 0x00ff);
	p_drive_current = buffer[9];
	persistent_time = buffer[10];
	p_pulse = buffer[11];
	p_gain = buffer[12];
	p_time = buffer[13];
	p_pulse_length = buffer[14];
	l_atime = buffer[15];

	if (buffer[17] == 0xff)
		offset = (0xff - buffer[16]) * (-1);
	else
		offset = buffer[16];

	pr_info("[SSP] %s - %02x, %d, %d, %d, %d, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %d", __func__,
		chipId, threshold_high, threshold_low, threshold_detect_high, threshold_detect_low,
		p_drive_current, persistent_time, p_pulse, p_gain, p_time, p_pulse_length, l_atime, offset);

	return snprintf(buf, PAGE_SIZE, "\"THD\":\"%d %d %d %d\","\
		"\"PDRIVE_CURRENT\":\"%02x\","\
		"\"PERSIST_TIME\":\"%02x\","\
		"\"PPULSE\":\"%02x\","\
		"\"PGAIN\":\"%02x\","\
		"\"PTIME\":\"%02x\","\
		"\"PPLUSE_LEN\":\"%02x\","\
		"\"ATIME\":\"%02x\","\
		"\"POFFSET\":\"%d\"\n",
		threshold_high, threshold_low, threshold_detect_high, threshold_detect_low,
		p_drive_current, persistent_time, p_pulse, p_gain, p_time, p_pulse_length, l_atime, offset);
}

int proximity_save_calibration(struct ssp_data *data, int *cal_data, int size)
{
	int ret = 0;

	pr_info("[SSP] %s ", __func__);

	ret = file_manager_write(data, PROX_CAL_FILE_PATH, (char *)cal_data, size, PROX_CAL_FILE_INDEX);
	if (ret < 0) {
		pr_err("[SSP]: %s - Can't write prox cal to file\n", __func__);
		ret = -EIO;
	}
	
	pr_info("[SSP] %s: flag %d, gain %d\n", __func__, cal_data[0], cal_data[1]);

	return ret;
}

int set_prox_cal_to_ssp(struct ssp_data *data)
{
	int ret;
	int prox_cal[2] = {0, };
	struct ssp_msg *msg;

	if (!(data->uSensorState & (1 << PROXIMITY_SENSOR))) {
		pr_info("[SSP] %s - Skip this function!!!, proximity sensor is not connected(0x%llx)\n",
			__func__, data->uSensorState);
		return -EIO;
	}

	pr_info("[SSP] %s ", __func__);

	ret = load_prox_cal_from_nvm(data, prox_cal, sizeof(prox_cal));
	
	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_MCU_SET_PROX_CAL;
	msg->length = sizeof(prox_cal);
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(sizeof(prox_cal), GFP_KERNEL);
	msg->free_buffer = 1;
	memcpy(msg->buffer, &prox_cal, sizeof(prox_cal));

	ret = ssp_spi_async(data, msg);

	if (ret != SUCCESS) {
		pr_err("[SSP] %s - fail %d\n", __func__, ret);
	}

	pr_info("[SSP] %s : flag: %d, gain %d", __func__, prox_cal[0], prox_cal[1]);

	return ret;
}

static ssize_t proximity_cal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int cal_data[2] = {0, };
	struct ssp_data *data = dev_get_drvdata(dev);

	load_prox_cal_from_nvm(data, cal_data, sizeof(cal_data));

	pr_info("[SSP] %s ", __func__);

	return snprintf(buf, PAGE_SIZE, "%d,%d\n", cal_data[0], cal_data[1]);
}

static ssize_t proximity_cal_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int iRet = 0;
	int64_t enable = 0;
	struct ssp_data *data = dev_get_drvdata(dev);
#if defined(CONFIG_SENSORS_SSP_R9S)
	int cal_data[2] = {0, 2};
#else
	int cal_data[2] = {0, 1};
#endif

	if (!(data->uSensorState & (1 << PROXIMITY_SENSOR))) {
		pr_info("[SSP] %s - Skip this function!!!, proximity sensor is not connected(0x%llx)\n",
			__func__, data->uSensorState);
		return -EINVAL;
	}

	iRet = kstrtoll(buf, 10, &enable);

	if (iRet < 0)
		return iRet;

	pr_info("[SSP] %s - enable %d", __func__, enable);
	
	if (enable) {
		// ToDo: start proximity calibration
		iRet = ssp_send_cmd(data, MSG2SSP_AP_PROX_CAL_START, 0);

	} else {
		iRet = proximity_save_calibration(data, cal_data, sizeof(cal_data));
		if (iRet < 0)
			pr_err("[SSP] %s : initilaize fail", __func__);
		if (set_prox_cal_to_ssp(data) < 0)
			pr_err("[SSP]: %s - sending proximity calibration data failed\n", __func__);

	}

	return size;
}

static ssize_t proximity_offset_pass_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", 1);
}

static ssize_t proximity_position_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return get_prox(dev)->get_prox_position(dev, buf);
}

static ssize_t proximity_trim_check_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int iRet = 0, iRetries = 0;
	struct ssp_msg *msg;
	u8 buffer[8] = {0,};
	int trim;
retries:
	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_GET_PROX_TRIM;
	msg->length = 1;
	msg->options = AP2HUB_READ;
	msg->buffer = buffer;
	msg->free_buffer = 0;

	iRet = ssp_spi_async(data, msg);
	if (iRet != SUCCESS) {
		pr_err("[SSP] %s fail %d\n", __func__, iRet);

		if (iRetries++ < 2) {
			pr_err("[SSP] %s fail, retry\n", __func__);
			mdelay(5);
			goto retries;
		}
		return snprintf(buf, PAGE_SIZE, "NG\n");
	}
	
	trim = (int)buffer[0];
	pr_info("[SSP] %s - %d\n", __func__, trim);

	switch(trim) {
		case 0:
			return snprintf(buf, PAGE_SIZE, "UNTRIM\n");
		case 1:
			return snprintf(buf, PAGE_SIZE, "TRIM\n");			
	}
	return snprintf(buf, PAGE_SIZE, "NG\n");
}

static ssize_t proximity_prox_led_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	ssp_send_cmd(data, MSG2SSP_AP_PROX_LED_TEST_START, 0);

	msleep(1300);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d\n", data->prox_led_test.ret,
			data->prox_led_test.adc[0], data->prox_led_test.adc[1],
			data->prox_led_test.adc[2], data->prox_led_test.adc[3]);
}

static DEVICE_ATTR(vendor, 0440, proximity_vendor_show, NULL);
static DEVICE_ATTR(name, 0440, proximity_name_show, NULL);
static DEVICE_ATTR(prox_probe, 0440, proximity_probe_show, NULL);
static DEVICE_ATTR(thresh_high, 0660, proximity_thresh_high_show, proximity_thresh_high_store);
static DEVICE_ATTR(thresh_low, 0660, proximity_thresh_low_show, proximity_thresh_low_store);
static DEVICE_ATTR(thresh_detect_high, 0660, proximity_thresh_detect_high_show, proximity_thresh_detect_high_store);
static DEVICE_ATTR(thresh_detect_low, 0660, proximity_thresh_detect_low_show, proximity_thresh_detect_low_store);
static DEVICE_ATTR(prox_trim, 0440, proximity_default_trim_show, NULL);
static DEVICE_ATTR(raw_data, 0440, proximity_raw_data_show, NULL);
static DEVICE_ATTR(prox_avg, 0660, proximity_avg_show, proximity_avg_store);
static DEVICE_ATTR(barcode_emul_en, 0660, barcode_emul_enable_show, barcode_emul_enable_store);
static DEVICE_ATTR(prox_alert_thresh, 0440, proximity_alert_thresh_show, NULL);
static DEVICE_ATTR(dhr_sensor_info, 0440, prox_light_get_dhr_sensor_info_show, NULL);
static DEVICE_ATTR(prox_cal, 0660, proximity_cal_show, proximity_cal_store);
static DEVICE_ATTR(prox_offset_pass, 0440, proximity_offset_pass_show, NULL);

static DEVICE_ATTR(prox_position, 0440, proximity_position_show, NULL);
static DEVICE_ATTR(trim_check, 0444, proximity_trim_check_show, NULL);

static DEVICE_ATTR(prox_led_test, 0444, proximity_prox_led_test_show, NULL);

static struct device_attribute *prox_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_prox_probe,
	&dev_attr_thresh_high,
	&dev_attr_thresh_low,
	&dev_attr_thresh_detect_high,
	&dev_attr_thresh_detect_low,
	&dev_attr_prox_trim,
	&dev_attr_raw_data,
	&dev_attr_prox_avg,
	&dev_attr_barcode_emul_en,
	&dev_attr_prox_alert_thresh,
	&dev_attr_dhr_sensor_info,
	&dev_attr_prox_cal,
	&dev_attr_prox_offset_pass,
	&dev_attr_prox_position,
	&dev_attr_trim_check,
	&dev_attr_prox_led_test,
	NULL,
};

void initialize_prox_factorytest(struct ssp_data *data)
{
	memset(&prox_manager, 0, sizeof(prox_manager));
	push_back(&prox_manager, "DEFAULT", &prox_default);
#ifdef CONFIG_SENSORS_STK33910
	push_back(&prox_manager, "STK33910", get_prox_stk33910());
#endif
#ifdef CONFIG_SENSORS_TMD4907
	push_back(&prox_manager, "TMD4907", get_prox_tmd4907());
#endif
#ifdef CONFIG_SENSORS_TMD4912
	push_back(&prox_manager, "TMD4912", get_prox_tmd4912());
#endif
	sensors_register(&data->prox_device, data,
		prox_attrs, "proximity_sensor");
}

void remove_prox_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->prox_device, prox_attrs);
}
