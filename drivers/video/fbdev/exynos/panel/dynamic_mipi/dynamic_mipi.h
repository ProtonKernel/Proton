#ifndef __DYNAMIC_MIPI_H__
#define __DYNAMIC_MIPI_H__

#include "band_info.h"
#include "../panel_drv.h"

#define DM_REQ_CTX_HIBER		0x0
#define DM_REQ_CTX_RIL			0x1
#define DM_REQ_CTX_PWR_ON		0x2

int dynamic_mipi_probe(struct panel_device *panel, struct dm_total_band_info *freq_set);
int update_dynamic_mipi(struct panel_device *panel, int idx);
int panel_dm_set_ffc(struct panel_device *panel);
int panel_dm_off_ffc(struct panel_device *panel);

#endif //__DYNAMIC_MIPI_H__

