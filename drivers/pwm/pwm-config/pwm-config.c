#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/pinctrl/consumer.h>

static int pwm_config_probe(struct platform_device *pdev)
{
	struct pinctrl *config_pinctrl;

	pr_info("pwm_config: probe\n");

	if (!pdev->dev.of_node) {
		pr_err("pwm_config: %s failed, DT is NULL", __func__);
		return -ENODEV;
	}

	config_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(config_pinctrl))
		pr_err("pwm_config: Failed to get pinctrl(%d)\n", IS_ERR(config_pinctrl));

	pr_info("pwm_config : probe done\n");
	return 0;
}

static int pwm_config_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id pwm_config_match[] = {
	{.compatible = "samsung,pwm_pinctrl", },
	{}
};

static struct platform_driver pwm_config_pdrv = {
	.driver = {
		.name = "pwm_config",
		.owner = THIS_MODULE,
		.of_match_table = pwm_config_match,
	},
	.probe = pwm_config_probe,
	.remove = pwm_config_remove,
};

module_platform_driver(pwm_config_pdrv);

MODULE_AUTHOR("Samsung Corporation");
MODULE_DESCRIPTION("Pwm pinctrl configuration for LSI AP");
MODULE_LICENSE("GPL v2");
