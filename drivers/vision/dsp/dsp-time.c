// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/version.h>

#include "dsp-log.h"
#include "dsp-time.h"

void dsp_time_get_timestamp(struct dsp_time *time, int opt)
{
	dsp_enter();
	if (opt == TIMESTAMP_START)
		getrawmonotonic(&time->start);
	else if (opt == TIMESTAMP_END)
		getrawmonotonic(&time->end);
	else
		dsp_warn("time opt is invaild(%d)\n", opt);
	dsp_leave();
}
#if KERNEL_VERSION(5, 4, 0) > LINUX_VERSION_CODE
void dsp_time_get_interval(struct dsp_time *time)
{
	dsp_enter();
	time->interval = timespec_sub(time->end, time->start);
	dsp_leave();
}
#else
void dsp_time_get_interval(struct dsp_time *time)
{
	time_t delta_sec;
	long long delta_nsec;

	dsp_enter();

	delta_sec = time->end.tv_sec - time->start.tv_sec;
	delta_nsec = time->end.tv_nsec - time->start.tv_nsec;

	while (delta_nsec >= NSEC_PER_SEC) {
		/*
		 * The following asm() prevents the compiler from
		 * optimising this loop into a modulo operation.
		 */
		// TODO : Try to find 'asm usage' later
		asm("" : "+rm"(delta_nsec));
		delta_nsec -= NSEC_PER_SEC;
		++delta_sec;
	}

	while (delta_nsec < 0) {
		asm("" : "+rm"(delta_nsec));
		delta_nsec -= NSEC_PER_SEC;
		--delta_sec;
	}

	time->interval.tv_sec = delta_sec;
	time->interval.tv_nsec = delta_nsec;

	dsp_leave();
}
#endif

void dsp_time_print(struct dsp_time *time, const char *f, ...)
{
	char buf[128];
	va_list args;
	int len;

	dsp_enter();
	dsp_time_get_interval(time);

	va_start(args, f);
	len = vsnprintf(buf, sizeof(buf), f, args);
	va_end(args);

	if (len > 0)
		dsp_info("[%5lu.%06lu ms] %s\n",
				time->interval.tv_sec * 1000UL +
				time->interval.tv_nsec / 1000000UL,
				(time->interval.tv_nsec % 1000000UL),
				buf);
	else
		dsp_info("[%5lu.%06lu ms] INVALID\n",
				time->interval.tv_sec * 1000UL +
				time->interval.tv_nsec / 1000000UL,
				(time->interval.tv_nsec % 1000000UL));
	dsp_leave();
}
