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

#ifndef _DISPLAYPORT_TOPOLOGY_H_
#define _DISPLAYPORT_TOPOLOGY_H_

#define MAX_TOPOLOGY_DEPTH 15
#define MAX_VC_CNT 1

#define DPTX_MST_FEC_DSC_DISABLE 0x0000
#define DPTX_MST_FEC_ENABLE 0x0001
#define DPTX_MST_DSC_ENABLE 0x0002

#define CEIL(x)	((x-(u32)(x) > 0 ? (u32)(x+1) : (u32)(x)))

/* Peer device Type def.*/
#define NO_DEVICE_CONNECTED 0x00
#define SST_TX 0x01
#define BRANCH_DEVICE 0x02
#define SST_RX 0x03
#define DP_TO_CONVERTER 0x04
#define DP_TO_WIRELESS 0x05
#define WIRELESS_TO_DP 0x06
#define RSV_TYPE 0x07

#define MAX_TOPOLOGY_POOL 25

struct displaypot_topology_data {
	u8 device_guid[16];
	u8 rad[15];
	u8 rad_second_path[15]; /*Path for Multi path or Loop case*/
	u8 device_type;
	int req_done;
	u32 payload_bandwidth;
	struct displaypot_topology_data *next;
	struct displaypot_topology_data *prev;
};

struct displayport_vc_config {
	u32 port_num;
	u32 timeslot;
	u32 timeslot_start_no;
	u32 pbn;
	u32 xvalue;
	u32 yvalue;
	u32 mode_enable;
};

extern struct displaypot_topology_data *displayport_topology_head;

void displayport_topology_add_rad(u8 *return_RAD, u8 *rad, u8 port_number);
void displayport_topology_add_list(u8 *guid, u8 *rad, u8 device_type);

int displayport_mst_payload_table_delete(u32 ch);
void displayport_topology_make(void);
void displayport_topology_clr_vc(void);
void displayport_topology_set_vc(u32 ch);
void displayport_topology_delete_vc(u32 ch);
#endif
