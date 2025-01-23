/* SPDX-License-Identifier: GPL-2.0 */
#ifndef EXYNOS1280_CCLK_GPU_H
#define EXYNOS1280_CCLK_GPU_H

#define GPU_FREQ_STOCK_KHZ_MAX (858000)
#ifndef CPU_MAX
#define CPU_MAX INT_MAX
#endif

/* GPU Clocks */
#ifdef CONFIG_SOC_EXYNOS2100_GPU_OC // Overclocked frequencies
/* Max frequency for the GPU */
#define GPU_FREQ_KHZ_MAX (1001000)
#define GPU_FREQ_KHZ_MIN (130000)

static const unsigned int gpu_custom_clock[] = {1040000, 949000, 858000, 767000, 676000, 585000, 494000, 403000, 312000, 221000, 130000};
static const unsigned int gpu_custom_min_threshold[] = {78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 0};
static const unsigned int gpu_custom_max_threshold[] = {100, 97, 95, 93, 95, 95, 95, 95, 90, 85, 85};
static const unsigned int gpu_custom_staycount[] = {5, 5, 5, 5, 3, 1, 1, 1, 1, 1, 1};
static const unsigned int gpu_custom_mem_freq[] = {3172000, 3172000, 3172000, 2730000, 2535000, 2288000, 2028000, 2028000, 1539000, 1014000, 676000};
static const unsigned int gpu_custom_lit[] = {858000, 858000, 858000, 858000, 858000, 858000, 858000, 858000, 0, 0, 0};
static const unsigned int gpu_custom_llc_ways[] = {16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0};
static const unsigned int gpu_custom_mid = 0;
static const unsigned int gpu_custom_big = CPU_MAX;

#else // Stock frequencies
/* Max frequency for the GPU */
#define GPU_FREQ_KHZ_MAX (858000)
#define GPU_FREQ_KHZ_MIN (130000)

/* DVFS table converted to arrays */
static const unsigned int gpu_custom_clock[] = {858000, 767000, 676000, 585000, 494000, 403000, 312000, 221000, 130000};
static const unsigned int gpu_custom_min_threshold[] = {78, 78, 78, 78, 78, 78, 78, 78, 78};
static const unsigned int gpu_custom_max_threshold[] = {95, 93, 95, 95, 95, 95, 90, 85, 85};
static const unsigned int gpu_custom_staycount[] = {5, 5, 5, 3, 1, 1, 1, 1, 1};
static const unsigned int gpu_custom_mem_freq[] = {3172000, 2730000, 2535000, 2288000, 2028000, 2028000, 1539000, 1014000, 676000};
static const unsigned int gpu_custom_lit[] = {858000, 858000, 858000, 858000, 858000, 858000, 858000, 0, 0};
static const unsigned int gpu_custom_llc_ways[] = {16, 16, 16, 16, 16, 16, 0, 0, 0};
static const unsigned int gpu_custom_mid = 0;
static const unsigned int gpu_custom_big = CPU_MAX;
#endif

static const int gpu_custom_array_size = sizeof(gpu_custom_clock) / sizeof(gpu_custom_clock[0]);

#endif