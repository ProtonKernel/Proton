/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Core file for Samsung EXYNOS VOTF driver
 * (FIMC-IS PostProcessing Generic Distortion Correction driver)
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/sched/clock.h>
#include <linux/dma-iommu.h>
#include "camerapp-votf.h"
#include "camerapp-votf-reg.h"
#include "camerapp-debug.h"

static struct votf_dev *votfdev;

static inline int search_ip_idx(struct votf_info *vinfo)
{
	int svc, ip, id;

	svc = vinfo->service;
	id = vinfo->id;

	for (ip = 0; ip < IP_MAX; ip++) {
		if (vinfo->ip == votfdev->votf_table[svc][ip][id].ip)
			return ip;
	}

	pr_err("%s: do not find ip index(0x%x)\n", __func__, vinfo->ip);

	return IP_MAX;
}

static bool check_vinfo(struct votf_info *vinfo)
{
	if (!vinfo) {
		pr_err("%s: votf info is null\n", __func__);
		return false;
	}
	if (vinfo->service >= SERVICE_CNT || vinfo->id >= ID_MAX ||
			search_ip_idx(vinfo) >= IP_MAX) {
		pr_err("%s: invalid votf input(svc:%d, ip:%d, id:%d)\n", __func__,
			vinfo->service, vinfo->ip, vinfo->id);
		return false;
	}
	return true;
}

u32 get_offset(struct votf_info *vinfo, int c2s_tws, int c2s_trs, int c2a_tws, int c2a_trs)
{
	u32 offset = 0;
	u32 reg_offset = 0;
	u32 service_offset = 0;
	u32 gap = 0;
	int module;
	int module_type;
	int ip, id, service;

	if (!votfdev || !check_vinfo(vinfo))
		return VOTF_INVALID;

	service = vinfo->service;
	ip = search_ip_idx(vinfo);
	id = vinfo->id;

	module = votfdev->votf_table[service][ip][id].module;
	module_type = votfdev->votf_table[service][ip][id].module_type;

	if (module == C2SERV) {
		if (service == TWS) {
			if (c2s_tws == -1)
				return VOTF_INVALID;
			reg_offset = c2serv_regs[c2s_tws].sfr_offset;
			service_offset = votfdev->votf_module_addr[module_type].tws_addr;
			gap = votfdev->votf_module_addr[module_type].tws_gap;

			/* Between m16s16 tws8 and tws9, the gap is not 0x1C but 0x20 */
			if ((module_type == M16S16) && (id >= 9))
				offset += 0x4;
		} else {
			if (c2s_trs == -1)
				return VOTF_INVALID;
			reg_offset = c2serv_regs[c2s_trs].sfr_offset;
			service_offset = votfdev->votf_module_addr[module_type].trs_addr;
			gap = votfdev->votf_module_addr[module_type].trs_gap;

			if (module_type == M16S16) {
				if (id >= 5)
					offset += 0x24;
				if (id >= 10)
					offset += 0x24;
				if (id >= 15)
					offset += 0x24;
			}
		}
	} else {
		if (service == TWS) {
			if (c2a_tws == -1)
				return VOTF_INVALID;
			reg_offset = c2agent_regs[c2a_tws].sfr_offset;
			service_offset = votfdev->votf_module_addr[module_type].tws_addr;
			gap = votfdev->votf_module_addr[module_type].tws_gap;
		} else {
			if (c2a_trs == -1)
				return VOTF_INVALID;
			reg_offset = c2agent_regs[c2a_trs].sfr_offset;
			service_offset = votfdev->votf_module_addr[module_type].trs_addr;
			gap = votfdev->votf_module_addr[module_type].trs_gap;
		}
	}
	offset += service_offset + (id * gap) + reg_offset;

	return offset;
}

void votfitf_init(void)
{
	int ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is NULL\n", __func__);
		return;
	}

	votfdev->ring_request = 0;
	memset(votfdev->ring_pair, VS_DISCONNECTED, sizeof(votfdev->ring_pair));
	memset(votfdev->votf_cfg, 0, sizeof(struct votf_service_cfg));

	for (ip = 0; ip < IP_MAX; ip++) {
		atomic_set(&votfdev->ip_enable_cnt[ip], 0);

		for (id = 0; id < ID_MAX; id++)
			atomic_set(&votfdev->id_enable_cnt[ip][id], 0);
	}
}
EXPORT_SYMBOL_GPL(votfitf_init);

static void votf_init(void)
{
	votfitf_init();

	spin_lock_init(&votfdev->votf_slock);
	memset(votfdev->votf_module_addr, 0, sizeof(struct votf_module_type_addr));
	memset(votfdev->votf_table, 0, sizeof(struct votf_table_info));
}

void votf_sfr_dump(void)
{
	int svc, ip, id;
	int module;
	int module_type;
	u32 base_addr;

	for (svc = 0; svc < SERVICE_CNT; svc++) {
		for (ip = 0; ip < IP_MAX; ip++) {
			for (id = 0; id < ID_MAX; id++) {
				if (!votfdev->votf_table[svc][ip][id].use)
					continue;

				module = votfdev->votf_table[svc][ip][id].module;
				module_type = votfdev->votf_table[svc][ip][id].module_type;
				base_addr = votfdev->votf_table[svc][ip][id].addr;

				pr_info("VOTF SFR dump (base(0x%x), ip(%d), id(%d))\n",
					base_addr, ip, id);
				camerapp_hw_votf_sfr_dump(votfdev->votf_addr[ip], module, module_type);
				break;
			}
		}
	}
}
EXPORT_SYMBOL_GPL(votf_sfr_dump);

int votfitf_create_ring(void)
{
	bool do_create = false;
	bool check_ring = false;
	int svc, ip, id;
	int module;
	u32 local_ip;
	unsigned long flags;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return 0;
	}

	spin_lock_irqsave(&votfdev->votf_slock, flags);
	votfdev->ring_request++;
	pr_info("%s: votf_request(%d)\n", __func__, votfdev->ring_request);

	/* To check the reference count mismatch caused by sudden close */
	for (svc = 0; svc < SERVICE_CNT && !check_ring; svc++) {
		for (ip = 0; ip < IP_MAX && !check_ring; ip++) {
			for (id = 0; id < ID_MAX && !check_ring; id++) {
				if (!votfdev->votf_table[svc][ip][id].use)
					continue;

				module = votfdev->votf_table[svc][ip][id].module;
				if (camerapp_check_votf_ring(votfdev->votf_addr[ip], module))
					check_ring = true;
			}
		}
	}

	if (votfdev->ring_request > 1) {
		if (!check_ring) {
			pr_info("%s: votf reference count is mismatched, do reset\n", __func__);
			votfdev->ring_request = 1;
			memset(votfdev->ring_pair, VS_DISCONNECTED, sizeof(votfdev->ring_pair));
		} else {
			pr_info("%s: votf ring has already been created(%d)\n", __func__, votfdev->ring_request);
			spin_unlock_irqrestore(&votfdev->votf_slock, flags);
			return 0;
		}
	}
	for (svc = 0; svc < SERVICE_CNT; svc++) {
		for (ip = 0; ip < IP_MAX; ip++) {
			for (id = 0; id < ID_MAX; id++) {
				if (!votfdev->votf_table[svc][ip][id].use)
					continue;
				module = votfdev->votf_table[svc][ip][id].module;
				local_ip = votfdev->votf_table[svc][ip][id].ip;
				camerapp_hw_votf_create_ring(votfdev->votf_addr[ip], local_ip, module);
				do_create = true;

				/* support only setA register and immediately set mode */
				camerapp_hw_votf_set_sel_reg(votfdev->votf_addr[ip], 0x1, 0x1);
				break;
			}
		}
	}

	if (!do_create)
		pr_err("%s: invalid request to create votf ring\n", __func__);

	spin_unlock_irqrestore(&votfdev->votf_slock, flags);
	return 1;
}
EXPORT_SYMBOL_GPL(votfitf_create_ring);

int votfitf_create_link(int src_ip, int dst_ip)
{
	struct votf_info src_vinfo, dst_vinfo;
	int src_ip_idx, dst_ip_idx;
	int src_module, dst_module;
	bool src_check, dst_check;
	int id;
	u32 offset;
	unsigned long flags;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return 0;
	}

	src_check = dst_check = false;

	src_vinfo.service = TWS;
	src_vinfo.ip = src_ip;
	src_vinfo.id = 0;

	src_ip_idx = search_ip_idx(&src_vinfo);
	if (!votfdev->votf_table[TWS][src_ip_idx][0].use) {
		pr_err("%s: invalied votf ip 0x%x\n", __func__, src_ip);
		return 0;
	}
	src_module = votfdev->votf_table[TWS][src_ip_idx][0].module;

	dst_vinfo.service = TRS;
	dst_vinfo.ip = dst_ip;
	dst_vinfo.id = 0;

	dst_ip_idx = search_ip_idx(&dst_vinfo);
	if (!votfdev->votf_table[TRS][dst_ip_idx][0].use) {
		pr_err("%s: invalied votf ip 0x%x\n", __func__, dst_ip);
		return 0;
	}
	dst_module = votfdev->votf_table[TRS][dst_ip_idx][0].module;

	spin_lock_irqsave(&votfdev->votf_slock, flags);

	if (!camerapp_check_votf_ring(votfdev->votf_addr[src_ip_idx],
		src_module)) {
		camerapp_hw_votf_create_ring(votfdev->votf_addr[src_ip_idx],
				src_ip, src_module);

		/* support only setA register and immediately set mode */
		camerapp_hw_votf_set_sel_reg(votfdev->votf_addr[src_ip_idx],
				0x1, 0x1);
		pr_info("%s: 0x%x votf is enabled\n", __func__, src_ip);
	} else {
		pr_info("%s: 0x%x votf has already been enabled\n", __func__, src_ip);
		src_check = true;
	}

	atomic_inc(&votfdev->ip_enable_cnt[src_ip_idx]);

	if (!camerapp_check_votf_ring(votfdev->votf_addr[dst_ip_idx],
		dst_module)) {
		camerapp_hw_votf_create_ring(votfdev->votf_addr[dst_ip_idx],
				dst_ip, dst_module);

		/* support only setA register and immediately set mode */
		camerapp_hw_votf_set_sel_reg(votfdev->votf_addr[dst_ip_idx],
				0x1, 0x1);
		pr_info("%s: 0x%x votf is enabled\n", __func__, dst_ip);
	} else {
		pr_info("%s: 0x%x votf has already been enabled\n", __func__, dst_ip);
		dst_check = true;
	}

	atomic_inc(&votfdev->ip_enable_cnt[dst_ip_idx]);

	for (id = 0; id < ID_MAX; id++) {
		src_vinfo.id = dst_vinfo.id = id;
		if (!src_check && votfdev->votf_table[TWS][src_ip_idx][id].use) {
			offset = get_offset(&src_vinfo, C2SERV_R_TWS_ENABLE, -1,
					C2AGENT_R_TWS_ENABLE, -1);
			camerapp_hw_votf_set_enable(votfdev->votf_addr[src_ip_idx],
					offset, 0);
		}

		if (!dst_check && votfdev->votf_table[TRS][dst_ip_idx][id].use) {
			offset = get_offset(&dst_vinfo, -1, C2SERV_R_TRS_ENABLE,
					-1, C2AGENT_R_TRS_ENABLE);
			camerapp_hw_votf_set_enable(votfdev->votf_addr[dst_ip_idx],
					offset, 0);
		}
	}

	spin_unlock_irqrestore(&votfdev->votf_slock, flags);

	return 1;
}
EXPORT_SYMBOL_GPL(votfitf_create_link);

int votfitf_destroy_ring(void)
{
	bool do_destroy = false;
	int svc, ip, id;
	int module;
	u32 local_ip;
	unsigned long flags;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return 0;
	}

	spin_lock_irqsave(&votfdev->votf_slock, flags);

	if (!votfdev->ring_request) {
		pr_info("%s: votf ring has already been destroyed(%d)\n", __func__, votfdev->ring_request);
		spin_unlock_irqrestore(&votfdev->votf_slock, flags);
		return 0;
	}

	votfdev->ring_request--;
	pr_info("%s: votf_request(%d)\n", __func__, votfdev->ring_request);

	if (votfdev->ring_request) {
		pr_info("%s: other IPs are still using the votf ring(%d)\n", __func__, votfdev->ring_request);
		spin_unlock_irqrestore(&votfdev->votf_slock, flags);
		return 0;
	}

	for (svc = 0; svc < SERVICE_CNT; svc++) {
		for (ip = 0; ip < IP_MAX; ip++) {
			for (id = 0; id < ID_MAX; id++) {
				if (!votfdev->votf_table[svc][ip][id].use)
					continue;
				module = votfdev->votf_table[svc][ip][id].module;
				local_ip = votfdev->votf_table[svc][ip][id].ip;
				camerapp_hw_votf_destroy_ring(votfdev->votf_addr[ip], local_ip, module);
				do_destroy = true;
				break;
			}
		}
	}

	if (do_destroy) {
		memset(votfdev->ring_pair, VS_DISCONNECTED, sizeof(votfdev->ring_pair));
		memset(votfdev->votf_cfg, 0, sizeof(struct votf_service_cfg));
	} else {
		pr_err("%s: invalid request to destroy votf ring\n", __func__);
	}

	spin_unlock_irqrestore(&votfdev->votf_slock, flags);
	return 1;
}
EXPORT_SYMBOL_GPL(votfitf_destroy_ring);

int votfitf_destroy_link(int src_ip, int dst_ip)
{
	struct votf_info src_vinfo, dst_vinfo;
	int src_ip_idx, dst_ip_idx;
	int src_module, dst_module;
	int src_cnt, dst_cnt;
	unsigned long flags;
	int id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return 0;
	}

	src_vinfo.service = TWS;
	src_vinfo.ip = src_ip;
	src_vinfo.id = 0;

	src_ip_idx = search_ip_idx(&src_vinfo);
	src_module = votfdev->votf_table[TWS][src_ip_idx][0].module;

	dst_vinfo.service = TRS;
	dst_vinfo.ip = dst_ip;
	dst_vinfo.id = 0;

	dst_ip_idx = search_ip_idx(&dst_vinfo);
	dst_module = votfdev->votf_table[TRS][dst_ip_idx][0].module;

	spin_lock_irqsave(&votfdev->votf_slock, flags);

	src_cnt = atomic_dec_return(&votfdev->ip_enable_cnt[src_ip_idx]);
	if (!src_cnt) {
		camerapp_hw_votf_destroy_ring(votfdev->votf_addr[src_ip_idx],
				src_ip, src_module);
		camerapp_hw_votf_reset(votfdev->votf_addr[src_ip_idx], src_module);
		pr_info("%s: 0x%x votf disable & sw_reset\n", __func__, src_ip);

		for (id = 0; id < ID_MAX; id++)
			atomic_set(&votfdev->id_enable_cnt[src_ip_idx][id], 0);
	} else {
		if (src_cnt > 0) {
			pr_info("%s: 0x%x votf is still in use(%d)\n", __func__,
				src_ip, src_cnt);
		} else {
			pr_err("%s: 0x%x votf has invalid count(%d)\n", __func__,
				src_ip, src_cnt);
			atomic_set(&votfdev->ip_enable_cnt[src_ip_idx], 0);
		}
	}

	dst_cnt = atomic_dec_return(&votfdev->ip_enable_cnt[dst_ip_idx]);
	if (!dst_cnt) {
		camerapp_hw_votf_destroy_ring(votfdev->votf_addr[dst_ip_idx],
				dst_ip, dst_module);
		camerapp_hw_votf_reset(votfdev->votf_addr[dst_ip_idx], dst_module);
		pr_info("%s: 0x%x votf disable & sw_reset\n", __func__, dst_ip);

		for (id = 0; id < ID_MAX; id++)
			atomic_set(&votfdev->id_enable_cnt[dst_ip_idx][id], 0);
	} else {
		if (dst_cnt > 0) {
			pr_info("%s: 0x%x votf is still in use(%d)\n", __func__,
				dst_ip, dst_cnt);
		} else {
			pr_err("%s: 0x%x votf has invalid count(%d)\n", __func__,
				dst_ip, dst_cnt);
			atomic_set(&votfdev->ip_enable_cnt[dst_ip_idx], 0);
		}
	}

	spin_unlock_irqrestore(&votfdev->votf_slock, flags);

	return 1;
}
EXPORT_SYMBOL_GPL(votfitf_destroy_link);

int votfitf_set_service_cfg(struct votf_info *vinfo, struct votf_service_cfg *cfg)
{
	u32 dest;
	u32 offset;
	u32 token_size;
	u32 frame_size;
	int id, ip;
	int service;
	int module;
	u32 change, cnt_enable;
	struct votf_info conn_vinfo;
	int idx_ip, conn_idx_ip;
	unsigned long flags;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return 0;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return 0;
	}

	ip = vinfo->ip;
	id = vinfo->id;
	service = vinfo->service;

	idx_ip = search_ip_idx(vinfo);

	if (!cfg || !votfdev->votf_table[service][idx_ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return 0;
	}

	module = votfdev->votf_table[service][idx_ip][id].module;
	change = cfg->option & VOTF_OPTION_MSK_CHANGE;
	cnt_enable = cfg->option & VOTF_OPTION_MSK_COUNT;

	spin_lock_irqsave(&votfdev->votf_slock, flags);

	if (cnt_enable)
		atomic_inc(&votfdev->id_enable_cnt[idx_ip][id]);

	if (!change && votfdev->ring_pair[service][idx_ip][id] == VS_CONNECTED) {
		pr_info("%s: already connected service(%d, 0x%04X, %d)\n", __func__, service, ip, id);
		spin_unlock_irqrestore(&votfdev->votf_slock, flags);
		return 0;
	}

	conn_vinfo.ip = cfg->connected_ip;
	conn_vinfo.id = cfg->connected_id;
	conn_vinfo.service = !service;

	conn_idx_ip = search_ip_idx(&conn_vinfo);

	if (service == TWS) {
		if (!change && votfdev->ring_pair[TRS][conn_idx_ip][cfg->connected_id] == VS_CONNECTED) {
			pr_info("%s: already connected service(%d, 0x%04X, %d)->(%d, 0x%04X, %d)\n",
					__func__, service, ip, id, TRS, cfg->connected_ip, cfg->connected_id);
			spin_unlock_irqrestore(&votfdev->votf_slock, flags);
			return 0;
		}

		dest = (cfg->connected_ip << 4) | (cfg->connected_id);
		offset = get_offset(vinfo, C2SERV_R_TWS_DEST, -1, C2AGENT_R_TWS_DEST, -1);

		if (offset != VOTF_INVALID) {
			camerapp_hw_votf_set_dest(votfdev->votf_addr[idx_ip], offset, dest);
			votfdev->votf_cfg[service][idx_ip][id].connected_ip = cfg->connected_ip;
			votfdev->votf_cfg[service][idx_ip][id].connected_id = cfg->connected_id;

			votfdev->ring_pair[service][idx_ip][id] = VS_READY;
		}

		if ((votfdev->ring_pair[service][idx_ip][id] == VS_READY) &&
			(votfdev->ring_pair[TRS][conn_idx_ip][cfg->connected_id] == VS_READY)) {
			votfdev->ring_pair[service][idx_ip][id] = VS_CONNECTED;
			votfdev->ring_pair[TRS][conn_idx_ip][cfg->connected_id] = VS_CONNECTED;

			votfdev->votf_cfg[TRS][conn_idx_ip][cfg->connected_id].connected_ip = ip;
			votfdev->votf_cfg[TRS][conn_idx_ip][cfg->connected_id].connected_id = id;
		}
	} else {
		if (!change && votfdev->ring_pair[TWS][conn_idx_ip][cfg->connected_id] == VS_CONNECTED) {
			pr_info("%s: already connected service(%d, 0x%04X, %d)->(%d, 0x%04X, %d)\n",
					__func__, service, ip, id, TWS, cfg->connected_ip, cfg->connected_id);
			spin_unlock_irqrestore(&votfdev->votf_slock, flags);
			return 0;
		}

		votfdev->votf_cfg[service][idx_ip][id].connected_ip = cfg->connected_ip;
		votfdev->votf_cfg[service][idx_ip][id].connected_id = cfg->connected_id;

		votfdev->ring_pair[service][idx_ip][id] = VS_READY;

		if ((votfdev->ring_pair[service][idx_ip][id] == VS_READY) &&
			(votfdev->ring_pair[TWS][conn_idx_ip][cfg->connected_id] == VS_READY)) {
			/* TRS cannot guarantee valid connected ip and id, so it is stable to check valid ip and id */
			if ((votfdev->votf_cfg[TWS][conn_idx_ip][cfg->connected_id].connected_ip == ip) &&
				(votfdev->votf_cfg[TWS][conn_idx_ip][cfg->connected_id].connected_id == id)) {
				votfdev->ring_pair[service][idx_ip][id] = VS_CONNECTED;
				votfdev->ring_pair[TWS][conn_idx_ip][cfg->connected_id] = VS_CONNECTED;
			}
		}
	}

	offset = get_offset(vinfo, C2SERV_R_TWS_ENABLE, C2SERV_R_TRS_ENABLE, C2AGENT_R_TWS_ENABLE,
			C2AGENT_R_TRS_ENABLE);
	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_enable(votfdev->votf_addr[idx_ip], offset, cfg->enable);
		votfdev->votf_cfg[service][idx_ip][id].enable = cfg->enable;
	}

	offset = get_offset(vinfo, C2SERV_R_TWS_LIMIT, C2SERV_R_TRS_LIMIT, C2AGENT_R_TWS_LIMIT, C2AGENT_R_TRS_LIMIT);
	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_limit(votfdev->votf_addr[idx_ip], offset, cfg->limit);
		votfdev->votf_cfg[service][idx_ip][id].limit = cfg->limit;
	}

	token_size = cfg->token_size;
	frame_size = cfg->height;
	if (module == C2AGENT) {
		token_size = cfg->bitwidth * cfg->width * cfg->token_size / 8;
		frame_size = cfg->bitwidth * cfg->width * cfg->height / 8;
	}
	/* set frame size internally */
	if (service == TRS)
		votfitf_set_frame_size(vinfo, frame_size);

	offset = get_offset(vinfo, C2SERV_R_TWS_LINES_IN_TOKEN, C2SERV_R_TRS_LINES_IN_TOKEN, C2AGENT_R_TWS_TOKEN_SIZE,
			C2AGENT_R_TRS_TOKEN_SIZE);
	if (offset != VOTF_INVALID) {
		votfitf_set_token_size(vinfo, token_size);
		if (service == TRS)
			votfitf_set_first_token_size(vinfo, token_size);
		/* save token_size in line unit(not calculated token size) */
		votfdev->votf_cfg[service][idx_ip][id].token_size = cfg->token_size;
	}

	spin_unlock_irqrestore(&votfdev->votf_slock, flags);
	return 1;
}
EXPORT_SYMBOL_GPL(votfitf_set_service_cfg);

int votfitf_set_service_cfg_m_alone(struct votf_info *vinfo, struct votf_service_cfg *cfg)
{
	u32 dest;
	u32 offset;
	int id, ip;
	int service;
	int idx_ip;
	unsigned long flags;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return 0;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return 0;
	}

	ip = vinfo->ip;
	id = vinfo->id;
	service = vinfo->service;
	idx_ip = search_ip_idx(vinfo);

	if (!cfg || !votfdev->votf_table[service][idx_ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return 0;
	}

	spin_lock_irqsave(&votfdev->votf_slock, flags);

	dest = (cfg->connected_ip << 4) | (cfg->connected_id);
	offset = get_offset(vinfo, C2SERV_R_TWS_DEST, -1, C2AGENT_R_TWS_DEST, -1);
	if (offset != VOTF_INVALID)
		camerapp_hw_votf_set_dest(votfdev->votf_addr[idx_ip], offset, dest);

	offset = get_offset(vinfo, C2SERV_R_TWS_ENABLE, C2SERV_R_TRS_ENABLE,
			C2AGENT_R_TWS_ENABLE,C2AGENT_R_TRS_ENABLE);
	if (offset != VOTF_INVALID)
		camerapp_hw_votf_set_enable(votfdev->votf_addr[idx_ip], offset, cfg->enable);

	offset = get_offset(vinfo, C2SERV_R_TWS_LIMIT, C2SERV_R_TRS_LIMIT,
			C2AGENT_R_TWS_LIMIT, C2AGENT_R_TRS_LIMIT);
	if (offset != VOTF_INVALID)
		camerapp_hw_votf_set_limit(votfdev->votf_addr[idx_ip], offset, cfg->limit);

	offset = get_offset(vinfo, C2SERV_R_TWS_LINES_IN_TOKEN, C2SERV_R_TRS_LINES_IN_TOKEN,
			C2AGENT_R_TWS_TOKEN_SIZE, C2AGENT_R_TRS_TOKEN_SIZE);
	if (offset != VOTF_INVALID) {
		votfitf_set_token_size(vinfo, cfg->token_size);
		/* save token_size in line unit(not calculated token size) */
		votfdev->votf_cfg[service][idx_ip][id].token_size = cfg->token_size;
	}

	spin_unlock_irqrestore(&votfdev->votf_slock, flags);
	return 1;
}
EXPORT_SYMBOL_GPL(votfitf_set_service_cfg_m_alone);

int votfitf_set_first_token_size(struct votf_info *vinfo, u32 size)
{
	u32 offset;
	int ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return 0;
	}

	service = vinfo->service;
	ip = search_ip_idx(vinfo);
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	if (service != TRS)
		return ret;

	offset = get_offset(vinfo, -1, C2SERV_R_TRS_LINES_IN_FIRST_TOKEN, -1, C2AGENT_TRS_CROP_FIRST_TOKEN_SIZE);
	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_first_token_size(votfdev->votf_addr[ip], offset, size);
		ret = 1;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(votfitf_set_first_token_size);

/* set token size */
int votfitf_set_token_size(struct votf_info *vinfo, u32 size)
{
	u32 offset;
	int ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = search_ip_idx(vinfo);
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, C2SERV_R_TWS_LINES_IN_TOKEN, C2SERV_R_TRS_LINES_IN_TOKEN, C2AGENT_R_TWS_TOKEN_SIZE,
			C2AGENT_R_TRS_TOKEN_SIZE);

	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_token_size(votfdev->votf_addr[ip], offset, size);
		ret = 1;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(votfitf_set_token_size);

/* set frame size */
int votfitf_set_frame_size(struct votf_info *vinfo, u32 size)
{
	u32 offset;
	int ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return 0;
	}

	service = vinfo->service;
	ip = search_ip_idx(vinfo);
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	if (service != TRS)
		return ret;

	offset = get_offset(vinfo, -1, C2SERV_R_TRS_LINES_COUNT, -1, C2AGENT_TRS_FRAME_SIZE);
	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_frame_size(votfdev->votf_addr[ip], offset, size);
		ret = 1;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(votfitf_set_frame_size);

int votfitf_set_flush(struct votf_info *vinfo)
{
	u32 offset;
	u32 try_cnt = 0;
	int ret = 0;
	int service, ip, id;
	int connected_ip, connected_id;
	struct votf_info conn_vinfo;
	int conn_idx_ip;
	u32 ip_cnt, id_cnt;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = search_ip_idx(vinfo);
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	connected_ip = votfdev->votf_cfg[service][ip][id].connected_ip;
	connected_id = votfdev->votf_cfg[service][ip][id].connected_id;

	conn_vinfo.ip = connected_ip;
	conn_vinfo.id = connected_id;
	conn_vinfo.service = !service;

	conn_idx_ip = search_ip_idx(&conn_vinfo);

	ip_cnt = atomic_read(&votfdev->ip_enable_cnt[ip]);
	id_cnt = atomic_read(&votfdev->id_enable_cnt[ip][id]);
	if (id_cnt > 1) {
		pr_info("%s: 0x%x votf(%d) %d id(%d) is still in use\n",
			__func__, vinfo->ip, ip_cnt, id, id_cnt);
		atomic_dec(&votfdev->id_enable_cnt[ip][id]);

		return ret;
	}

	offset = get_offset(vinfo, C2SERV_R_TWS_FLUSH, C2SERV_R_TRS_FLUSH, C2AGENT_R_TWS_FLUSH, -1);

	if (offset != VOTF_INVALID) {
		if (votfitf_get_busy(vinfo))
			pr_warn("[VOTF] busy before flush\n");

		camerapp_hw_votf_set_flush(votfdev->votf_addr[ip], offset);
		ret = 1;
		while (votfitf_get_busy(vinfo)) {
			if (++try_cnt >= 10000) {
				pr_err("[VOTF] timeout waiting clear busy - flush fail");
				ret = -ETIME;
				break;
			}
			udelay(10);
		}

		atomic_dec(&votfdev->id_enable_cnt[ip][id]);
	}

	if (service == TWS) {
		votfdev->ring_pair[TWS][ip][id] = VS_DISCONNECTED;
		votfdev->ring_pair[TRS][conn_idx_ip][connected_id] = VS_DISCONNECTED;
	} else {
		votfdev->ring_pair[TRS][ip][id] = VS_DISCONNECTED;
		votfdev->ring_pair[TWS][conn_idx_ip][connected_id] = VS_DISCONNECTED;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(votfitf_set_flush);

int votfitf_set_flush_alone(struct votf_info *vinfo)
{
	u32 offset;
	u32 try_cnt = 0;
	int ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = search_ip_idx(vinfo);
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}
	offset = get_offset(vinfo, C2SERV_R_TWS_FLUSH, C2SERV_R_TRS_FLUSH, C2AGENT_R_TWS_FLUSH, -1);

	if (offset != VOTF_INVALID) {
		if (votfitf_get_busy(vinfo))
			pr_warn("[VOTF] busy before flush\n");

		camerapp_hw_votf_set_flush(votfdev->votf_addr[ip], offset);
		ret = 1;
		while (votfitf_get_busy(vinfo)) {
			if (++try_cnt >= 10000) {
				pr_err("[VOTF] timeout waiting clear busy - flush fail");
				ret = -ETIME;
				break;
			}
			udelay(10);
		}
	}

	return ret;
}
EXPORT_SYMBOL_GPL(votfitf_set_flush_alone);

int votfitf_set_crop_start(struct votf_info *vinfo, bool start)
{
	u32 offset;
	int ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = search_ip_idx(vinfo);
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, -1, -1, -1, C2AGENT_TRS_CROP_TOKENS_START);
	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_crop_start(votfdev->votf_addr[ip], offset, start);
		ret = 1;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(votfitf_set_crop_start);

int votfitf_get_crop_start(struct votf_info *vinfo)
{
	u32 offset;
	int ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = search_ip_idx(vinfo);
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, -1, -1, -1, C2AGENT_TRS_CROP_TOKENS_START);
	if (offset != VOTF_INVALID) {
		ret = camerapp_hw_votf_get_crop_start(votfdev->votf_addr[ip], offset);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(votfitf_get_crop_start);

int votfitf_set_crop_enable(struct votf_info *vinfo, bool enable)
{
	u32 offset;
	int ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = search_ip_idx(vinfo);
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, -1, -1, -1, C2AGENT_TRS_CROP_ENABLE);
	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_crop_enable(votfdev->votf_addr[ip], offset, enable);
		ret = 1;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(votfitf_set_crop_enable);

int votfitf_get_crop_enable(struct votf_info *vinfo)
{
	u32 offset;
	int ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = search_ip_idx(vinfo);
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, -1, -1, -1, C2AGENT_TRS_CROP_ENABLE);
	if (offset != VOTF_INVALID) {
		ret = camerapp_hw_votf_get_crop_enable(votfdev->votf_addr[ip], offset);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(votfitf_get_crop_enable);

int votfitf_set_trs_lost_cfg(struct votf_info *vinfo, struct votf_lost_cfg *cfg)
{
	u32 offset;
	u32 val = 0;
	int ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = search_ip_idx(vinfo);
	id = vinfo->id;

	if (!cfg || !votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, -1, C2SERV_R_TRS_CONNECTION_LOST_RECOVER_ENABLE, -1, -1);

	if (cfg->recover_enable)
		val |= (0x1 << 0);
	if (cfg->flush_enable)
		val |= (0x1 << 1);

	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_recover_enable(votfdev->votf_addr[ip], offset, val);
		ret = 1;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(votfitf_set_trs_lost_cfg);

int votfitf_reset(struct votf_info *vinfo, int type)
{
	int ip, id;
	int service;
	int module;
	int connected_ip, connected_id;
	struct votf_info conn_vinfo;
	int conn_idx_ip;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return -ENODEV;
	}

	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return -EINVAL;
	}

	ip = search_ip_idx(vinfo);
	id = vinfo->id;
	service = vinfo->service;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	module = votfdev->votf_table[service][ip][id].module;
	connected_ip = votfdev->votf_cfg[service][ip][id].connected_ip;
	connected_id = votfdev->votf_cfg[service][ip][id].connected_id;

	conn_vinfo.ip = connected_ip;
	conn_vinfo.id = connected_id;
	conn_vinfo.service = !service;

	conn_idx_ip = search_ip_idx(&conn_vinfo);

	if (type == SW_CORE_RESET) {
		/*
		 * Writing to this signal, automatically generate SW core reset pulse
		 * that flushes the DMA and resets all registers but the APB registers
		 */
		camerapp_hw_votf_sw_core_reset(votfdev->votf_addr[ip], module);
		camerapp_hw_votf_sw_core_reset(votfdev->votf_addr[conn_idx_ip], module);
	} else {
		/* Writing to this signal, automatically generates SW reset pulse */
		camerapp_hw_votf_reset(votfdev->votf_addr[ip], module);
		camerapp_hw_votf_reset(votfdev->votf_addr[conn_idx_ip], module);
	}

	if (service == TWS) {
		votfdev->ring_pair[TWS][ip][id] = VS_DISCONNECTED;
		votfdev->ring_pair[TRS][conn_idx_ip][connected_id] = VS_DISCONNECTED;
	} else {
		votfdev->ring_pair[TRS][ip][id] = VS_DISCONNECTED;
		votfdev->ring_pair[TWS][conn_idx_ip][connected_id] = VS_DISCONNECTED;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(votfitf_reset);

int votfitf_set_start(struct votf_info *vinfo)
{
	u32 offset;
	int ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = search_ip_idx(vinfo);
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, -1, -1, C2AGENT_R_TWS_START, C2AGENT_R_TRS_START);
	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_start(votfdev->votf_addr[ip], offset, 0x1);
		ret = 1;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(votfitf_set_start);

int votfitf_set_finish(struct votf_info *vinfo)
{
	u32 offset;
	int ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = search_ip_idx(vinfo);
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, -1, -1, C2AGENT_R_TWS_FINISH, C2AGENT_R_TRS_FINISH);
	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_finish(votfdev->votf_addr[ip], offset, 0x1);
		ret = 1;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(votfitf_set_finish);

int votfitf_set_threshold(struct votf_info *vinfo, bool high, u32 value)
{
	u32 offset;
	int ret = 0;
	int c2a_tws, c2a_trs;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = search_ip_idx(vinfo);
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	if (high) {
		c2a_tws = C2AGENT_R_TWS_HIGH_THRESHOLD;
		c2a_trs = C2AGENT_R_TRS_HIGH_THRESHOLD;
	} else {
		c2a_tws = C2AGENT_R_TWS_LOW_THRESHOLD;
		c2a_trs = C2AGENT_R_TRS_LOW_THRESHOLD;
	}

	offset = get_offset(vinfo, -1, -1, c2a_tws, c2a_trs);
	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_threshold(votfdev->votf_addr[ip], offset, value);
		ret = 1;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(votfitf_set_threshold);

u32 votfitf_get_threshold(struct votf_info *vinfo, bool high)
{
	u32 offset;
	u32 ret = 0;
	int c2a_tws, c2a_trs;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = search_ip_idx(vinfo);
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	if (high) {
		c2a_tws = C2AGENT_R_TWS_HIGH_THRESHOLD;
		c2a_trs = C2AGENT_R_TRS_HIGH_THRESHOLD;
	} else {
		c2a_tws = C2AGENT_R_TWS_LOW_THRESHOLD;
		c2a_trs = C2AGENT_R_TRS_LOW_THRESHOLD;
	}

	offset = get_offset(vinfo, -1, -1, c2a_tws, c2a_trs);
	if (offset != VOTF_INVALID)
		ret = camerapp_hw_votf_get_threshold(votfdev->votf_addr[ip], offset);

	return ret;
}
EXPORT_SYMBOL_GPL(votfitf_get_threshold);

int votfitf_set_read_bytes(struct votf_info *vinfo, u32 bytes)
{
	u32 offset;
	int ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = search_ip_idx(vinfo);
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, -1, -1, -1, C2AGENT_R_TRS_READ_BYTES);
	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_read_bytes(votfdev->votf_addr[ip], offset, bytes);
		ret = 1;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(votfitf_set_read_bytes);

int votfitf_get_fullness(struct votf_info *vinfo)
{
	u32 offset;
	u32 ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = search_ip_idx(vinfo);
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, -1, -1, C2AGENT_R_TWS_FULLNESS, C2AGENT_R_TRS_FULLNESS);
	if (offset != VOTF_INVALID)
		ret = camerapp_hw_votf_get_fullness(votfdev->votf_addr[ip], offset);

	return ret;
}
EXPORT_SYMBOL_GPL(votfitf_get_fullness);

u32 votfitf_get_busy(struct votf_info *vinfo)
{
	u32 offset;
	u32 ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = search_ip_idx(vinfo);
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, C2SERV_R_TWS_BUSY, C2SERV_R_TRS_BUSY, C2AGENT_R_TWS_BUSY, C2AGENT_R_TRS_BUSY);
	if (offset != VOTF_INVALID)
		ret = camerapp_hw_votf_get_busy(votfdev->votf_addr[ip], offset);

	return ret;
}
EXPORT_SYMBOL_GPL(votfitf_get_busy);

int votfitf_set_irq_enable(struct votf_info *vinfo, u32 irq)
{
	u32 offset;
	u32 ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = search_ip_idx(vinfo);
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, -1, -1, C2AGENT_R_TWS_IRQ_ENABLE, C2AGENT_R_TRS_IRQ_ENBALE);
	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_irq_enable(votfdev->votf_addr[ip], offset, irq);
		ret = 1;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(votfitf_set_irq_enable);

int votfitf_set_irq_status(struct votf_info *vinfo, u32 irq)
{
	u32 offset;
	u32 ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = search_ip_idx(vinfo);
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, -1, -1, C2AGENT_R_TWS_IRQ_ENABLE, C2AGENT_R_TRS_IRQ_ENBALE);
	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_irq_status(votfdev->votf_addr[ip], offset, irq);
		ret = 1;
	}

	return ret;

}
EXPORT_SYMBOL_GPL(votfitf_set_irq_status);

int votfitf_set_irq(struct votf_info *vinfo, u32 irq)
{
	u32 offset;
	u32 ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = search_ip_idx(vinfo);
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, -1, -1, C2AGENT_R_TWS_IRQ_ENABLE, C2AGENT_R_TRS_IRQ_ENBALE);
	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_irq(votfdev->votf_addr[ip], offset, irq);
		ret = 1;
	}

	return ret;

}
EXPORT_SYMBOL_GPL(votfitf_set_irq);

int votfitf_set_irq_clear(struct votf_info *vinfo, u32 irq)
{
	u32 offset;
	u32 ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = search_ip_idx(vinfo);
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, -1, -1, C2AGENT_R_TWS_IRQ_CLEAR, C2AGENT_R_TRS_IRQ_CLEAR);
	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_irq_clear(votfdev->votf_addr[ip], offset, irq);
		ret = 1;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(votfitf_set_irq_clear);

void votfitf_disable_service(void)
{
	struct votf_info vinfo;
	int svc, id;
	int ip_idx;
	u32 offset;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return;
	}

	for (ip_idx = 0; ip_idx < IP_MAX; ip_idx++) {
		if (votfdev->votf_table[TWS][ip_idx][0].use)
			svc = TWS;
		else if (votfdev->votf_table[TRS][ip_idx][0].use)
			svc = TRS;
		else
			continue;

		vinfo.ip = votfdev->votf_table[svc][ip_idx][0].ip;
		vinfo.service = svc;

		for (id = 0; id < ID_MAX; id++) {
			vinfo.id = id;

			if(!votfdev->votf_table[svc][ip_idx][id].use)
				continue;

			offset = get_offset(&vinfo, C2SERV_R_TWS_ENABLE,
					C2SERV_R_TRS_ENABLE,
					C2AGENT_R_TWS_ENABLE,
					C2AGENT_R_TRS_ENABLE);

			if (offset != VOTF_INVALID) {
				camerapp_hw_votf_set_enable(
						votfdev->votf_addr[ip_idx],
						offset, 0);
			}
		}
	}

	pr_info("%s complete\n", __func__);
}
EXPORT_SYMBOL_GPL(votfitf_disable_service);

bool votfitf_check_votf_ring(void __iomem *base_addr, int module)
{
	return camerapp_check_votf_ring(base_addr, module);
}
EXPORT_SYMBOL_GPL(votfitf_check_votf_ring);

void votfitf_votf_create_ring(void __iomem *base_addr, int ip, int module)
{
	camerapp_hw_votf_create_ring(base_addr, ip, module);
}
EXPORT_SYMBOL_GPL(votfitf_votf_create_ring);

void votfitf_votf_set_sel_reg(void __iomem *base_addr, u32 set, u32 mode)
{
	camerapp_hw_votf_set_sel_reg(base_addr, set, mode);
}
EXPORT_SYMBOL_GPL(votfitf_votf_set_sel_reg);

u32 votfitf_wrapper_reset(void __iomem *base_addr)
{
	return camerapp_hw_votf_wrapper_reset(base_addr);
}
EXPORT_SYMBOL_GPL(votfitf_wrapper_reset);

int votfitf_check_wait_con(struct votf_info *s_vinfo, struct votf_info *d_vinfo)
{
	int s_id, s_ip, s_service, s_idx_ip;
	int d_id, d_ip, d_service, d_idx_ip;
	int module;
	u32 tws_value, trs_value;
	u32 tws_state, trs_state;
	struct votf_debug_info *debug_info;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return -EINVAL;
	}

	if (!check_vinfo(s_vinfo)) {
		pr_err("%s: invalid src votf_info\n", __func__);
		return -EINVAL;
	}

	if (!check_vinfo(d_vinfo)) {
		pr_err("%s: invalid dst votf_info\n", __func__);
		return -EINVAL;
	}

	s_ip = s_vinfo->ip;
	s_id = s_vinfo->id;
	s_service = s_vinfo->service;
	s_idx_ip = search_ip_idx(s_vinfo);

	if (!votfdev->votf_table[s_service][s_idx_ip][s_id].use) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	module = votfdev->votf_table[s_service][s_idx_ip][s_id].module;

	d_ip = d_vinfo->ip;
	d_id = d_vinfo->id;
	d_service = d_vinfo->service;
	d_idx_ip = search_ip_idx(d_vinfo);

	/* TWS: check wait connection */
	camerapp_hw_votf_get_debug_state(votfdev->votf_addr[s_idx_ip],
			module, s_id, s_service, &tws_value, &tws_state);

	debug_info = &votfdev->votf_table[s_service][s_idx_ip][s_id].debug_info;
	debug_info->time = local_clock();
	debug_info->debug[0] = tws_value;
	debug_info->debug[1] = tws_state;

	/* TRS: check wait connection */
	camerapp_hw_votf_get_debug_state(votfdev->votf_addr[d_idx_ip],
			module, d_id, d_service, &trs_value, &trs_state);

	debug_info = &votfdev->votf_table[d_service][d_idx_ip][d_id].debug_info;
	debug_info->time = local_clock();
	debug_info->debug[0] = trs_value;
	debug_info->debug[1] = trs_state;

	/* generate token */
	if ((tws_state & (GENMASK(3,0))) == VOTF_TWS_WAIT_CON &&
	    (trs_state & (GENMASK(3,0))) == VOTF_TRS_WAIT_CON) {
		pr_err("%s: before debug state(TWS(0x%04X-%d): 0x%X, TRS(0x%04X-%d): 0x%X)\n",
			__func__, s_ip, s_id, tws_state, d_ip, d_id, trs_state);

		/* rejection token */
		camerapp_hw_votf_rejection_token(votfdev->votf_addr[s_idx_ip], module,
			d_ip, d_id, s_ip, s_id);

		/* TWS: check wait connection */
		camerapp_hw_votf_get_debug_state(votfdev->votf_addr[s_idx_ip],
				module, s_id, s_service, &tws_value, &tws_state);

		/* TRS: check wait connection */
		camerapp_hw_votf_get_debug_state(votfdev->votf_addr[d_idx_ip],
				module, d_id, d_service, &trs_value, &trs_state);

		pr_err("%s: after debug state(TWS(0x%04X-%d): 0x%X, TRS(0x%04X-%d): 0x%X)\n",
			__func__, s_ip, s_id, tws_state, d_ip, d_id, trs_state);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(votfitf_check_wait_con);

int votfitf_check_invalid_state(struct votf_info *s_vinfo, struct votf_info *d_vinfo)
{
	int s_id, s_ip, s_service, s_idx_ip;
	int d_id, d_ip, d_service, d_idx_ip;
	int module;
	u32 tws_value, trs_value;
	u32 tws_state, trs_state;
	struct votf_debug_info *debug_info;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return -EINVAL;
	}

	if (!check_vinfo(s_vinfo)) {
		pr_err("%s: invalid src votf_info\n", __func__);
		return -EINVAL;
	}

	if (!check_vinfo(d_vinfo)) {
		pr_err("%s: invalid dst votf_info\n", __func__);
		return -EINVAL;
	}

	s_ip = s_vinfo->ip;
	s_id = s_vinfo->id;
	s_service = s_vinfo->service;
	s_idx_ip = search_ip_idx(s_vinfo);

	if (!votfdev->votf_table[s_service][s_idx_ip][s_id].use) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	module = votfdev->votf_table[s_service][s_idx_ip][s_id].module;

	d_ip = d_vinfo->ip;
	d_id = d_vinfo->id;
	d_service = d_vinfo->service;
	d_idx_ip = search_ip_idx(d_vinfo);

	/* TWS: check wait connection */
	camerapp_hw_votf_get_debug_state(votfdev->votf_addr[s_idx_ip],
			module, s_id, s_service, &tws_value, &tws_state);

	debug_info = &votfdev->votf_table[s_service][s_idx_ip][s_id].debug_info;
	debug_info->time = local_clock();
	debug_info->debug[0] = tws_value;
	debug_info->debug[1] = tws_state;

	/* TRS: check wait connection */
	camerapp_hw_votf_get_debug_state(votfdev->votf_addr[d_idx_ip],
			module, d_id, d_service, &trs_value, &trs_state);

	debug_info = &votfdev->votf_table[d_service][d_idx_ip][d_id].debug_info;
	debug_info->time = local_clock();
	debug_info->debug[0] = trs_value;
	debug_info->debug[1] = trs_state;

	/* s2d for debugging */
	if ((tws_state & (GENMASK(3,0))) == VOTF_WAIT_TOKEN_ACK ||
	    (trs_state & (GENMASK(3,0))) == VOTF_WAIT_RESET_ACK) {
		pr_err("%s: debug state(TWS(0x%04X-%d): 0x%X, TRS(0x%04X-%d): 0x%X)\n",
			__func__, s_ip, s_id, tws_state, d_ip, d_id, trs_state);

		camerapp_debug_s2d(true, "VOTF invalid state");
	}

	/* flush votf if IDLE && busy state */
	if (((tws_state & (GENMASK(3,0))) == VOTF_IDLE) && votfitf_get_busy(s_vinfo)) {
		votfitf_set_flush_alone(s_vinfo);
		pr_warn("%s: flush TWS((0x%04X-%d), invalid state (busy, state:0x%X)\n",
			__func__, s_ip, s_id, tws_state);
	}
	if (((trs_state & (GENMASK(3,0))) == VOTF_IDLE) && votfitf_get_busy(d_vinfo)) {
		votfitf_set_flush_alone(d_vinfo);
		pr_warn("%s: flush TRS((0x%04X-%d), invalid state (busy, state:0x%X)\n",
			__func__, d_ip, d_id, trs_state);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(votfitf_check_invalid_state);

int votf_runtime_resume(struct device *dev)
{
	pr_info("%s: votf_runtime_resume was called\n", __func__);
	return 0;
}

int votf_runtime_suspend(struct device *dev)
{
	pr_info("%s: votf_runtime_suspend was called\n", __func__);
	return 0;
}

int parse_votf_table(struct device_node *np)
{
	int ret = 0;
	int id_num, ip_num = 0;
	u32 local_ip;
	int id_cnt;
	int idx = 0;
	int service;
	struct device_node *table_np;
	struct votf_table_info temp;

	for_each_child_of_node(np, table_np) {
		of_property_read_u32_index(table_np, "info", idx++, &temp.addr);
		of_property_read_u32_index(table_np, "info", idx++, &local_ip);
		of_property_read_u32_index(table_np, "info", idx++, &id_cnt);
		of_property_read_u32_index(table_np, "info", idx++, &temp.module);
		of_property_read_u32_index(table_np, "info", idx++, &temp.service);
		of_property_read_u32_index(table_np, "info", idx++, &temp.module_type);

		for (id_num = 0; id_num < id_cnt; id_num++) {
			service = temp.service;
			votfdev->votf_table[service][ip_num][id_num].use = true;
			votfdev->votf_table[service][ip_num][id_num].addr = temp.addr;
			votfdev->votf_table[service][ip_num][id_num].ip = local_ip;
			votfdev->votf_table[service][ip_num][id_num].id = id_num;
			votfdev->votf_table[service][ip_num][id_num].service = temp.service;
			votfdev->votf_table[service][ip_num][id_num].module = temp.module;
			votfdev->votf_table[service][ip_num][id_num].module_type = temp.module_type;
		}
		idx = 0;
		ip_num++;
	}
	return ret;
}

int parse_votf_service(struct device_node *np)
{
	int idx = 0;
	int module_type;
	struct device_node *table_np;

	struct votf_module_type_addr temp;

	for_each_child_of_node(np, table_np) {
		of_property_read_u32_index(table_np, "info", idx++, &module_type);
		of_property_read_u32_index(table_np, "info", idx++, &temp.tws_addr);
		of_property_read_u32_index(table_np, "info", idx++, &temp.trs_addr);
		of_property_read_u32_index(table_np, "info", idx++, &temp.tws_gap);
		of_property_read_u32_index(table_np, "info", idx++, &temp.trs_gap);

		votfdev->votf_module_addr[module_type].tws_addr = temp.tws_addr;
		votfdev->votf_module_addr[module_type].trs_addr = temp.trs_addr;
		votfdev->votf_module_addr[module_type].tws_gap = temp.tws_gap;
		votfdev->votf_module_addr[module_type].trs_gap = temp.trs_gap;

		pr_info("%s tws_addr(%x), .tws_addr(%x), tws_gap(%x), trs_gat(%x)\n",
			__func__, temp.tws_addr, temp.trs_addr, temp.tws_gap, temp.trs_gap);

		idx = 0;
	}
	return 0;
}

int votf_probe(struct platform_device *pdev)
{
	u32 addr = 0;
	int ip;
	int ret = 1;
	struct device *dev;
	struct device_node *votf_np = NULL;
	struct device_node *service_np = NULL;
	struct device_node *np;

	votfdev = devm_kzalloc(&pdev->dev, sizeof(struct votf_dev), GFP_KERNEL);
	if (!votfdev)
		return -ENOMEM;

	votf_init();
	dev = &pdev->dev;
	votfdev->dev = &pdev->dev;
	np = dev->of_node;

	votf_np = of_get_child_by_name(np, "table0");
	if (votf_np) {
		ret = parse_votf_table(votf_np);
		if (ret) {
			pr_err("%s: parse_votf_table is fail(%d)", __func__, ret);
			ret = 0;
		}
	}

	service_np = of_get_child_by_name(np, "service");
	if (service_np) {
		ret = parse_votf_service(service_np);
		if (ret) {
			pr_err("%s: parse_votf_service is fail(%d)", __func__, ret);
			ret = 0;
		}
	}

	votfdev->use_axi = of_property_read_bool(np, "use_axi");
	dev_info(&pdev->dev, "%s: votf driver is uesd %s interface(%d)\n", __func__,
			votfdev->use_axi ? "AXI-APB" : "ring", votfdev->use_axi);

	if (votfdev->use_axi)
		votfdev->domain = iommu_get_domain_for_dev(dev);

	for (ip = 0; ip < IP_MAX; ip++) {
		if (votfdev->votf_table[TWS][ip][0].use || votfdev->votf_table[TRS][ip][0].use) {
			if (votfdev->votf_table[TWS][ip][0].addr)
				addr = votfdev->votf_table[TWS][ip][0].addr;
			else if (votfdev->votf_table[TRS][ip][0].addr)
				addr = votfdev->votf_table[TRS][ip][0].addr;
			else
				pr_err("%s: Invalid votf address\n", __func__);

			if (votfdev->use_axi) {
				ret = iommu_map(votfdev->domain, addr, addr,
						0x10000, 0);
				if (ret) {
					dev_err(&pdev->dev,
						"%s: iommu_map is fail(%d)",
						__func__, ret);
					return ret;
				}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
				iommu_dma_reserve_iova(dev, addr, 0x10000);
#endif
			}
			votfdev->votf_addr[ip] = ioremap(addr, 0x10000);
		}
	}

	platform_set_drvdata(pdev, votfdev);

	dev_info(&pdev->dev, "Driver probed successfully\n");
	pr_info("%s successfully done.\n", __func__);

	return ret;
}

int votf_remove(struct platform_device *pdev)
{
	u32 addr = 0;
	int ip;

	if (!votfdev) {
		dev_err(&pdev->dev, "no memory for VOTF device\n");
		return -ENOMEM;
	}

	for (ip = 0; ip < IP_MAX; ip++) {
		if (votfdev->votf_table[TWS][ip][0].addr)
			addr = votfdev->votf_table[TWS][ip][0].addr;
		else if (votfdev->votf_table[TRS][ip][0].addr)
			addr = votfdev->votf_table[TRS][ip][0].addr;
		else
			pr_err("%s: Invalid votf address\n", __func__);

		iounmap(votfdev->votf_addr[ip]);

		if (votfdev->use_axi)
			iommu_unmap(votfdev->domain, addr, 0x10000);
	}

	devm_kfree(&pdev->dev, votfdev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

void votf_shutdown(struct platform_device *pdev)
{
}

const struct of_device_id exynos_votf_match[] = {
	{
		.compatible = "samsung,exynos-camerapp-votf",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_votf_match);

struct platform_driver votf_driver = {
	.probe		= votf_probe,
	.remove		= votf_remove,
	.shutdown	= votf_shutdown,
	.driver = {
		.name	= MODULE_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_votf_match),
	}
};

module_platform_driver(votf_driver);

MODULE_AUTHOR("SamsungLSI Camera");
MODULE_DESCRIPTION("EXYNOS CameraPP VOTF driver");
MODULE_LICENSE("GPL");
