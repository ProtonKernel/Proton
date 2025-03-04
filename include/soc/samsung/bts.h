/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 *
 * This program is free software; you can redistributs it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 */

#ifndef __EXYNOS_BTS_H_
#define __EXYNOS_BTS_H_

/**
 * struct bts_bw - BTS bandwidth information
 * @name:	name of IP
 * @peak:	IP Peak bandwidth
 * @read:	Average read Bandwidth
 * @write:	Average write Bandwidth
 *
 */
struct bts_bw {
	char			*name;
	unsigned int		peak;
	unsigned int		read;
	unsigned int		write;
};

#if defined(CONFIG_EXYNOS_BTS) || defined(CONFIG_EXYNOS_BTS_MODULE)
int bts_get_bwindex(const char *name);
int bts_update_bw(unsigned int index, struct bts_bw bw);
unsigned int bts_get_scenindex(const char *name);
int bts_add_scenario(unsigned int index);
int bts_del_scenario(unsigned int index);

void bts_pd_sync(unsigned int cal_id, int on);

int bts_change_mo(unsigned int scen, unsigned int ip,
		  unsigned int rmo, unsigned int wmo);

#else /* CONFIG_EXYNOS_BTS */

#define bts_get_bwindex(a) do {} while (0)
#define bts_update_bw(a, b) do {} while (0)
#define bts_get_scenindex(a) do {} while (0)
#define bts_add_scenario(a) do {} while (0)
#define bts_del_scenario(a) do {} while (0)
#define bts_pd_sync(a, b) do {} while (0)
#define bts_change_mo(a, b, c, d) do {} while(0)

#endif /* CONFIG_EXYNOS_BTS */

#define bts_update_scen(a, b) do {} while (0)
#define exynos_bts_scitoken_setting(a) do {} while (0)

#endif /* __EXYNOS_BTS_H_ */
