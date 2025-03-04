#include "../ssp.h"
#include "sensors.h"

enum {
	OFF = 0,
	ON = 1
};

enum {
	X = 0,
	Y = 1,
	Z = 2,
	AXIS_MAX
};

struct factory_cover_status_data {
	char cover_status[10];
	uint8_t axis_select;
	int32_t threshold;
	int32_t init[AXIS_MAX];
	int32_t attach[AXIS_MAX];
	int32_t attach_extremum[AXIS_MAX];
	int32_t detach[AXIS_MAX];
	char attach_result[10];
	char detach_result[10];
	char final_result[10];

	int32_t attach_diff;
	int32_t detach_diff;
	int32_t failed_attach_max;
	int32_t failed_attach_min;
	uint8_t saturation;

	int event_count;
};

#define ATTACH          "ATTACH"
#define DETACH          "DETACH"
#define FACTORY_PASS    "PASS"
#define FACTORY_FAIL    "FAIL"

#define AXIS_SELECT      X
#define THRESHOLD        700
#define DETACH_MARGIN    100
#define SATURATION_VALUE 4900
#define MAG_DELAY_MS     50

#define COVER_DETACH            0 // OPEN
#define COVER_ATTACH            1 // CLOSE
#define COVER_ATTACH_NFC_ACTIVE 2 // CLOSE

static struct factory_cover_status_data *factory_data;
static char sysfs_cover_status[10];

static int log_cnt = 0;

static ssize_t nfc_cover_status_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	if (data->fcd_data.nfc_cover_status == COVER_ATTACH || data->fcd_data.nfc_cover_status == COVER_ATTACH_NFC_ACTIVE) {
		snprintf(sysfs_cover_status, 10, "CLOSE");
	} else if (data->fcd_data.nfc_cover_status == COVER_DETACH) {
		snprintf(sysfs_cover_status, 10, "OPEN");
	}

	pr_info("[SSP] %s [FACTORY] nfc_cover_status=%s", __func__, sysfs_cover_status);

	return snprintf(buf, PAGE_SIZE, "%s\n",	sysfs_cover_status);
}

static ssize_t nfc_cover_status_store(struct device *dev,
				      struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int status = 0;
	int ret = 0;
	struct ssp_msg *msg;

	ret = kstrtoint(buf, 10, &status);
	if (ret < 0)
		return ret;

	if (status < 0 || status > 2) {
		pr_err("[SSP] %s invalid status %d", __func__, status);
		return -EINVAL;
	}

	data->fcd_data.nfc_cover_status = status;

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_SET_FCD_COVER_STATUS;
	msg->length = sizeof(data->fcd_data.nfc_cover_status);
	msg->options = AP2HUB_WRITE;
	msg->buffer = (u8 *)&data->fcd_data.nfc_cover_status;
	msg->free_buffer = 0;
	ret = ssp_spi_async(data, msg);

	if (ret < 0)
		pr_err("[SSP] send failed");

	pr_info("[SSP] %s nfc_cover_status is %d \n", __func__,  data->fcd_data.nfc_cover_status);

	return size;
}

static void enable_factory_test(struct ssp_data *data, int request)
{
	int ret = 0;
	struct ssp_msg *msg;

	data->fcd_data.factory_cover_status = request;

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = FCD_FACTORY;
	msg->length = sizeof(data->fcd_data.factory_cover_status);
	msg->options = AP2HUB_WRITE;
	msg->buffer = (u8 *)&data->fcd_data.factory_cover_status;
	msg->free_buffer = 0;

	ret = ssp_spi_async(data, msg);

	if (ret < 0) {
		pr_err("[SSP]: send factory_cover_status failed \n");
		return;
	}

	if (request == ON)
		factory_data->event_count = 0;
}

static void factory_data_init(struct ssp_data *data, struct sensor_value *flip_cover_detector_data)
{
	int mag_data[AXIS_MAX];
	int axis = 0;

	pr_info("[SSP] %s ", __func__);

	memset(factory_data, 0, sizeof(struct factory_cover_status_data));
	mag_data[X] = flip_cover_detector_data->uncal_mag_x;
	mag_data[Y] = flip_cover_detector_data->uncal_mag_y;
	mag_data[Z] = flip_cover_detector_data->uncal_mag_z;

	pr_info("[SSP] [FACTORY] init data : %d %d %d ", mag_data[X], mag_data[Y], mag_data[Z]);

	factory_data->axis_select = data->fcd_data.axis_update;
	factory_data->threshold = (data->fcd_data.threshold_update > 0) ? data->fcd_data.threshold_update
																	: data->fcd_data.threshold_update * (-1);

	for (axis = X; axis < AXIS_MAX; axis++) {
		factory_data->init[axis] = mag_data[axis];
		factory_data->attach[axis] = mag_data[axis];
	}

	factory_data->failed_attach_max = mag_data[factory_data->axis_select];
	factory_data->failed_attach_min = mag_data[factory_data->axis_select];

	snprintf(factory_data->cover_status, 10, DETACH);
	snprintf(factory_data->attach_result, 10, FACTORY_FAIL);
	snprintf(factory_data->detach_result, 10, FACTORY_FAIL);
	snprintf(factory_data->final_result, 10, FACTORY_FAIL);
}

void check_cover_detection_factory(struct ssp_data *data, struct sensor_value *flip_cover_detector_data)
{
	int axis = 0;
	int axis_select = factory_data->axis_select;
	int mag_data[AXIS_MAX];

	if (factory_data->event_count == 0) {
		factory_data_init(data, flip_cover_detector_data);
		factory_data->event_count++;
	}

	mag_data[X] = flip_cover_detector_data->uncal_mag_x;
	mag_data[Y] = flip_cover_detector_data->uncal_mag_y;
	mag_data[Z] = flip_cover_detector_data->uncal_mag_z;
	factory_data->saturation = flip_cover_detector_data->saturation;

	if (log_cnt % 10 == 0) {
		pr_info("[SSP] [FACTORY] uncal mag : %d %d %d, saturation : %d \n",
			mag_data[X], mag_data[Y], mag_data[Z], factory_data->saturation);
		log_cnt = 0;
	}

	log_cnt++;

	if (!strcmp(factory_data->cover_status, DETACH)) {
		if (mag_data[axis_select] > factory_data->failed_attach_max) {
			factory_data->failed_attach_max = mag_data[axis_select];

			if (abs(factory_data->failed_attach_max - factory_data->init[axis_select])
				> abs(factory_data->failed_attach_min - factory_data->init[axis_select])) {
				for (axis = X; axis < AXIS_MAX; axis++)
					factory_data->attach[axis] = mag_data[axis];
			}
		} else if (mag_data[axis_select] < factory_data->failed_attach_min) {
			factory_data->failed_attach_min = mag_data[axis_select];

			if (abs(factory_data->failed_attach_max - factory_data->init[axis_select])
				< abs(factory_data->failed_attach_min - factory_data->init[axis_select])) {
				for (axis = X; axis < AXIS_MAX; axis++)
					factory_data->attach[axis] = mag_data[axis];
			}
		}

		/*pr_info("[SSP] [FACTORY] : failed_attach_max=%d, failed_attach_min=%d\n",
			  factory_data->failed_attach_max, factory_data->failed_attach_min);*/

		factory_data->attach_diff = mag_data[axis_select] - factory_data->init[axis_select];

		if (abs(factory_data->attach_diff) > factory_data->threshold) {
			snprintf(factory_data->cover_status, 10, ATTACH);
			snprintf(factory_data->attach_result, 10, FACTORY_PASS);
			for (axis = X; axis < AXIS_MAX; axis++) {
				factory_data->attach[axis] = mag_data[axis];
				factory_data->attach_extremum[axis] = mag_data[axis];
			}
			pr_info("[SSP] [FACTORY] : COVER ATTACHED \n");
		}
	} else {
		if (factory_data->attach_diff > 0) {
			if (factory_data->saturation) {
				for (axis = X; axis < AXIS_MAX; axis++)
					mag_data[axis] = SATURATION_VALUE;
			}

			if (mag_data[axis_select] > factory_data->attach_extremum[axis_select]) {
				for (axis = X; axis < AXIS_MAX; axis++) {
					factory_data->attach_extremum[axis] = mag_data[axis];
					factory_data->detach[axis] = 0;
				}
			}
		} else {
			if (factory_data->saturation) {
				for (axis = X; axis < AXIS_MAX; axis++)
					mag_data[axis] = -SATURATION_VALUE;
			}

			if (mag_data[axis_select] < factory_data->attach_extremum[axis_select]) {
				for (axis = X; axis < AXIS_MAX; axis++) {
					factory_data->attach_extremum[axis] = mag_data[axis];
					factory_data->detach[axis] = 0;
				}
			}
		}

		factory_data->detach_diff = mag_data[axis_select] - factory_data->attach_extremum[axis_select];

		if (factory_data->attach_diff > 0) {
			if (mag_data[axis_select] < (factory_data->attach_extremum[axis_select] - DETACH_MARGIN)) {
				for (axis = X; axis < AXIS_MAX; axis++)
					factory_data->detach[axis] = mag_data[axis];
			}

			if (factory_data->detach_diff < -factory_data->threshold) {
				snprintf(factory_data->cover_status, 10, DETACH);
				snprintf(factory_data->detach_result, 10, FACTORY_PASS);
				snprintf(factory_data->final_result, 10, FACTORY_PASS);
				data->fcd_data.factory_cover_status = OFF;
				pr_info("[SSP] [FACTORY] : COVER_DETACHED \n");
			}
		} else {
			if (mag_data[axis_select] > (factory_data->attach_extremum[axis_select] + DETACH_MARGIN)) {
				for (axis = X; axis < AXIS_MAX; axis++)
					factory_data->detach[axis] = mag_data[axis];
			}

			if (factory_data->detach_diff > factory_data->threshold) {
				snprintf(factory_data->cover_status, 10, DETACH);
				snprintf(factory_data->detach_result, 10, FACTORY_PASS);
				snprintf(factory_data->final_result, 10, FACTORY_PASS);
				data->fcd_data.factory_cover_status = OFF;
				pr_info("[SSP] [FACTORY]: COVER_DETACHED \n");
			}
		}
	}

	/*pr_info("[SSP] [FACTORY] : cover_status=%s, axis_select=%d, thd=%d, \
		    x_init=%d, x_attach=%d, x_min_max=%d, x_detach=%d, \
		    y_init=%d, y_attach=%d, y_min_max=%d, y_detach=%d, \
		    z_init=%d, z_attach=%d, z_min_max=%d, z_detach=%d, \
		    attach_result=%s, detach_result=%s, final_result=%s \n",
		    factory_data->cover_status, factory_data->axis_select, factory_data->threshold,
		    factory_data->init[X], factory_data->attach[X], factory_data->attach_extremum[X],
		    factory_data->detach[X], factory_data->init[Y], factory_data->attach[Y],
		    factory_data->attach_extremum[Y], factory_data->detach[Y], factory_data->init[Z],
		    factory_data->attach[Z], factory_data->attach_extremum[Z], factory_data->detach[Z],
		    factory_data->attach_result, factory_data->detach_result, factory_data->final_result);*/
}

static ssize_t factory_cover_status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("[SSP] [FACTORY] status=%s, axis=%d, thd=%d, \
		   x_init=%d, x_attach=%d, x_min_max=%d, x_detach=%d, \
		   y_init=%d, y_attach=%d, y_min_max=%d, y_detach=%d, \
		   z_init=%d, z_attach=%d, z_min_max=%d, z_detach=%d, \
		   attach_result=%s, detach_result=%s, final_result=%s \n",
		   factory_data->cover_status, factory_data->axis_select, factory_data->threshold,
		   factory_data->init[X], factory_data->attach[X], factory_data->attach_extremum[X],
		   factory_data->detach[X], factory_data->init[Y], factory_data->attach[Y],
		   factory_data->attach_extremum[Y], factory_data->detach[Y], factory_data->init[Z],
		   factory_data->attach[Z], factory_data->attach_extremum[Z], factory_data->detach[Z],
		   factory_data->attach_result, factory_data->detach_result, factory_data->final_result);

	return snprintf(buf, PAGE_SIZE, "%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s,%s,%s\n",
		factory_data->cover_status, factory_data->axis_select, factory_data->threshold,
		factory_data->init[X], factory_data->attach[X], factory_data->attach_extremum[X],
		factory_data->detach[X], factory_data->init[Y], factory_data->attach[Y],
		factory_data->attach_extremum[Y], factory_data->detach[Y], factory_data->init[Z],
		factory_data->attach[Z], factory_data->attach_extremum[Z], factory_data->detach[Z],
		factory_data->attach_result, factory_data->detach_result, factory_data->final_result);
}

static ssize_t factory_cover_status_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int factory_test_request = 0;
	int ret = 0;

	ret = kstrtoint(buf, 10, &factory_test_request);
	if (ret < 0)
		return ret;

	if (factory_test_request < 0 || factory_test_request > 1) {
		pr_err("[SSP] [FACTORY] invalid status %d \n", factory_test_request);
		return -EINVAL;
	}

	log_cnt = 0;

	if (factory_test_request == ON /*&& data->fcd_data.factory_cover_status == OFF*/)
		enable_factory_test(data, ON);
	else if (factory_test_request == OFF /*&& data->fcd_data.factory_cover_status == ON*/)
		enable_factory_test(data, OFF);

	pr_info("[SSP] [FACTORY] factory_cover_status is %d \n", data->fcd_data.factory_cover_status);
	return size;
}

static ssize_t axis_threshold_setting_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	pr_info("[SSP] [FACTORY] axis=%d, threshold=%d \n", data->fcd_data.axis_update, data->fcd_data.threshold_update);

	return snprintf(buf, PAGE_SIZE, "%d,%d\n", data->fcd_data.axis_update, data->fcd_data.threshold_update);
}

static ssize_t axis_threshold_setting_store(struct device *dev,
					    struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	struct ssp_msg *msg;
	int ret;
	int8_t axis;
	int threshold;
	int8_t shub_data[5] = {0};

	ret = sscanf(buf, "%d,%d", &axis, &threshold);

	if (ret != 2) {
		pr_err("[SSP]: [FACTORY] Invalid values received, ret=%d\n", ret);
		return -EINVAL;
	}

	if (axis < 0 || axis >= AXIS_MAX) {
		pr_err("[SSP]: [FACTORY] Invalid axis value received\n");
		return -EINVAL;
	}

	pr_info("[SSP]: [FACTORY] axis=%d, threshold=%d\n", axis, threshold);

	data->fcd_data.axis_update = axis;
	data->fcd_data.threshold_update = threshold;

	memcpy(shub_data, &data->fcd_data.axis_update, sizeof(data->fcd_data.axis_update));
	memcpy(shub_data + 1, &data->fcd_data.threshold_update, sizeof(data->fcd_data.threshold_update));

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_SET_FCD_AXIS_THRES;
	msg->length = sizeof(shub_data);
	msg->options = AP2HUB_WRITE;
	msg->buffer = (u8 *)&shub_data;
	msg->free_buffer = 0;
	ret = ssp_spi_async(data, msg);

	if (ret < 0)
		pr_err("[SSP]: send axis/threshold failed, ret=%d \n", ret);

	return size;
}

static DEVICE_ATTR(nfc_cover_status, 0664, nfc_cover_status_show, nfc_cover_status_store);
static DEVICE_ATTR(factory_cover_status, 0664, factory_cover_status_show, factory_cover_status_store);
static DEVICE_ATTR(axis_threshold_setting, 0664, axis_threshold_setting_show, axis_threshold_setting_store);

static struct device_attribute *fcd_attrs[] = {
	&dev_attr_nfc_cover_status,
	&dev_attr_factory_cover_status,
	&dev_attr_axis_threshold_setting,
	NULL,
};

void initialize_fcd_factorytest(struct ssp_data *data)
{
	factory_data = kzalloc(sizeof(*factory_data), GFP_KERNEL);
	sensors_register(&data->fcd_device, data,
		fcd_attrs, "flip_cover_detector_sensor");
}

void remove_fcd_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->fcd_device, fcd_attrs);
}