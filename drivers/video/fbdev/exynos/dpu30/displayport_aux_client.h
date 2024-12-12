/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung SoC DisplayPort Messaging AUX Client driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _DISPLAYPORT_AUX_CLIENT_H_
#define _DISPLAYPORT_AUX_CLIENT_H_

extern struct displayport_msg_aux_tx msg_aux_tx;
extern struct displayport_sb_msg_header sb_msg_header;

#define MAX_MST_DEV 15

/* Message Transaction Layer Protocol Start */
#define GET_MESSAGE_TRANSACTION_VERSION 0x00
#define LINK_ADDRESS 0x01
#define CONNECTION_STATUS_NOTIFY 0x02
#define ENUM_PATH_RESOURCES 0x10
#define ALLOCATE_PAYLOAD 0x11
#define QUERY_PAYLOAD 0x12
#define RESOURCE_STATUS_NOTIFY 0x13
#define CLEAR_PAYLOAD_ID_TABLE 0x14
#define REMOTE_DPCD_WRITE 0x21
#define REMOTE_I2C_READ 0x22

#define MAX_MSG_REQ_SIZE 20
#define MAX_MSG_REP_SIZE 557 /* LINK_ADDRESS ack data size of MAX_MST_DEV */
#define GUID_SIZE 16
#define AVAILABLE_PBN_SIZE 2
#define PAYLOAD_BW_NUM_SIZE 2
#define PORT_SIZE 16

#define DPCD_ADD 20
#define NUM_OF_BYTES_TO_READ 8
#define NUM_OF_BYTES_TO_WRITE 8
#define NUM_OF_I2C_TRANSACTIONS 2
#define WRITE_I2C_DEVICE_ID 7
#define I2C_DATA_TO_WRITE 8
#define NO_STOP_BIT 1
#define I2C_TRANSACTION_DELAY 4
#define READ_I2C_DEVICE_ID 7

struct displayport_msg_aux_tx {
	u8 req_id;
	u8 port_num;
	u8 guid[GUID_SIZE];
	u8 legacy_dev_plug_status;
	u8 dp_dev_plug_status;
	u8 msg_cap_status;
	u8 input_port;
	u8 peer_dev_type;
	u8 num_sdp_streams;
	u8 vc_payload_id;
	u8 payload_bw_num[PAYLOAD_BW_NUM_SIZE];
	u8 sdp_stream_sink[MAX_MST_DEV];
	u8 available_pbn[AVAILABLE_PBN_SIZE];
	u32 dpcd_address;
	u8 num_write_bytes;
	u8 write_data;
	u8 num_i2c_tx;
	u8 write_i2c_dev_id;
	u8 no_stop_bit;
	u8 i2c_tx_delay;
	u8 read_i2c_dev_id;
	u8 num_read_bytes;
};

struct displayport_msg_aux_rx {
	u8 req_id;
	u8 message_transaction_version_number;
	u8 clear_payload_id_table;
	u8 global_unique_identifier[GUID_SIZE];
	u8 number_of_ports;
	u8 input_port[PORT_SIZE];
	u8 peer_device_type[PORT_SIZE];
	u8 port_number[PORT_SIZE];
	u8 messaging_capability_status[PORT_SIZE];
	u8 displayport_device_plug_status[PORT_SIZE];
	u8 current_capabilities_structure[16];
	u8 legacy_device_plug_status[PORT_SIZE];
	u8 dpcd_revision[PORT_SIZE];
	//u8 peer_global_unique_identifier[16];
	u8 peer_global_unique_identifier[PORT_SIZE][GUID_SIZE];
	u8 number_sdp_streams[PORT_SIZE];
	u8 number_sdp_stream_sinks[PORT_SIZE];
	u32 full_payload_bandwidth_number_available;
	u32 payload_bandwidth_number;
	u8 virtual_channel_payload_identifier;
	u32 allocated_payload_bandwidth_number;
	u32 allocated_pbn;
	u8 number_of_bytes_read;
	u8 *data_read;
};
/* Message Transaction Layer Protocol End*/

/* Sideband MSG Layer Protocol Start */
#define MAX_SB_MSG_PKT_SIZE 48
#define SB_MSG_INIT_READ_SIZE 16

enum displayport_sb_msg_type {
	DOWN_REQ,
	DOWN_REP,
	UP_REQ,
	UP_REP,
};

#define DPCD_ADD_DOWN_REQ 0x1000
#define DPCD_ADD_UP_REP 0x1200
#define DPCD_ADD_DOWN_REP 0x1400
#define DPCD_ADD_UP_REQ 0x1600

struct displayport_sb_msg_header {
	u8 link_cnt_total;
	u8 link_cnt_remain;
	u8 relative_address[MAX_MST_DEV];
	u8 broadcast_msg;
	u8 path_msg;
	u8 sb_msg_body_length;
	u8 start_of_msg_transcation;
	u8 end_of_msg_transcation;
	u8 msg_seq_no;
	u8 sb_msg_header_crc;
	int header_size;
};
/* Sideband MSG Layer Protocol End*/

void displayport_msg_tx(u32 msg_type);
void displayport_msg_rx(u32 msg_type);
void displayport_msg_aux_remote_i2c_read(u8 *edid_buf);
#endif
