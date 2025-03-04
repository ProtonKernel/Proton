// SPDX-License-Identifier: GPL-2.0-only
/*
 * Samsung debugging features for Samsung's SoC's.
 *
 * Copyright (c) 2014-2019 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 */

/* keys are grouped by size */
static char key32[][MAX_ITEM_KEY_LEN] = {
	"ID", "KTIME", "BIN", "RR",
	"DPM", "SMP", "MER", "PCB", "SMD",
	"CHI", "LPI", "CDI", "LEV", "DCN",
	"WAK", "ASB", "PSITE", "DDRID", "RST",
	"INFO2", "INFO3", "RBASE", "MAGIC", "RSTCNT",
	"FPMU",
};

static char key64[][MAX_ITEM_KEY_LEN] = {
	"ETC", "BAT", "FAULT", "PINFO", "HINT",
	"EPD", "MOCP", "SOCP", "ASV", "IDS",
	"HLEHLD", "PWR", "PWROFF", "PINT", "PSTAT",
	"PWROFFS", "PINTS", "PSTATS", "CPS",
};

static char key256[][MAX_ITEM_KEY_LEN] = {
	"KLG", "BUS", "PANIC", "PC", "LR",
	"BUG", "ESR", "SMU", "FREQ", "ODR",
	"AUD", "UNFZ", "UP", "DOWN", "WDGC",
	"HLTYPE",
};

static char key1024[][MAX_ITEM_KEY_LEN] = {
	"CPU0", "CPU1", "CPU2", "CPU3", "CPU4",
	"CPU5", "CPU6", "CPU7", "MFC", "STACK",
	"REGS", "HLDATA", "HLFREQ", "FPMUMSG",
};

/* keys are grouped by sysfs node */
static char akeys[][MAX_ITEM_KEY_LEN] = {
	"ID", "KTIME", "BIN", "CPS", "FAULT",
	"BUG", "PC", "LR", "STACK", "RR",
	"RSTCNT", "PINFO", "SMU", "BUS", "DPM",
	"ETC", "ESR", "MER", "PCB", "SMD",
	"CHI", "LPI", "CDI", "WDGC", "KLG", "PANIC",
	"LEV", "DCN", "WAK", "BAT", "SMP",
	"HLTYPE", "HLDATA", "HLFREQ", "HLEHLD", "FPMU",
};

static char bkeys[][MAX_ITEM_KEY_LEN] = {
	"ID", "RR", "ASB", "PSITE", "DDRID",
	"MOCP", "SOCP", "RST", "INFO2", "INFO3",
	"RBASE", "MAGIC", "PWR", "PWROFF", "PINT", "PSTAT",
	"PWROFFS", "PINTS", "PSTATS",
	"EPD", "UNFZ", "FREQ",
};

static char ckeys[][MAX_ITEM_KEY_LEN] = {
	"ID", "RR", "CPU0", "CPU1", "CPU2",
	"CPU3", "CPU4", "CPU5", "CPU6", "CPU7",
};

static char fkeys[][MAX_ITEM_KEY_LEN] = {
	"ID", "RR", "UP", "DOWN", "FPMUMSG",
};

static char mkeys[][MAX_ITEM_KEY_LEN] = {
	"ID", "RR", "MFC", "ODR", "AUD",
	"HINT"
};

static char tkeys[][MAX_ITEM_KEY_LEN] = {
	"ID", "RR", "ASV", "IDS", "REGS",
};
