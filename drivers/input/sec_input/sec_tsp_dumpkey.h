#ifndef	__SEC_TSP_DUMPKEY_H__
#define __SEC_TSP_DUMPKEY_H__
struct sec_tsp_dumpkey_param {
	unsigned int keycode;
	int down;
};

struct tsp_dump_callbacks {
	void (*inform_dump)(struct device *dev);
};
#endif /* __SEC_TSP_DUMPKEY_H__ */
