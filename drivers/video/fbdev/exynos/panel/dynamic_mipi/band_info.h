#ifndef __BAND_INFO_H__
#define __BAND_INFO_H__

#include "../../dpu30/panels/exynos_panel.h"

#define TBL_ARR_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define DEFINE_FREQ_RANGE(_min, _max, _idx, _ddi_osc)	\
{							\
	.min = _min,			\
	.max = _max,			\
	.freq_idx = _idx,		\
	.ddi_osc = _ddi_osc,	\
}

#define DEFINE_FREQ_SET(_array)	\
{								\
	.array = _array,			\
	.size = TBL_ARR_SIZE(_array),	\
}

struct dm_band_info {
	int min;
	int max;
	int freq_idx;
	int ddi_osc;
};


struct dm_total_band_info {
	struct dm_band_info *array;
	unsigned int size;
};

struct __packed ril_noti_info {
	u8 rat;
	u32 band;
	u32 channel;
};

enum {
	FREQ_RANGE_850	= 1,
	FREQ_RANGE_900 = 2,
	FREQ_RANGE_1800 = 3,
	FREQ_RANGE_1900 = 4,
	FREQ_RANGE_WB01 = 11,
	FREQ_RANGE_WB02 = 12,
	FREQ_RANGE_WB03 = 13,
	FREQ_RANGE_WB04 = 14,
	FREQ_RANGE_WB05 = 15,
	FREQ_RANGE_WB07 = 17,
	FREQ_RANGE_WB08 = 18,
	FREQ_RANGE_TD1 = 51,
	FREQ_RANGE_TD2 = 52,
	FREQ_RANGE_TD3 = 53,
	FREQ_RANGE_TD4 = 54,
	FREQ_RANGE_TD5 = 55,
	FREQ_RANGE_TD6 = 56,
	FREQ_RANGE_BC0  = 61,
	FREQ_RANGE_BC1  = 62,
	FREQ_RANGE_BC10 = 71,
	FREQ_RANGE_LB01 = 91,
	FREQ_RANGE_LB02 = 92,
	FREQ_RANGE_LB03 = 93,
	FREQ_RANGE_LB04 = 94,
	FREQ_RANGE_LB05 = 95,
	FREQ_RANGE_LB07 = 97,
	FREQ_RANGE_LB08 = 98,
	FREQ_RANGE_LB12 = 102,
	FREQ_RANGE_LB13 = 103,
	FREQ_RANGE_LB14 = 104,
	FREQ_RANGE_LB17 = 107,
	FREQ_RANGE_LB18 = 108,
	FREQ_RANGE_LB19 = 109,
	FREQ_RANGE_LB20 = 110,
	FREQ_RANGE_LB21 = 111,
	FREQ_RANGE_LB25 = 115,
	FREQ_RANGE_LB26 = 116,
	FREQ_RANGE_LB28 = 118,
	FREQ_RANGE_LB29 = 119,
	FREQ_RANGE_LB30 = 120,
	FREQ_RANGE_LB32 = 122,
	FREQ_RANGE_LB34 = 124,
	FREQ_RANGE_LB38 = 128,
	FREQ_RANGE_LB39 = 129,
	FREQ_RANGE_LB40 = 130,
	FREQ_RANGE_LB41 = 131,
	FREQ_RANGE_LB42 = 132,
	FREQ_RANGE_LB48 = 138,
	FREQ_RANGE_LB66 = 156,
	FREQ_RANGE_LB71 = 161,
	FREQ_RANGE_N005	= 260,
	FREQ_RANGE_N008	= 263,
	FREQ_RANGE_N028	= 283,
	FREQ_RANGE_N071	= 326,
	FREQ_RANGE_MAX = 327,
};

#define MAX_DYNAMIC_FREQ		5
#define MAX_PANEL_FFC_CMD		32

#define MAX_OSC_CNT	2

struct dm_status_info {
	u32 request_df;
	u32 target_df;
	u32 current_df;
	u32 ffc_df;
	u32 req_ctx;

	/* Frame count that delays the frequency hopping when display turns on, read from dt*/
	u32 skip_frame_cnt;
	atomic_t frame_cnt;

	u32 current_ddi_osc;
	u32 request_ddi_osc;
};

struct dm_hs_info {
	unsigned int hs_clk;
	struct stdphy_pms dphy_pms;

	unsigned int ffc_cmd_sz;
	unsigned char ffc_cmd[MAX_OSC_CNT][MAX_PANEL_FFC_CMD];
};

struct dm_dt_info {
	int dm_default;
	int dm_hs_list_cnt;
	struct dm_hs_info dm_hs_list[MAX_DYNAMIC_FREQ];

	int ffc_off_cmd_sz;
	char ffc_off_cmd[MAX_PANEL_FFC_CMD];
};


struct dynamic_mipi_info {
	int enabled;
	struct notifier_block dm_noti;
	struct dm_status_info dm_status;
	struct dm_dt_info dm_dt;
	/*band info from panel file */
	struct dm_total_band_info *total_band;
};


struct dm_param_info {
	struct stdphy_pms pms;
	u32 req_ctx;
};

#endif //__BAND_INFO_H__
