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

#include "displayport.h"

struct displayport_msg_aux_tx msg_aux_tx;
struct displayport_msg_aux_rx msg_aux_rx;
struct displayport_sb_msg_header sb_msg_header;

static int displayport_msg_bytealigned(int length, int count)
{
	int align = length * count;
	int return_val = 0;
	int a = 0;

	if ((align % 8) != 0)
		a = (align % 8);
	else
		a = 8;

	return_val = 8 - a;

	return return_val;
}

u8 displayport_sideband_msg_header_crc(u32 size, u8 *data)
{
	/* CRC-4 */
	u8 bitmask = 0x80;
	u8 bitshift = 7;
	u8 arrayindex = 0;
	int number_of_bits = size * 8 - 4; /* except last crc nibble */
	u8 remainder = 0;

	while (number_of_bits != 0) {
		number_of_bits--;
		remainder <<= 1;
		remainder |= (data[arrayindex] & bitmask) >> bitshift;
		bitmask >>= 1;
		bitshift--;

		if (bitmask == 0) {
			bitmask = 0x80;
			bitshift = 7;
			arrayindex++;
		}

		if ((remainder & 0x10) == 0x10)
			remainder ^= 0x13;
	}

	number_of_bits = 4;

	while (number_of_bits != 0) {
		number_of_bits--;
		remainder <<= 1;

		if ((remainder & 0x10) != 0)
			remainder ^= 0x13;
	}

	return remainder;
}

u8 displayport_sideband_msg_body_crc(u32 size, u8 *data)
{
	/* CRC-8 */
	u8 bitmask = 0x80;
	u8 bitshift = 7;
	u8 arrayindex = 0;
	u16 number_of_bits = size * 8;
	u16 remainder = 0;

	while (number_of_bits != 0) {
		number_of_bits--;
		remainder <<= 1;
		remainder |= (data[arrayindex] & bitmask) >> bitshift;
		bitmask >>= 1;
		bitshift--;

		if (bitmask == 0) {
			bitmask = 0x80;
			bitshift = 7;
			arrayindex++;
		}

		if ((remainder & 0x100) == 0x100)
			remainder ^= 0xD5;
	}

	number_of_bits = 8;

	while (number_of_bits != 0) {
		number_of_bits--;
		remainder <<= 1;

		if ((remainder & 0x100) != 0)
			remainder ^= 0xD5;
	}

	return remainder & 0xFF;
}

int displayport_sideband_msg_make_header(u8 *msg_buf)
{
	int header_size = 0;
	u8 header_crc = 0;
	int i = 0;
	int relative_addr_length = 0;
	u8 buff_1 = 0;
	u8 buff_2 = 0;
	u8 depth = 1;

	relative_addr_length = (displayport_msg_bytealigned(4, (sb_msg_header.link_cnt_total - 1))
			+ 4 * (sb_msg_header.link_cnt_total - 1)) / 8;

	msg_buf[header_size++] = sb_msg_header.link_cnt_total << 4
			| sb_msg_header.link_cnt_remain;

	for (i = 0; i < relative_addr_length; i++) {
		buff_1 = sb_msg_header.relative_address[depth++];
		buff_2 = sb_msg_header.relative_address[depth++];

		if ((i + 1) * 2 <= (sb_msg_header.link_cnt_total - 1)) {
			msg_buf[header_size++] = (buff_1 << 4) | buff_2;
			displayport_dbg("buff1 = 0x%02x buff2 = 0x%02x    %d <= %d\n",
					buff_1, buff_2, (i+1)*2, (sb_msg_header.link_cnt_total-1));
		} else {
			msg_buf[header_size++] = (buff_1 << 4) | 0x00;
			displayport_dbg("zeorpadding)buff1 = 0x%02x buff2 = 0x%02x    %d > %d\n",
					buff_1, buff_2, (i+1)*2, (sb_msg_header.link_cnt_total-1));
		}
	}

	msg_buf[header_size++] = (sb_msg_header.broadcast_msg << 7)
			| sb_msg_header.path_msg << 6
			| sb_msg_header.sb_msg_body_length;

	msg_buf[header_size++] = (sb_msg_header.start_of_msg_transcation << 7)
			| sb_msg_header.end_of_msg_transcation << 6
			| sb_msg_header.msg_seq_no << 4;

	header_crc = displayport_sideband_msg_header_crc(header_size, msg_buf);

	msg_buf[header_size - 1] |= header_crc;

	return header_size;
}

int displayport_sideband_msg_tx(u32 sb_msg_buf_dpcd_addr,
		u32 data_size, u8 *data)
{
	int ret = 0;
	u8 sb_msg_tx_buf[MAX_SB_MSG_PKT_SIZE];
	int i = 0;
	int header_size = 0;
	int sb_msg_tx_buf_size = 0;

	for (i = 0; i < MAX_SB_MSG_PKT_SIZE; i++)
		sb_msg_tx_buf[i] = 0x00;

	header_size = displayport_sideband_msg_make_header(sb_msg_tx_buf);

	for (i = 0; i < data_size; i++)
		sb_msg_tx_buf[header_size + i] = data[i];

	sb_msg_tx_buf_size = header_size + i;

	sb_msg_tx_buf[sb_msg_tx_buf_size++] = displayport_sideband_msg_body_crc(data_size, data);

	displayport_dbg("sb_msg_tx_buf_size = %d\n", sb_msg_tx_buf_size);

	for (i = 0; i < sb_msg_tx_buf_size; i++)
		displayport_dbg("sb_msg_tx_buf[%d] = 0x%02x\n", i, sb_msg_tx_buf[i]);

	ret = displayport_reg_dpcd_write_burst(sb_msg_buf_dpcd_addr, sb_msg_tx_buf_size, sb_msg_tx_buf);
	if (ret != 0)
		displayport_err("displayport_sideband_msg_write fail\n");

	return ret;
}

u32 displayport_sideband_msg_get_buf_dpcd_addr(u32 msg_type)
{
	u32 sb_msg_sddr = 0;

	switch (msg_type) {
	case DOWN_REQ:
		sb_msg_sddr = DPCD_ADD_DOWN_REQ;
		break;
	case DOWN_REP:
		sb_msg_sddr = DPCD_ADD_DOWN_REP;
		break;
	case UP_REQ:
		sb_msg_sddr = DPCD_ADD_UP_REQ;
		break;
	case UP_REP:
		sb_msg_sddr = DPCD_ADD_UP_REP;
		break;
	default:
		sb_msg_sddr = DPCD_ADD_DOWN_REQ;
	}

	return sb_msg_sddr;
}

void displayport_msg_tx(u32 msg_type)
{
	u8 msg_tx_buf[MAX_MSG_REQ_SIZE];
	int msg_tx_buf_size = 0;
	int i = 0;
	u32 sb_msg_buf_dpcd_addr = displayport_sideband_msg_get_buf_dpcd_addr(msg_type);

	for (i = 0; i < MAX_MSG_REQ_SIZE; i++)
		msg_tx_buf[i] = 0x00;

	msg_tx_buf[msg_tx_buf_size++] = msg_aux_tx.req_id;

	switch (msg_aux_tx.req_id) {
	case GET_MESSAGE_TRANSACTION_VERSION:
		msg_tx_buf[msg_tx_buf_size++] = msg_aux_tx.port_num << 4;
		break;
	case LINK_ADDRESS:
		/* LINK_ADDRESS has no data */
		break;
	case CONNECTION_STATUS_NOTIFY:
		/* CONNECTION_STATUS_NOTIFY ack reply(UP_REP) has no data */
		break;
	case ENUM_PATH_RESOURCES:
		msg_tx_buf[msg_tx_buf_size++] = msg_aux_tx.port_num << 4;
		break;
	case ALLOCATE_PAYLOAD:
		msg_tx_buf[msg_tx_buf_size++] = (msg_aux_tx.port_num << 4)
				| msg_aux_tx.num_sdp_streams;
		msg_tx_buf[msg_tx_buf_size++] = msg_aux_tx.vc_payload_id;
		msg_tx_buf[msg_tx_buf_size++] = msg_aux_tx.payload_bw_num[0];
		msg_tx_buf[msg_tx_buf_size++] = msg_aux_tx.payload_bw_num[1];

		for (i = 0; i < msg_aux_tx.num_sdp_streams; i += 2) {
			if (i + 1 < msg_aux_tx.num_sdp_streams)
				msg_tx_buf[msg_tx_buf_size++] = (msg_aux_tx.sdp_stream_sink[i] << 4)
						| msg_aux_tx.sdp_stream_sink[i + 1];
			else
				msg_tx_buf[msg_tx_buf_size++] = msg_aux_tx.sdp_stream_sink[i] << 4;
		}
		break;
	case QUERY_PAYLOAD:
		msg_tx_buf[msg_tx_buf_size++] = msg_aux_tx.port_num << 4;
		msg_tx_buf[msg_tx_buf_size++] = msg_aux_tx.vc_payload_id;
		break;
	case RESOURCE_STATUS_NOTIFY:
		msg_tx_buf[msg_tx_buf_size++] = msg_aux_tx.port_num << 4;

		for (i = 0; i < GUID_SIZE; i++)
			msg_tx_buf[msg_tx_buf_size++] = msg_aux_tx.guid[i];

		for (i = 0; i < AVAILABLE_PBN_SIZE; i++)
			msg_tx_buf[msg_tx_buf_size++] = msg_aux_tx.available_pbn[i];
		break;
	case CLEAR_PAYLOAD_ID_TABLE:
		/* CLEAR_PAYLOAD_ID_TABLE has no data */
		break;
	case REMOTE_DPCD_WRITE:
		msg_tx_buf[msg_tx_buf_size++] =
				(msg_aux_tx.port_num << 4) | (msg_aux_tx.dpcd_address >> 16);
		msg_tx_buf[msg_tx_buf_size++] =
				(msg_aux_tx.dpcd_address >> 8) & 0x000000FF;
		msg_tx_buf[msg_tx_buf_size++] =
				msg_aux_tx.dpcd_address & 0x000000FF;
		msg_tx_buf[msg_tx_buf_size++] =	msg_aux_tx.num_write_bytes;
		msg_tx_buf[msg_tx_buf_size++] = msg_aux_tx.write_data;
		break;
	case REMOTE_I2C_READ:
		msg_tx_buf[msg_tx_buf_size++] =
				(msg_aux_tx.port_num << 4) | msg_aux_tx.num_i2c_tx;
		msg_tx_buf[msg_tx_buf_size++] =	msg_aux_tx.write_i2c_dev_id;
		msg_tx_buf[msg_tx_buf_size++] =	msg_aux_tx.num_write_bytes;
		msg_tx_buf[msg_tx_buf_size++] = msg_aux_tx.write_data;
		msg_tx_buf[msg_tx_buf_size++] =
				(msg_aux_tx.no_stop_bit << 4) | msg_aux_tx.i2c_tx_delay;
		msg_tx_buf[msg_tx_buf_size++] = msg_aux_tx.read_i2c_dev_id;
		msg_tx_buf[msg_tx_buf_size++] = msg_aux_tx.num_read_bytes;
		break;
	default:
		displayport_err("Not Support req_id = 0x%02x\n", msg_aux_tx.req_id);
		break;
	}

	displayport_dbg("msg_tx_buf_size = %d\n", msg_tx_buf_size);

	for (i = 0; i < msg_tx_buf_size; i++)
		displayport_dbg("msg_tx_buf[%d] = 0x%02x\n", i, msg_tx_buf[i]);

	displayport_sideband_msg_tx(sb_msg_buf_dpcd_addr, msg_tx_buf_size, msg_tx_buf);
}

int displayport_sideband_msg_parse_header(u8 *msg_buf)
{
	u8 *ptr = NULL;
	u8 *crc_ptr = NULL;
	int header_size = 0;
	u8 header_crc = 0;
	int number_of_byte = 0;
	int i = 0;
	u8 rad[MAX_MST_DEV] = {0, };

	ptr = msg_buf;
	crc_ptr = msg_buf;

	for (i = 1; i < MAX_MST_DEV; i++)
		rad[i] = 0xFF;

	sb_msg_header.link_cnt_total = (*ptr & 0xF0) >> 4;
	sb_msg_header.link_cnt_remain = *(ptr++) & 0xF;

	if (sb_msg_header.link_cnt_total > 1) {
		displayport_info("link_cnt_total = %d\n", sb_msg_header.link_cnt_total);
		if (((sb_msg_header.link_cnt_total - 1) % 2) == 0)
			number_of_byte = (sb_msg_header.link_cnt_total - 1) / 2;
		else
			number_of_byte = (sb_msg_header.link_cnt_total - 1) / 2 + 1;

		for (i = 0; i < (sb_msg_header.link_cnt_total - 1); i++) {
			if (i % 2 == 0) {
				rad[i] = (*ptr & 0xF0) >> 4;
				if (i == (sb_msg_header.link_cnt_total - 2))
					ptr++;
			} else
				rad[i] = (*(ptr++) & 0xF);

			sb_msg_header.relative_address[i] = rad[i];
			displayport_info("Received RAD(%d) = 0x%02x\n",
					i, sb_msg_header.relative_address[i]);
		}
	}

	header_size = 3 + number_of_byte;

	sb_msg_header.header_size = header_size;
	sb_msg_header.broadcast_msg = ((*ptr) & 0x80) >> 7;
	sb_msg_header.path_msg = ((*ptr) & 0x40) >> 6;
	sb_msg_header.sb_msg_body_length = ((*ptr++) & 0x3F);
	sb_msg_header.start_of_msg_transcation = ((*ptr) & 0x80) >> 7;
	sb_msg_header.end_of_msg_transcation = ((*ptr) & 0x40) >> 6;
	sb_msg_header.msg_seq_no = ((*ptr) & 0x10) >> 4;
	sb_msg_header.sb_msg_header_crc = *(ptr++) & 0xF;

	header_crc = displayport_sideband_msg_header_crc(header_size, crc_ptr);

	if (sb_msg_header.sb_msg_header_crc != header_crc) {
		displayport_err("Received SB Header CRC Error, 0x%02x != 0x%02x\n",
				sb_msg_header.sb_msg_header_crc, header_crc);
		return -1;
	}

	displayport_dbg("sb_msg_rx_header_size = %d\n", header_size);

	for (i = 0; i < header_size; i++)
		displayport_dbg("sb_msg_rx_header[%d] = 0x%02x\n", i, msg_buf[i]);

	return header_size;
}

u8 displayport_sideband_msg_get_rdy_bit(u32 msg_type)
{
	u8 rdy_irq_bit = 0;

	switch (msg_type) {
	case DOWN_REP:
		rdy_irq_bit = DOWN_REP_MSG_RDY;
		break;
	case UP_REQ:
		rdy_irq_bit = UP_REQ_MSG_RDY;
		break;
	default:
		rdy_irq_bit = DOWN_REP_MSG_RDY;
	}

	return rdy_irq_bit;
}

int displayport_msg_aux_wait_rep_rdy_clr(u32 msg_type)
{
	int ret = 0;
	int cnt = 200;
	u8 val = 0;
	u8 rdy_irq_bit = displayport_sideband_msg_get_rdy_bit(msg_type);

	do {
		displayport_reg_dpcd_write(DPCD_ADD_DEVICE_SERVICE_IRQ_VECTOR, 1, &rdy_irq_bit);
		val = 0;
		usleep_range(1000, 1030);
		displayport_reg_dpcd_read(DPCD_ADD_DEVICE_SERVICE_IRQ_VECTOR, 1, &val);
		cnt--;
	} while (((val & rdy_irq_bit) == rdy_irq_bit) && cnt);

	if (cnt > 0) {
		do {
			displayport_reg_dpcd_write(DPCD_ADD_DEVICE_SERVICE_IRQ_VECTOR_ESI0, 1, &rdy_irq_bit);
			val = 0;
			usleep_range(1000, 1030);
			displayport_reg_dpcd_read(DPCD_ADD_DEVICE_SERVICE_IRQ_VECTOR_ESI0, 1, &val);
			cnt--;
		} while (((val & rdy_irq_bit) == rdy_irq_bit) && cnt);
	}

	if (!cnt) {
		displayport_err("%s is timeout.\n", __func__);
		ret = -ETIMEDOUT;
	}

	return ret;
}

int displayport_sideband_msg_rx(u32 msg_type, u8 *data)
{
	int i = 0;
	u8 sb_msg_rx_buf[MAX_SB_MSG_PKT_SIZE] = {0, };
	u8 sb_msg_rx_buf_added[MAX_SB_MSG_PKT_SIZE] = {0, };
	int header_size = 0;
	int dpcd_read_size = SB_MSG_INIT_READ_SIZE;
	int reply_data_size = 0;
	int total_size = 0;
	int data_size = 0;
	u32 sb_msg_buf_dpcd_addr = displayport_sideband_msg_get_buf_dpcd_addr(msg_type);
	u8 body_crc = 0;

	displayport_reg_dpcd_read_burst(sb_msg_buf_dpcd_addr, dpcd_read_size, sb_msg_rx_buf);

	header_size = displayport_sideband_msg_parse_header(sb_msg_rx_buf);

	reply_data_size = sb_msg_header.sb_msg_body_length - 1;
	total_size = sb_msg_header.header_size + sb_msg_header.sb_msg_body_length;
	sb_msg_buf_dpcd_addr += dpcd_read_size;
	dpcd_read_size = total_size - SB_MSG_INIT_READ_SIZE;

	if (dpcd_read_size > 0) { /* Need more DPCD read for Sideband Message body */
		displayport_reg_dpcd_read_burst(sb_msg_buf_dpcd_addr,
				dpcd_read_size, sb_msg_rx_buf_added);

		for (i = 0; i < SB_MSG_INIT_READ_SIZE - sb_msg_header.header_size; i++)
			data[data_size++] = sb_msg_rx_buf[header_size + i];

		for (i = 0; i < dpcd_read_size; i++)
			data[data_size++] = sb_msg_rx_buf_added[i];
	} else {
		for (i = 0; i < sb_msg_header.sb_msg_body_length; i++)
			data[data_size++] = sb_msg_rx_buf[header_size + i];
	}

	if(displayport_msg_aux_wait_rep_rdy_clr(msg_type) < 0)
		displayport_err("MSG_RDY not clr fail\n");

	body_crc = displayport_sideband_msg_body_crc(reply_data_size, data);

	if (body_crc != data[data_size - 1])
		displayport_err("msg_type = %d, SB Body CRC Error 0x%02x != 0x%02x\n",
				msg_type, body_crc, data[data_size - 1]);

	displayport_dbg("msg_type = %d, sb_msg_rx_body_size = %d\n", msg_type, data_size);

	for (i = 0; i < data_size; i++)
		displayport_dbg("sb_msg_rx_body[%d] = 0x%02x\n", i, data[i]);

	return data_size - 1; /* except body CRC */
}

int displayport_msg_aux_wait_rep_rdy(u32 msg_type)
{
	int ret = 0;
	int cnt = 200;
	u8 val = 0;
	u8 rdy_irq_bit = displayport_sideband_msg_get_rdy_bit(msg_type);

	do {
		val = 0;
		usleep_range(1000, 1030);
		displayport_reg_dpcd_read(DPCD_ADD_DEVICE_SERVICE_IRQ_VECTOR, 1, &val);
		cnt--;
	} while (((val & rdy_irq_bit) != rdy_irq_bit) && cnt);

	if (cnt > 0) {
		do {
			val = 0;
			usleep_range(1000, 1030);
			displayport_reg_dpcd_read(DPCD_ADD_DEVICE_SERVICE_IRQ_VECTOR_ESI0, 1, &val);
			cnt--;
		} while (((val & rdy_irq_bit) != rdy_irq_bit) && cnt);
	}

	if (!cnt) {
		displayport_err("%s is timeout.\n", __func__);
		ret = -ETIMEDOUT;
	}

	return ret;
}

u32 displayport_msg_get_max_size(u32 msg_type)
{
	u32 max_msg_size = 0;

	switch (msg_type) {
	case DOWN_REP:
		max_msg_size = MAX_MSG_REP_SIZE;
		break;
	case UP_REQ:
		max_msg_size = MAX_MSG_REQ_SIZE;
		break;
	default:
		max_msg_size = MAX_MSG_REP_SIZE;
	}

	return max_msg_size;
}

void displayport_msg_rx(u32 msg_type)
{
	u8 *ptr = NULL;
	u8 msg_rx_buf[MAX_MSG_REP_SIZE];
	int msg_rx_buf_size = 0;
	int total_msg_rx_buf_size = 0;
	int i = 0;
	int j = 0;
	int retry_cnt = 3;
	u8 *peer_input_port = NULL;
	u8 *peer_device_port_number = NULL;
	u8 *peer_guid = NULL;
	u8 *peer_device_type = NULL;
	u8 rad[MAX_MST_DEV];
	u8 return_RAD[MAX_MST_DEV];
	int ch = 0;
	u32 max_msg_size = displayport_msg_get_max_size(msg_type);
	struct displayport_device *displayport = get_displayport_drvdata();

	for (i = 0; i < MAX_MST_DEV; i++)
		return_RAD[i] = rad[i] = 0xFF;

MSG_RX_READ_RETRY:
	for (i = 0; i < max_msg_size; i += msg_rx_buf_size) {
		if((displayport_msg_aux_wait_rep_rdy(msg_type) < 0) && (retry_cnt > 0)) {
			msg_rx_buf_size = 0;
			total_msg_rx_buf_size = 0;
			retry_cnt--;
			goto MSG_RX_READ_RETRY;
		} else if (retry_cnt <= 0) {
			msg_aux_rx.req_id = 0xFF;
			displayport_err("MSG_RDY not set fail\n");
			break;
		}

		msg_rx_buf_size = displayport_sideband_msg_rx(msg_type, &msg_rx_buf[i]);

		if (sb_msg_header.end_of_msg_transcation == 1) {
			total_msg_rx_buf_size = i + msg_rx_buf_size;
			break;
		}
	}

	displayport_dbg("total_msg_rx_buf_size = %d\n", total_msg_rx_buf_size);

	for (i = 0; i < total_msg_rx_buf_size; i++)
		displayport_dbg("msg_rx_buf[%d] = 0x%02x\n", i, msg_rx_buf[i]);

	ptr = msg_rx_buf;
	msg_aux_rx.req_id = *(ptr++);

	if (msg_type == DOWN_REP) {
		if ((msg_aux_tx.req_id != msg_aux_rx.req_id) && retry_cnt > 0) {
			displayport_dbg("DOWN_REP ID fail\n");
			displayport_dbg("TX req_id : %d\n", msg_aux_tx.req_id);
			displayport_dbg("RX req_id id : %d\n", msg_aux_rx.req_id);
			msg_rx_buf_size = 0;
			total_msg_rx_buf_size = 0;
			retry_cnt--;
			goto MSG_RX_READ_RETRY;
		} else if (retry_cnt <= 0) {
			msg_aux_rx.req_id = 0xFF;
			displayport_err("DOWN_REP ID fail after retry read\n");
		}
	}

	switch (msg_aux_rx.req_id) {
	case GET_MESSAGE_TRANSACTION_VERSION:
		displayport_dbg("Receive GET_MESSAGE_TRANSACTION_VERSION\n");
		msg_aux_rx.message_transaction_version_number = *ptr;
		break;
	case LINK_ADDRESS:
		displayport_dbg("Receive LINK_ADDRESS\n");

		for (i = 0; i < GUID_SIZE; i++) {
			msg_aux_rx.global_unique_identifier[i] = *(ptr++);
			displayport_dbg("global_unique_identifier[%d] = 0x%2x\n",
					i, msg_aux_rx.global_unique_identifier[i]);
		}

		msg_aux_rx.number_of_ports = *(ptr++);
		displayport_dbg("number_of_ports = %d\n", msg_aux_rx.number_of_ports);

		for (i = 0; i < msg_aux_rx.number_of_ports; i++) {
			msg_aux_rx.input_port[i] = (((*ptr) & 0x80) >> 7);
			msg_aux_rx.peer_device_type[i] = (((*ptr) & 0x70) >> 4);
			msg_aux_rx.port_number[i] = (*(ptr++) & 0x0F);
			msg_aux_rx.messaging_capability_status[i] = (((*ptr) & 0x80) >> 7);
			msg_aux_rx.displayport_device_plug_status[i] = (((*ptr) & 0x40) >> 6);

			displayport_dbg("input_port[%d] = %d\n", i, msg_aux_rx.input_port[i]);
			displayport_dbg("peer_device_type[%d] = %d\n", i, msg_aux_rx.peer_device_type[i]);
			displayport_dbg("port_number[%d] = %d\n", i, msg_aux_rx.port_number[i]);
			displayport_dbg("messaging_capability_status[%d] = %d\n",
					i, msg_aux_rx.messaging_capability_status[i]);
			displayport_dbg("displayport_device_plug_status[%d] = %d\n",
					i, msg_aux_rx.displayport_device_plug_status[i]);

			if (ch < MAX_VC_CNT
					&& msg_aux_rx.input_port[i] == 0
					&& msg_aux_rx.displayport_device_plug_status[i] == 1) {
				ch = displayport_get_sst_id_with_port_number(msg_aux_rx.port_number[i], FIND_SST_ID);
				if (ch < 0) {
					ch = displayport_get_sst_id_with_port_number(msg_aux_rx.port_number[i], ALLOC_SST_ID);
					if (ch >= 0) {
						displayport->sst[ch]->hpd_state = HPD_PLUG_WORK;
						queue_delayed_work(displayport->dp_wq, &displayport->hpd_plug_work, 10);
					}
				}
			}

			if (msg_aux_rx.peer_device_type[i] == 0x5) {
				for (j = 0; j < sizeof(msg_aux_rx.current_capabilities_structure); j++) {
					msg_aux_rx.current_capabilities_structure[j] |= (((*ptr++) & 0x3F) << 2);
					msg_aux_rx.current_capabilities_structure[j] |= (((*ptr) & 0xC0) >> 6);
				}
			}

			if (msg_aux_rx.input_port[i] == 0) {
				msg_aux_rx.legacy_device_plug_status[i] = (((*ptr++) & 0x20) >> 5);
				msg_aux_rx.dpcd_revision[i] = *(ptr++);

				for (j = 0; j < GUID_SIZE; j++) {
					msg_aux_rx.peer_global_unique_identifier[i][j] = *(ptr++);
					displayport_dbg("peer_global_unique_identifier[%d][%d] = 0x%2x\n",
							i, j, msg_aux_rx.peer_global_unique_identifier[i][j]);
				}

				msg_aux_rx.number_sdp_streams[i] = (((*ptr) & 0xF0) >> 4);
				msg_aux_rx.number_sdp_stream_sinks[i] = ((*ptr++) & 0xF);
			} else
				ptr++;

			rad[i] = msg_aux_rx.port_number[i];
		}

		peer_input_port = msg_aux_rx.input_port;
		peer_device_port_number = msg_aux_rx.port_number;
		peer_guid = msg_aux_rx.peer_global_unique_identifier[0];
		peer_device_type = msg_aux_rx.peer_device_type;

		if (displayport_topology_head->next == NULL)
			displayport_topology_add_list(msg_aux_rx.global_unique_identifier,
					/*rad*/sb_msg_header.relative_address, BRANCH_DEVICE);

		if (msg_aux_rx.number_of_ports > 1) {
			for (i = 0; i < msg_aux_rx.number_of_ports; i++) {
				if (*(peer_input_port++) == 0) {
					displayport_topology_add_rad(return_RAD,
							/*rad*/sb_msg_header.relative_address, *peer_device_port_number);

					displayport_topology_add_list(peer_guid,
							return_RAD, *peer_device_type);

					for (j = 0; j < GUID_SIZE; j++)
						peer_guid++;
				}

				peer_device_port_number++;
				peer_device_type++;
			}
		}
		break;
	case CONNECTION_STATUS_NOTIFY:
		displayport_dbg("Receive CONNECTION_STATUS_NOTIFY(UP_REQ)\n");
		msg_aux_rx.port_number[0] = *(ptr++) >> 4;

		for (i = 0; i < GUID_SIZE; i++) {
			msg_aux_rx.global_unique_identifier[i] = *(ptr++);
			displayport_dbg("global_unique_identifier[%d] = 0x%2x\n",
					i, msg_aux_rx.global_unique_identifier[i]);
		}

		msg_aux_rx.legacy_device_plug_status[0] = ((*ptr) & 0x40) >> 6;
		msg_aux_rx.displayport_device_plug_status[0] = ((*ptr) & 0x20) >> 5;
		msg_aux_rx.messaging_capability_status[0] = ((*ptr) & 0x10) >> 4;
		msg_aux_rx.input_port[0] = ((*ptr) & 0x08) >> 3;
		msg_aux_rx.peer_device_type[0] = (*ptr) & 0x07;

		/* for UP_REP */
		msg_aux_tx.req_id = CONNECTION_STATUS_NOTIFY;
		sb_msg_header.link_cnt_total = 0x1;
		sb_msg_header.link_cnt_remain = 0x0;
		for (i = 0; i < 15; i++)
			sb_msg_header.relative_address[i] = return_RAD[i];
		sb_msg_header.broadcast_msg = 0x1;
		sb_msg_header.path_msg = 0x0;
		sb_msg_header.sb_msg_body_length = 0x02;
		sb_msg_header.start_of_msg_transcation = 0x1;
		sb_msg_header.end_of_msg_transcation = 0x1;
		sb_msg_header.msg_seq_no = 0x0;
		displayport_msg_tx(UP_REP);

		if (msg_aux_rx.input_port[0] == 0
				&& msg_aux_rx.displayport_device_plug_status[0] == 1) {
			ch = displayport_get_sst_id_with_port_number(msg_aux_rx.port_number[0], FIND_SST_ID);
			if (ch < 0) {
				ch = displayport_get_sst_id_with_port_number(msg_aux_rx.port_number[0], ALLOC_SST_ID);
				if (ch >= 0) {
					displayport->sst[ch]->hpd_state = HPD_PLUG_WORK;
					queue_delayed_work(displayport->dp_wq, &displayport->hpd_plug_work, 10);
				}
			}
		} else if (msg_aux_rx.input_port[0] == 0
				&& msg_aux_rx.displayport_device_plug_status[0] == 0) {
			ch = displayport_get_sst_id_with_port_number(msg_aux_rx.port_number[0],	FIND_SST_ID);
			if (ch >= 0) {
				displayport->sst[ch]->hpd_state = HPD_UNPLUG_WORK;
				queue_delayed_work(displayport->dp_wq, &displayport->hpd_unplug_work, 10);
			}
		}
		break;
	case ENUM_PATH_RESOURCES:
		displayport_dbg("Receive ENUM_PATH_RESOURCES\n");
		break;
	case ALLOCATE_PAYLOAD:
		displayport_dbg("Receive ALLOCATE_PAYLOAD\n");
		msg_aux_rx.port_number[0] = *(ptr++) >> 4;
		msg_aux_rx.virtual_channel_payload_identifier = *(ptr++);

		if (((msg_aux_tx.port_num != msg_aux_rx.port_number[0])
				|| (msg_aux_tx.vc_payload_id != msg_aux_rx.virtual_channel_payload_identifier))
				&& retry_cnt > 0) {
			displayport_dbg("ALLOCATE_PAYLOAD ack fail\n");
			displayport_dbg("DWON_REQ port_num : %d\n", msg_aux_tx.port_num);
			displayport_dbg("DWON_REP port_num : %d\n", msg_aux_rx.port_number[0]);
			displayport_dbg("DWON_REQ vc_payload_id : %d\n", msg_aux_tx.vc_payload_id);
			displayport_dbg("DWON_REP vc_payload_id : %d\n", msg_aux_rx.virtual_channel_payload_identifier);
			msg_rx_buf_size = 0;
			total_msg_rx_buf_size = 0;
			retry_cnt--;
			goto MSG_RX_READ_RETRY;
		}

		msg_aux_rx.payload_bandwidth_number = 0;
		msg_aux_rx.payload_bandwidth_number |= *(ptr++);
		msg_aux_rx.payload_bandwidth_number = msg_aux_rx.payload_bandwidth_number << 8;
		msg_aux_rx.payload_bandwidth_number |= *(ptr++);
		break;
	case QUERY_PAYLOAD:
		displayport_dbg("Receive QUERY_PAYLOAD\n");
		break;
	case CLEAR_PAYLOAD_ID_TABLE:
		displayport_dbg("Receive CLEAR_PAYLOAD_ID_TABLE\n");
		break;
	case REMOTE_DPCD_WRITE:
		displayport_dbg("Receive REMOTE_DPCD_WRITE\n");
		break;
	default:
		displayport_err("Not Support req_id = 0x%2x\n", msg_aux_rx.req_id);
		break;
	}
}

void displayport_msg_aux_remote_i2c_read(u8 *edid_buf)
{
	u8 *ptr = NULL;
	u8 msg_rx_buf[MAX_MSG_REP_SIZE];
	int msg_rx_buf_size = 0;
	int total_msg_rx_buf_size = 0;
	int retry_cnt = 3;
	int i = 0;
	u32 max_msg_size = displayport_msg_get_max_size(DOWN_REP);

REMOTE_I2C_READ_RETRY:
	for (i = 0; i < max_msg_size; i += msg_rx_buf_size) {
		if((displayport_msg_aux_wait_rep_rdy(DOWN_REP) < 0) && (retry_cnt > 0)) {
			msg_rx_buf_size = 0;
			total_msg_rx_buf_size = 0;
			retry_cnt--;
			goto REMOTE_I2C_READ_RETRY;
		} else if (retry_cnt <= 0) {
			msg_aux_rx.req_id = 0xFF;
			displayport_err("MSG_RDY not set fail\n");
			break;
		}

		msg_rx_buf_size = displayport_sideband_msg_rx(DOWN_REP, &msg_rx_buf[i]);

		if (sb_msg_header.end_of_msg_transcation == 1) {
			total_msg_rx_buf_size = i + msg_rx_buf_size;
			break;
		}
	}

	ptr = msg_rx_buf;
	msg_aux_rx.req_id = *(ptr++);

	if ((msg_aux_tx.req_id != REMOTE_I2C_READ) && retry_cnt > 0) {
		displayport_dbg("DOWN_REP REMOTE_I2C_READ ID fail\n");
		displayport_dbg("RX req_id id : %d\n", msg_aux_rx.req_id);
		msg_rx_buf_size = 0;
		total_msg_rx_buf_size = 0;
		retry_cnt--;
		goto REMOTE_I2C_READ_RETRY;
	} else if (retry_cnt <= 0) {
		msg_aux_rx.req_id = 0xFF;
		displayport_err("REMOTE_I2C_READ ID fail after retry read\n");
	}

	msg_aux_rx.port_number[0] = *(ptr++);
	msg_aux_rx.number_of_bytes_read = *(ptr++);

	memcpy(edid_buf, ptr, EDID_BLOCK_SIZE);
}
