/*
 * kunit_notifier.c
 * initialize, register, unregister kunit test modules
 */
#include <kunit/test.h>
#include "kunit_notifier.h"

kunit_notifier_chain_init(abc_hub_test_module);
kunit_notifier_chain_init(abc_common_test_module);
kunit_notifier_chain_init(sec_battery_test_module);
kunit_notifier_chain_init(sec_adc_test_module);
kunit_notifier_chain_init(sec_battery_dt_test_module);
kunit_notifier_chain_init(sec_battery_misc_test_module);
kunit_notifier_chain_init(sec_battery_sysfs_test_module);
kunit_notifier_chain_init(sec_battery_thermal_test_module);
kunit_notifier_chain_init(sec_battery_ttf_test_module);
kunit_notifier_chain_init(sec_battery_vote_test_module);
kunit_notifier_chain_init(sec_battery_wc_test_module);
kunit_notifier_chain_init(sec_cisd_test_module);
kunit_notifier_chain_init(sec_pd_test_module);
kunit_notifier_chain_init(sec_step_charging_test_module);
kunit_notifier_chain_init(usb_typec_manager_notifier_test_module);
kunit_notifier_chain_init(sec_cmd_test_module);

static __init int kunit_notifier_init(void)
{
	pr_info("%s\n", __func__);

	kunit_notifier_chain_register(abc_hub_test_module);
	kunit_notifier_chain_register(abc_common_test_module);
	kunit_notifier_chain_register(sec_battery_test_module);
	kunit_notifier_chain_register(sec_adc_test_module);
	kunit_notifier_chain_register(sec_battery_dt_test_module);
	kunit_notifier_chain_register(sec_battery_misc_test_module);
	kunit_notifier_chain_register(sec_battery_sysfs_test_module);
	kunit_notifier_chain_register(sec_battery_thermal_test_module);
	kunit_notifier_chain_register(sec_battery_ttf_test_module);
	kunit_notifier_chain_register(sec_battery_vote_test_module);
	kunit_notifier_chain_register(sec_battery_wc_test_module);
	kunit_notifier_chain_register(sec_cisd_test_module);
	kunit_notifier_chain_register(sec_pd_test_module);
	kunit_notifier_chain_register(sec_step_charging_test_module);
	kunit_notifier_chain_register(usb_typec_manager_notifier_test_module);
	kunit_notifier_chain_register(sec_cmd_test_module);
	return 0;
}

static __exit void kunit_notifier_exit(void)
{
	kunit_notifier_chain_unregister(abc_hub_test_module);
	kunit_notifier_chain_unregister(abc_common_test_module);
	kunit_notifier_chain_unregister(sec_battery_test_module);
	kunit_notifier_chain_unregister(usb_typec_manager_notifier_test_module);
	kunit_notifier_chain_unregister(sec_cmd_test_module);
}

module_init(kunit_notifier_init);
module_exit(kunit_notifier_exit);

MODULE_DESCRIPTION("Samsung KUnit Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
