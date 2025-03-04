#ifndef __FVMAP_H__
#define __FVMAP_H__

#define BLOCK_ADDR_SIZE			(3)
#define CHILD_ID_NUM			(12)

/* FV(Frequency Voltage MAP) */
struct fvmap_header {
	unsigned char domain_id;
	unsigned char num_of_lv;
	unsigned char num_of_members;
	unsigned char num_of_pll;
	unsigned char num_of_mux;
	unsigned char num_of_div;
	unsigned short o_famrate;
	unsigned char init_lv;
	unsigned char num_of_child;
	unsigned char parent_id;
	unsigned char parent_offset;
	unsigned short block_addr[BLOCK_ADDR_SIZE];
	unsigned short o_members;
	unsigned short o_ratevolt;
	unsigned short o_tables;

	unsigned int init_rate;
	unsigned int min_rate;
	unsigned int max_rate;
	unsigned char child_id[CHILD_ID_NUM];
	unsigned char copy_col;
};

struct clocks {
	unsigned short addr[0];
};

struct pll_header {
	unsigned int addr;
	unsigned short o_lock;
	unsigned short level;
	unsigned int pms[0];
};

struct rate_volt {
	u32 rate:24;
	u32 volt:8;
};

struct rate_volt_header {
	struct rate_volt table[0];
};

struct dvfs_table {
	unsigned char val[0];
};

struct freq_volt {
	unsigned int rate;
	unsigned int volt;
};

#if defined(CONFIG_ACPM_DVFS) || defined(CONFIG_ACPM_DVFS_MODULE)
extern int fvmap_init(void __iomem *sram_base);
extern int fvmap_get_voltage_table(unsigned int id, unsigned int *table);
extern int fvmap_get_freq_volt_table(unsigned int id, void *freq_volt_table,
		unsigned int table_size);
#else
static inline int fvmap_init(void __iomem *sram_base)
{
	return 0;
}

static inline int fvmap_get_voltage_table(unsigned int id, unsigned int *table)
{
	return 0;
}
static inline int fvmap_get_freq_volt_table(unsigned int id, void *freq_volt_table,
		unsigned int table_size);
{
	return 0;
}
#endif
#endif
