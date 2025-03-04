/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Defines CMU_PMU mapping table.
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 */

struct cmu_pmu {
	unsigned int cmu;
	char pmu[20];
};

struct cmu_pmu cmu_pmu_map[] = {
	{0x18C00000, "pd-aud"},
	{0x17000000, "pd-csis"},
	{0x17500000, "pd-dns"},
	{0x19E00000, "pd-dpub"},
	{0x19C00000, "pd-dpuf0"},
	{0x1AE00000, "pd-dpuf1"},
	{0x18B00000, "pd-g2d"},
	{0x18400000, "pd-embedded_g3d"},
	{0x10A00000, "pd-hsi0"},
	{0x17400000, "pd-itp"},
	{0x17700000, "pd-lme"},
	{0x18900000, "pd-m2m"},
	{0x17800000, "pd-mcfp0"},
	{0x17A00000, "pd-mcfp1"},
	{0x15C00000, "pd-mcsc"},
	{0x18600000, "pd-mfc0"},
	{0x18700000, "pd-mfc1"},
	{0x19800000, "pd-npu00"},
	{0x19900000, "pd-npu01"},
	{0x19a00000, "pd-npu10"},
	{0x19b00000, "pd-npu11"},
	{0x17C00000, "pd-npus"},
	{0x18200000, "pd-ssp"},
	{0x16E00000, "pd-taa"},
	{0x16600000, "pd-vpc"},
	{0x16800000, "pd-vpd0"},
	{0x16900000, "pd-vpd1"},
	{0x16A00000, "pd-vpd2"},
	{0x15500000, "pd-vts"},
	{0x18000000, "pd-yuvpp"},
};
