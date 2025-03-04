#include "../ssp.h"
#include "sensors.h"

/*************************************************************************/
/* Functions                                                             */
/*************************************************************************/
static ssize_t proximity_position_show(struct device *dev, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	switch(data->ap_type) {
		case 0:
			return snprintf(buf, PAGE_SIZE, "1.0 1.0 1.0\n");
		default:
			return snprintf(buf, PAGE_SIZE, "0.0 0.0 0.0\n");
	}

	return snprintf(buf, PAGE_SIZE, "0.0 0.0 0.0\n");
}

struct proximity_t prox_stk33910 = {
	.name = "STK33910",
	.vendor = "SITRON",
	.get_prox_position = proximity_position_show
};

struct proximity_t* get_prox_stk33910(){
	return &prox_stk33910;
}
