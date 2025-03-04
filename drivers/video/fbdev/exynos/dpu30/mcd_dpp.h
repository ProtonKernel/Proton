/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung EXYNOS MCD HDR driver

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "dpp.h"

int dpp_get_mcdhdr_info(struct dpp_device *dpp);

#ifdef CONFIG_MCDHDR
int dpp_set_mcdhdr_params(struct dpp_device *dpp, struct dpp_params_info *p);
void dpp_stop_mcdhdr(struct dpp_device *dpp);
#else
static inline int dpp_set_mcdhdr_params(struct dpp_device *dpp, struct dpp_params_info *p) { return 0; }
static inline void dpp_stop_mcdhdr(struct dpp_device *dpp) { return; };
#endif

