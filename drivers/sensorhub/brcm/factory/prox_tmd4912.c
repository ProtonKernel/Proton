#include "../ssp.h"
#include "sensors.h"

/*************************************************************************/
/* Functions                                                             */
/*************************************************************************/
static ssize_t proximity_position_show(struct device *dev, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	switch(data->ap_type) {
#if defined(CONFIG_SENSORS_SSP_PICASSO)
		case 0:
			return snprintf(buf, PAGE_SIZE, "45.1 8.0 2.4\n");
		case 1:
			return snprintf(buf, PAGE_SIZE, "42.6 8.0 2.4\n");
		case 2:
			return snprintf(buf, PAGE_SIZE, "43.8 8.0 2.4\n");
#endif
		default:
			return snprintf(buf, PAGE_SIZE, "0.0 0.0 0.0\n");
	}

	return snprintf(buf, PAGE_SIZE, "0.0 0.0 0.0\n");
}

struct proximity_t prox_tmd4912 = {
	.name = "TMD4912",
	.vendor = "AMS",
	.get_prox_position = proximity_position_show
};

struct proximity_t* get_prox_tmd4912(){
	return &prox_tmd4912;
}