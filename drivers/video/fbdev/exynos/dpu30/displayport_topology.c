/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung SoC DisplayPort Topology driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "displayport.h"

int topology_pool_index;
struct displaypot_topology_data topology_head[MAX_TOPOLOGY_POOL];
struct displaypot_topology_data *displayport_topology_head;

void displayport_mst_clear_payload_id_table(void)
{
	int i;
	u8 target_RAD[15] = {0,};

	for (i = 1; i < 15; i++)
		target_RAD[i] = 0xFF;

	msg_aux_tx.req_id = CLEAR_PAYLOAD_ID_TABLE;
#if 0
	msg_aux_tx.port_num = 1;

	for (i = 0; i < GUID_SIZE; i++)
		msg_aux_tx.guid[i] = i + 0xA0;

	msg_aux_tx.legacy_dev_plug_status = 1;
	msg_aux_tx.dp_dev_plug_status = 1;
	msg_aux_tx.msg_cap_status = 1;
	msg_aux_tx.input_port = 1;
	msg_aux_tx.peer_dev_type = 5;
	msg_aux_tx.num_sdp_streams = 0;
	msg_aux_tx.vc_payload_id = 1;
	msg_aux_tx.payload_bw_num[0] = 0x04;
	msg_aux_tx.payload_bw_num[1] = 0x17;
#endif
	sb_msg_header.link_cnt_total = 0x1;
	sb_msg_header.link_cnt_remain = 0x0;
	for (i = 0; i < 15; i++)
		sb_msg_header.relative_address[i] = target_RAD[i];
	sb_msg_header.broadcast_msg = 0x0;
	sb_msg_header.path_msg = 0x0;
	sb_msg_header.sb_msg_body_length = 0x02;
	sb_msg_header.start_of_msg_transcation = 0x1;
	sb_msg_header.end_of_msg_transcation = 0x1;
	sb_msg_header.msg_seq_no = 0x0;

	displayport_msg_tx(DOWN_REQ);
	displayport_msg_rx(DOWN_REP);
}

void displayport_mst_vc_payload_table_update_set(void)
{
	u8 val = 0;

	val = VC_PAYLOAD_ID_TABLE_UPDATE;
	displayport_reg_dpcd_write(DPCD_ADD_PAYLOAD_TABLE_UPDATE_STATUS, 1, &val);
}

void displayport_mst_wait_vc_payload_table_update(void)
{
	u32 cnt = 50; /* wait max 1s */
	u8 val = 0;

	do {
		val = 0;
		msleep(20);
		displayport_reg_dpcd_read(DPCD_ADD_PAYLOAD_TABLE_UPDATE_STATUS, 1, &val);
		cnt--;
	} while (((val & VC_PAYLOAD_ID_TABLE_UPDATE) != VC_PAYLOAD_ID_TABLE_UPDATE) && cnt);

	if (!cnt)
		displayport_err("%s is timeout.\n", __func__);
}

static int displayport_mst_payload_table_update(u32 ch,
		struct displayport_device *displayport)
{
	int ret = 0;
	int i = 0;
	u8 val[3] = {0, };

	displayport->sst[ch]->vc_config->timeslot_start_no = 1;

	for (i = 0; i < MAX_VC_CNT; i++) {
		if (ch != i)
			displayport->sst[ch]->vc_config->timeslot_start_no += displayport->sst[i]->vc_config->timeslot;
	}

	displayport_info("VC%d timeslot_start_no = %d, timeslot = %d",
			ch + 1, displayport->sst[ch]->vc_config->timeslot_start_no,
			displayport->sst[ch]->vc_config->timeslot);

	displayport_reg_set_strm_x_y(ch,
			displayport->sst[ch]->vc_config->xvalue,
			displayport->sst[ch]->vc_config->yvalue);

	displyaport_reg_set_vc_payload_id_timeslot(ch,
			displayport->sst[ch]->vc_config->timeslot_start_no,
			displayport->sst[ch]->vc_config->timeslot);

	displayport_mst_vc_payload_table_update_set();

	val[0] = ch + 1;
	val[1] = displayport->sst[ch]->vc_config->timeslot_start_no;
	val[2] = displayport->sst[ch]->vc_config->timeslot;
	ret = displayport_reg_dpcd_write_burst(DPCD_ADD_PAYLOAD_ALLOCATE_SET, 3, val);
	if (ret != 0)
		displayport_err("DPCD_ADD_PAYLOAD_ALLOCATE_SET fail\n");

	displayport_mst_wait_vc_payload_table_update();

	displayport_reg_set_vc_payload_update_flag();

	return ret;
}

int displayport_mst_payload_table_delete(u32 ch)
{
	int ret = 0;
	u8 val[3] = {0, };
	struct displayport_device *displayport = get_displayport_drvdata();

	displayport_mst_vc_payload_table_update_set();

	val[0] = ch + 1;
	val[1] = displayport->sst[ch]->vc_config->timeslot_start_no;
	val[2] = 0;
	ret = displayport_reg_dpcd_write_burst(DPCD_ADD_PAYLOAD_ALLOCATE_SET, 3, val);
	if (ret != 0)
		displayport_err("DPCD_ADD_PAYLOAD_ALLOCATE_SET fail\n");

	displayport_mst_wait_vc_payload_table_update();

	displayport_reg_set_strm_x_y(ch, 0, 0);
	displyaport_reg_set_vc_payload_id_timeslot_delete(ch, displayport);

	displayport_reg_set_vc_payload_update_flag();

	return ret;
}

struct displaypot_topology_data *displayport_topology_malloc(void)
{
	if (topology_pool_index >= MAX_TOPOLOGY_POOL) {
		displayport_err("topology mallc fail\n");
		return NULL;
	}

	return &topology_head[topology_pool_index++];
}

static void displayport_topology_print_tree(void)
{
	struct displaypot_topology_data *head = NULL;
	struct displaypot_topology_data *node = NULL;
	int i = 0;
	int j = 0;
	char *device_type[8] = {"No connection", "Source or SST Branch",
			"MST Branch", "\x1b[32mSST Sink\x1b[0m",
			"DP to Legacy converter", "DP to Wireless converter",
			"Wireless to DP converter", "Reserved"};

	displayport_dbg("Topology List Print\n");

	head = displayport_topology_head;

	if (head == NULL) {
		displayport_err("List head is NULL\n");
		return;
	}

	node = head->next;

	if (node == NULL)
		displayport_err("No registed List\n");

	while (node != NULL) {
		i++;
		displayport_dbg("%d) GUID [", i);

		for (j = 0; j < 16; j++)
			displayport_dbg("0x%02x,", node->device_guid[j]);

		displayport_dbg("] device type : %s", device_type[node->device_type]);
		displayport_dbg(" request done : %d", node->req_done);
		displayport_dbg("\tRAD [");

		for (j = 0; j < 15; j++) {
			if (node->rad[j] != 0xff)
				displayport_dbg("%d.", node->rad[j]);
		}

		if (node->rad_second_path[0] != 0xff) {
			displayport_dbg("]//[");
			for (j = 0; j < 15; j++) {
				if (node->rad_second_path[j] != 0xff)
					displayport_dbg("%d.", node->rad_second_path[j]);
			}
		}

		displayport_dbg("]\n");
		node = node->next;
	}
}

static void displayport_topology_print_rad(u8 *rad)
{
	int i = 0;
	u8 *ptr = rad;

	displayport_info("[");
	for (i = 0; i < 15; i++) {
		if (ptr[i] != 0xFF)
			displayport_info("%d.", ptr[i]);
	}

	displayport_info("]\n");
}

static int displayport_topology_rad_depth_return(u8 *rad)
{
	int rad_depth = 0;
	int i = 0;
	u8 *ptr = rad;

	for (i = 0; i < MAX_TOPOLOGY_DEPTH; i++) {
		if (ptr[i] == 0xFF)
			break;

		rad_depth++;
	}

	return rad_depth;
}

static struct displaypot_topology_data *displayport_topology_rad_search(u8 *rad)
{
	struct displaypot_topology_data *head = NULL;
	struct displaypot_topology_data *node_ptr = NULL;
	int i = 0;

	head = displayport_topology_head;
	node_ptr = displayport_topology_head->next;

	while (node_ptr != NULL) {
		for (i = 0; i < 15; i++) {
			if (node_ptr->rad[i] != rad[i])
				break;
		}

		if (i == 15)
			return node_ptr;

		node_ptr = node_ptr->next;
	}

	return head;
}

static u8 *displayport_topology_rad_return(int port_num)
{
	struct displaypot_topology_data *node_ptr = NULL;
	u8 *rad = NULL;
	int i = 0;
	int j = 0;

	for (i = 0; i < 6; i++) {
		displayport_dbg("topology seq: %d\n", i);
		node_ptr = displayport_topology_head;

		if (node_ptr == NULL) {
			displayport_err("topology head is null\n");
			return NULL;
		}

		for (j = 0; j < i; j++) {
			if (node_ptr->next)
				node_ptr = node_ptr->next;
			else {
				displayport_err("topology node is null\n");
				return NULL;
			}
		}

		rad = node_ptr->rad;

		displayport_dbg("RAD -> ");
		for (j = 0; j < 15; j++) {
			if (rad[j] == 0xFF)
				break;

			displayport_dbg("%d ", rad[j]);
		}
		displayport_dbg("\n");

		if (rad[j - 1] == port_num)
			break;
	}

	return rad;
}

static int displayport_topology_guid_list_check(u8 *guid)
{
	struct displaypot_topology_data *head = NULL;
	struct displaypot_topology_data *node_ptr = NULL;
	/*u8 *ptr = guid;*/

	head = displayport_topology_head;

	if (head->next == NULL) {
		displayport_err("guid_list_check: head->next is null\n");
		return 1;
	}

	node_ptr = head->next;

	while (node_ptr != NULL) {
#if 0
		if (memcmp(node_ptr->Device_GUID, GUID, 16) == 0) {
			printf("Already registed GUID...Dual Path or Loop Path\n");
			return -1;
		}
#endif
		node_ptr = node_ptr->next;
	}

	displayport_info("GUID has to be registed \n");
	return 1;
}

static u8 *displayport_topology_branch_device_search(struct displaypot_topology_data *head)
{
	struct displaypot_topology_data *ptr = NULL;
	u8 rad[15];
	int i = 0;

	displayport_info("Topology branch_device_search\n");

	for (i = 0; i < 15; i++)
		rad[i] = 0xFF; /*	memset(RAD,0xff,Max_Topology_depth);*/

	ptr = head;
#if 0
	while (ptr != NULL) {
		if ((ptr->device_type == Branch_device) && (ptr->req_done == -1)) {
			memcpy(RAD, ptr->RAD, Max_Topology_depth);

			printf("Branch Device detect !! RAD [");
			for (i = 0; i < 15; i++) {
				if (ptr->RAD[i] != 0xff)
					printf("%01d, ", ptr->RAD[i]);
			}

			printf("]\n");
			return ptr->RAD;
		}

		ptr = ptr->next;
	}
#endif
	if ((ptr->device_type == BRANCH_DEVICE) && (ptr->req_done == -1)) {

		for (i = 0; i < 15; i++)
			rad[i] = ptr->rad[i]; /*memcpy(rad, ptr->RAD, Max_Topology_depth);*/

		displayport_info("Branch Device detect!!! RAD[");
		for (i = 0; i < 15; i++) {
			if (ptr->rad[i] != 0xff)
				displayport_info("%d.", ptr->rad[i]);
		}
		displayport_info("]\n");
		return ptr->rad;
	}

	displayport_info("Topology branch_device_search done\n");

	return NULL;
}

static struct displaypot_topology_data *displayport_topology_add_node(u8 *guid,
		u8 *rad, u8 device_type)
{
	struct displaypot_topology_data *head = NULL;
	struct displaypot_topology_data *return_node = NULL;
	struct displaypot_topology_data *ptr = NULL;
	int i = 0;
	u8 init_RAD[15] = {0, };

	for (i = 0; i < 15; i++)
		init_RAD[i] = 0xFF;

	displayport_dbg("add node: RAD[0]=%d, RAD[1]=%d, RAD[2]=%d, device_type=%d\n",
		rad[0], rad[1], rad[2], device_type);

	head = displayport_topology_head;
#if 1
	return_node = displayport_topology_malloc();
#else
	return_node = kzalloc(sizeof(struct displaypot_topology_data), GFP_KERNEL);
#endif
	ptr = head;

	for (i = 0; i < 16; i++)
		return_node->device_guid[i] = guid[i]; /* memcpy(return_node->device_guid, guid, 16); */

	for (i = 0; i < 15; i++)
		return_node->rad[i] = rad[i]; /* memcpy(return_node->rad,rad, 15); */

	for (i = 0; i < 15; i++)
		return_node->rad_second_path[i] = init_RAD[i]; /* memcpy(return_node->rad_second_path, init_RAD, 15); */

	return_node->device_type = device_type;
	return_node->req_done = -1;
	return_node->next = NULL;
	return_node->prev = NULL;

	if (ptr->next != NULL) {
		while (ptr->next != NULL) {
			ptr = ptr->next;
			i++;
		}

		ptr->next = return_node;
		return_node->prev = ptr;
	} else {
		head->next = return_node;
		return_node->prev = head;
	}

	return return_node;
}

void displayport_topology_add_rad(u8 *return_RAD, u8 *rad, u8 port_number)
{
	int length = 0;
	int i = 0;
	u8 *ptr = NULL;

	for (i = 0; i < MAX_TOPOLOGY_DEPTH; i++)
		return_RAD[i] = 0xFF; /* memset(return_RAD, 0xff, MAX_TOPOLOGY_DEPTH); */

	ptr = return_RAD;

	length = displayport_topology_rad_depth_return(rad);

	for (i = 0; i < length; i++)
		*(return_RAD++) = rad[i];

	*(return_RAD++) = port_number;

	displayport_info("add rad: %d, port: %d, depth: %d\n",
			*(return_RAD), port_number, length);

	return_RAD = ptr;
}

static void displayport_topology_manage_init(void)
{
	displayport_info("Topology manage init\n");
#if 1
	displayport_topology_head = displayport_topology_malloc();
#else
	displayport_topology_head = kzalloc(sizeof(struct displaypot_topology_data), GFP_KERNEL);
#endif
	displayport_topology_head->next = NULL;
	displayport_topology_head->prev = NULL;
}

static void displayport_topology_clear(void)
{
	int i, j;

	displayport_info("Topology clear\n");

	for (i = 0; i < MAX_TOPOLOGY_POOL; i++) {
		for (j = 0; j < 16; j++)
			topology_head[i].device_guid[j] = 0;
		for (j = 0; j < 15; j++)
			topology_head[i].rad[j] = 0xFF;
		for (j = 0; j < 15; j++)
			topology_head[i].rad_second_path[j] = 0xFF;
		topology_head[i].device_type = NO_DEVICE_CONNECTED;
		topology_head[i].req_done = 0;
		topology_head[i].payload_bandwidth = 0;
		topology_head[i].next = NULL;
		topology_head[i].prev = NULL;
	}

	topology_pool_index = 0;

	if (displayport_topology_head == NULL) {
		displayport_info("Topology already cleared\n");
		return;
	}

	displayport_topology_head->next = NULL;
	displayport_topology_head->prev = NULL;
	displayport_topology_head = NULL;
}

void displayport_topology_req_done(u8 *rad)
{
	struct displaypot_topology_data *node_ptr = NULL;

	node_ptr = displayport_topology_rad_search(rad);

	displayport_info("topology req done: %d. %d. %d\n", rad[0], rad[1], rad[2]);

	node_ptr->req_done = 1;
}

int displayport_topology_allocate_payload_set(u32 ch,
		struct displayport_device *displayport, u8 *rad)
{
	int i = 0;
	int ret = 0;
	int link_cnt = 0;
	u8 port_number = displayport->sst[ch]->vc_config->port_num;
	u8 number_sdp_streams = 0;
	/* struct displaypot_topology_data *ptr; */
	u32 payload_bandwidth_number = displayport->sst[ch]->vc_config->pbn;
	u8 sdp_stream_sink[1] = {0x0};

	displayport_info("ALLOCATE_PAYLOAD start\n");

	if (rad == NULL) {
		displayport_err("ALLOCATE_PAYLOAD rad is null\n");
		return -1;
	}

	displayport_topology_print_rad(rad);
#if 0
	ptr = displayport_topology_rad_search(rad);

	if (ptr->payload_bandwidth > payload_bandwidth_number) {
		displayport_err("[ERROR] payload_bandwidth_number > device full payload bandwidth\n");
		return -1;
	}
#endif
	for (link_cnt = 0; link_cnt < MAX_TOPOLOGY_DEPTH; link_cnt++) {
		if (rad[link_cnt] == 0xFF)
			break;
	}

	displayport_info("ALLOCATE_PAYLOAD port : %d\n", port_number);

	sdp_stream_sink[0] = ch + 1; /* Virtual Channel starts with 1 */

	msg_aux_tx.req_id = ALLOCATE_PAYLOAD;
	msg_aux_tx.port_num = port_number;
	msg_aux_tx.num_sdp_streams = number_sdp_streams;
	msg_aux_tx.vc_payload_id = ch + 1; /* Virtual Channel starts with 1 */
	msg_aux_tx.payload_bw_num[0] = (payload_bandwidth_number & 0xFF00) >> 8;
	msg_aux_tx.payload_bw_num[1] = payload_bandwidth_number & 0xFF;

	for (i = 0; i < MAX_MST_DEV; i++)
		msg_aux_tx.sdp_stream_sink[i] = 0;

	sb_msg_header.link_cnt_total = link_cnt - 1;
	sb_msg_header.link_cnt_remain = link_cnt - 2;

	for (i = 0; i < 15; i++)
		sb_msg_header.relative_address[i] = rad[i];

	sb_msg_header.broadcast_msg = 0x0;
	sb_msg_header.path_msg = 0x1;
	sb_msg_header.sb_msg_body_length = 0x06;
	sb_msg_header.start_of_msg_transcation = 0x1;
	sb_msg_header.end_of_msg_transcation = 0x1;
	sb_msg_header.msg_seq_no = 0x0;

	displayport_msg_tx(DOWN_REQ);
	displayport_msg_rx(DOWN_REP);

	displayport_info("ALLOCATE_PAYLOAD end\n");

	return ret;
}

static int displayport_topology_link_address(u8 *rad, int msn)
{
	int i = 0;
	int ret = 0;
	int link_count = 0;

	displayport_info("LINK_ADDRESS start\n");
	displayport_topology_print_rad(rad);
	/* Buffer_config(); */

	link_count = displayport_topology_rad_depth_return(rad);

	displayport_info("LINK_ADDRESS - link_count: %d\n", link_count);

	if (link_count == 1)
		displayport_topology_manage_init();

	msg_aux_tx.req_id = LINK_ADDRESS;
#if 0
	msg_aux_tx.port_num = 1;

	for (i = 0; i < GUID_SIZE; i++)
		msg_aux_tx.guid[i] = i + 0xA0;

	msg_aux_tx.legacy_dev_plug_status = 1;
	msg_aux_tx.dp_dev_plug_status = 1;
	msg_aux_tx.msg_cap_status = 1;
	msg_aux_tx.input_port = 1;
	msg_aux_tx.peer_dev_type = 5;
	msg_aux_tx.num_sdp_streams = 0;
	msg_aux_tx.vc_payload_id = 1;
	msg_aux_tx.payload_bw_num[0] = 0x04;
	msg_aux_tx.payload_bw_num[1] = 0x17;

	for (i = 0; i < 15; i++)
		msg_aux_tx.sdp_stream_sink[i] = 0x0F - i;

	msg_aux_tx.available_pbn[0] = 0x56;
	msg_aux_tx.available_pbn[1] = 0x78;
#endif
	sb_msg_header.link_cnt_total = link_count;
	sb_msg_header.link_cnt_remain = link_count - 1;
	for (i = 0; i < 15; i++)
		sb_msg_header.relative_address[i] = rad[i];
	sb_msg_header.broadcast_msg = 0x0;
	sb_msg_header.path_msg = 0x0;
	sb_msg_header.sb_msg_body_length = 0x02;
	sb_msg_header.start_of_msg_transcation = 0x1;
	sb_msg_header.end_of_msg_transcation = 0x1;
	sb_msg_header.msg_seq_no = msn;
	/*sb_msg_header.msg_seq_no = 0x0;*/

	displayport_msg_tx(DOWN_REQ);
	displayport_msg_rx(DOWN_REP);

	displayport_topology_req_done(sb_msg_header.relative_address);

	displayport_info("LINK_ADDRESS end\n");

	return ret;
}

void displayport_topology_add_list(u8 *guid, u8 *rad, u8 device_type)
{
	struct displaypot_topology_data *head = displayport_topology_head;
	int depth_1 = 0;
	//int depth_2 = 0;
	int guid_list_check = 0;
	//struct displaypot_topology_data *topology_index;
	struct displaypot_topology_data *topology_node;
	struct displaypot_topology_data *pre_node_index;
	struct displaypot_topology_data *added_node_index;

	u8 *ptr = NULL;

	u8 pre_node_rad[15];
	u8 added_port = 0;
	u8 *rad_ptr = NULL;
	int i = 0;
	u8 return_rad[15];

	for (i = 0; i < 15; i++)
		pre_node_rad[i] = 0xFF; /* memset(Pre_node_RAD, 0xff, 15); */

	rad_ptr = rad;
	ptr = rad;
	topology_node = head;

	depth_1 = displayport_topology_rad_depth_return(ptr);

	for (i = 0; i < depth_1 - 1; i++)
		pre_node_rad[i] = rad_ptr[i];

	added_port = rad_ptr[depth_1 - 1];

	displayport_info("add list: RAD[0]=%d, RAD[1]=%d, RAD[2]=%d, port=%d, depth=%d, device_type=%d\n",
			rad[0], rad[1], rad[2], depth_1, added_port, device_type);

	pre_node_index = displayport_topology_rad_search(pre_node_rad);

	guid_list_check = displayport_topology_guid_list_check(guid);

	if (guid_list_check == 1) {
		added_node_index = displayport_topology_add_node(guid, rad, device_type);
		if ((pre_node_index != topology_node) && (pre_node_index->rad_second_path[0] != 0xFF)) { /* Pre node is not Header and Second path has... */
			displayport_topology_add_rad(return_rad, pre_node_index->rad_second_path, added_port);

			for (i = 0; i < 15; i++)
				added_node_index->rad_second_path[i] = return_rad[i];

			/* memcpy(Added_node_index->rad_second_path,Add_RAD(Pre_node_index->rad_second_path, Added_port), 15); */
		}
	} else {
#if 0
		topology_index = GUID_search(topology_node, GUID);
		depth_2 = RAD_Depth_return(topology_index->RAD);
		if (depth_2 > depth_1) { /* Assign Short Path to RAD 1 */
			printf("R.A.D Update to short path!!\n");
			memcpy(topology_index->RAD_Second_path, topology_index->RAD, 15);
			memcpy(topology_index->RAD, ptr, 15);
		} else if (memcmp(topology_index->RAD, ptr, 15) != 0) {
			printf("R.A.D Update to Second path!!\n");
			/* dump("GUID", topology_index->Device_GUID, 16); */
			memcpy(topology_index->RAD_Second_path, ptr, 15);
			/* dump("1) RAD", topology_index->RAD, 15); */
			/* dump("2) RAD", topology_index->RAD_Second_path, 15); */
		}
#endif
	}
}

void displayport_topology_make(void)
{
	int i = 0;
	u8 init_RAD[15] = {0, };
	int msn = 1;
	struct displaypot_topology_data *ptr = NULL;
	u8 *rad = NULL;
	u8 *branch_rad = NULL;

	displayport_topology_clear();

	for (i = 1; i < 15; i++)
		init_RAD[i] = 0xFF;

	displayport_info("Topology make\n");

	displayport_topology_link_address(init_RAD, msn);

	ptr = displayport_topology_head->next;

	while (ptr != NULL) {
		rad = ptr->rad;
		branch_rad = displayport_topology_branch_device_search(ptr);

		if (branch_rad != NULL) {
			msn = 0;
			displayport_topology_link_address(rad, msn);
		}

		ptr = ptr->next;
	}

	displayport_topology_print_tree();
}

void displayport_topology_clr_vc(void)
{
	int i = 0;
	u8 val[3] = {0, };
	struct displayport_device *displayport = get_displayport_drvdata();

	for (i = 0; i < MAX_VC_CNT; i++)
		displayport_clr_vc_config(i, displayport);

	displayport_mst_vc_payload_table_update_set();
	val[0] = 0x00;
	val[1] = 0x00;
	val[2] = 0x3F;
	displayport_reg_dpcd_write_burst(DPCD_ADD_PAYLOAD_ALLOCATE_SET, 3, val);
	displayport_mst_wait_vc_payload_table_update();

	displayport_mst_clear_payload_id_table();
}

void displayport_topology_set_vc(u32 ch)
{
	u8 *target_rad;
	struct displayport_device *displayport = get_displayport_drvdata();

	displayport_calc_vc_config(ch, displayport);
	displayport_mst_payload_table_update(ch, displayport);

	target_rad = displayport_topology_rad_return(displayport->sst[ch]->vc_config->port_num);

	if (target_rad != NULL && displayport->sst[ch]->vc_config->port_num != 0)
		displayport_topology_allocate_payload_set(ch, displayport, target_rad);
	else
		displayport_err("can't find target rad");
}

void displayport_topology_delete_vc(u32 ch)
{
	u8 *target_rad;
	struct displayport_device *displayport = get_displayport_drvdata();

	displayport_info("displayport_topology_delete_vc\n");

	displayport->sst[ch]->vc_config->pbn = 0;
	displayport->sst[ch]->vc_config->xvalue = 0;
	displayport->sst[ch]->vc_config->yvalue = 0;
	displayport->sst[ch]->vc_config->mode_enable = 0;

	target_rad = displayport_topology_rad_return(displayport->sst[ch]->vc_config->port_num);

	if (target_rad != NULL && displayport->sst[ch]->vc_config->port_num != 0)
		displayport_topology_allocate_payload_set(ch, displayport, target_rad);
	else
		displayport_err("can't find target rad");
}
