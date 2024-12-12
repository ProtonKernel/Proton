
/* sound/soc/samsung/vts/vts_s_lif_nm.h
 *
 * ALSA SoC - Samsung VTS Serial Local Interface driver
 *
 * Copyright (c) 2019 Samsung Electronics Co. Ltd.
  *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_VTS_S_LIF_NM_H
#define __SND_SOC_VTS_S_LIF_NM_H

int vts_s_lif_cfg_gpio(struct device *dev, const char *name);
void vts_s_lif_dmic_aud_en(struct device *dev, int en);
void vts_s_lif_dmic_if_en(struct device *dev, int en);

#endif /* __SND_SOC_VTS_S_LIF_NM_H */
