/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/slab.h>

#include "npu-config.h"
#include "npu-log.h"

unsigned int configs[NPU_CONFIG_NUM];
const char *npu_vertex_name;

static const char *cfg_name[] = {
	"npu_max_buffer",
	"npu_max_plane",
	"npu_max_graph",
	"npu_max_frame",
	"npu_minor",
	"mw_q_size",
	"frame_q_size",
	"npu_fw_flog_keep_size",
	"npu_mailbox_default_tid",
	"tcnto0_off",
	"npu_mailbox_hdr_section_len",
	"npu_mailbox_size",
	"npu_mailbox_base",
	"power_down_delay_on_emergency",
	"streamoff_delay_on_emergency",
	"npu_fw_base_addr",
	"npu_cm7_release_hack",
	"npu_shared_mem_payload",
	"npu_c2agent_0",
	"npu_c2agent_1",
	"npu_votf",
	"npu_votf_size",
	"npu_pbha_hint_00",
	"npu_pbha_hint_01",
	"npu_pbha_hint_10",
	"npu_pbha_hint_11",
	"transactions_per_core",
	"cmdq_complexity_per_core",
	"lastq_time_threshold",
};

static const char *dfn_name[] = {
	"ncp_version",
	"mailbox_version",
	"buffer_q_list_size",
	"ncp_mgmt_list_size",
	"npu_max_msg_id_cnt",
};

int npu_create_config(struct device *dev)
{
#if !BRING_UP
	int index = 0;
#endif

	npu_vertex_name = kzalloc(16, GFP_KERNEL);

#if BRING_UP
	probe_info("Start create configs from define");
	npu_vertex_name = "npu";
	configs[NPU_MAX_BUFFER] = TMP_NPU_MAX_BUFFER;
	configs[NPU_MAX_PLANE] = TMP_NPU_MAX_PLANE;
	configs[NPU_MAX_GRAPH] = TMP_NPU_MAX_GRAPH;
	configs[NPU_MAX_FRAME] = TMP_NPU_MAX_FRAME;
	configs[NPU_MINOR] = TMP_NPU_MINOR;
	configs[NW_Q_SIZE] = TMP_NW_Q_SIZE;
	configs[FRAME_Q_SIZE] = TMP_FRAME_Q_SIZE;
	configs[NPU_FW_LOG_KEEP_SIZE] = TMP_NPU_FW_LOG_KEEP_SIZE;
	configs[NPU_MAILBOX_DEFAULT_TID] = TMP_NPU_MAILBOX_DEFAULT_TID;
	configs[TCNTO0_OFF] = TMP_TCNTO0_OFF;
	configs[NPU_MAILBOX_HDR_SECTION_LEN] = TMP_NPU_MAILBOX_HDR_SECTION_LEN;
	configs[NPU_MAILBOX_SIZE] = TMP_NPU_MAILBOX_SIZE;
	configs[NPU_MAILBOX_BASE] = TMP_NPU_MAILBOX_BASE;
	configs[POWER_DOWN_DELAY_ON_EMERGENCY] = TMP_POWER_DOWN_DELAY_ON_EMERGENCY;
	configs[STREAMOFF_DELAY_ON_EMERGENCY] = TMP_STREAMOFF_DELAY_ON_EMERGENCY;
	configs[NPU_FW_BASE_ADDR] = TMP_NPU_FW_BASE_ADDR;
	configs[NPU_CM7_RELEASE_HACK] = TMP_NPU_CM7_RELEASE_HACK;
	configs[NPU_SHARED_MEM_PAYLOAD] = TMP_NPU_SHARED_MEM_PAYLOAD;
	configs[NPU_C2AGENT_0] = TMP_NPU_C2AGENT_0;
	configs[NPU_C2AGENT_1] = TMP_NPU_C2AGENT_1;
	configs[NPU_VOTF] = TMP_NPU_VOTF;
	configs[NPU_VOTF_SIZE] = TMP_NPU_VOTF_SIZE;
	configs[NPU_PBHA_HINT_00] = TMP_NPU_PBHA_HINT_00;
	configs[NPU_PBHA_HINT_01] = TMP_NPU_PBHA_HINT_01;
	configs[NPU_PBHA_HINT_10] = TMP_NPU_PBHA_HINT_10;
	configs[NPU_PBHA_HINT_11] = TMP_NPU_PBHA_HINT_11;
	configs[TRANSACTIONS_PER_CORE] = TMP_TRANSACTIONS_PER_CORE;
	configs[CMDQ_COMPLEXITY_PER_CORE] = TMP_CMDQ_COMPLEXITY_PER_CORE;
	configs[LASTQ_TIME_THRESHOLD] = TMP_LASTQ_TIME_THRESHOLD;
#else
	probe_info("Start create configs from dts");
	if (of_property_read_string(dev->of_node, "vertex_name",
							&npu_vertex_name)) {
		probe_info("failed npu create_config %s\n", "vertex_name");
		goto exit;
	}

	for (index = 0; index < NPU_CONFIG_NUM; index++) {
		if (of_property_read_u32_index(dev->of_node,
				"configs", index, &configs[index])) {
			probe_info("failed npu create_config %d\n", index);
			goto exit;
		}
	}

	npu_print_config();
#endif

	probe_info("Create NPU configs done.\n");
	return 0;
#if !BRING_UP
exit:
	probe_err("Create NPU configs failed\n");
	return -1;
#endif
}

void npu_print_config(void)
{
#if DEBUG_CONFIG
	probe_info("NPU driver configs.\n");
	probe_info("vertex_name : %s, ", npu_vertex_name);
	probe_info("%s : %u, %s : %u, %s : %u, %s : %u, "
		"%s : %u, %s : %u, %s : %u, %s : %u, "
		"%s : %u, %s : %u, %s : %u, %s : %u, "
		"%s : %u, %s : %u, %s : %u, %s : %u, "
		"%s : %u, %s : %u, %s : %u, %s : %u, "
		"%s : %u, %s : %u, %s : %u, %s : %u, "
		"%s : %u\n",
		cfg_name[NPU_MAX_BUFFER], configs[NPU_MAX_BUFFER],
		cfg_name[NPU_MAX_PLANE], configs[NPU_MAX_PLANE],
		cfg_name[NPU_MAX_GRAPH], configs[NPU_MAX_GRAPH],
		cfg_name[NPU_MAX_FRAME], configs[NPU_MAX_FRAME],
		cfg_name[NPU_MINOR], configs[NPU_MINOR],
		cfg_name[NW_Q_SIZE], configs[NW_Q_SIZE],
		cfg_name[FRAME_Q_SIZE], configs[FRAME_Q_SIZE],
		cfg_name[NPU_FW_LOG_KEEP_SIZE], configs[NPU_FW_LOG_KEEP_SIZE],
		cfg_name[NPU_MAILBOX_DEFAULT_TID], configs[NPU_MAILBOX_DEFAULT_TID],
		cfg_name[TCNTO0_OFF], configs[TCNTO0_OFF],
		cfg_name[NPU_MAILBOX_HDR_SECTION_LEN], configs[NPU_MAILBOX_HDR_SECTION_LEN],
		cfg_name[NPU_MAILBOX_BASE], configs[NPU_MAILBOX_BASE],
		cfg_name[NPU_MAILBOX_SIZE], configs[NPU_MAILBOX_SIZE],
		cfg_name[POWER_DOWN_DELAY_ON_EMERGENCY], configs[POWER_DOWN_DELAY_ON_EMERGENCY],
		cfg_name[STREAMOFF_DELAY_ON_EMERGENCY], configs[STREAMOFF_DELAY_ON_EMERGENCY],
		cfg_name[NPU_FW_BASE_ADDR], configs[NPU_FW_BASE_ADDR],
		cfg_name[NPU_SHARED_MEM_PAYLOAD], configs[NPU_SHARED_MEM_PAYLOAD],
		cfg_name[NPU_C2AGENT_0], configs[NPU_C2AGENT_0],
		cfg_name[NPU_C2AGENT_1], configs[NPU_C2AGENT_1],
		cfg_name[NPU_VOTF], configs[NPU_VOTF],
		cfg_name[NPU_VOTF_SIZE], configs[NPU_VOTF_SIZE],
		cfg_name[NPU_PBHA_HINT_00], configs[NPU_PBHA_HINT_00],
		cfg_name[NPU_PBHA_HINT_01], configs[NPU_PBHA_HINT_01],
		cfg_name[NPU_PBHA_HINT_10], configs[NPU_PBHA_HINT_10],
		cfg_name[NPU_PBHA_HINT_11], configs[NPU_PBHA_HINT_11]);
	probe_info("%s : %u, %s : %u, %s : %u, %s : %u, %s : %u\n",
		dfn_name[DFN_NCP_VERSION], NCP_VERSION,
		dfn_name[DFN_MAILBOX_VERSION], MAILBOX_VERSION,
		dfn_name[DFN_BUFFER_Q_LIST_SIZE], BUFFER_Q_LIST_SIZE,
		dfn_name[DFN_NCP_MGMT_LIST_SIZE], NCP_MGMT_LIST_SIZE,
		dfn_name[DFN_NPU_MAX_MSG_ID_CNT], NPU_MAX_MSG_ID_CNT);
#endif
}
