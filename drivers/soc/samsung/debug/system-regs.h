/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SYSTEM_REGS_H
#define _SYSTEM_REGS_H

struct armv8_a_ERRSELR_EL1_field {
	u64 SEL		:16;
	u64 RES0	:48;
};

struct armv8_a_ERRIDR_EL1_field {
	u64 NUM		:16;
	u64 RES0	:48;
};

struct armv8_a_ERXSTATUS_EL1_field {
	u64 SERR	:8;
	u64 IERR	:8;
	u64 RES0	:4;
	u64 UET		:2;
	u64 PN		:1;
	u64 DE		:1;
	u64 CE		:2;
	u64 MV		:1;
	u64 OF		:1;
	u64 ER		:1;
	u64 UE		:1;
	u64 Valid	:1;
	u64 AV		:1;
	u64 RES1	:32;
};

struct armv8_a_ERXMISC0_EL1_field {
	u64 REG;
};

struct armv8_a_ERXMISC1_EL1_field {
	u64 REG;
};

struct armv8_a_ERXADDR_EL1_field {
	u64 REG;
};

#define ARMv8_A_SYSTEM_REGISTER(name)			\
typedef struct armv8_a_##name {				\
	union {						\
		struct armv8_a_##name##_field field;	\
		u64 reg;				\
	};						\
} name##_t;

ARMv8_A_SYSTEM_REGISTER(ERRSELR_EL1);
ARMv8_A_SYSTEM_REGISTER(ERRIDR_EL1);
ARMv8_A_SYSTEM_REGISTER(ERXSTATUS_EL1);
ARMv8_A_SYSTEM_REGISTER(ERXMISC0_EL1);
ARMv8_A_SYSTEM_REGISTER(ERXMISC1_EL1);
ARMv8_A_SYSTEM_REGISTER(ERXADDR_EL1);

#endif
