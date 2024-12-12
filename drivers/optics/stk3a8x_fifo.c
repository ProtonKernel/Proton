/*
 *  stk3a8x_tuning.c - Linux kernel modules for sensortek stk3a8x,
 *  proximity/ambient light sensor (Algorithm)
 *
 *  Copyright (C) 2012~2018 Taka Chiu, sensortek Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/pm.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/unistd.h>
#include <asm/uaccess.h>
//#include <asm/segment.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>
//#include <linux/sensors.h>
#include "stk3a8x.h"

#define Q22		0x3FFFFF
#define Q26		0x3FFFFFF
#define Q45		0x1FFFFFFFFFFF

#ifdef STK_FIFO_ENABLE
#ifdef STK_FFT_FLICKER
int16_t stk_fft_real[128] =
{
	0x4000, 0x3FFB, 0x3FEC, 0x3FD3, 0x3FB1, 0x3F84, 0x3F4E, 0x3F0E, 0x3EC5, 0x3E71, 0x3E14, 0x3DAE, 0x3D3E, 0x3CC5, 0x3C42, 0x3BB6,
	0x3B20, 0x3A82, 0x39DA, 0x392A, 0x3871, 0x37AF, 0x36E5, 0x3612, 0x3536, 0x3453, 0x3367, 0x3274, 0x3179, 0x3076, 0x2F6B, 0x2E5A,
	0x2D41, 0x2C21, 0x2AFA, 0x29CD, 0x2899, 0x275F, 0x261F, 0x24DA, 0x238E, 0x223D, 0x20E7, 0x1F8B, 0x1E2B, 0x1CC6, 0x1B5D, 0x19EF,
	0x187D, 0x1708, 0x158F, 0x1413, 0x1294, 0x1111, 0x0F8C, 0x0E05, 0x0C7C, 0x0AF1, 0x0964, 0x07D5, 0x0645, 0x04B5, 0x0323, 0x0192,
	0x0000, 0xFE6E, 0xFCDD, 0xFB4B, 0xF9BB, 0xF82B, 0xF69C, 0xF50F, 0xF384, 0xF1FB, 0xF074, 0xEEEF, 0xED6C, 0xEBED, 0xEA71, 0xE8F8,
	0xE783, 0xE611, 0xE4A3, 0xE33A, 0xE1D5, 0xE075, 0xDF19, 0xDDC3, 0xDC72, 0xDB26, 0xD9E1, 0xD8A1, 0xD767, 0xD633, 0xD506, 0xD3DF,
	0xD2BF, 0xD1A6, 0xD095, 0xCF8A, 0xCE87, 0xCD8C, 0xCC99, 0xCBAD, 0xCACA, 0xC9EE, 0xC91B, 0xC851, 0xC78F, 0xC6D6, 0xC626, 0xC57E,
	0xC4E0, 0xC44A, 0xC3BE, 0xC33B, 0xC2C2, 0xC252, 0xC1EC, 0xC18F, 0xC13B, 0xC0F2, 0xC0B2, 0xC07C, 0xC04F, 0xC02D, 0xC014, 0xC005
};

int16_t stk_fft_imag[128] =
{
	0x0000, 0xFE6E, 0xFCDD, 0xFB4B, 0xF9BB, 0xF82B, 0xF69C, 0xF50F, 0xF384, 0xF1FB, 0xF074, 0xEEEF, 0xED6C, 0xEBED, 0xEA71, 0xE8F8,
	0xE783, 0xE611, 0xE4A3, 0xE33A, 0xE1D5, 0xE075, 0xDF19, 0xDDC3, 0xDC72, 0xDB26, 0xD9E1, 0xD8A1, 0xD767, 0xD633, 0xD506, 0xD3DF,
	0xD2BF, 0xD1A6, 0xD095, 0xCF8A, 0xCE87, 0xCD8C, 0xCC99, 0xCBAD, 0xCACA, 0xC9EE, 0xC91B, 0xC851, 0xC78F, 0xC6D6, 0xC626, 0xC57E,
	0xC4E0, 0xC44A, 0xC3BE, 0xC33B, 0xC2C2, 0xC252, 0xC1EC, 0xC18F, 0xC13B, 0xC0F2, 0xC0B2, 0xC07C, 0xC04F, 0xC02D, 0xC014, 0xC005,
	0xC000, 0xC005, 0xC014, 0xC02D, 0xC04F, 0xC07C, 0xC0B2, 0xC0F2, 0xC13B, 0xC18F, 0xC1EC, 0xC252, 0xC2C2, 0xC33B, 0xC3BE, 0xC44A,
	0xC4E0, 0xC57E, 0xC626, 0xC6D6, 0xC78F, 0xC851, 0xC91B, 0xC9EE, 0xCACA, 0xCBAD, 0xCC99, 0xCD8C, 0xCE87, 0xCF8A, 0xD095, 0xD1A6,
	0xD2BF, 0xD3DF, 0xD506, 0xD633, 0xD767, 0xD8A1, 0xD9E1, 0xDB26, 0xDC72, 0xDDC3, 0xDF19, 0xE075, 0xE1D5, 0xE33A, 0xE4A3, 0xE611,
	0xE783, 0xE8F8, 0xEA71, 0xEBED, 0xED6C, 0xEEEF, 0xF074, 0xF1FB, 0xF384, 0xF50F, 0xF69C, 0xF82B, 0xF9BB, 0xFB4B, 0xFCDD, 0xFE6E
};

int16_t stk_windows[128] =
{
	0x051E, 0x0527, 0x0543, 0x0570, 0x05AF, 0x0600, 0x0663, 0x06D7, 0x075C, 0x07F2, 0x0898, 0x094E, 0x0A14, 0x0AE9, 0x0BCD, 0x0CBE,
	0x0DBE, 0x0EC9, 0x0FE2, 0x1105, 0x1234, 0x136C, 0x14AE, 0x15F8, 0x174A, 0x18A4, 0x1A03, 0x1B67, 0x1CD0, 0x1E3D, 0x1FAC, 0x211D,
	0x228F, 0x2400, 0x2571, 0x26E0, 0x284D, 0x29B6, 0x2B1A, 0x2C79, 0x2DD3, 0x2F25, 0x306F, 0x31B1, 0x32EA, 0x3418, 0x353C, 0x3654,
	0x3760, 0x385F, 0x3950, 0x3A34, 0x3B09, 0x3BCF, 0x3C85, 0x3D2C, 0x3DC2, 0x3E47, 0x3EBB, 0x3F1D, 0x3F6F, 0x3FAE, 0x3FDB, 0x3FF6,
	0x3FFF, 0x3FF6, 0x3FDB, 0x3FAE, 0x3F6F, 0x3F1E, 0x3EBB, 0x3E47, 0x3DC2, 0x3D2C, 0x3C86, 0x3BD0, 0x3B0A, 0x3A35, 0x3951, 0x3860,
	0x3761, 0x3655, 0x353D, 0x3419, 0x32EB, 0x31B2, 0x3070, 0x2F26, 0x2DD4, 0x2C7B, 0x2B1C, 0x29B7, 0x284E, 0x26E2, 0x2573, 0x2402,
	0x2290, 0x211E, 0x1FAD, 0x1E3E, 0x1CD2, 0x1B69, 0x1A04, 0x18A5, 0x174C, 0x15FA, 0x14AF, 0x136D, 0x1235, 0x1106, 0x0FE3, 0x0ECA,
	0x0DBF, 0x0CBF, 0x0BCE, 0x0AEA, 0x0A15, 0x094F, 0x0899, 0x07F2, 0x075C, 0x06D7, 0x0663, 0x0600, 0x05AF, 0x0570, 0x0543, 0x0527
};

// sec_hamming : 0~65535, Q16
static const int32_t hamming[128] =
{
	5243, 5279, 5388, 5569, 5822, 6146, 6541, 7005, 7538, 8137, 8803, 9532, 10323, 11175, 12086, 13052,
	14073, 15144, 16265, 17431, 18641, 19891, 21178, 22500, 23853, 25233, 26638, 28064, 29508, 30966, 32435, 33910,
	35389, 36869, 38344, 39813, 41271, 42714, 44141, 45546, 46926, 48279, 49600, 50888, 52138, 53348, 54514, 55635,
	56706, 57727, 58693, 59603, 60455, 61247, 61976, 62642, 63241, 63774, 64238, 64633, 64957, 65210, 65391, 65500,
	65536, 65500, 65391, 65210, 64957, 64633, 64238, 63774, 63241, 62642, 61976, 61247, 60455, 59603, 58693, 57727,
	56706, 55635, 54514, 53348, 52138, 50888, 49600, 48279, 46926, 45546, 44141, 42714, 41271, 39813, 38344, 36869,
	35389, 33910, 32435, 30966, 29508, 28064, 26638, 25233, 23853, 22500, 21178, 19891, 18641, 17431, 16265, 15144,
	14073, 13052, 12086, 11175, 10323, 9532, 8803, 8137, 7538, 7005, 6541, 6146, 5822, 5569, 5388, 5279,
};

// sec_sin, sec_cos : -65536~65536
static const int32_t sin[512] =
{
	0, 804, 1608, 2412, 3216, 4019, 4821, 5623, 6424, 7224, 8022, 8820, 9616, 10411, 11204, 11996, 12785, 13573, 14359, 15143, 15924,
	16703, 17479, 18253, 19024, 19792, 20557, 21320, 22078, 22834, 23586, 24335, 25080, 25821, 26558, 27291, 28020, 28745, 29466, 30182,
	30893, 31600, 32303, 33000, 33692, 34380, 35062, 35738, 36410, 37076, 37736, 38391, 39040, 39683, 40320, 40951, 41576, 42194, 42806,
	43412, 44011, 44604, 45190, 45769, 46341, 46906, 47464, 48015, 48559, 49095, 49624, 50146, 50660, 51166, 51665, 52156, 52639, 53114,
	53581, 54040, 54491, 54934, 55368, 55794, 56212, 56621, 57022, 57414, 57798, 58172, 58538, 58896, 59244, 59583, 59914, 60235, 60547,
	60851, 61145, 61429, 61705, 61971, 62228, 62476, 62714, 62943, 63162, 63372, 63572, 63763, 63944, 64115, 64277, 64429, 64571, 64704,
	64827, 64940, 65043, 65137, 65220, 65294, 65358, 65413, 65457, 65492, 65516, 65531, 65536, 65531, 65516, 65492, 65457, 65413, 65358,
	65294, 65220, 65137, 65043, 64940, 64827, 64704, 64571, 64429, 64277, 64115, 63944, 63763, 63572, 63372, 63162, 62943, 62714, 62476,
	62228, 61971, 61705, 61429, 61145, 60851, 60547, 60235, 59914, 59583, 59244, 58896, 58538, 58172, 57798, 57414, 57022, 56621, 56212,
	55794, 55368, 54934, 54491, 54040, 53581, 53114, 52639, 52156, 51665, 51166, 50660, 50146, 49624, 49095, 48559, 48015, 47464, 46906,
	46341, 45769, 45190, 44604, 44011, 43412, 42806, 42194, 41576, 40951, 40320, 39683, 39040, 38391, 37736, 37076, 36410, 35738, 35062,
	34380, 33692, 33000, 32303, 31600, 30893, 30182, 29466, 28745, 28020, 27291, 26558, 25821, 25080, 24335, 23586, 22834, 22078, 21320,
	20557, 19792, 19024, 18253, 17479, 16703, 15924, 15143, 14359, 13573, 12785, 11996, 11204, 10411, 9616, 8820, 8022, 7224, 6424, 5623,
	4821, 4019, 3216, 2412, 1608, 804, 0, -804, -1608, -2412, -3216, -4019, -4821, -5623, -6424, -7224, -8022, -8820, -9616, -10411, -11204,
	-11996, -12785, -13573, -14359, -15143, -15924, -16703, -17479, -18253, -19024, -19792, -20557, -21320, -22078, -22834, -23586, -24335,
	-25080, -25821, -26558, -27291, -28020, -28745, -29466, -30182, -30893, -31600, -32303, -33000, -33692, -34380, -35062, -35738, -36410,
	-37076, -37736, -38391, -39040, -39683, -40320, -40951, -41576, -42194, -42806, -43412, -44011, -44604, -45190, -45769, -46341, -46906,
	-47464, -48015, -48559, -49095, -49624, -50146, -50660, -51166, -51665, -52156, -52639, -53114, -53581, -54040, -54491, -54934, -55368,
	-55794, -56212, -56621, -57022, -57414, -57798, -58172, -58538, -58896, -59244, -59583, -59914, -60235, -60547, -60851, -61145, -61429,
	-61705, -61971, -62228, -62476, -62714, -62943, -63162, -63372, -63572, -63763, -63944, -64115, -64277, -64429, -64571, -64704, -64827,
	-64940, -65043, -65137, -65220, -65294, -65358, -65413, -65457, -65492, -65516, -65531, -65536, -65531, -65516, -65492, -65457, -65413,
	-65358, -65294, -65220, -65137, -65043, -64940, -64827, -64704, -64571, -64429, -64277, -64115, -63944, -63763, -63572, -63372, -63162,
	-62943, -62714, -62476, -62228, -61971, -61705, -61429, -61145, -60851, -60547, -60235, -59914, -59583, -59244, -58896, -58538, -58172,
	-57798, -57414, -57022, -56621, -56212, -55794, -55368, -54934, -54491, -54040, -53581, -53114, -52639, -52156, -51665, -51166, -50660,
	-50146, -49624, -49095, -48559, -48015, -47464, -46906, -46341, -45769, -45190, -44604, -44011, -43412, -42806, -42194, -41576, -40951,
	-40320, -39683, -39040, -38391, -37736, -37076, -36410, -35738, -35062, -34380, -33692, -33000, -32303, -31600, -30893, -30182, -29466,
	-28745, -28020, -27291, -26558, -25821, -25080, -24335, -23586, -22834, -22078, -21320, -20557, -19792, -19024, -18253, -17479, -16703,
	-15924, -15143, -14359, -13573, -12785, -11996, -11204, -10411, -9616, -8820, -8022, -7224, -6424, -5623, -4821, -4019, -3216, -2412, -1608, -804
};

static const int32_t cos[512] =
{
	65536, 65531, 65516, 65492, 65457, 65413, 65358, 65294, 65220, 65137, 65043, 64940, 64827, 64704, 64571, 64429, 64277, 64115, 63944, 63763,
	63572, 63372, 63162, 62943, 62714, 62476, 62228, 61971, 61705, 61429, 61145, 60851, 60547, 60235, 59914, 59583, 59244, 58896, 58538, 58172,
	57798, 57414, 57022, 56621, 56212, 55794, 55368, 54934, 54491, 54040, 53581, 53114, 52639, 52156, 51665, 51166, 50660, 50146, 49624, 49095,
	48559, 48015, 47464, 46906, 46341, 45769, 45190, 44604, 44011, 43412, 42806, 42194, 41576, 40951, 40320, 39683, 39040, 38391, 37736, 37076,
	36410, 35738, 35062, 34380, 33692, 33000, 32303, 31600, 30893, 30182, 29466, 28745, 28020, 27291, 26558, 25821, 25080, 24335, 23586, 22834,
	22078, 21320, 20557, 19792, 19024, 18253, 17479, 16703, 15924, 15143, 14359, 13573, 12785, 11996, 11204, 10411, 9616, 8820, 8022, 7224, 6424,
	5623, 4821, 4019, 3216, 2412, 1608, 804, 0, -804, -1608, -2412, -3216, -4019, -4821, -5623, -6424, -7224, -8022, -8820, -9616, -10411, -11204,
	-11996, -12785, -13573, -14359, -15143, -15924, -16703, -17479, -18253, -19024, -19792, -20557, -21320, -22078, -22834, -23586, -24335, -25080,
	-25821, -26558, -27291, -28020, -28745, -29466, -30182, -30893, -31600, -32303, -33000, -33692, -34380, -35062, -35738, -36410, -37076, -37736,
	-38391, -39040, -39683, -40320, -40951, -41576, -42194, -42806, -43412, -44011, -44604, -45190, -45769, -46341, -46906, -47464, -48015, -48559,
	-49095, -49624, -50146, -50660, -51166, -51665, -52156, -52639, -53114, -53581, -54040, -54491, -54934, -55368, -55794, -56212, -56621, -57022,
	-57414, -57798, -58172, -58538, -58896, -59244, -59583, -59914, -60235, -60547, -60851, -61145, -61429, -61705, -61971, -62228, -62476, -62714,
	-62943, -63162, -63372, -63572, -63763, -63944, -64115, -64277, -64429, -64571, -64704, -64827, -64940, -65043, -65137, -65220, -65294, -65358,
	-65413, -65457, -65492, -65516, -65531, -65536, -65531, -65516, -65492, -65457, -65413, -65358, -65294, -65220, -65137, -65043, -64940, -64827,
	-64704, -64571, -64429, -64277, -64115, -63944, -63763, -63572, -63372, -63162, -62943, -62714, -62476, -62228, -61971, -61705, -61429, -61145,
	-60851, -60547, -60235, -59914, -59583, -59244, -58896, -58538, -58172, -57798, -57414, -57022, -56621, -56212, -55794, -55368, -54934, -54491,
	-54040, -53581, -53114, -52639, -52156, -51665, -51166, -50660, -50146, -49624, -49095, -48559, -48015, -47464, -46906, -46341, -45769, -45190,
	-44604, -44011, -43412, -42806, -42194, -41576, -40951, -40320, -39683, -39040, -38391, -37736, -37076, -36410, -35738, -35062, -34380, -33692,
	-33000, -32303, -31600, -30893, -30182, -29466, -28745, -28020, -27291, -26558, -25821, -25080, -24335, -23586, -22834, -22078, -21320, -20557,
	-19792, -19024, -18253, -17479, -16703, -15924, -15143, -14359, -13573, -12785, -11996, -11204, -10411, -9616, -8820, -8022, -7224, -6424, -5623,
	-4821, -4019, -3216, -2412, -1608, -804, 0, 804, 1608, 2412, 3216, 4019, 4821, 5623, 6424, 7224, 8022, 8820, 9616, 10411, 11204, 11996, 12785,
	13573, 14359, 15143, 15924, 16703, 17479, 18253, 19024, 19792, 20557, 21320, 22078, 22834, 23586, 24335, 25080, 25821, 26558, 27291, 28020,
	28745, 29466, 30182, 30893, 31600, 32303, 33000, 33692, 34380, 35062, 35738, 36410, 37076, 37736, 38391, 39040, 39683, 40320, 40951, 41576,
	42194, 42806, 43412, 44011, 44604, 45190, 45769, 46341, 46906, 47464, 48015, 48559, 49095, 49624, 50146, 50660, 51166, 51665, 52156, 52639,
	53114, 53581, 54040, 54491, 54934, 55368, 55794, 56212, 56621, 57022, 57414, 57798, 58172, 58538, 58896, 59244, 59583, 59914, 60235, 60547,
	60851, 61145, 61429, 61705, 61971, 62228, 62476, 62714, 62943, 63162, 63372, 63572, 63763, 63944, 64115, 64277, 64429, 64571, 64704, 64827,
	64940, 65043, 65137, 65220, 65294, 65358, 65413, 65457, 65492, 65516, 65531
};

int64_t out_r[SEC_FIFO_SIZE] = { 0 };
int64_t out_i[SEC_FIFO_SIZE] = { 0 };

static uint32_t calc_average(uint32_t* buffer, int count)
{
	int i;
	uint32_t sum = 0;
	uint16_t average = 0;

	if ((NULL != buffer) && (count > 0))
	{
		for (i = 0; i < count; i++)
		{
			sum += buffer[i];
		}

		average = sum / count;
	}

	return average;
}

static uint64_t calc_thd(uint16_t clear_avg_fifo, uint32_t clear_avg, uint32_t* fft_out_data)
{
	uint64_t threshold;
	uint64_t ratio_fixed;
	uint64_t ratio_final;

	if (clear_avg_fifo <= 1)
	{
		ratio_fixed = (FLICKER_THD_CLEAR * FLICKER_GAIN_MAX); // *1000*1000
	}
	else
	{
		if (clear_avg)
			ratio_fixed = (FLICKER_THD_CLEAR << 7) / clear_avg; // clear_avg left-shifted 7.
		else
			ratio_fixed = (FLICKER_THD_CLEAR << 7); // clear_avg left-shifted 7.
	}

	if (FLICKER_THD_RATIO_AUTO > ratio_fixed)
		// 1,000,000 * 256 --> 28bit   therefore <<7:35bit max
		ratio_final = FLICKER_THD_RATIO_AUTO; // *1000*1000
	else
		ratio_final = ratio_fixed;

	threshold = ((uint64_t)fft_out_data[0] * ratio_final) / 1000000 * FLICKER_THD_RATIO / 1000;
	//                        +26bit           +35bit        -20bit        +2bit         -10bit
	// dbg_flicker("calc_thd threshold = %lld, fifoout[0] %lld, ratio_final %lld \n", threshold, fft_out_data[0], ratio_final);
	return threshold;
}

static uint64_t SEC_sqrt(uint64_t x)
{
	register uint64_t result, tmp;
	result = 0;
	tmp = (1LL << 62);  // second-to-top bit set

	while (tmp > x)
	{
		tmp >>= 2;
	}

	while (tmp != 0)
	{
		if (x >= (result + tmp))
		{
			x -= result + tmp;
			result += 2 * tmp;  // <-- faster than 2 * one
		}

		result >>= 1;
		tmp >>= 2;
	}

	return result;
}

void get_magnitude(int64_t* data_r, int64_t* data_i, int32_t* buffer, int size)
	// data : 41~50 bits
{
	//data must be twice as long as size
	int i;

	for (i = 0; i < size; ++i)
	{
		//sqrt(real^2 + imaginary^2)
		uint64_t square = 0;
		//int64_t t_r = data_r[i] >> 4;
		//int64_t t_i = data_i[i] >> 4;
		square = ((data_r[i] * data_r[i]) >> 1) + ((data_i[i] * data_i[i]) >> 1);
		buffer[i] = (int32_t)SEC_sqrt(square); //26 bit
		if (buffer[i] > Q26) {
			buffer[i] = Q26;
			err_flicker("buffer overflow (Q26)");
		}
	}
}

// n is a power of 2
int _SEC_log2n(int n)
{
	//int len = sizeof(int) * 8;
	int len = 32, i;

	for (i = 0; i < len; i++)
	{
		if ((n & 1) == 1)
			return i;
		else
			n >>= 1;
	}

	return -1;
}

// Utility function for reversing the bits
// of given index x
unsigned int SEC_bitReverse(unsigned int x, int log2n)
{
	int n = 0, i;

	for (i = 0; i < log2n; i++)
	{
		n <<= 1;
		n |= (x & 1);
		x >>= 1;
	}

	return n;
}

void _SEC_fft(int32_t* a_r, int32_t* a_i, int64_t* A_r, int64_t* A_i, int n)
{
	int log2n = _SEC_log2n(n), s, k, j;
	unsigned int i;

	// bit reversal of the given array
	for (i = 0; i < n; ++i)
	{
		int rev = SEC_bitReverse(i, log2n);
		A_r[i] = ((int64_t)a_r[rev]) << 16; // 16+16=32   Q16
		A_i[i] = ((int64_t)a_i[rev]) << 16; // 16+16=32   Q16
	}

	for (s = 1; s <= log2n; s++)
	{
		int m = 1 << s;
		int m_2 = m >> 1;
		int64_t wm_r = (int64_t)cos[512 >> s];    //wm_r = cos(2 pi / m)    Q16
		int64_t wm_i = -(int64_t)sin[512 >> s];   //wm_i = -sin(2 pi / m)    Q16

		for (k = 0; k < n; k += m)
		{
			int64_t w_r = 1LL << 16;  // Q16 => 65536
			int64_t w_i = 0;          // Q16

			for (j = 0; j < m_2; j++)
			{
				int i1 = k + j;
				int i2 = i1 + m_2;
				int64_t t_r = w_r * A_r[i2] - w_i * A_i[i2];   // Q16*Q16,  16+32=48~55
				int64_t t_i = w_r * A_i[i2] + w_i * A_r[i2];   // Q16*Q16,  16+32=48~55
				int64_t u_r = A_r[i1];    // Q16,  32
				int64_t u_i = A_i[i1];    // Q16
				int64_t w2_r = w_r * wm_r - w_i * wm_i;   // Q16*Q16,  16+16=32
				int64_t w2_i = w_r * wm_i + w_i * wm_r;   // Q16*Q16
				t_r >>= 16;    // Q16,  32~49
				t_i >>= 16;    // Q16
				A_r[i1] = u_r + t_r;      // Q16,  48~55
				A_i[i1] = u_i + t_i;
				A_r[i2] = u_r - t_r;
				A_i[i2] = u_i - t_i;
				w_r = w2_r >> 16;    // Q16
				w_i = w2_i >> 16;    // Q16
			}
		}
	}
}

void SEC_FFT(int32_t* buf_r, int32_t* buf_i, int32_t* data, int size)
	// buf_r <= 16bits data
{
	int i;
	memset(out_r, 0, sizeof(int64_t) * SEC_FIFO_SIZE);
	memset(out_i, 0, sizeof(int64_t) * SEC_FIFO_SIZE);
	_SEC_fft(buf_r, buf_i, out_r, out_i, size);

	for (i = 0; i < SEC_FIFO_SIZE; i++)
	{
		if (out_r[i] > Q45) {
			out_r[i] = Q45;
			err_flicker("out_r overflow (Q45)");
		}
		out_r[i] >>= 19;

		if (out_i[i] > Q45) {
			out_i[i] = Q45;
			err_flicker("out_i overflow (Q45)");
		}
		out_i[i] >>= 19;
	}

	get_magnitude(out_r, out_i, data, size);
}

void SEC_windows(struct stk3a8x_data *alps_data, int32_t* data, int size)
{
	int i;
	int32_t buf_r[SEC_FIFO_SIZE] = { 0 };
	int32_t buf_i[SEC_FIFO_SIZE] = { 0 };

	if (size > SEC_FIFO_SIZE) // index overflow
		return;

	for (i = 0; i < size; i++)
	{
		if (!alps_data->saturation && data[i] >= 0xFFFF) {
			dbg_flicker("DEBUG_FLICKER saturation");
			alps_data->saturation = true;
		}

		buf_r[i] = ((int64_t)data[i] * (int64_t)hamming[i]) >> 10;        // 16+16-10=22   Q6
		if (buf_r[i] > Q22) {
			buf_r[i] = Q22;
			err_flicker("buf_r overflow (Q22)");
		}
		dbg_flicker("data[%d] => %d buf[%d] => %d", i, data[i], i, buf_r[i]);
	}

	SEC_FFT(buf_r, buf_i, data, size);
}

void stk_fft_main(fft_array* buf, fft_array* out, uint8_t n, uint8_t step)
{
	uint8_t i = 0;
	fft_array t;

	if (step < n)
	{
		stk_fft_main(out, buf, n, step * 2);
		stk_fft_main(out + step, buf + step, n, step * 2);

		for (i = 0; i < n; i += 2 * step)
		{
			t.real = (int16_t)((stk_fft_real[i] * out[i + step].real) >> Q_offset) -
				(int16_t)((stk_fft_imag[i] * out[i + step].imag) >> Q_offset);
			t.imag = (int16_t)((stk_fft_real[i] * out[i + step].imag) >> Q_offset) +
				(int16_t)((stk_fft_imag[i] * out[i + step].real) >> Q_offset);
			buf[i / 2].real = out[i].real + t.real;
			buf[i / 2].imag = out[i].imag + t.imag;
			buf[(i + n) / 2].real = out[i].real - t.real;
			buf[(i + n) / 2].imag = out[i].imag - t.imag;
		}
	}
}

void stk_fft_entry(fft_array* buf, uint8_t n)
{
	uint8_t i = 0;
	fft_array out[n];

	for (i = 0; i < n; i++)
	{
		out[i].real = buf[i].real;
		out[i].imag = buf[i].imag;
	}

	stk_fft_main(buf, out, n, 1);
}

void get_current_gain(struct stk3a8x_data *alps_data, uint16_t* wide_gain, uint16_t* clear_gain)
{
	int32_t err;
	uint8_t reg_value;
	err = STK3A8X_REG_READ(alps_data, STK3A8X_REG_GAINCTRL);

	if (err < 0)
	{
		err_flicker("Read 0x%X failed\n", STK3A8X_REG_GAINCTRL);
		return;
	}

	reg_value = (uint8_t)err;

	// check wide gain
	if ((reg_value & 0x4) >> 2)
	{
		*wide_gain = 7;
	}
	else
	{
		*wide_gain = ((reg_value & 0x30) >> 4) * 2;
	}

	// check clear gain
	if ((reg_value & 0x2) >> 1)
	{
		*clear_gain = 7;
	}
	else
	{
		err = STK3A8X_REG_READ(alps_data, STK3A8X_REG_ALSCTRL1);

		if (err < 0)
		{
			err_flicker("Read 0x%X failed\n", STK3A8X_REG_ALSCTRL1);
			return;
		}

		reg_value = (uint8_t)err;
		*clear_gain = ((reg_value & 0x30) >> 4) * 2;
	}

	// check wide & clear A gian
	err = STK3A8X_REG_READ(alps_data, STK3A8X_REG_AGAIN);

	if (err < 0)
	{
		err_flicker("Read 0x%X failed\n", STK3A8X_REG_AGAIN);
		return;
	}

	reg_value = (uint8_t)err;

	if (((reg_value & 0x0C) >> 2) == 2)
	{
		*clear_gain -= 1;
	}
	else
	{
		*clear_gain += (1 - ((reg_value & 0x0C) >> 2));
	}

	if (((reg_value & 0x30) >> 4) == 2)
	{
		*wide_gain -= 1;
	}
	else
	{
		*wide_gain += (1 - ((reg_value & 0x30) >> 4));
	}

	return;
}

void SEC_fft_entry(struct stk3a8x_data *alps_data)
{
	static uint32_t clear_buffer[SEC_FIFO_SIZE] = { 0 };
	static uint32_t wide_buffer[SEC_FIFO_SIZE] = { 0 };
	int16_t clear_gain = 0;
	int16_t wide_gain = 0;
	uint32_t clear_average_fifo = 0;
	uint32_t wide_average_fifo = 0;
	uint32_t clear_average = 0;
	uint32_t wide_average = 0;
	static uint32_t buf[SEC_FIFO_SIZE] = { 0, };
	int max_freq = 0, i = 0;
	uint16_t flicker_freq = 65535;
	uint64_t thd = 0;

	if (alps_data->als_info.is_gain_changing)
	{
		flicker_freq = alps_data->fifo_info.last_flicker_freq;
		alps_data->als_info.raw_data.flicker = flicker_freq;
		err_flicker("FIFO data gain is changing! Skip fft calulation.\n");
		return;
	}

    /*if (alps_data->als_info.is_data_saturated)
	{
		flicker_freq = alps_data->fifo_info.last_flicker_freq;
		alps_data->als_info.raw_data.flicker = flicker_freq;
		err_flicker("Max data in FIFO is saturated! Skip fft calulation.\n");
		return;
    }*/

	for (i = 0; i < FFT_BLOCK_SIZE; i++)
	{
		wide_buffer[i] = (uint32_t)(alps_data->fifo_info.fifo_data0[i]);
		clear_buffer[i] = (uint32_t)(alps_data->fifo_info.fifo_data1[i]);
	}

	get_current_gain(alps_data, &wide_gain, &clear_gain);
	clear_average_fifo = calc_average(clear_buffer, SEC_FIFO_SIZE);
	wide_average_fifo = calc_average(wide_buffer, SEC_FIFO_SIZE);
	clear_average = ((uint32_t)clear_average_fifo) << 7;   // Q7
	wide_average = ((uint32_t)wide_average_fifo) << 7;     // Q7
	if(clear_gain < 0)
	{
		clear_average <<= STK_ABS(clear_gain);
	}
	else
	{
		clear_average >>= clear_gain;
	}

	if(wide_gain < 0)
	{
		wide_average <<= STK_ABS(wide_gain);
	}
	else
	{
		wide_average >>= wide_gain;
	}

	memcpy(buf, wide_buffer, sizeof(buf));
	SEC_windows(alps_data, buf, SEC_FIFO_SIZE);
	max_freq = 5;

	for (i = 0; i <= 50; ++i)
	{
		dbg_flicker("FFT[%d] = %u", i, buf[i]);
		if (i > 5 && buf[i] > buf[max_freq])           // 50~500Hz
		{
			max_freq = i;
		}
	}

	thd = calc_thd(clear_average_fifo, clear_average, buf);

	if (buf[max_freq] > thd)
	{
		flicker_freq = ((max_freq * SEC_SAMPLING_FREQ) / SEC_FIFO_SIZE);
	}
	else
	{
		flicker_freq = 0;
	}

	alps_data->fifo_info.last_flicker_freq = flicker_freq;

	dbg_flicker("cavg_fifo:%d cavg:%d cgain:%d | wavg_fifo:%d wavg:%d wgain:%d",
			clear_average_fifo, clear_average, clear_gain, wide_average_fifo, wide_average, wide_gain);

	dbg_flicker("flicker_freq : %d\n", flicker_freq);

	alps_data->als_info.raw_data.wideband = wide_average;
	alps_data->als_info.raw_data.clear = clear_average;
	alps_data->als_info.raw_data.flicker = flicker_freq;
	alps_data->als_info.raw_data.wide_gain = wide_gain;
	alps_data->als_info.raw_data.clear_gain = clear_gain;
	alps_data->als_info.is_raw_update = true;
}

void fft_entry(struct stk3a8x_data *alps_data, uint16_t* fifo_source, uint16_t fifo_count, uint32_t* freq_data)
{
	uint8_t i;
	uint64_t fft_sum = 0;
	uint32_t fft_amplitude = 0, fft_temp[50], fft_avg = 0;
	uint64_t fft_sd = 0;
	uint16_t freq_id = 0;
	uint32_t sum_raw = 0;
	uint16_t avg_raw = 0;
	fft_array in_buf[FFT_BLOCK_SIZE];
	memmove(alps_data->fifo_info.fft_buf,
			fifo_source,
			fifo_count * sizeof(uint16_t));

	// normalize
	for (i = 0; i < FFT_BLOCK_SIZE; i++)
	{
		sum_raw += *(alps_data->fifo_info.fft_buf + i);
	}

	avg_raw = sum_raw / FFT_BLOCK_SIZE;

	// windows
	for (i = 0; i < FFT_BLOCK_SIZE; i++)
	{
		in_buf[i].real = (((*(alps_data->fifo_info.fft_buf + i) - avg_raw) * stk_windows[i]) >> Q_offset);
		in_buf[i].imag = 0;
	}

	stk_fft_entry(in_buf, FFT_BLOCK_SIZE);

	for (i = 1; i < 51; i++)
	{
		fft_temp[i - 1] = in_buf[i].real * in_buf[i].real + in_buf[i].imag * in_buf[i].imag;
		fft_sum += fft_temp[i - 1];

		if (fft_temp[i - 1] > fft_amplitude)
		{
			fft_amplitude = fft_temp[i - 1];
			freq_id = 10 * i;
		}
	}

	fft_avg = fft_sum / 50;

	for (i = 0; i < 50; i++)
	{
		*(freq_data + i) = fft_temp[i];
		fft_sd += ((fft_temp[i] - fft_avg) * (fft_temp[i] - fft_avg));
	}

	fft_sd /= 50;

	if ((fft_sd <= FFT_PEAK_THRESHOLD) || (freq_id < 50))
	{
		freq_id = 0;
	}

	alps_data->als_info.last_raw_data[2] = freq_id;
}
#endif

void stk3a8x_free_fifo_data(struct stk3a8x_data * alps_data)
{
	kfree(alps_data->fifo_info.fifo_data0);
	alps_data->fifo_info.fifo_data0 = NULL;
	kfree(alps_data->fifo_info.fifo_data1);
	alps_data->fifo_info.fifo_data1 = NULL;
}
void stk3a8x_alloc_fifo_data(struct stk3a8x_data * alps_data, uint32_t size)
{
	/* Allocate memory for fifo data */
	if (alps_data->fifo_info.fifo_data0 == NULL)
	{
		alps_data->fifo_info.fifo_data0 = kzalloc(size * sizeof(uint16_t), GFP_KERNEL);

		if (alps_data->fifo_info.fifo_data0 == NULL)
		{
			//malloc failed, out of memory
			err_flicker("fifo_data0 malloc failed, out of memory\n");
		}
	}

	if (alps_data->fifo_info.fifo_data1 == NULL)
	{
		alps_data->fifo_info.fifo_data1 = kzalloc(size * sizeof(uint16_t), GFP_KERNEL);

		if (alps_data->fifo_info.fifo_data1 == NULL)
		{
			//malloc failed, out of memory
			err_flicker("fifo_data1 malloc failed, out of memory\n");
		}
	}
}
void stk3a8x_fifo_init(struct stk3a8x_data * alps_data)
{
	int32_t err;
	uint8_t flag_value;
	err = STK3A8X_REG_READ(alps_data, STK3A8X_REG_FIFOCTRL1);

	if (err < 0)
	{
		err_flicker("Read flag failed\n");
		return;
	}

	flag_value = (uint8_t)err;
	alps_data->fifo_info.data_type = (flag_value & STK3A8X_FIFO_SEL_MASK);
	info_flicker("data_type = %d\n", alps_data->fifo_info.data_type);

	switch (alps_data->fifo_info.data_type)
	{
		case STK3A8X_FIFO_SEL_DATA_FLICKER:
		case STK3A8X_FIFO_SEL_DATA_IR:
			alps_data->fifo_info.frame_byte = 2;
			alps_data->fifo_info.read_frame = STK_FIFO_I2C_READ_FRAME;
			alps_data->fifo_info.target_frame_count = STK_FIFO_I2C_READ_FRAME_TARGET;
			break;

		case STK3A8X_FIFO_SEL_DATA_FLICKER_IR:
			alps_data->fifo_info.frame_byte = 4;
			alps_data->fifo_info.read_frame = STK_FIFO_I2C_READ_FRAME;
			alps_data->fifo_info.target_frame_count = STK_FIFO_I2C_READ_FRAME_TARGET;
			break;

		default:
			alps_data->fifo_info.frame_byte = 0xFF;
			err_flicker("Frame Type ERROR!\n");
			break;
	}

	alps_data->fifo_info.read_max_data_byte = alps_data->fifo_info.frame_byte * STK_FIFO_I2C_READ_FRAME;
	alps_data->fifo_info.fifo_reading = false;
	alps_data->fifo_info.block_size = STK_FIFO_GET_MAX_FRAME;
	alps_data->als_info.als_it = 100;
	alps_data->fifo_info.fifo_data0 = NULL;
	alps_data->fifo_info.fifo_data1 = NULL;
}
void stk3a8x_pause_fifo(struct stk3a8x_data * alps_data, bool en)
{
	int32_t ret;
	uint16_t flag_value = en ? 0x80 : 0x00;
	ret = STK3A8X_REG_READ_MODIFY_WRITE(alps_data,
			STK3A8X_REG_FIFOCTRL3,
			flag_value,
			0x80);

	if (ret < 0)
	{
		err_flicker("fail, ret=%d\n", ret);
		return;
	}
}
int32_t stk3a8x_enable_fifo(struct stk3a8x_data * alps_data, bool en)
{
	int32_t ret = 0;
	uint8_t fifo_status, flag_value = 0, i2c_data_reg[2] = {0};
	uint16_t max_frame_count = 1024 / alps_data->fifo_info.frame_byte;

	if (alps_data->fifo_info.enable == en)
	{
		err_flicker("FIFO already set\n");
		return ret;
	}

	ret = STK3A8X_REG_READ(alps_data, STK3A8X_REG_FIFOCTRL1);

	if (ret < 0)
	{
		err_flicker("Can't read FIFOCTRL1");
		return ret;
	}
	else
	{
		fifo_status = (uint8_t)ret;
	}

	if (en)
	{
		fifo_status = (STK3A8X_FIFO_MODE_STREAM | fifo_status);

		if (alps_data->als_info.is_dri)
		{
			i2c_data_reg[0] = (alps_data->fifo_info.target_frame_count >> 8) & 0x03;
			i2c_data_reg[1] = alps_data->fifo_info.target_frame_count & 0xFF;
			flag_value = STK3A8X_FOVR_EN_MASK | STK3A8X_FWM_EN_MASK | STK3A8X_FFULL_EN_MASK;
		}
		else
		{
			i2c_data_reg[0] = (max_frame_count >> 8) & 0x03;
			i2c_data_reg[1] = max_frame_count & 0xFF;
		}
	}
	else
	{
		fifo_status = (fifo_status & (~STK3A8X_FIFO_MODE_STREAM));
	}

	ret = STK3A8X_REG_WRITE_BLOCK(alps_data,
			STK3A8X_REG_THD1_FIFO_FCNT,
			i2c_data_reg,
			sizeof(i2c_data_reg) / sizeof(i2c_data_reg[0]));

	if (ret < 0)
	{
		err_flicker("fail, ret=%d\n", ret);
		return ret;
	}

	ret = STK3A8X_REG_READ_MODIFY_WRITE(alps_data,
			STK3A8X_REG_FIFOCTRL2,
			flag_value,
			0xFF);

	if (ret < 0)
	{
		err_flicker("fail, ret=%d\n", ret);
		return ret;
	}

	ret = STK3A8X_REG_READ_MODIFY_WRITE(alps_data,
			STK3A8X_REG_FIFOCTRL1,
			fifo_status,
			0xFF);

	if (ret < 0)
	{
		err_flicker("fail, ret=%d\n", ret);
		return ret;
	}

	alps_data->fifo_info.enable = en;
	return ret;
}

void stk3a8x_avg_data(struct stk3a8x_data * alps_data)
{
	uint16_t i;
	uint32_t F_sum = 0, C_sum = 0;

	for (i = 0; i < alps_data->fifo_info.last_frame_count; i++)
	{
		F_sum += alps_data->fifo_info.fifo_data0[i];
		C_sum += alps_data->fifo_info.fifo_data1[i];
	}

	alps_data->als_info.last_raw_data[0] = (uint16_t)(F_sum / alps_data->fifo_info.last_frame_count);
	alps_data->als_info.last_raw_data[1] = (uint16_t)(C_sum / alps_data->fifo_info.last_frame_count);
}

void stk3a8x_fifo_get_data(struct stk3a8x_data * alps_data, uint16_t frame_num)
{
	uint8_t offset, *raw_data;
	int16_t i, frame_count = 0, read_frame_num = 0;
	int32_t ret, read_bytes;

	if (alps_data->fifo_info.fifo_data0 == NULL)
	{
		err_flicker("NULL pointer\n");
		return;
	}

	raw_data = kzalloc(alps_data->fifo_info.read_max_data_byte * sizeof(uint16_t), GFP_KERNEL);
	memset(alps_data->fifo_info.fifo_data0, 0, STK_FIFO_GET_MAX_FRAME * sizeof(uint16_t));
	memset(alps_data->fifo_info.fifo_data1, 0, STK_FIFO_GET_MAX_FRAME * sizeof(uint16_t));

	for (frame_count = 0 ; frame_count < frame_num ; frame_count += (alps_data->fifo_info.read_frame))
	{
		read_frame_num = (int16_t)(frame_num - frame_count);

		if (read_frame_num >= alps_data->fifo_info.read_frame)
		{
			read_bytes = alps_data->fifo_info.read_max_data_byte;
			read_frame_num = alps_data->fifo_info.read_frame;
		}
		else
		{
			read_bytes = alps_data->fifo_info.frame_byte * read_frame_num;
		}

		memset(raw_data, 0, alps_data->fifo_info.read_max_data_byte * sizeof(uint16_t));
		ret = STK3A8X_REG_BLOCK_READ(alps_data, STK3A8X_REG_FIFO_OUT, read_bytes, raw_data);

		if (ret < 0)
		{
			kfree(raw_data);
			err_flicker("fail, err=0x%x", ret);
			return;
		}
#if 0 //DEBUG
		ret = STK3A8X_REG_READ(alps_data, STK3A8X_REG_FIFO_FLAG);
		if (ret < 0)
		{
			err_flicker("Read flag failed\n");
			return;
		}

		err_flicker("i2c_flag_reg = 0x%x\n", ret);

#endif

		switch (alps_data->fifo_info.data_type)
		{
			case STK3A8X_FIFO_SEL_DATA_FLICKER:
			case STK3A8X_FIFO_SEL_DATA_IR:
				for (i = 0, offset = 0; i < read_frame_num; i++, offset += alps_data->fifo_info.frame_byte)
				{
					*(alps_data->fifo_info.fifo_data0 + frame_count + i) = (*(raw_data + offset) << 8 | *(raw_data + offset + 1));//ps data
				}

				break;

			case STK3A8X_FIFO_SEL_DATA_FLICKER_IR:
				for (i = 0, offset = 0; i < read_frame_num; i++, offset += alps_data->fifo_info.frame_byte)
				{
					*(alps_data->fifo_info.fifo_data1 + frame_count + i) = (*(raw_data + offset) << 8 | *(raw_data + offset + 1));//als data
					*(alps_data->fifo_info.fifo_data0 + frame_count + i) = (*(raw_data + offset + 2) << 8 | *(raw_data + offset + 3));//c1 data
				}

				break;

			default:
				err_flicker("unavailable!\n");
				break;
		}
	}

	alps_data->fifo_info.last_frame_count = frame_num;
	kfree(raw_data);
}

#ifdef STK_FIFO_ENABLE
void stk3a8x_get_fifo_data_polling(struct stk3a8x_data *alps_data)
{
	int32_t ret;
	uint8_t i2c_flag_reg;
	uint8_t raw_data[2] = {0};
	uint16_t frame_num;

	if (alps_data->fifo_info.enable == false)
	{
		err_flicker("FIFO is disable\n");
		return;
	}

	alps_data->fifo_info.fifo_reading = true;
	alps_data->als_info.is_raw_update = false;

	ret = STK3A8X_REG_READ(alps_data, STK3A8X_REG_FIFO_FLAG);
	if (ret < 0)
	{
		err_flicker("Read flag failed\n");
		goto err;
	}

	i2c_flag_reg = (uint8_t)ret;

	if ((i2c_flag_reg & (STK3A8X_FLG_FIFO_OVR_MASK | STK3A8X_FLG_FIFO_WM_MASK | STK3A8X_FLG_FIFO_FULL_MASK)) != 0)
	{
		//TODO every event occur
		dbg_flicker("i2c_flag_reg = 0x%x\n", i2c_flag_reg);
	}

	ret = STK3A8X_REG_BLOCK_READ(alps_data, STK3A8X_REG_FIFOFCNT1, 2, raw_data);

	if (ret < 0)
	{
		err_flicker("fail, err=0x%x", ret);
		goto err;
	}

	frame_num = ((raw_data[0] << 8) | raw_data[1]);
	dbg_flicker("frame_num = %d\n", frame_num);

	if (frame_num < STK_FIFO_GET_MAX_FRAME)
	{
		err_flicker("frame not enough");
		goto err;
	}
	else
	{
		alps_data->als_info.is_data_ready = true;

		//stk3a8x_pause_fifo(alps_data, true);
		if (frame_num > STK_FIFO_GET_MAX_FRAME)
		{
			frame_num = STK_FIFO_GET_MAX_FRAME;
		}
	}

	if (alps_data->fifo_info.enable == true)
	{
		stk3a8x_fifo_get_data(alps_data, frame_num);
		//stk3a8x_pause_fifo(alps_data, false);
	}

	if (alps_data->fifo_info.last_frame_count != 0)
	{
		stk3a8x_avg_data(alps_data);
	}

err:
	alps_data->fifo_info.fifo_reading = false;
}

void stk3a8x_get_fifo_data_dri(struct stk3a8x_data * alps_data)
{
	int32_t ret;
	uint8_t i2c_flag_reg;
	uint8_t raw_data[2] = {0};
	uint16_t frame_num;
	alps_data->fifo_info.fifo_reading = true;
	alps_data->als_info.is_raw_update = false;

	ret = STK3A8X_REG_READ(alps_data, STK3A8X_REG_FIFO_FLAG);

	if (ret < 0)
	{
		err_flicker("Read flag failed\n");
		return;
	}

	i2c_flag_reg = (uint8_t)ret;

	if ((i2c_flag_reg & (STK3A8X_FLG_FIFO_OVR_MASK | STK3A8X_FLG_FIFO_WM_MASK | STK3A8X_FLG_FIFO_FULL_MASK)) == 0)
	{
		err_flicker("i2c_flag_reg = 0x%x\n", i2c_flag_reg);
		return;
	}

	ret = STK3A8X_REG_BLOCK_READ(alps_data, STK3A8X_REG_FIFOFCNT1, 2, raw_data);

	if (ret < 0)
	{
		err_flicker("fail, err=0x%x", ret);
		return;
	}

	frame_num = ((raw_data[0] << 8) | raw_data[1]);
	dbg_flicker("frame_num = %d\n", frame_num);

	if (frame_num < STK_FIFO_GET_MAX_FRAME)
	{
		return;
	}
	else
	{
		if (frame_num > STK_FIFO_GET_MAX_FRAME)
		{
			frame_num = STK_FIFO_GET_MAX_FRAME;
		}
	}

	i2c_flag_reg = 0;
	ret = STK3A8X_REG_READ_MODIFY_WRITE(alps_data,
			STK3A8X_REG_FIFOCTRL2,
			i2c_flag_reg,
			0xFF);
	alps_data->fifo_info.fifo_reading = true;

	if (alps_data->fifo_info.enable)
	{
		stk3a8x_fifo_get_data(alps_data, frame_num);
	}

	alps_data->fifo_info.fifo_reading = false;

	if (alps_data->fifo_info.enable)
	{
		if (alps_data->fifo_info.last_frame_count != 0)
		{
			stk3a8x_avg_data(alps_data);
		}

		i2c_flag_reg = STK3A8X_FOVR_EN_MASK | STK3A8X_FWM_EN_MASK | STK3A8X_FFULL_EN_MASK;
		ret = STK3A8X_REG_READ_MODIFY_WRITE(alps_data,
				STK3A8X_REG_FIFOCTRL2,
				i2c_flag_reg,
				0xFF);
	}
	else
	{
		info_flicker("Free FIFO array");
		stk3a8x_free_fifo_data(alps_data);
	}

	alps_data->fifo_info.fifo_reading = false;
}
#endif
#endif
