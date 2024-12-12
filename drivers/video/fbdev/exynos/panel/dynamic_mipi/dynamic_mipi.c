#include "../panel.h"
#include "../panel_drv.h"
#include "../panel_debug.h"
#include "dynamic_mipi.h"

#include <linux/dev_ril_bridge.h>

#define FFC_NODE_NAME "dynamic_mipi_ffc"


static struct dm_band_info *search_dynamic_freq_idx(struct panel_device *panel, int band_idx, int freq)
{
	int i, ret = 0;
	int min, max, array_idx;
	struct dm_total_band_info *total_band;
	struct dm_band_info *band_info = NULL;
	struct dynamic_mipi_info *dm;

	dm = &panel->dynamic_mipi;

	if (band_idx >= FREQ_RANGE_MAX) {
		panel_err("exceed max band idx : %d\n", band_idx);
		ret = -1;
		goto search_exit;
	}

	total_band = &dm->total_band[band_idx];
	if (total_band == NULL) {
		panel_err("failed to find band_idx : %d\n", band_idx);
		ret = -1;
		goto search_exit;
	}

	array_idx = total_band->size;
	if (array_idx == 1) {
		band_info = &total_band->array[0];
		panel_info("Found adap_freq idx(0): %d, osc: %d\n",
				band_info->freq_idx, band_info->ddi_osc);
		return band_info;
	}

	for (i = 0; i < array_idx; i++) {
		band_info = &total_band->array[i];
		panel_info("min : %d, max : %d\n", band_info->min, band_info->max);

		min = (int)freq - band_info->min;
		max = (int)freq - band_info->max;

		if ((min >= 0) && (max <= 0)) {
			panel_info("Found adap_freq idx: %d, osc: %d\n",
					band_info->freq_idx, band_info->ddi_osc);

			return band_info;
		}
	}

	if (i >= array_idx) {
		panel_err("can't found freq idx\n");
		band_info = NULL;
		goto search_exit;
	}

search_exit:
	return band_info;
}


int update_dynamic_mipi(struct panel_device *panel, int idx)
{
	struct dynamic_mipi_info *dm;
	struct dm_dt_info *dm_dt;
	struct dm_hs_info *dm_hs;
	struct dm_status_info *dm_status;

	dm = &panel->dynamic_mipi;
	dm_dt = &dm->dm_dt;
	dm_status = &dm->dm_status;

	if ((idx < 0) || (idx >= dm_dt->dm_hs_list_cnt)) {
		panel_err("invalid idx : %d\n", idx);
		return -EINVAL;
	}

	dm_hs = &dm_dt->dm_hs_list[idx];
	panel_info("MDC:DM: Idx: %d HS : %d\n", idx, dm_hs->hs_clk);

	dm_status->request_df = idx;
	dm_status->req_ctx = DM_REQ_CTX_RIL;

	return 0;
}



static int dm_ril_notifier(struct notifier_block *self, unsigned long size, void *buf)
{
	struct panel_device *panel;
	struct dev_ril_bridge_msg *msg;
	struct ril_noti_info *ch_info;
	struct dm_status_info *dm_status;
	struct dm_band_info *band_info;
	struct dynamic_mipi_info *dm;

	dm = container_of(self, struct dynamic_mipi_info, dm_noti);
	if (dm == NULL) {
		panel_err("MCD:DM:dm is null\n");
		goto exit_notifier;
	}

	panel = container_of(dm, struct panel_device, dynamic_mipi);
	if (panel == NULL) {
		panel_err("MCD:DM:panel is null\n");
		goto exit_notifier;
	}

	dm_status = &dm->dm_status;
	if (dm_status == NULL) {
		panel_err("MCD:DM:dymanic status is null\n");
		goto exit_notifier;
	};

	if (!dm->enabled) {
		panel_err("MCD:DM:dynamic mipi is disabled\n");
		goto exit_notifier;
	}

	msg = (struct dev_ril_bridge_msg *)buf;
	if (msg == NULL) {
		panel_err("MCD:DM:msg is null\n");
		goto exit_notifier;
	}

	if (msg->dev_id == IPC_SYSTEM_CP_CHANNEL_INFO &&
		msg->data_len == sizeof(struct ril_noti_info)) {
		ch_info = (struct ril_noti_info *)msg->data;
		if (ch_info == NULL) {
			panel_err("MCD:DM:ch_info is null\n");
			goto exit_notifier;
		}

		panel_info("MCD:DM:(band:%d, channel:%d)\n",
				ch_info->band, ch_info->channel);

		band_info = search_dynamic_freq_idx(panel, ch_info->band, ch_info->channel);
		if (band_info == NULL) {
			panel_info("MCD:DM:failed to search freq idx\n");
			goto exit_notifier;
		}

		if (band_info->ddi_osc != dm_status->current_ddi_osc) {
			panel_info("MCD:DM:ddi osc was changed %d -> %d\n",
				dm_status->current_ddi_osc, band_info->ddi_osc);
			dm_status->request_ddi_osc = band_info->ddi_osc;
		}

		if (band_info->freq_idx != dm_status->current_df)
			update_dynamic_mipi(panel, band_info->freq_idx);

	}
exit_notifier:
	return 0;
}

static int init_dynamic_mipi(struct panel_device *panel)
{
	int ret = 0;
	struct dm_status_info *dm_status;
	struct dynamic_mipi_info *dm;

	dm = &panel->dynamic_mipi;

	dm_status = &dm->dm_status;
	if (dm_status == NULL) {
		panel_err("MCD:DM:dynamic status is null\n");
		ret = -EINVAL;
		goto init_exit;
	}

	dm->enabled = 1;
	dm_status->req_ctx = DM_REQ_CTX_PWR_ON;

	dm_status->request_df = dm->dm_dt.dm_default;
	dm_status->current_df = dm->dm_dt.dm_default;
	dm_status->target_df = dm->dm_dt.dm_default;
	dm_status->ffc_df = dm->dm_dt.dm_default;

init_exit:
	return ret;
}

static int parse_dynamic_mipi_freq(struct device_node *node, struct dynamic_mipi_info *dm)
{
	int ret = 0;
	int i, cnt;
	struct dm_hs_info *dm_hs;
	struct dm_dt_info *dm_dt;
	struct device_node *freq_node;
	unsigned int dft_hs_clk = 0;

	if (!dm) {
		panel_err("MCD:DM:dm is null\n");
		goto exit_parse;
	}

	dm_dt = &dm->dm_dt;
	cnt = of_property_count_u32_elems(node, "dynamic_mipi_tbl");
	if (cnt  <= 0) {
		panel_warn("MCD:DM:can't found dynamic freq info\n");
		return -EINVAL;
	}

	panel_info("MCD:DM:MIPI FREQ CNT : %d\n", cnt);
	if (cnt > MAX_DYNAMIC_FREQ) {
		panel_info("MCD:DM:freq cnt exceed max freq num (%d:%d)\n",
				cnt, MAX_DYNAMIC_FREQ);
		cnt = MAX_DYNAMIC_FREQ;
	}

	dm_dt->dm_hs_list_cnt = cnt;
	of_property_read_u32(node, "timing,dsi-hs-clk", &dft_hs_clk);
	panel_info("MCD:DM:Default hs clock(%d)\n", dft_hs_clk);

	for (i = 0; i < cnt; i++) {
		dm_hs = &dm_dt->dm_hs_list[i];
		freq_node = of_parse_phandle(node, "dynamic_mipi_tbl", i);

		of_property_read_u32(freq_node, "hs-clk", &dm_hs->hs_clk);
		if (dft_hs_clk == dm_hs->hs_clk) {
			dm_dt->dm_default = i;
			panel_info("MCD DM:Default Freq idx  : %d, hs : %d\n",
					dm_dt->dm_default, dm_hs->hs_clk);
		}

		of_property_read_u32_array(freq_node, "pmsk", (u32 *)&dm_hs->dphy_pms,
			sizeof(struct stdphy_pms)/sizeof(unsigned int));

		panel_info("MCD:DM:%d:HS Freq: %d: P:%d:0x%x, M:%d:0x%x, S:%d:0x%x, K:%d:0x%x\n",
			i, dm_hs->hs_clk, dm_hs->dphy_pms.p, dm_hs->dphy_pms.p,
			dm_hs->dphy_pms.m, dm_hs->dphy_pms.m, dm_hs->dphy_pms.s, dm_hs->dphy_pms.s,
			dm_hs->dphy_pms.k, dm_hs->dphy_pms.k);
	}
exit_parse:
	return ret;

}


static int parse_ffc_cmd(struct device_node *node, char *node_name, struct dm_hs_info *dm_hs, int ffc_idx, int osc_idx)
{
	int i;
	int hs_clk, osc_clk;
	int ffc_cmd_cnt;
	struct device_node *ffc_node;

	ffc_node = of_parse_phandle(node, node_name, ffc_idx);

	of_property_read_u32(ffc_node, "hs-clk", &hs_clk);
	of_property_read_u32(ffc_node, "osc", &osc_clk);
	panel_info("mipi hs: %d, ffc hs: %d, osc: %d\n", dm_hs->hs_clk, hs_clk, osc_clk);

	if (hs_clk != dm_hs->hs_clk) {
		panel_err("MCD:DM:Wrong FFC Command MIPI-HS:%d, FFC-HS:%d\n",
			hs_clk, dm_hs->hs_clk);
		goto exit_parse;
	}

	ffc_cmd_cnt = of_property_count_u8_elems(ffc_node, "command");
	if (ffc_cmd_cnt <= 0) {
		panel_err("MCD:DM:Wrong ffc cmd cnt: %d\n", ffc_cmd_cnt);
		goto exit_parse;
	}

	dm_hs->ffc_cmd_sz = ffc_cmd_cnt;
	if (ffc_cmd_cnt > MAX_PANEL_FFC_CMD) {
		panel_err("MCD:DM:Exceed ffc command count: %d:%d\n", ffc_cmd_cnt, MAX_PANEL_FFC_CMD);
		goto exit_parse;
	}
	of_property_read_u8_array(ffc_node, "command", dm_hs->ffc_cmd[osc_idx], ffc_cmd_cnt);

	for (i = 0; i < ffc_cmd_cnt; i++)
		panel_info("cmd: %x\n", dm_hs->ffc_cmd[osc_idx][i]);

exit_parse:
	return 0;
}


static int parse_panel_ffc_freq(struct device_node *node, struct dynamic_mipi_info *dm)
{
	char node_name[32];
	struct dm_dt_info *dm_dt;
	int i, j, cnt[MAX_OSC_CNT];

	if (!dm) {
		panel_err("MCD:DM:dm is null\n");
		goto exit_parse;
	}
	dm_dt = &dm->dm_dt;

	panel_info("%s cnt: %d\n", __func__, of_property_count_u32_elems(node, "dynamic_mipi_ffc0"));

	for (i = 0; i < MAX_OSC_CNT; i++) {
		sprintf(node_name, "%s%d", FFC_NODE_NAME, i);
		cnt[i] = of_property_count_u32_elems(node, node_name);
		panel_info("%s cnt[%d]: %d\n", node_name, i, cnt[i]);
		if (cnt[i]  <= 0) {
			if (i == 0) {
				panel_err("MCD:DM:can't found mandatory ffc command info\n");
				goto exit_parse;
			}
			panel_warn("MCD:DM:can't found %s\n", node_name);
			continue;
		}

		if (cnt[i] != dm_dt->dm_hs_list_cnt) {
			panel_err("MCD:DM:Wrong ffc command count, mipi hs freq count:%d, ffc freq count: %d\n",
			dm_dt->dm_hs_list_cnt, cnt[i]);
			goto exit_parse;
		}

		for (j = 0; j < cnt[i]; j++)
			parse_ffc_cmd(node, node_name, &dm_dt->dm_hs_list[j], j, i);

	}

exit_parse:
	return 0;
}

static int parse_panel_ffc_off(struct device_node *node, struct dynamic_mipi_info *dm)
{
	int hs_clk;
	struct dm_dt_info *dm_dt;
	struct device_node *ffc_node;

	if (!dm) {
		panel_err("MCD:DM:dm is null\n");
		goto exit_parse;
	}
	dm_dt = &dm->dm_dt;

	ffc_node = of_parse_phandle(node, "dynamic_mipi_ffc_off", 0);
	if (ffc_node == NULL) {
		panel_info("MCD:DM:does not support ffc off\n");
		dm_dt->ffc_off_cmd_sz = 0;
		goto exit_parse;
	}

	of_property_read_u32(ffc_node, "hs-clk", &hs_clk);
	panel_info("ffc off hs : %d\n", hs_clk);

	dm_dt->ffc_off_cmd_sz = of_property_count_u8_elems(ffc_node, "command");
	if (dm_dt->ffc_off_cmd_sz > MAX_PANEL_FFC_CMD) {
		panel_err("MCD:DM:Exceed ffc command count: %d:%d\n", dm_dt->ffc_off_cmd_sz, MAX_PANEL_FFC_CMD);
		goto exit_parse;
	}
	of_property_read_u8_array(ffc_node, "command", dm_dt->ffc_off_cmd, dm_dt->ffc_off_cmd_sz);

exit_parse:
	return 0;
}

static int parse_skip_frame_count(struct device_node *node, struct dynamic_mipi_info *dm)
{
	u32 skip_cnt = 0;
	struct dm_status_info *dm_status = NULL;

	if (!dm) {
		panel_err("MCD:DM:dm is null\n");
		goto exit_parse;
	}

	dm_status = &dm->dm_status;
	dm_status->skip_frame_cnt = 0;
	atomic_set(&dm_status->frame_cnt, 0);

	if (of_property_read_u32(node, "dynamic_mipi_skip_frame", &skip_cnt)) {
		panel_err("MCD:DM: skip frame count was not defined, set to 0\n");
		goto exit_parse;
	}

	dm_status->skip_frame_cnt = skip_cnt;
	atomic_set(&dm_status->frame_cnt, skip_cnt);
	panel_info("MCD:DM: skip frame count : %d\n", skip_cnt);

exit_parse:
	return 0;
}



static int parse_dynamic_mipi_dt(struct panel_device *panel)
{
	int ret = 0;
	struct device *dev;
	struct device_node *node;
	struct dynamic_mipi_info *dm = &panel->dynamic_mipi;

	if (!panel) {
		panel_err("MCD:DM:Panel is null\n");
		goto exit_parse;
	}

	dev = panel->dev;
	node = panel->ddi_node;
	if (node == NULL) {
		panel_err("MCD:DM:ddi node is NULL\n");
		node = of_parse_phandle(dev->of_node, "ddi_info", 0);
	}

	ret = parse_skip_frame_count(node, dm);
	if (ret) {
		panel_err("MCD:DM:failed to parse skip frame count\n");
		goto exit_parse;
	}

	ret = parse_dynamic_mipi_freq(node, dm);
	if (ret) {
		panel_err("MCD:DM:failed to get mipi freq table\n");
		goto exit_parse;
	}

	ret = parse_panel_ffc_freq(node, dm);
	if (ret) {
		panel_err("MCD:DM:failed to get ffc freq command table\n");
		goto exit_parse;
	}

	ret = parse_panel_ffc_off(node, dm);
	if (ret) {
		panel_err("MCD:DM:failed to get ffc off command\n");
		goto exit_parse;
	}



exit_parse:
	return ret;
}


int panel_dm_set_ffc(struct panel_device *panel)
{
	int ret = 0;
	struct panel_state *state = &panel->state;

	if (state->connect_panel == PANEL_DISCONNECT) {
		panel_warn("panel no use\n");
		return 0;
	}

	if (state->cur_state == PANEL_STATE_OFF ||
		state->cur_state == PANEL_STATE_ON || !IS_PANEL_ACTIVE(panel))
		return 0;

	mutex_lock(&panel->op_lock);

	panel_info("called\n");

	ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_DM_SET_FFC_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to set PANEL_FFC_SEQ\n");
		goto exit_set;
	}

exit_set:
	mutex_unlock(&panel->op_lock);
	return ret;
}

int panel_dm_off_ffc(struct panel_device *panel)
{
	int ret = 0;
	struct panel_state *state = &panel->state;
	struct dynamic_mipi_info *dm;

	if (state->connect_panel == PANEL_DISCONNECT) {
		panel_warn("panel no use\n");
		return 0;
	}

	if (state->cur_state == PANEL_STATE_OFF ||
		state->cur_state == PANEL_STATE_ON || !IS_PANEL_ACTIVE(panel))
		return 0;

	mutex_lock(&panel->op_lock);

	dm = &panel->dynamic_mipi;
	if (dm->dm_dt.ffc_off_cmd_sz < 1) {
		panel_warn("ffc_off cmd size is 0\n");
		goto exit_off;
	}

	panel_info("called\n");
	ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_DM_OFF_FFC_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to set PANEL_FFC_SEQ\n");
		goto exit_off;
	}

exit_off:
	mutex_unlock(&panel->op_lock);
	return ret;

}



int dynamic_mipi_probe(struct panel_device *panel, struct dm_total_band_info *total_band)
{
	int ret = 0;
	struct dynamic_mipi_info *dm;

	if ((!panel) || (!total_band)) {
		panel_err("Wrong Param\n");
		goto exit_probe;
	}

	ret = parse_dynamic_mipi_dt(panel);
	if (ret) {
		panel_err("failed to parse df\n");
		goto exit_probe;
	}

	dm = &panel->dynamic_mipi;
	dm->total_band = total_band;

	init_dynamic_mipi(panel);

	dm->dm_noti.notifier_call = dm_ril_notifier;
	register_dev_ril_bridge_event_notifier(&dm->dm_noti);

	panel_info("MCD:DM:probe done\n");

exit_probe:
	return ret;
}
