/*
 * kunit_notifier.h
 */
#include <linux/notifier.h>

extern int register_kunit_notifier(struct notifier_block *nb);
extern int unregister_kunit_notifier(struct notifier_block *nb);

extern struct test_module abc_hub_test_module;
extern struct test_module abc_common_test_module;
extern struct test_module sec_battery_test_module;
extern struct test_module sec_adc_test_module;
extern struct test_module sec_battery_dt_test_module;
extern struct test_module sec_battery_misc__module;
extern struct test_module sec_battery_sysfs_test_module;
extern struct test_module sec_battery_thermal_test_module;
extern struct test_module sec_battery_ttf_test_module;
extern struct test_module sec_battery_vote_test_module;
extern struct test_module sec_battery_wc_test_module;
extern struct test_module sec_cisd_test_module;
extern struct test_module sec_pd_test_module;
extern struct test_module usb_typec_manager_notifier_test_module;
extern struct test_module sec_cmd_test_module;
extern struct test_module sec_step_charging_test_module;

/*
 * kunit_notifier_chain_init() - initialize kunit notifier for module built
 */
#define kunit_notifier_chain_init(module)					\
	extern struct test_module module;					\
	static int kunit_run_notify_##module(struct notifier_block *self,	\
			unsigned long event, void *data)			\
	{									\
		if (test_run_tests(&module)) {		\
			pr_warn("kunit error: %s\n", module.name);		\
			return NOTIFY_BAD;					\
		}								\
		return NOTIFY_OK;						\
	}									\
	static struct notifier_block callchain_notifier_##module = {		\
		.notifier_call = kunit_run_notify_##module,			\
	};

#define kunit_notifier_chain_register(module)					\
	register_kunit_notifier(&callchain_notifier_##module);

#define kunit_notifier_chain_unregister(module)					\
	unregister_kunit_notifier(&callchain_notifier_##module);
