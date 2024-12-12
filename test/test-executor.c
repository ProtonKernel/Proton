#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/printk.h>
#include <linux/reboot.h>
#include <kunit/test.h>
#include <linux/notifier.h>
#include <linux/module.h>

extern char __test_modules_start;
extern char __test_modules_end;

static int kunit_shutdown;
core_param(kunit_shutdown, kunit_shutdown, int, 0644);

bool test_run_all_tests(void)
{
	struct test_module **module;
	struct test_module ** const test_modules_start =
			(struct test_module **) &__test_modules_start;
	struct test_module ** const test_modules_end =
			(struct test_module **) &__test_modules_end;
	bool has_test_failed = false;

	for (module = test_modules_start; module < test_modules_end; ++module) {
		if (test_run_tests(*module))
			has_test_failed = true;
	}

	if (kunit_shutdown)
		kernel_halt();

	return !has_test_failed;
}

BLOCKING_NOTIFIER_HEAD(kunit_notify_chain);

int test_executor_init(void)
{
#ifndef CONFIG_UML
	int noti = 0;
#endif
	/* Trigger the built-in kunit tests */
	if (!test_run_all_tests())
		pr_warn("Running built-in kunit tests are unsuccessful.\n");
#ifndef CONFIG_UML
	/* Trigger the module kunit tests */
	noti = blocking_notifier_call_chain(&kunit_notify_chain, 0, NULL);
	if (noti == NOTIFY_OK || noti == NOTIFY_DONE)
		return 0;
	pr_warn("Running kunit_notifier_calls are unsuccessful. errno: 0x%x", noti);
#endif
	return 0;
}
EXPORT_SYMBOL_KUNIT(test_executor_init);

int register_kunit_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&kunit_notify_chain, nb);
}
EXPORT_SYMBOL_KUNIT(register_kunit_notifier);

int unregister_kunit_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&kunit_notify_chain, nb);
}
EXPORT_SYMBOL_KUNIT(unregister_kunit_notifier);
