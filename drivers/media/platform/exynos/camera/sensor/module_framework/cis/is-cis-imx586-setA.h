/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_IMX582_SET_A_H
#define IS_CIS_IMX582_SET_A_H

#include "is-cis.h"
#include "is-cis-imx586.h"

/*
 * [Mode Information]
 *
 * Reference File : IMX582_SEC-DPHY-26MHz_RegisterSetting_ver1.00-4.00_b1_20190508.xlsx
 * Update Data    : 2019-05-08
 * Author         : takkyoum.kim
 *
 * - Global Setting -
 *
 * - Remosaic Full For Single Still Remosaic Capture -
 *    [  0 ] REG_A   : Remosaic Full 8000x6000 15fps       : Single Still Remosaic Capture (4:3) ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2200
 *
 * - 2x2 BIN Full For Still Preview / Capture -
 *    [  1 ] REG_C_2 : 2x2 Binning Full 4000x3000 30fps    : Single Still Preview/Capture (4:3)  ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2200
 *
 * - 2x2 BIN V2H2 For HighSpeed Recording -
 *    [  2 ] REG_D   : 2x2 Binning V2H2 2000x1128 240fps   : High Speed Recording (16:9)         ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2200
 *
 */

const u32 sensor_imx586_setfile_A_Global[] = {
	//External Clock Setting
	0x0136, 0x1A, 0x01,
	0x0137, 0x00, 0x01,
	//Register version
	0x3C7E, 0x0A, 0x01,
	0x3C7F, 0x0A, 0x01,
	//Signaling mode Setting
	0x0111, 0x02, 0x01,
	//Global Setting
	0x380C, 0x00, 0x01,
	0x3C00, 0x10, 0x01,
	0x3C01, 0x10, 0x01,
	0x3C02, 0x10, 0x01,
	0x3C03, 0x10, 0x01,
	0x3C04, 0x10, 0x01,
	0x3C05, 0x01, 0x01,
	0x3C06, 0x00, 0x01,
	0x3C07, 0x00, 0x01,
	0x3C08, 0x03, 0x01,
	0x3C09, 0xFF, 0x01,
	0x3C0A, 0x01, 0x01,
	0x3C0B, 0x00, 0x01,
	0x3C0C, 0x00, 0x01,
	0x3C0D, 0x03, 0x01,
	0x3C0E, 0xFF, 0x01,
	0x3C0F, 0x20, 0x01,
	0x3F88, 0x00, 0x01,
	0x3F8E, 0x00, 0x01,
	0x5282, 0x01, 0x01,
	0x9004, 0x14, 0x01,
	0x9200, 0xF4, 0x01,
	0x9201, 0xA7, 0x01,
	0x9202, 0xF4, 0x01,
	0x9203, 0xAA, 0x01,
	0x9204, 0xF4, 0x01,
	0x9205, 0xAD, 0x01,
	0x9206, 0xF4, 0x01,
	0x9207, 0xB0, 0x01,
	0x9208, 0xF4, 0x01,
	0x9209, 0xB3, 0x01,
	0x920A, 0xB7, 0x01,
	0x920B, 0x34, 0x01,
	0x920C, 0xB7, 0x01,
	0x920D, 0x36, 0x01,
	0x920E, 0xB7, 0x01,
	0x920F, 0x37, 0x01,
	0x9210, 0xB7, 0x01,
	0x9211, 0x38, 0x01,
	0x9212, 0xB7, 0x01,
	0x9213, 0x39, 0x01,
	0x9214, 0xB7, 0x01,
	0x9215, 0x3A, 0x01,
	0x9216, 0xB7, 0x01,
	0x9217, 0x3C, 0x01,
	0x9218, 0xB7, 0x01,
	0x9219, 0x3D, 0x01,
	0x921A, 0xB7, 0x01,
	0x921B, 0x3E, 0x01,
	0x921C, 0xB7, 0x01,
	0x921D, 0x3F, 0x01,
	0x921E, 0x77, 0x01,
	0x921F, 0x77, 0x01,
	0x9222, 0xC4, 0x01,
	0x9223, 0x4B, 0x01,
	0x9224, 0xC4, 0x01,
	0x9225, 0x4C, 0x01,
	0x9226, 0xC4, 0x01,
	0x9227, 0x4D, 0x01,
	0x9810, 0x14, 0x01,
	0x9814, 0x14, 0x01,
	0x99B2, 0x20, 0x01,
	0x99B3, 0x0F, 0x01,
	0x99B4, 0x0F, 0x01,
	0x99B5, 0x0F, 0x01,
	0x99B6, 0x0F, 0x01,
	0x99E4, 0x0F, 0x01,
	0x99E5, 0x0F, 0x01,
	0x99E6, 0x0F, 0x01,
	0x99E7, 0x0F, 0x01,
	0x99E8, 0x0F, 0x01,
	0x99E9, 0x0F, 0x01,
	0x99EA, 0x0F, 0x01,
	0x99EB, 0x0F, 0x01,
	0x99EC, 0x0F, 0x01,
	0x99ED, 0x0F, 0x01,
	0xA569, 0x06, 0x01,
	0xA56A, 0x13, 0x01,
	0xA56B, 0x13, 0x01,
	0xA679, 0x20, 0x01,
	0xA830, 0x68, 0x01,
	0xA831, 0x56, 0x01,
	0xA832, 0x2B, 0x01,
	0xA833, 0x55, 0x01,
	0xA834, 0x55, 0x01,
	0xA835, 0x16, 0x01,
	0xA837, 0x51, 0x01,
	0xA838, 0x34, 0x01,
	0xA854, 0x4F, 0x01,
	0xA855, 0x48, 0x01,
	0xA856, 0x45, 0x01,
	0xA857, 0x02, 0x01,
	0xA85A, 0x23, 0x01,
	0xA85B, 0x16, 0x01,
	0xA85C, 0x12, 0x01,
	0xA85D, 0x02, 0x01,
	0xAC72, 0x01, 0x01,
	0xAC73, 0x26, 0x01,
	0xAC74, 0x01, 0x01,
	0xAC75, 0x26, 0x01,
	0xAC76, 0x00, 0x01,
	0xAC77, 0xC4, 0x01,
	0xB051, 0x02, 0x01,
	0xC020, 0x01, 0x01,
	0xC61D, 0x00, 0x01,
	0xC625, 0x00, 0x01,
	0xC638, 0x03, 0x01,
	0xC63B, 0x01, 0x01,
	0xE286, 0x31, 0x01,
	0xE2A6, 0x32, 0x01,
	0xE2C6, 0x33, 0x01,
	0xEA4B, 0x00, 0x01,
	0xEA4C, 0x00, 0x01,
	0xEA4D, 0x00, 0x01,
	0xEA4E, 0x00, 0x01,
	0xF000, 0x00, 0x01,
	0xF001, 0x10, 0x01,
	0xF00C, 0x00, 0x01,
	0xF00D, 0x40, 0x01,
	0xF030, 0x00, 0x01,
	0xF031, 0x10, 0x01,
	0xF03C, 0x00, 0x01,
	0xF03D, 0x40, 0x01,
	0xF44B, 0x80, 0x01,
	0xF44C, 0x10, 0x01,
	0xF44D, 0x06, 0x01,
	0xF44E, 0x80, 0x01,
	0xF44F, 0x10, 0x01,
	0xF450, 0x06, 0x01,
	0xF451, 0x80, 0x01,
	0xF452, 0x10, 0x01,
	0xF453, 0x06, 0x01,
	0xF454, 0x80, 0x01,
	0xF455, 0x10, 0x01,
	0xF456, 0x06, 0x01,
	0xF457, 0x80, 0x01,
	0xF458, 0x10, 0x01,
	0xF459, 0x06, 0x01,
	0xF478, 0x20, 0x01,
	0xF479, 0x80, 0x01,
	0xF47A, 0x80, 0x01,
	0xF47B, 0x20, 0x01,
	0xF47C, 0x80, 0x01,
	0xF47D, 0x80, 0x01,
	0xF47E, 0x20, 0x01,
	0xF47F, 0x80, 0x01,
	0xF480, 0x80, 0x01,
	0xF481, 0x20, 0x01,
	0xF482, 0x60, 0x01,
	0xF483, 0x80, 0x01,
	0xF484, 0x20, 0x01,
	0xF485, 0x60, 0x01,
	0xF486, 0x80, 0x01,
	0x3E14, 0x01, 0x01,
	0xE013, 0x01, 0x01,
	0xE186, 0x2B, 0x01,
	//Image Quality adjustment setting
	0x9852, 0x00, 0x01,
	0x9954, 0x0F, 0x01,
	0xA7AD, 0x01, 0x01,
	0xA7CB, 0x01, 0x01,
	0xAE06, 0x04, 0x01, /* LPF */
	0xAE07, 0x04, 0x01,
	0xAE08, 0x04, 0x01,
	0xAE09, 0x04, 0x01,
	0xAE0A, 0x04, 0x01,
	0xAE0B, 0x04, 0x01, /* LPF end */
	0xAE12, 0x58, 0x01,
	0xAE13, 0x58, 0x01,
	0xAE15, 0x10, 0x01,
	0xAE16, 0x10, 0x01,
	0xAF05, 0x48, 0x01,
	0xB07C, 0x02, 0x01,
};

const u32 sensor_imx582_setfile_A_REMOSAIC_FULL_8000x6000_15FPS[] = {
	//MIPI output setting
	0x0112, 0x0A, 0x01,
	0x0113, 0x0A, 0x01,
	0x0114, 0x03, 0x01,
	//Line Length PCK Setting
	0x0342, 0x3C, 0x01,
	0x0343, 0x90, 0x01,
	//Frame Length Lines Setting
	0x0340, 0x18, 0x01,
	0x0341, 0x2A, 0x01,
	//ROI Setting
	0x0344, 0x00, 0x01,
	0x0345, 0x00, 0x01,
	0x0346, 0x00, 0x01,
	0x0347, 0x00, 0x01,
	0x0348, 0x1F, 0x01,
	0x0349, 0x3F, 0x01,
	0x034A, 0x17, 0x01,
	0x034B, 0x6F, 0x01,
	//Mode Setting
	0x0220, 0x62, 0x01,
	0x0222, 0x01, 0x01,
	0x0900, 0x00, 0x01,
	0x0901, 0x11, 0x01,
	0x0902, 0x0A, 0x01,
	0x3140, 0x00, 0x01,
	0x3246, 0x01, 0x01,
	0x3247, 0x01, 0x01,
	0x3F15, 0x00, 0x01,
	//Digital Crop & Scaling
	0x0401, 0x00, 0x01,
	0x0404, 0x00, 0x01,
	0x0405, 0x10, 0x01,
	0x0408, 0x00, 0x01,
	0x0409, 0x00, 0x01,
	0x040A, 0x00, 0x01,
	0x040B, 0x00, 0x01,
	0x040C, 0x1F, 0x01,
	0x040D, 0x40, 0x01,
	0x040E, 0x17, 0x01,
	0x040F, 0x70, 0x01,
	//Output Size Setting
	0x034C, 0x1F, 0x01,
	0x034D, 0x40, 0x01,
	0x034E, 0x17, 0x01,
	0x034F, 0x70, 0x01,
	//Clock Setting
	0x0301, 0x05, 0x01,
	0x0303, 0x02, 0x01,
	0x0305, 0x04, 0x01,
	0x0306, 0x01, 0x01,
	0x0307, 0x4C, 0x01,
	0x030B, 0x01, 0x01,
	0x030D, 0x03, 0x01,
	0x030E, 0x01, 0x01,
	0x030F, 0x1F, 0x01,
	0x0310, 0x01, 0x01,
	//Other Setting
	0x3620, 0x01, 0x01,
	0x3621, 0x01, 0x01,
	0x3C11, 0x08, 0x01,
	0x3C12, 0x08, 0x01,
	0x3C13, 0x2A, 0x01,
	0x3F0C, 0x00, 0x01,
	0x3F14, 0x01, 0x01,
	0x3F80, 0x08, 0x01,
	0x3F81, 0x76, 0x01,
	0x3F8C, 0x00, 0x01,
	0x3F8D, 0x14, 0x01,
	0x3FF8, 0x00, 0x01,
	0x3FF9, 0x00, 0x01,
	0x3FFE, 0x03, 0x01,
	0x3FFF, 0xA5, 0x01,
	//Integration Setting
	0x0202, 0x17, 0x01,
	0x0203, 0xFA, 0x01,
	0x0224, 0x01, 0x01,
	0x0225, 0xF4, 0x01,
	0x3FE0, 0x01, 0x01,
	0x3FE1, 0xF4, 0x01,
	//Gain Setting
	0x0204, 0x00, 0x01,
	0x0205, 0x70, 0x01,
	0x0216, 0x00, 0x01,
	0x0217, 0x70, 0x01,
	0x0218, 0x01, 0x01,
	0x0219, 0x00, 0x01,
	0x020E, 0x01, 0x01,
	0x020F, 0x00, 0x01,
	0x0210, 0x01, 0x01,
	0x0211, 0x00, 0x01,
	0x0212, 0x01, 0x01,
	0x0213, 0x00, 0x01,
	0x0214, 0x01, 0x01,
	0x0215, 0x00, 0x01,
	0x3FE2, 0x00, 0x01,
	0x3FE3, 0x70, 0x01,
	0x3FE4, 0x01, 0x01,
	0x3FE5, 0x00, 0x01,
	//HDR output
	0x3141, 0x05, 0x01,
	//LSC Setting
	0x0B00, 0x00, 0x01,
	//AE Hist Setting
	0x323B, 0x00, 0x01,
	//Flicker Setting
	0x323C, 0x00, 0x01,
	//DPC output Setting
	0x0B06, 0x01, 0x01,
	//PDAF TYPE Setting
	0x3E20, 0x02, 0x01,
	0x3E3B, 0x01, 0x01,
	0x4434, 0x00, 0x01,
	0x4435, 0xF8, 0x01,
};

const u32 sensor_imx586_setfile_A_REMOSAIC_CROP_4000x3000_30FPS[] = {
	//MIPI output setting
	0x0112, 0x0A, 0x01,
	0x0113, 0x0A, 0x01,
	0x0114, 0x03, 0x01,
	//Line Length PCK Setting
	0x0342, 0x3C, 0x01,
	0x0343, 0x90, 0x01,
	//Frame Length Lines Setting
	0x0340, 0x0E, 0x01,
	0x0341, 0x7F, 0x01,
	//ROI Setting
	0x0344, 0x00, 0x01,
	0x0345, 0x00, 0x01,
	0x0346, 0x05, 0x01,
	0x0347, 0xD8, 0x01,
	0x0348, 0x1F, 0x01,
	0x0349, 0x3F, 0x01,
	0x034A, 0x11, 0x01,
	0x034B, 0x97, 0x01,
	//Mode Setting
	0x0220, 0x62, 0x01,
	0x0222, 0x01, 0x01,
	0x0900, 0x00, 0x01,
	0x0901, 0x11, 0x01,
	0x0902, 0x0A, 0x01,
	0x3140, 0x00, 0x01,
	0x3246, 0x01, 0x01,
	0x3247, 0x01, 0x01,
	0x3F15, 0x00, 0x01,
	//Digital Crop & Scaling
	0x0401, 0x00, 0x01,
	0x0404, 0x00, 0x01,
	0x0405, 0x10, 0x01,
	0x0408, 0x07, 0x01,
	0x0409, 0xD0, 0x01,
	0x040A, 0x00, 0x01,
	0x040B, 0x04, 0x01,
	0x040C, 0x0F, 0x01,
	0x040D, 0xA0, 0x01,
	0x040E, 0x0B, 0x01,
	0x040F, 0xB8, 0x01,
	//Output Size Setting
	0x034C, 0x0F, 0x01,
	0x034D, 0xA0, 0x01,
	0x034E, 0x0B, 0x01,
	0x034F, 0xB8, 0x01,
	//Clock Setting
	0x0301, 0x05, 0x01,
	0x0303, 0x02, 0x01,
	0x0305, 0x04, 0x01,
	0x0306, 0x01, 0x01,
	0x0307, 0x4C, 0x01,
	0x030B, 0x01, 0x01,
	0x030D, 0x03, 0x01,
	0x030E, 0x00, 0x01,
	0x030F, 0x9C, 0x01,
	0x0310, 0x01, 0x01,
	//Other Setting
	0x3620, 0x01, 0x01,
	0x3621, 0x01, 0x01,
	0x3C11, 0x08, 0x01,
	0x3C12, 0x08, 0x01,
	0x3C13, 0x2A, 0x01,
	0x3F0C, 0x00, 0x01,
	0x3F14, 0x01, 0x01,
	0x3F80, 0x08, 0x01,
	0x3F81, 0x76, 0x01,
	0x3F8C, 0x00, 0x01,
	0x3F8D, 0x14, 0x01,
	0x3FF8, 0x00, 0x01,
	0x3FF9, 0x00, 0x01,
	0x3FFE, 0x03, 0x01,
	0x3FFF, 0xA5, 0x01,
	//Integration Setting
	0x0202, 0x0E, 0x01,
	0x0203, 0x4F, 0x01,
	0x0224, 0x01, 0x01,
	0x0225, 0xF4, 0x01,
	0x3FE0, 0x01, 0x01,
	0x3FE1, 0xF4, 0x01,
	//Gain Setting
	0x0204, 0x00, 0x01,
	0x0205, 0x70, 0x01,
	0x0216, 0x00, 0x01,
	0x0217, 0x70, 0x01,
	0x0218, 0x01, 0x01,
	0x0219, 0x00, 0x01,
	0x020E, 0x01, 0x01,
	0x020F, 0x00, 0x01,
	0x0210, 0x01, 0x01,
	0x0211, 0x00, 0x01,
	0x0212, 0x01, 0x01,
	0x0213, 0x00, 0x01,
	0x0214, 0x01, 0x01,
	0x0215, 0x00, 0x01,
	0x3FE2, 0x00, 0x01,
	0x3FE3, 0x70, 0x01,
	0x3FE4, 0x01, 0x01,
	0x3FE5, 0x00, 0x01,
	//HDR output
	0x3141, 0x05, 0x01,
	//LSC Setting
	0x0B00, 0x00, 0x01,
	//AE Hist Setting
	0x323B, 0x00, 0x01,
	//Flicker Setting
	0x323C, 0x00, 0x01,
	//DPC output Setting
	0x0B06, 0x01, 0x01,
	//PDAF TYPE Setting
	0x3E20, 0x02, 0x01,
	0x3E3B, 0x01, 0x01,
	0x4434, 0x00, 0x01,
	0x4435, 0x7C, 0x01,
};

const u32 sensor_imx586_setfile_A_2X2BIN_FULL_4000x3000_30FPS[] = {
	//MIPI output setting
	0x0112, 0x0A, 0x01,
	0x0113, 0x0A, 0x01,
	0x0114, 0x03, 0x01,
	//Line Length PCK Setting
	0x0342, 0x46, 0x01,
	0x0343, 0x20, 0x01,
	//Frame Length Lines Setting
	0x0340, 0x0C, 0x01,
	0x0341, 0x84, 0x01,
	//ROI Setting
	0x0344, 0x00, 0x01,
	0x0345, 0x00, 0x01,
	0x0346, 0x00, 0x01,
	0x0347, 0x00, 0x01,
	0x0348, 0x1F, 0x01,
	0x0349, 0x3F, 0x01,
	0x034A, 0x17, 0x01,
	0x034B, 0x6F, 0x01,
	//Mode Setting
	0x0220, 0x62, 0x01,
	0x0222, 0x01, 0x01,
	0x0900, 0x01, 0x01,
	0x0901, 0x22, 0x01,
	0x0902, 0x08, 0x01,
	0x3140, 0x00, 0x01,
	0x3246, 0x81, 0x01,
	0x3247, 0x81, 0x01,
	0x3F15, 0x00, 0x01,
	//Digital Crop & Scaling
	0x0401, 0x00, 0x01,
	0x0404, 0x00, 0x01,
	0x0405, 0x10, 0x01,
	0x0408, 0x00, 0x01,
	0x0409, 0x00, 0x01,
	0x040A, 0x00, 0x01,
	0x040B, 0x00, 0x01,
	0x040C, 0x0F, 0x01,
	0x040D, 0xA0, 0x01,
	0x040E, 0x0B, 0x01,
	0x040F, 0xB8, 0x01,
	//Output Size Setting
	0x034C, 0x0F, 0x01,
	0x034D, 0xA0, 0x01,
	0x034E, 0x0B, 0x01,
	0x034F, 0xB8, 0x01,
	//Clock Setting
	0x0301, 0x05, 0x01,
	0x0303, 0x02, 0x01,
	0x0305, 0x04, 0x01,
	0x0306, 0x01, 0x01,
	0x0307, 0x4C, 0x01,
	0x030B, 0x01, 0x01,
	0x030D, 0x03, 0x01,
	0x030E, 0x00, 0x01,
	0x030F, 0x9C, 0x01,
	0x0310, 0x01, 0x01,
	//Other Setting
	0x3620, 0x00, 0x01,
	0x3621, 0x00, 0x01,
	0x3C11, 0x04, 0x01,
	0x3C12, 0x03, 0x01,
	0x3C13, 0x2D, 0x01,
	0x3F0C, 0x01, 0x01,
	0x3F14, 0x00, 0x01,
	0x3F80, 0x00, 0x01,
	0x3F81, 0x14, 0x01,
	0x3F8C, 0x00, 0x01,
	0x3F8D, 0x14, 0x01,
	0x3FF8, 0x01, 0x01,
	0x3FF9, 0x18, 0x01,
	0x3FFE, 0x01, 0x01,
	0x3FFF, 0xE0, 0x01,
	//Integration Setting
	0x0202, 0x0C, 0x01,
	0x0203, 0x54, 0x01,
	0x0224, 0x01, 0x01,
	0x0225, 0xF4, 0x01,
	0x3FE0, 0x01, 0x01,
	0x3FE1, 0xF4, 0x01,
	//Gain Setting
	0x0204, 0x00, 0x01,
	0x0205, 0x70, 0x01,
	0x0216, 0x00, 0x01,
	0x0217, 0x70, 0x01,
	0x0218, 0x01, 0x01,
	0x0219, 0x00, 0x01,
	0x020E, 0x01, 0x01,
	0x020F, 0x00, 0x01,
	0x0210, 0x01, 0x01,
	0x0211, 0x00, 0x01,
	0x0212, 0x01, 0x01,
	0x0213, 0x00, 0x01,
	0x0214, 0x01, 0x01,
	0x0215, 0x00, 0x01,
	0x3FE2, 0x00, 0x01,
	0x3FE3, 0x70, 0x01,
	0x3FE4, 0x01, 0x01,
	0x3FE5, 0x00, 0x01,
	//HDR output
	0x3141, 0x05, 0x01,
	//LSC Setting
	0x0B00, 0x00, 0x01,
	//AE Hist Setting
	0x323B, 0x00, 0x01,
	//Flicker Setting
	0x323C, 0x00, 0x01,
	//DPC output Setting
	0x0B06, 0x00, 0x01,
	//PDAF TYPE Setting
	0x3E20, 0x02, 0x01,
	0x3E3B, 0x01, 0x01,
	0x4434, 0x01, 0x01,
	0x4435, 0xF0, 0x01,
};

const u32 sensor_imx586_setfile_A_2X2BIN_CROP_4000x2252_30FPS[] = {
	//MIPI output setting
	0x0112, 0x0A, 0x01,
	0x0113, 0x0A, 0x01,
	0x0114, 0x03, 0x01,
	//Line Length PCK Setting
	0x0342, 0x46, 0x01,
	0x0343, 0x20, 0x01,
	//Frame Length Lines Setting
	0x0340, 0x0C, 0x01,
	0x0341, 0x84, 0x01,
	//ROI Setting
	0x0344, 0x00, 0x01,
	0x0345, 0x00, 0x01,
	0x0346, 0x02, 0x01,
	0x0347, 0xE8, 0x01,
	0x0348, 0x1F, 0x01,
	0x0349, 0x3F, 0x01,
	0x034A, 0x14, 0x01,
	0x034B, 0x7F, 0x01,
	//Mode Setting
	0x0220, 0x62, 0x01,
	0x0222, 0x01, 0x01,
	0x0900, 0x01, 0x01,
	0x0901, 0x22, 0x01,
	0x0902, 0x08, 0x01,
	0x3140, 0x00, 0x01,
	0x3246, 0x81, 0x01,
	0x3247, 0x81, 0x01,
	0x3F15, 0x00, 0x01,
	//Digital Crop & Scaling
	0x0401, 0x00, 0x01,
	0x0404, 0x00, 0x01,
	0x0405, 0x10, 0x01,
	0x0408, 0x00, 0x01,
	0x0409, 0x00, 0x01,
	0x040A, 0x00, 0x01,
	0x040B, 0x00, 0x01,
	0x040C, 0x0F, 0x01,
	0x040D, 0xA0, 0x01,
	0x040E, 0x08, 0x01,
	0x040F, 0xCC, 0x01,
	//Output Size Setting
	0x034C, 0x0F, 0x01,
	0x034D, 0xA0, 0x01,
	0x034E, 0x08, 0x01,
	0x034F, 0xCC, 0x01,
	//Clock Setting
	0x0301, 0x05, 0x01,
	0x0303, 0x02, 0x01,
	0x0305, 0x04, 0x01,
	0x0306, 0x01, 0x01,
	0x0307, 0x4C, 0x01,
	0x030B, 0x01, 0x01,
	0x030D, 0x03, 0x01,
	0x030E, 0x00, 0x01,
	0x030F, 0x9C, 0x01,
	0x0310, 0x01, 0x01,
	//Other Setting
	0x3620, 0x00, 0x01,
	0x3621, 0x00, 0x01,
	0x3C11, 0x04, 0x01,
	0x3C12, 0x03, 0x01,
	0x3C13, 0x2D, 0x01,
	0x3F0C, 0x01, 0x01,
	0x3F14, 0x00, 0x01,
	0x3F80, 0x00, 0x01,
	0x3F81, 0x14, 0x01,
	0x3F8C, 0x00, 0x01,
	0x3F8D, 0x14, 0x01,
	0x3FF8, 0x01, 0x01,
	0x3FF9, 0x18, 0x01,
	0x3FFE, 0x01, 0x01,
	0x3FFF, 0xE0, 0x01,
	//Integration Setting
	0x0202, 0x0C, 0x01,
	0x0203, 0x54, 0x01,
	0x0224, 0x01, 0x01,
	0x0225, 0xF4, 0x01,
	0x3FE0, 0x01, 0x01,
	0x3FE1, 0xF4, 0x01,
	//Gain Setting
	0x0204, 0x00, 0x01,
	0x0205, 0x70, 0x01,
	0x0216, 0x00, 0x01,
	0x0217, 0x70, 0x01,
	0x0218, 0x01, 0x01,
	0x0219, 0x00, 0x01,
	0x020E, 0x01, 0x01,
	0x020F, 0x00, 0x01,
	0x0210, 0x01, 0x01,
	0x0211, 0x00, 0x01,
	0x0212, 0x01, 0x01,
	0x0213, 0x00, 0x01,
	0x0214, 0x01, 0x01,
	0x0215, 0x00, 0x01,
	0x3FE2, 0x00, 0x01,
	0x3FE3, 0x70, 0x01,
	0x3FE4, 0x01, 0x01,
	0x3FE5, 0x00, 0x01,
	//HDR output
	0x3141, 0x05, 0x01,
	//LSC Setting
	0x0B00, 0x00, 0x01,
	//AE Hist Setting
	0x323B, 0x00, 0x01,
	//Flicker Setting
	0x323C, 0x00, 0x01,
	//DPC output Setting
	0x0B06, 0x00, 0x01,
	//PDAF TYPE Setting
	0x3E20, 0x02, 0x01,
	0x3E3B, 0x01, 0x01,
	0x4434, 0x01, 0x01,
	0x4435, 0xF0, 0x01,
};

const u32 sensor_imx586_setfile_A_REMOSAIC_CROP_4000x2252_30FPS[] = {
	//MIPI output setting
	0x0112, 0x0A, 0x01,
	0x0113, 0x0A, 0x01,
	0x0114, 0x03, 0x01,
	//Line Length PCK Setting
	0x0342, 0x3C, 0x01,
	0x0343, 0x90, 0x01,
	//Frame Length Lines Setting
	0x0340, 0x0E, 0x01,
	0x0341, 0x7F, 0x01,
	//ROI Setting
	0x0344, 0x00, 0x01,
	0x0345, 0x00, 0x01,
	0x0346, 0x05, 0x01,
	0x0347, 0xD8, 0x01,
	0x0348, 0x1F, 0x01,
	0x0349, 0x3F, 0x01,
	0x034A, 0x11, 0x01,
	0x034B, 0x97, 0x01,
	//Mode Setting
	0x0220, 0x62, 0x01,
	0x0222, 0x01, 0x01,
	0x0900, 0x00, 0x01,
	0x0901, 0x11, 0x01,
	0x0902, 0x0A, 0x01,
	0x3140, 0x00, 0x01,
	0x3246, 0x01, 0x01,
	0x3247, 0x01, 0x01,
	0x3F15, 0x00, 0x01,
	//Digital Crop & Scaling
	0x0401, 0x00, 0x01,
	0x0404, 0x00, 0x01,
	0x0405, 0x10, 0x01,
	0x0408, 0x07, 0x01,
	0x0409, 0xD0, 0x01,
	0x040A, 0x01, 0x01,
	0x040B, 0x7A, 0x01,
	0x040C, 0x0F, 0x01,
	0x040D, 0xA0, 0x01,
	0x040E, 0x08, 0x01,
	0x040F, 0xCC, 0x01,
	//Output Size Setting
	0x034C, 0x0F, 0x01,
	0x034D, 0xA0, 0x01,
	0x034E, 0x08, 0x01,
	0x034F, 0xCC, 0x01,
	//Clock Setting
	0x0301, 0x05, 0x01,
	0x0303, 0x02, 0x01,
	0x0305, 0x04, 0x01,
	0x0306, 0x01, 0x01,
	0x0307, 0x4C, 0x01,
	0x030B, 0x01, 0x01,
	0x030D, 0x03, 0x01,
	0x030E, 0x00, 0x01,
	0x030F, 0x9C, 0x01,
	0x0310, 0x01, 0x01,
	//Other Setting
	0x3620, 0x01, 0x01,
	0x3621, 0x01, 0x01,
	0x3C11, 0x08, 0x01,
	0x3C12, 0x08, 0x01,
	0x3C13, 0x2A, 0x01,
	0x3F0C, 0x00, 0x01,
	0x3F14, 0x01, 0x01,
	0x3F80, 0x08, 0x01,
	0x3F81, 0x76, 0x01,
	0x3F8C, 0x00, 0x01,
	0x3F8D, 0x14, 0x01,
	0x3FF8, 0x00, 0x01,
	0x3FF9, 0x00, 0x01,
	0x3FFE, 0x03, 0x01,
	0x3FFF, 0xA5, 0x01,
	//Integration Setting
	0x0202, 0x0E, 0x01,
	0x0203, 0x4F, 0x01,
	0x0224, 0x01, 0x01,
	0x0225, 0xF4, 0x01,
	0x3FE0, 0x01, 0x01,
	0x3FE1, 0xF4, 0x01,
	//Gain Setting
	0x0204, 0x00, 0x01,
	0x0205, 0x70, 0x01,
	0x0216, 0x00, 0x01,
	0x0217, 0x70, 0x01,
	0x0218, 0x01, 0x01,
	0x0219, 0x00, 0x01,
	0x020E, 0x01, 0x01,
	0x020F, 0x00, 0x01,
	0x0210, 0x01, 0x01,
	0x0211, 0x00, 0x01,
	0x0212, 0x01, 0x01,
	0x0213, 0x00, 0x01,
	0x0214, 0x01, 0x01,
	0x0215, 0x00, 0x01,
	0x3FE2, 0x00, 0x01,
	0x3FE3, 0x70, 0x01,
	0x3FE4, 0x01, 0x01,
	0x3FE5, 0x00, 0x01,
	//HDR output
	0x3141, 0x05, 0x01,
	//LSC Setting
	0x0B00, 0x00, 0x01,
	//AE Hist Setting
	0x323B, 0x00, 0x01,
	//Flicker Setting
	0x323C, 0x00, 0x01,
	//DPC output Setting
	0x0B06, 0x01, 0x01,
	//PDAF TYPE Setting
	0x3E20, 0x02, 0x01,
	0x3E3B, 0x01, 0x01,
	0x4434, 0x00, 0x01,
	0x4435, 0x7C, 0x01,
};

const u32 sensor_imx586_setfile_A_2X2BIN_CROP_4000x2252_60FPS[] = {
	//MIPI output setting
	0x0112, 0x0A, 0x01,
	0x0113, 0x0A, 0x01,
	0x0114, 0x03, 0x01,
	//Line Length PCK Setting
	0x0342, 0x23, 0x01,
	0x0343, 0x10, 0x01,
	//Frame Length Lines Setting
	0x0340, 0x0C, 0x01,
	0x0341, 0x84, 0x01,
	//ROI Setting
	0x0344, 0x00, 0x01,
	0x0345, 0x00, 0x01,
	0x0346, 0x02, 0x01,
	0x0347, 0xE8, 0x01,
	0x0348, 0x1F, 0x01,
	0x0349, 0x3F, 0x01,
	0x034A, 0x14, 0x01,
	0x034B, 0x7F, 0x01,
	//Mode Setting
	0x0220, 0x62, 0x01,
	0x0222, 0x01, 0x01,
	0x0900, 0x01, 0x01,
	0x0901, 0x22, 0x01,
	0x0902, 0x08, 0x01,
	0x3140, 0x00, 0x01,
	0x3246, 0x81, 0x01,
	0x3247, 0x81, 0x01,
	0x3F15, 0x00, 0x01,
	//Digital Crop & Scaling
	0x0401, 0x00, 0x01,
	0x0404, 0x00, 0x01,
	0x0405, 0x10, 0x01,
	0x0408, 0x00, 0x01,
	0x0409, 0x00, 0x01,
	0x040A, 0x00, 0x01,
	0x040B, 0x00, 0x01,
	0x040C, 0x0F, 0x01,
	0x040D, 0xA0, 0x01,
	0x040E, 0x08, 0x01,
	0x040F, 0xCC, 0x01,
	//Output Size Setting
	0x034C, 0x0F, 0x01,
	0x034D, 0xA0, 0x01,
	0x034E, 0x08, 0x01,
	0x034F, 0xCC, 0x01,
	//Clock Setting
	0x0301, 0x05, 0x01,
	0x0303, 0x02, 0x01,
	0x0305, 0x04, 0x01,
	0x0306, 0x01, 0x01,
	0x0307, 0x4C, 0x01,
	0x030B, 0x01, 0x01,
	0x030D, 0x03, 0x01,
	0x030E, 0x01, 0x01,
	0x030F, 0x1F, 0x01,
	0x0310, 0x01, 0x01,
	//Other Setting
	0x3620, 0x00, 0x01,
	0x3621, 0x00, 0x01,
	0x3C11, 0x04, 0x01,
	0x3C12, 0x03, 0x01,
	0x3C13, 0x2D, 0x01,
	0x3F0C, 0x01, 0x01,
	0x3F14, 0x00, 0x01,
	0x3F80, 0x05, 0x01,
	0x3F81, 0x8C, 0x01,
	0x3F8C, 0x00, 0x01,
	0x3F8D, 0x14, 0x01,
	0x3FF8, 0x00, 0x01,
	0x3FF9, 0x3C, 0x01,
	0x3FFE, 0x01, 0x01,
	0x3FFF, 0xF0, 0x01,
	//Integration Setting
	0x0202, 0x0C, 0x01,
	0x0203, 0x54, 0x01,
	0x0224, 0x01, 0x01,
	0x0225, 0xF4, 0x01,
	0x3FE0, 0x01, 0x01,
	0x3FE1, 0xF4, 0x01,
	//Gain Setting
	0x0204, 0x00, 0x01,
	0x0205, 0x70, 0x01,
	0x0216, 0x00, 0x01,
	0x0217, 0x70, 0x01,
	0x0218, 0x01, 0x01,
	0x0219, 0x00, 0x01,
	0x020E, 0x01, 0x01,
	0x020F, 0x00, 0x01,
	0x0210, 0x01, 0x01,
	0x0211, 0x00, 0x01,
	0x0212, 0x01, 0x01,
	0x0213, 0x00, 0x01,
	0x0214, 0x01, 0x01,
	0x0215, 0x00, 0x01,
	0x3FE2, 0x00, 0x01,
	0x3FE3, 0x70, 0x01,
	0x3FE4, 0x01, 0x01,
	0x3FE5, 0x00, 0x01,
	//HDR output
	0x3141, 0x05, 0x01,
	//LSC Setting
	0x0B00, 0x00, 0x01,
	//AE Hist Setting
	0x323B, 0x00, 0x01,
	//Flicker Setting
	0x323C, 0x00, 0x01,
	//DPC output Setting
	0x0B06, 0x00, 0x01,
	//PDAF TYPE Setting
	0x3E20, 0x02, 0x01,
	0x3E3B, 0x01, 0x01,
	0x4434, 0x01, 0x01,
	0x4435, 0xF0, 0x01,
};

const u32 sensor_imx586_setfile_A_2X2BIN_CROP_1984X1488_30FPS[] = {
	//MIPI output setting
	0x0112, 0x0A, 0x01,
	0x0113, 0x0A, 0x01,
	0x0114, 0x03, 0x01,
	//Line Length PCK Setting
	0x0342, 0x23, 0x01,
	0x0343, 0x10, 0x01,
	//Frame Length Lines Setting
	0x0340, 0x19, 0x01,
	0x0341, 0x0A, 0x01,
	//ROI Setting
	0x0344, 0x00, 0x01,
	0x0345, 0x00, 0x01,
	0x0346, 0x05, 0x01,
	0x0347, 0xE8, 0x01,
	0x0348, 0x1F, 0x01,
	0x0349, 0x3F, 0x01,
	0x034A, 0x11, 0x01,
	0x034B, 0x87, 0x01,
	//Mode Setting
	0x0220, 0x62, 0x01,
	0x0222, 0x01, 0x01,
	0x0900, 0x01, 0x01,
	0x0901, 0x22, 0x01,
	0x0902, 0x08, 0x01,
	0x3140, 0x00, 0x01,
	0x3246, 0x81, 0x01,
	0x3247, 0x81, 0x01,
	0x3F15, 0x00, 0x01,
	//Digital Crop & Scaling
	0x0401, 0x00, 0x01,
	0x0404, 0x00, 0x01,
	0x0405, 0x10, 0x01,
	0x0408, 0x03, 0x01,
	0x0409, 0xF0, 0x01,
	0x040A, 0x00, 0x01,
	0x040B, 0x00, 0x01,
	0x040C, 0x07, 0x01,
	0x040D, 0xC0, 0x01,
	0x040E, 0x05, 0x01,
	0x040F, 0xD0, 0x01,
	//Output Size Setting
	0x034C, 0x07, 0x01,
	0x034D, 0xC0, 0x01,
	0x034E, 0x05, 0x01,
	0x034F, 0xD0, 0x01,
	//Clock Setting
	0x0301, 0x05, 0x01,
	0x0303, 0x02, 0x01,
	0x0305, 0x04, 0x01,
	0x0306, 0x01, 0x01,
	0x0307, 0x4C, 0x01,
	0x030B, 0x01, 0x01,
	0x030D, 0x03, 0x01,
	0x030E, 0x00, 0x01,
	0x030F, 0x9C, 0x01,
	0x0310, 0x01, 0x01,
	//Other Setting
	0x3620, 0x00, 0x01,
	0x3621, 0x00, 0x01,
	0x3C11, 0x04, 0x01,
	0x3C12, 0x03, 0x01,
	0x3C13, 0x2D, 0x01,
	0x3F0C, 0x01, 0x01,
	0x3F14, 0x00, 0x01,
	0x3F80, 0x05, 0x01,
	0x3F81, 0x8C, 0x01,
	0x3F8C, 0x00, 0x01,
	0x3F8D, 0x14, 0x01,
	0x3FF8, 0x00, 0x01,
	0x3FF9, 0x3C, 0x01,
	0x3FFE, 0x01, 0x01,
	0x3FFF, 0xF0, 0x01,
	//Integration Setting
	0x0202, 0x18, 0x01,
	0x0203, 0xDA, 0x01,
	0x0224, 0x01, 0x01,
	0x0225, 0xF4, 0x01,
	0x3FE0, 0x01, 0x01,
	0x3FE1, 0xF4, 0x01,
	//Gain Setting
	0x0204, 0x00, 0x01,
	0x0205, 0x70, 0x01,
	0x0216, 0x00, 0x01,
	0x0217, 0x70, 0x01,
	0x0218, 0x01, 0x01,
	0x0219, 0x00, 0x01,
	0x020E, 0x01, 0x01,
	0x020F, 0x00, 0x01,
	0x0210, 0x01, 0x01,
	0x0211, 0x00, 0x01,
	0x0212, 0x01, 0x01,
	0x0213, 0x00, 0x01,
	0x0214, 0x01, 0x01,
	0x0215, 0x00, 0x01,
	0x3FE2, 0x00, 0x01,
	0x3FE3, 0x70, 0x01,
	0x3FE4, 0x01, 0x01,
	0x3FE5, 0x00, 0x01,
	//HDR output
	0x3141, 0x05, 0x01,
	//LSC Setting
	0x0B00, 0x00, 0x01,
	//AE Hist Setting
	0x323B, 0x00, 0x01,
	//Flicker Setting
	0x323C, 0x00, 0x01,
	//DPC output Setting
	0x0B06, 0x00, 0x01,
	//PDAF TYPE Setting
	0x3E20, 0x02, 0x01,
	0x3E3B, 0x01, 0x01,
	0x4434, 0x00, 0x01,
	0x4435, 0xF8, 0x01,
};

const u32 sensor_imx582_setfile_A_2X2BIN_V2H2_992X744_120FPS[] = {
	//MIPI output setting
	0x0112, 0x0A, 0x01,
	0x0113, 0x0A, 0x01,
	0x0114, 0x03, 0x01,
	//Line Length PCK Setting
	0x0342, 0x15, 0x01,
	0x0343, 0x00, 0x01,
	//Frame Length Lines Setting
	0x0340, 0x0A, 0x01,
	0x0341, 0x74, 0x01,
	//ROI Setting
	0x0344, 0x00, 0x01,
	0x0345, 0x00, 0x01,
	0x0346, 0x05, 0x01,
	0x0347, 0xE8, 0x01,
	0x0348, 0x1F, 0x01,
	0x0349, 0x3F, 0x01,
	0x034A, 0x11, 0x01,
	0x034B, 0x87, 0x01,
	//Mode Setting
	0x0220, 0x62, 0x01,
	0x0222, 0x01, 0x01,
	0x0900, 0x01, 0x01,
	0x0901, 0x44, 0x01,
	0x0902, 0x08, 0x01,
	0x3140, 0x00, 0x01,
	0x3246, 0x89, 0x01,
	0x3247, 0x89, 0x01,
	0x3F15, 0x00, 0x01,
	//Digital Crop & Scaling
	0x0401, 0x00, 0x01,
	0x0404, 0x00, 0x01,
	0x0405, 0x10, 0x01,
	0x0408, 0x01, 0x01,
	0x0409, 0xF8, 0x01,
	0x040A, 0x00, 0x01,
	0x040B, 0x00, 0x01,
	0x040C, 0x03, 0x01,
	0x040D, 0xE0, 0x01,
	0x040E, 0x02, 0x01,
	0x040F, 0xE8, 0x01,
	//Output Size Setting
	0x034C, 0x03, 0x01,
	0x034D, 0xE0, 0x01,
	0x034E, 0x02, 0x01,
	0x034F, 0xE8, 0x01,
	//Clock Setting
	0x0301, 0x05, 0x01,
	0x0303, 0x02, 0x01,
	0x0305, 0x04, 0x01,
	0x0306, 0x01, 0x01,
	0x0307, 0x4C, 0x01,
	0x030B, 0x01, 0x01,
	0x030D, 0x03, 0x01,
	0x030E, 0x00, 0x01,
	0x030F, 0x9C, 0x01,
	0x0310, 0x01, 0x01,
	//Other Setting
	0x3620, 0x00, 0x01,
	0x3621, 0x00, 0x01,
	0x3C11, 0x0C, 0x01,
	0x3C12, 0x05, 0x01,
	0x3C13, 0x2C, 0x01,
	0x3F0C, 0x00, 0x01,
	0x3F14, 0x00, 0x01,
	0x3F80, 0x02, 0x01,
	0x3F81, 0x67, 0x01,
	0x3F8C, 0x02, 0x01,
	0x3F8D, 0x44, 0x01,
	0x3FF8, 0x00, 0x01,
	0x3FF9, 0x00, 0x01,
	0x3FFE, 0x01, 0x01,
	0x3FFF, 0x90, 0x01,
	//Integration Setting
	0x0202, 0x0A, 0x01,
	0x0203, 0x44, 0x01,
	0x0224, 0x01, 0x01,
	0x0225, 0xF4, 0x01,
	0x3FE0, 0x01, 0x01,
	0x3FE1, 0xF4, 0x01,
	//Gain Setting
	0x0204, 0x00, 0x01,
	0x0205, 0x70, 0x01,
	0x0216, 0x00, 0x01,
	0x0217, 0x70, 0x01,
	0x0218, 0x01, 0x01,
	0x0219, 0x00, 0x01,
	0x020E, 0x01, 0x01,
	0x020F, 0x00, 0x01,
	0x0210, 0x01, 0x01,
	0x0211, 0x00, 0x01,
	0x0212, 0x01, 0x01,
	0x0213, 0x00, 0x01,
	0x0214, 0x01, 0x01,
	0x0215, 0x00, 0x01,
	0x3FE2, 0x00, 0x01,
	0x3FE3, 0x70, 0x01,
	0x3FE4, 0x01, 0x01,
	0x3FE5, 0x00, 0x01,
	//HDR output
	0x3141, 0x05, 0x01,
	//LSC Setting
	0x0B00, 0x00, 0x01,
	//AE Hist Setting
	0x323B, 0x00, 0x01,
	//Flicker Setting
	0x323C, 0x00, 0x01,
	//DPC output Setting
	0x0B06, 0x00, 0x01,
	//PDAF TYPE Setting
	0x3E20, 0x01, 0x01,
	0x3E3B, 0x00, 0x01,
	0x4434, 0x00, 0x01,
	0x4435, 0xF8, 0x01,
};

const struct sensor_pll_info_compact sensor_imx582_pllinfo_A_REMOSAIC_FULL_8000x6000_15FPS = {
	EXT_CLK_Mhz * 1000 * 1000,   /* ext_clk */
	2487000000ULL,               /* mipi_datarate = OPSYCK */
	215800000ULL,                /* pclk = VTPXCK of Clock Information */
	6186,                        /* frame_length_lines */
	15504,                       /* line_length_pck */
};

const struct sensor_pll_info_compact sensor_imx586_pllinfo_A_REMOSAIC_CROP_4000x3000_30FPS = {
	EXT_CLK_Mhz * 1000 * 1000,   /* ext_clk */
	1352000000ULL,               /* mipi_datarate = OPSYCK */
	215800000ULL,                /* pclk = VTPXCK of Clock Information */
	3711,                        /* frame_length_lines */
	15504,                       /* line_length_pck */
};

const struct sensor_pll_info_compact sensor_imx586_pllinfo_A_2X2BIN_FULL_4000x3000_30FPS = {
	EXT_CLK_Mhz * 1000 * 1000,   /* ext_clk */
	1352000000ULL,               /* mipi_datarate = OPSYCK */
	215800000ULL,                /* pclk = VTPXCK of Clock Information */
	3204,                        /* frame_length_lines */
	17952,                       /* line_length_pck */
};

const struct sensor_pll_info_compact sensor_imx582_pllinfo_A_2X2BIN_CROP_4000x2252_30FPS = {
	EXT_CLK_Mhz * 1000 * 1000,   /* ext_clk */
	1352000000ULL,               /* mipi_datarate = OPSYCK */
	215800000ULL,                /* pclk = VTPXCK of Clock Information */
	3204,                        /* frame_length_lines */
	17952,                       /* line_length_pck */
};

const struct sensor_pll_info_compact sensor_imx586_pllinfo_A_REMOSAIC_CROP_4000x2252_30FPS = {
	EXT_CLK_Mhz * 1000 * 1000,   /* ext_clk */
	1352000000ULL,               /* mipi_datarate = OPSYCK */
	215800000ULL,                /* pclk = VTPXCK of Clock Information */
	3711,                        /* frame_length_lines */
	15504,                       /* line_length_pck */
};

const struct sensor_pll_info_compact sensor_imx582_pllinfo_A_2X2BIN_CROP_4000x2252_60FPS = {
	EXT_CLK_Mhz * 1000 * 1000,   /* ext_clk */
	2487000000ULL,               /* mipi_datarate = OPSYCK */
	215800000ULL,                /* pclk = VTPXCK of Clock Information */
	3204,                        /* frame_length_lines */
	8976,                        /* line_length_pck */
};

const struct sensor_pll_info_compact sensor_imx586_pllinfo_A_2X2BIN_FULL_1984X1488_30FPS = {
	EXT_CLK_Mhz * 1000 * 1000,   /* ext_clk */
	1352000000ULL,               /* mipi_datarate = OPSYCK */
	215800000ULL,                /* pclk = VTPXCK of Clock Information */
	6410,                        /* frame_length_lines */
	8976,                        /* line_length_pck */
};

const struct sensor_pll_info_compact sensor_imx582_pllinfo_A_2X2BIN_V2H2_992X744_120FPS = {
	EXT_CLK_Mhz * 1000 * 1000,   /* ext_clk */
	1352000000ULL,               /* mipi_datarate = OPSYCK */
	215800000ULL,                /* pclk = VTPXCK of Clock Information */
	2676,                        /* frame_length_lines */
	5376,                        /* line_length_pck */
};

static const u32 *sensor_imx586_setfiles_A[] = {
	sensor_imx582_setfile_A_REMOSAIC_FULL_8000x6000_15FPS,
	sensor_imx586_setfile_A_2X2BIN_FULL_4000x3000_30FPS,
	sensor_imx586_setfile_A_REMOSAIC_CROP_4000x3000_30FPS,
	sensor_imx586_setfile_A_2X2BIN_CROP_4000x2252_30FPS,
	sensor_imx586_setfile_A_2X2BIN_CROP_4000x2252_60FPS,
	sensor_imx586_setfile_A_2X2BIN_CROP_1984X1488_30FPS,
	sensor_imx582_setfile_A_2X2BIN_V2H2_992X744_120FPS,
	sensor_imx586_setfile_A_REMOSAIC_CROP_4000x2252_30FPS,
};

static const u32 sensor_imx586_setfile_A_sizes[] = {
	ARRAY_SIZE(sensor_imx582_setfile_A_REMOSAIC_FULL_8000x6000_15FPS),
	ARRAY_SIZE(sensor_imx586_setfile_A_2X2BIN_FULL_4000x3000_30FPS),
	ARRAY_SIZE(sensor_imx586_setfile_A_REMOSAIC_CROP_4000x3000_30FPS),
	ARRAY_SIZE(sensor_imx586_setfile_A_2X2BIN_CROP_4000x2252_30FPS),
	ARRAY_SIZE(sensor_imx586_setfile_A_2X2BIN_CROP_4000x2252_60FPS),
	ARRAY_SIZE(sensor_imx586_setfile_A_2X2BIN_CROP_1984X1488_30FPS),
	ARRAY_SIZE(sensor_imx582_setfile_A_2X2BIN_V2H2_992X744_120FPS),
	ARRAY_SIZE(sensor_imx586_setfile_A_REMOSAIC_CROP_4000x2252_30FPS),
};

static const struct sensor_pll_info_compact *sensor_imx586_pllinfos_A[] = {
	&sensor_imx582_pllinfo_A_REMOSAIC_FULL_8000x6000_15FPS,
	&sensor_imx586_pllinfo_A_2X2BIN_FULL_4000x3000_30FPS,
	&sensor_imx586_pllinfo_A_REMOSAIC_CROP_4000x3000_30FPS,
	&sensor_imx582_pllinfo_A_2X2BIN_CROP_4000x2252_30FPS,
	&sensor_imx582_pllinfo_A_2X2BIN_CROP_4000x2252_60FPS,
	&sensor_imx586_pllinfo_A_2X2BIN_FULL_1984X1488_30FPS,
	&sensor_imx582_pllinfo_A_2X2BIN_V2H2_992X744_120FPS,
	&sensor_imx586_pllinfo_A_REMOSAIC_CROP_4000x2252_30FPS,
};

/* merge into sensor driver */
enum {
	CAM_IMX586_SET_A_full_DEFAULT_MIPI_CLOCK = 0,
	CAM_IMX586_SET_A_full_1243p67_MHZ = 0,
	CAM_IMX586_SET_A_full_1248_MHZ = 1,
};

static const u32 sensor_IMX586_setfile_A_mipi_FULL_1243p67_mhz[] = {
	0x030F, 0x1F, 0x01,
};

static const u32 sensor_IMX586_setfile_A_mipi_FULL_1248_mhz[] = {
	0x030F, 0x20, 0x01,
};

static const struct cam_mipi_setting sensor_IMX586_setfile_A_mipi_setting_FULL[] = {
	{ "1243p67 MHz", 2487,
	  sensor_IMX586_setfile_A_mipi_FULL_1243p67_mhz, ARRAY_SIZE(sensor_IMX586_setfile_A_mipi_FULL_1243p67_mhz) },
	{ "1248 MHz", 2496,
	  sensor_IMX586_setfile_A_mipi_FULL_1248_mhz, ARRAY_SIZE(sensor_IMX586_setfile_A_mipi_FULL_1248_mhz) },
};

enum {
	CAM_IMX586_SET_A_binning_DEFAULT_MIPI_CLOCK = 0,
	CAM_IMX586_SET_A_binning_30fps_676_MHZ = 0,
	CAM_IMX586_SET_A_binning_30fps_719p33_MHZ = 1,
};

static const u32 sensor_IMX586_setfile_A_mipi_BINNING_676_mhz[] = {
	0x030F, 0x9C, 0x01,
};

static const u32 sensor_IMX586_setfile_A_mipi_BINNING_719p33_mhz[] = {
	0x030F, 0xA5, 0x01,
};

static const struct cam_mipi_setting sensor_IMX586_setfile_A_mipi_setting_BINNING[] = {
	{ "676 MHz", 1352,
	 sensor_IMX586_setfile_A_mipi_BINNING_676_mhz, ARRAY_SIZE(sensor_IMX586_setfile_A_mipi_BINNING_676_mhz) },
	{ "719p33 MHz", 1438,
	  sensor_IMX586_setfile_A_mipi_BINNING_719p33_mhz, ARRAY_SIZE(sensor_IMX586_setfile_A_mipi_BINNING_719p33_mhz) },
};

/* must be sorted. if not, trigger panic in is_vendor_verify_mipi_channel */
static const struct cam_mipi_channel sensor_IMX586_setfile_A_mipi_channel_FULL[] = {
	{ CAM_RAT_BAND(CAM_RAT_1_GSM, CAM_BAND_001_GSM_GSM850), 0, 0, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_1_GSM, CAM_BAND_002_GSM_EGSM900), 0, 0, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_1_GSM, CAM_BAND_003_GSM_DCS1800), 0, 0, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_1_GSM, CAM_BAND_004_GSM_PCS1900), 0, 0, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_011_WCDMA_WB01), 10562, 10589, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_011_WCDMA_WB01), 10590, 10838, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_012_WCDMA_WB02), 9662, 9914, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_012_WCDMA_WB02), 9915, 9938, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_013_WCDMA_WB03), 1162, 1417, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_013_WCDMA_WB03), 1418, 1468, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_013_WCDMA_WB03), 1469, 1513, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_014_WCDMA_WB04), 1537, 1564, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_014_WCDMA_WB04), 1565, 1738, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_015_WCDMA_WB05), 4357, 4458, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_017_WCDMA_WB07), 2237, 2459, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_017_WCDMA_WB07), 2460, 2517, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_017_WCDMA_WB07), 2518, 2563, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_018_WCDMA_WB08), 2937, 2971, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_018_WCDMA_WB08), 2972, 3088, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_091_LTE_LB01), 0, 79, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_091_LTE_LB01), 80, 599, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_092_LTE_LB02), 600, 1128, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_092_LTE_LB02), 1129, 1199, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_093_LTE_LB03), 1200, 1734, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_093_LTE_LB03), 1735, 1837, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_093_LTE_LB03), 1838, 1949, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_094_LTE_LB04), 1950, 2029, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_094_LTE_LB04), 2030, 2399, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_095_LTE_LB05), 2400, 2430, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_095_LTE_LB05), 2431, 2649, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_097_LTE_LB07), 2750, 3218, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_097_LTE_LB07), 3219, 3335, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_097_LTE_LB07), 3336, 3449, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_098_LTE_LB08), 3450, 3543, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_098_LTE_LB08), 3544, 3799, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_102_LTE_LB12), 5010, 5111, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_102_LTE_LB12), 5112, 5179, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_103_LTE_LB13), 5180, 5194, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_103_LTE_LB13), 5195, 5279, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_104_LTE_LB14), 5280, 5379, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_107_LTE_LB17), 5730, 5781, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_107_LTE_LB17), 5782, 5849, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_108_LTE_LB18), 5850, 5885, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_108_LTE_LB18), 5886, 5970, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_108_LTE_LB18), 5971, 5999, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_109_LTE_LB19), 6000, 6149, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_110_LTE_LB20), 6150, 6253, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_110_LTE_LB20), 6254, 6337, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_110_LTE_LB20), 6338, 6449, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_111_LTE_LB21), 6450, 6599, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_115_LTE_LB25), 8040, 8568, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_115_LTE_LB25), 8569, 8689, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_116_LTE_LB26), 8690, 8735, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_116_LTE_LB26), 8736, 8820, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_116_LTE_LB26), 8821, 9039, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_118_LTE_LB28), 9210, 9659, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_119_LTE_LB29), 9660, 9769, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_120_LTE_LB30), 9770, 9829, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_120_LTE_LB30), 9830, 9869, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_122_LTE_LB32), 9920, 10253, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_122_LTE_LB32), 10254, 10359, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_124_LTE_LB34), 36200, 36349, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_128_LTE_LB38), 37750, 38096, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_128_LTE_LB38), 38097, 38212, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_128_LTE_LB38), 38213, 38249, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_129_LTE_LB39), 38250, 38649, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_130_LTE_LB40), 38650, 38697, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_130_LTE_LB40), 38698, 39209, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_130_LTE_LB40), 39210, 39320, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_130_LTE_LB40), 39321, 39649, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41), 39650, 40115, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41), 40116, 40229, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41), 40230, 40736, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41), 40737, 40852, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41), 40853, 41358, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41), 41359, 41475, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41), 41476, 41589, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 41590, 41720, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 41721, 41850, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 41851, 42342, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 42343, 42473, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 42474, 42964, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 42965, 43096, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 43097, 43589, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_138_LTE_LB48), 55240, 55736, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_138_LTE_LB48), 55737, 55869, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_138_LTE_LB48), 55870, 56358, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_138_LTE_LB48), 56359, 56492, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_138_LTE_LB48), 56493, 56739, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_156_LTE_LB66), 66436, 66515, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_156_LTE_LB66), 66516, 67030, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_156_LTE_LB66), 67031, 67138, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_156_LTE_LB66), 67139, 67335, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_161_LTE_LB71), 68586, 68644, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_161_LTE_LB71), 68645, 68935, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_4_TDSCDMA, CAM_BAND_051_TDSCDMA_TD1), 0, 0, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_4_TDSCDMA, CAM_BAND_052_TDSCDMA_TD2), 0, 0, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_4_TDSCDMA, CAM_BAND_053_TDSCDMA_TD3), 0, 0, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_4_TDSCDMA, CAM_BAND_054_TDSCDMA_TD4), 0, 0, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_4_TDSCDMA, CAM_BAND_055_TDSCDMA_TD5), 0, 0, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_4_TDSCDMA, CAM_BAND_056_TDSCDMA_TD6), 0, 0, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_5_CDMA, CAM_BAND_061_CDMA_BC0), 0, 0, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_5_CDMA, CAM_BAND_062_CDMA_BC1), 0, 0, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_5_CDMA, CAM_BAND_071_CDMA_BC10), 0, 0, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_260_NR5G_N005), 173800, 174400, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_260_NR5G_N005), 174420, 178780, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_263_NR5G_N008), 185000, 186860, CAM_IMX586_SET_A_full_1248_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_263_NR5G_N008), 186880, 191980, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_283_NR5G_N028), 151600, 159260, CAM_IMX586_SET_A_full_1243p67_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_283_NR5G_N028), 159280, 160580, CAM_IMX586_SET_A_full_1248_MHZ },
};

static const struct cam_mipi_channel sensor_IMX586_setfile_A_mipi_channel_BINNING[] = {
	{ CAM_RAT_BAND(CAM_RAT_1_GSM, CAM_BAND_001_GSM_GSM850), 0, 0, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_1_GSM, CAM_BAND_002_GSM_EGSM900), 0, 0, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_1_GSM, CAM_BAND_003_GSM_DCS1800), 0, 0, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_1_GSM, CAM_BAND_004_GSM_PCS1900), 0, 0, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_011_WCDMA_WB01), 10562, 10628, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_011_WCDMA_WB01), 10629, 10681, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_011_WCDMA_WB01), 10682, 10802, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_011_WCDMA_WB01), 10803, 10838, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_012_WCDMA_WB02), 9662, 9667, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_012_WCDMA_WB02), 9668, 9766, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_012_WCDMA_WB02), 9767, 9836, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_012_WCDMA_WB02), 9837, 9935, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_012_WCDMA_WB02), 9936, 9938, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_013_WCDMA_WB03), 1162, 1215, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_013_WCDMA_WB03), 1216, 1273, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_013_WCDMA_WB03), 1274, 1384, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_013_WCDMA_WB03), 1385, 1448, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_013_WCDMA_WB03), 1449, 1513, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_014_WCDMA_WB04), 1537, 1603, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_014_WCDMA_WB04), 1604, 1656, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_014_WCDMA_WB04), 1657, 1738, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_015_WCDMA_WB05), 4357, 4358, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_015_WCDMA_WB05), 4359, 4428, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_015_WCDMA_WB05), 4429, 4458, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_017_WCDMA_WB07), 2237, 2279, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_017_WCDMA_WB07), 2280, 2341, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_017_WCDMA_WB07), 2342, 2454, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_017_WCDMA_WB07), 2455, 2510, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_017_WCDMA_WB07), 2511, 2563, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_018_WCDMA_WB08), 2937, 3003, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_018_WCDMA_WB08), 3004, 3066, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_018_WCDMA_WB08), 3067, 3088, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_091_LTE_LB01), 0, 157, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_091_LTE_LB01), 158, 263, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_091_LTE_LB01), 264, 505, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_091_LTE_LB01), 506, 599, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_092_LTE_LB02), 600, 635, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_092_LTE_LB02), 636, 833, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_092_LTE_LB02), 834, 973, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_092_LTE_LB02), 974, 1171, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_092_LTE_LB02), 1172, 1199, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_093_LTE_LB03), 1200, 1331, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_093_LTE_LB03), 1332, 1447, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_093_LTE_LB03), 1448, 1669, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_093_LTE_LB03), 1670, 1796, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_093_LTE_LB03), 1797, 1949, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_094_LTE_LB04), 1950, 2107, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_094_LTE_LB04), 2108, 2213, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_094_LTE_LB04), 2214, 2399, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_095_LTE_LB05), 2400, 2427, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_095_LTE_LB05), 2428, 2567, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_095_LTE_LB05), 2568, 2649, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_097_LTE_LB07), 2750, 2859, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_097_LTE_LB07), 2860, 2983, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_097_LTE_LB07), 2984, 3208, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_097_LTE_LB07), 3209, 3321, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_097_LTE_LB07), 3322, 3449, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_098_LTE_LB08), 3450, 3607, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_098_LTE_LB08), 3608, 3733, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_098_LTE_LB08), 3734, 3799, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_102_LTE_LB12), 5010, 5085, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_102_LTE_LB12), 5086, 5179, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_103_LTE_LB13), 5180, 5214, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_103_LTE_LB13), 5215, 5279, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_104_LTE_LB14), 5280, 5379, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_107_LTE_LB17), 5730, 5755, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_107_LTE_LB17), 5756, 5849, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_108_LTE_LB18), 5850, 5967, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_108_LTE_LB18), 5968, 5999, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_109_LTE_LB19), 6000, 6107, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_109_LTE_LB19), 6108, 6149, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_110_LTE_LB20), 6150, 6281, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_110_LTE_LB20), 6282, 6421, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_110_LTE_LB20), 6422, 6449, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_111_LTE_LB21), 6450, 6599, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_115_LTE_LB25), 8040, 8075, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_115_LTE_LB25), 8076, 8273, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_115_LTE_LB25), 8274, 8413, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_115_LTE_LB25), 8414, 8611, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_115_LTE_LB25), 8612, 8689, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_116_LTE_LB26), 8690, 8817, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_116_LTE_LB26), 8818, 8957, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_116_LTE_LB26), 8958, 9039, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_118_LTE_LB28), 9210, 9333, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_118_LTE_LB28), 9334, 9473, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_118_LTE_LB28), 9474, 9659, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_119_LTE_LB29), 9660, 9769, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_120_LTE_LB30), 9770, 9859, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_120_LTE_LB30), 9860, 9869, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_122_LTE_LB32), 9920, 10003, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_122_LTE_LB32), 10004, 10209, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_122_LTE_LB32), 10210, 10341, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_122_LTE_LB32), 10342, 10359, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_124_LTE_LB34), 36200, 36311, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_124_LTE_LB34), 36312, 36349, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_128_LTE_LB38), 37750, 37807, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_128_LTE_LB38), 37808, 38010, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_128_LTE_LB38), 38011, 38145, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_128_LTE_LB38), 38146, 38249, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_129_LTE_LB39), 38250, 38307, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_129_LTE_LB39), 38308, 38445, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_129_LTE_LB39), 38446, 38645, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_129_LTE_LB39), 38646, 38649, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_130_LTE_LB40), 38650, 38901, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_130_LTE_LB40), 38902, 39000, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_130_LTE_LB40), 39001, 39239, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_130_LTE_LB40), 39240, 39348, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_130_LTE_LB40), 39349, 39577, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_130_LTE_LB40), 39578, 39649, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41), 39650, 39771, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41), 39772, 39969, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41), 39970, 40109, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41), 40110, 40307, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41), 40308, 40447, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41), 40448, 40650, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41), 40651, 40785, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41), 40786, 40999, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41), 41000, 41123, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41), 41124, 41348, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41), 41349, 41461, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41), 41462, 41589, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 41590, 41658, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 41659, 41743, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 41744, 41996, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 41997, 42092, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 42093, 42334, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 42335, 42441, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 42442, 42672, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 42673, 42790, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 42791, 43010, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 43011, 43139, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 43140, 43348, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 43349, 43487, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 43488, 43589, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_138_LTE_LB48), 55240, 55288, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_138_LTE_LB48), 55289, 55498, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_138_LTE_LB48), 55499, 55637, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_138_LTE_LB48), 55638, 55836, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_138_LTE_LB48), 55837, 55976, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_138_LTE_LB48), 55977, 56174, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_138_LTE_LB48), 56175, 56314, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_138_LTE_LB48), 56315, 56512, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_138_LTE_LB48), 56513, 56652, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_138_LTE_LB48), 56653, 56739, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_156_LTE_LB66), 66436, 66593, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_156_LTE_LB66), 66594, 66699, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_156_LTE_LB66), 66700, 66941, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_156_LTE_LB66), 66942, 67037, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_156_LTE_LB66), 67038, 67288, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_156_LTE_LB66), 67289, 67335, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_161_LTE_LB71), 68586, 68767, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_161_LTE_LB71), 68768, 68863, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_161_LTE_LB71), 68864, 68935, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_4_TDSCDMA, CAM_BAND_051_TDSCDMA_TD1), 0, 0, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_4_TDSCDMA, CAM_BAND_052_TDSCDMA_TD2), 0, 0, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_4_TDSCDMA, CAM_BAND_053_TDSCDMA_TD3), 0, 0, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_4_TDSCDMA, CAM_BAND_054_TDSCDMA_TD4), 0, 0, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_4_TDSCDMA, CAM_BAND_055_TDSCDMA_TD5), 0, 0, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_4_TDSCDMA, CAM_BAND_056_TDSCDMA_TD6), 0, 0, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_5_CDMA, CAM_BAND_061_CDMA_BC0), 0, 0, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_5_CDMA, CAM_BAND_062_CDMA_BC1), 0, 0, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_5_CDMA, CAM_BAND_071_CDMA_BC10), 0, 0, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_260_NR5G_N005), 173800, 177780, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_260_NR5G_N005), 177800, 178780, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_263_NR5G_N008), 185000, 188140, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_263_NR5G_N008), 188160, 191660, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_263_NR5G_N008), 191680, 191980, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_283_NR5G_N028), 151600, 153260, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_283_NR5G_N028), 153280, 156860, CAM_IMX586_SET_A_binning_30fps_719p33_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_283_NR5G_N028), 156900, 160580, CAM_IMX586_SET_A_binning_30fps_676_MHZ },
};

static const struct cam_mipi_sensor_mode sensor_IMX586_setfile_A_mipi_sensor_mode[] = {
	{ SENSOR_IMX586_REMOSAIC_FULL_8000X6000_15FPS,
		sensor_IMX586_setfile_A_mipi_channel_FULL, ARRAY_SIZE(sensor_IMX586_setfile_A_mipi_channel_FULL),
		sensor_IMX586_setfile_A_mipi_setting_FULL, ARRAY_SIZE(sensor_IMX586_setfile_A_mipi_setting_FULL)
	},
	{ SENSOR_IMX586_2X2BIN_FULL_4000X3000_30FPS,
		sensor_IMX586_setfile_A_mipi_channel_BINNING,	ARRAY_SIZE(sensor_IMX586_setfile_A_mipi_channel_BINNING),
		sensor_IMX586_setfile_A_mipi_setting_BINNING,	ARRAY_SIZE(sensor_IMX586_setfile_A_mipi_setting_BINNING)
	},
	{ SENSOR_IMX586_REMOSAIC_CROP_4000X3000_30FPS,
		sensor_IMX586_setfile_A_mipi_channel_BINNING,	ARRAY_SIZE(sensor_IMX586_setfile_A_mipi_channel_BINNING),
		sensor_IMX586_setfile_A_mipi_setting_BINNING,	ARRAY_SIZE(sensor_IMX586_setfile_A_mipi_setting_BINNING)
	},
	{ SENSOR_IMX586_2X2BIN_CROP_4000X2252_30FPS,
		sensor_IMX586_setfile_A_mipi_channel_BINNING,	ARRAY_SIZE(sensor_IMX586_setfile_A_mipi_channel_BINNING),
		sensor_IMX586_setfile_A_mipi_setting_BINNING,	ARRAY_SIZE(sensor_IMX586_setfile_A_mipi_setting_BINNING)
	},
	{ SENSOR_IMX586_2X2BIN_CROP_4000X2252_60FPS,
		sensor_IMX586_setfile_A_mipi_channel_FULL, ARRAY_SIZE(sensor_IMX586_setfile_A_mipi_channel_FULL),
		sensor_IMX586_setfile_A_mipi_setting_FULL, ARRAY_SIZE(sensor_IMX586_setfile_A_mipi_setting_FULL)
	},
	{ SENSOR_IMX586_2X2BIN_CROP_1984X1488_30FPS,
		NULL, 0,
		NULL, 0
	},
	{ SENSOR_IMX586_2X2BIN_V2H2_992X744_120FPS,
		NULL, 0,
		NULL, 0
	},
	{ SENSOR_IMX586_REMOSAIC_CROP_4000X2252_30FPS,
		sensor_IMX586_setfile_A_mipi_channel_BINNING,	ARRAY_SIZE(sensor_IMX586_setfile_A_mipi_channel_BINNING),
		sensor_IMX586_setfile_A_mipi_setting_BINNING,	ARRAY_SIZE(sensor_IMX586_setfile_A_mipi_setting_BINNING)
	},
};

/* structure for only verifying channel list. to prevent redundant checking */
const int sensor_IMX586_setfile_A_verify_sensor_mode[] = {
	SENSOR_IMX586_REMOSAIC_FULL_8000X6000_15FPS,
	SENSOR_IMX586_2X2BIN_FULL_4000X3000_30FPS,
};

#endif
