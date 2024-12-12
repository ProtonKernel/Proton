#ifndef __CMU_EWF_H__
#define __CMU_EWF_H__

#define EARLY_WAKEUP_FORCED_ENABLE0		(0x0a00)
#define EARLY_WAKEUP_FORCED_ENABLE1		(0x0a04)
#define EWF_MAX_INDEX				(64)

/* for EXYNOS2100 */
#define EWF_GRP_CAM             (51)
#define EWF_CAM_BLK             (0xF0000894)    //BUS0, BUS2, CORE, CSIS, MIF0, MIF1, MIF2, MIF3

#if defined(CONFIG_CMU_EWF) || defined(CONFIG_CMU_EWF_MODULE)
extern int get_cmuewf_index(struct device_node *np, unsigned int *index);
extern int set_cmuewf(unsigned int index, unsigned int en);
extern int early_wakeup_forced_enable_init(void);

#else
static inline int get_cmuewf_index(struct device_node *np, unsigned int *index)
{
	return 0;
}

static inline int set_cmuewf(unsigned int index, unsigned int en)
{
	return 0;
}
static int early_wakeup_forced_enable_init(void)
{
	return 0;
}
#endif
#endif
