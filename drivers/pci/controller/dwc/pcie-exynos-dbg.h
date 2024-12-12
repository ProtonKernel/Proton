/*
 * PCIe host controller driver for Samsung EXYNOS SoCs
 *
 * Copyright (C) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PCIE_EXYNOS_RC_DBG_H
#define __PCIE_EXYNOS_RC_DBG_H
int exynos_pcie_dbg_unit_test(struct device *dev, struct exynos_pcie *exynos_pcie);
int exynos_pcie_dbg_link_test(struct device *dev, struct exynos_pcie *exynos_pcie, int enable);

#endif
