#ifndef SEC_CLASS_H
#define SEC_CLASS_H

#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
extern struct device *sec_device_create(void *drvdata, const char *fmt);
extern void sec_device_destroy(dev_t devt);
extern struct device *sec_dev_get_by_name(const char *name);
#else
#define sec_device_create(a, b)		(-1)
#define sec_device_destroy(a)		do { } while (0)
#define sec_dev_get_by_name(a)		(-1)
#endif

#endif /* SEC_CLASS_H */
