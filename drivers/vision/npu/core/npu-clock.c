/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "npu-clock.h"

int npu_clk_get(struct npu_clocks *clocks, struct device *dev)
{
	const char **clk_ids;
	struct clk *clk;
	int ret, i;

	BUG_ON(!clocks);
	BUG_ON(!dev);

	clocks->clk_count = of_property_count_strings(dev->of_node, "clock-names");
	if (IS_ERR_VALUE((unsigned long)clocks->clk_count)) {
		probe_err("invalid clk list in %s node", dev->of_node->name);
		return -EINVAL;
	}

	clk_ids = (const char **)devm_kmalloc(dev,
				(clocks->clk_count + 1) * sizeof(const char *),
				GFP_KERNEL);
	if (!clk_ids) {
		probe_err("failed to alloc for clock ids");
		return -ENOMEM;
	}

	for (i = 0; i < clocks->clk_count; i++) {
		ret = of_property_read_string_index(dev->of_node, "clock-names",
								i, &clk_ids[i]);
		if (ret) {
			probe_err("failed to read clocks name %d from %s node\n",
					i, dev->of_node->name);
			return ret;
		}
	}
	clk_ids[clocks->clk_count] = NULL;

	clocks->clocks = (struct clk **) devm_kmalloc(dev,
			(clocks->clk_count + 1) * sizeof(struct clk *), GFP_KERNEL);
	if (!clocks->clocks) {
		probe_err("couldn't alloc clk list");
		return -ENOMEM;
	}

	for (i = 0; clk_ids[i] != NULL; i++) {
		clk = devm_clk_get(dev, clk_ids[i]);
		if (IS_ERR_OR_NULL(clk)) {
			probe_err("couldn't get %s clock\n", clk_ids[i]);
			goto err;
		}

		clocks->clocks[i] = clk;
		probe_info("got clock %s\n", clk_ids[i]);
	}
	clocks->clocks[i] = NULL;
	if (i != clocks->clk_count) {
		probe_err("clk count mismatch");
		goto err;
	}

	return 0;
err:
	return -EINVAL;
}

void npu_clk_put(struct npu_clocks *clocks, struct device *dev)
{
	int i;

	for (i = clocks->clk_count - 1; i >= 0; i--) {
		if (!clocks->clocks[i]) {
			npu_err("invalid clock in [%d]", i);
			continue;
		}
		devm_clk_put(dev, clocks->clocks[i]);
		clocks->clocks[i] = NULL;
	}
}

int npu_clk_prepare_enable(struct npu_clocks *clocks)
{
	int i;
	int ret;

	for (i = 0; i < clocks->clk_count; i++) {
		if (!clocks->clocks[i]) {
			npu_err("invalid clock in [%d]", i);
			goto err;
		}
		ret = clk_prepare_enable(clocks->clocks[i]);
		if (ret) {
			npu_err("couldn't prepare and enable clock[%d]\n", i);
			goto err;
		}
	}
	return 0;
err:
	/* roll back */
	for (i = i - 1; i >= 0; i--)
		clk_disable_unprepare(clocks->clocks[i]);

	return ret;
}

void npu_clk_disable_unprepare(struct npu_clocks *clocks)
{
	int i;

	for (i = clocks->clk_count - 1; i >= 0; i--) {
		if (!clocks->clocks[i]) {
			npu_err("invalid clock in [%d]", i);
			continue;
		}
		clk_disable_unprepare(clocks->clocks[i]);
		if (__clk_is_enabled(clocks->clocks[i]))
			npu_err("tryed to disable clock %d but failed\n", i);
	}
}
