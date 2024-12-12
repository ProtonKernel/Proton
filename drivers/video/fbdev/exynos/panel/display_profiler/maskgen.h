/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * Motto Support Driver
 * Author: Kimyung Lee <kernel.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DISPLAY_PROFILER_MASKGEN_H__
#define __DISPLAY_PROFILER_MASKGEN_H__

#define MASK_CHAR_WIDTH 5
#define MASK_CHAR_HEIGHT 9
#define MASK_OUTPUT_WIDTH 1440
#define MASK_OUTPUT_HEIGHT 400
#define BGP_DATA_MAX 0x3FFF

#define MASK_DATA_SIZE_DEFAULT 4096

enum MPRINT_PACKET_TYPE {
	MPRINT_PACKET_TYPE_TRANS = 0b10,
	MPRINT_PACKET_TYPE_BLACK = 0b11,
	MAX_MPRINT_PACKET_TYPE,
};

struct mprint_config {
	int debug;
	int scale;
	int color;
	int skip_y;
	int spacing;
	int padd_x;
	int padd_y;
	int resol_x;
	int resol_y;
	int max_len;
};

struct mprint_props {
	struct mprint_config *conf;
	struct mprint_packet *pkts;
	int cursor_x;
	int cursor_y;
	int pkts_max;
	int pkts_size;
	int pkts_pos;
	u8 *data;
	int data_size;
	int data_max;
	char output[32];
};

struct mprint_packet {
	unsigned char type;
	unsigned short size;
};

int char_to_mask_img(struct mprint_props *p, char *str);
char *get_mprint_config_name(int i);
#endif
