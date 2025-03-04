/*
 * Copyright (C) 2021 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "flicker_test.h"

/* Waiting for an interrupt error */
#define TEST_WAIT_TIME (30)

static struct test_env env;
static struct test_data *data;
static struct result_data test_result = {0};

static struct mutex test_lock;
static void (*err_handler)(void);

static int g_Hz = -1;

enum hrtimer_restart __als_flickering_func(struct hrtimer *timer);
enum hrtimer_restart __als_flickering_force_stop_func(struct hrtimer *timer);
int __als_flickering_start(void);
void __als_set_led_mode(int current_index);
int __als_eol_init_alloc(void);

/**
 * als_eol_set_err_handler - Register handler for specific driver
 *
 * @handler	 : error callback function
 *
 * You can register test error handler (dump register, fail safe operations, etc)
 */
void als_eol_set_err_handler(void (*handler)(void)){
	err_handler = handler;
}
EXPORT_SYMBOL(als_eol_set_err_handler);

/**
 * als_set_fixed_hz - Experimental feature to test flickering
 *
 * @Hz	: Flickering hertz which you want to make. 50~500
 *
 */
void als_set_fixed_hz(int Hz)
{
	if (Hz >= 50 && Hz <= 500)
		g_Hz = Hz;
	printk(KERN_INFO "%s - gHz:%d", __func__, g_Hz);
}
EXPORT_SYMBOL(als_set_fixed_hz);


/**
 * als_eol_update_als - Record every test value
 *
 * @awb      : current 'awb' value
 *            (You can use any value as you need. Maybe Infrared value will be used in most cases)
 * @clear    : current clear value detected by sensor. It means visible light in light spectrum
 * @wideband : current Wideband value detected by sensor.
 *             wideband light include visible light and infrared.
 *
 */
void als_eol_update_als(int awb, int clear, int wideband, int uv)
{
	if (data == NULL || data->eol_enable == false)
		return;

	if (data->eol_count > EOL_SKIP_COUNT) {
		data->eol_awb += awb;
		data->eol_clear += clear;
		data->eol_wideband += wideband;
		data->eol_uv += uv;
	}

	if (data->eol_state == EOL_STATE_INIT) {
		memset(&test_result, 0, sizeof(struct result_data));

		data->eol_count = 0;
		data->eol_awb = 0;
		data->eol_clear = 0;
		data->eol_wideband = 0;
		data->eol_flicker = 0;
		data->eol_flicker_count = 0;
		data->eol_state = EOL_STATE_100;
		data->eol_pulse_count = 0;
		data->eol_uv = 0;
	} else if (data->eol_state < EOL_STATE_DONE) {
		data->eol_count++;
		printk(KERN_INFO"%s - eol_state:%d, eol_cnt:%d, flk_avr:%d (sum:%d, cnt:%d), ir:%d, clear:%d, wide:%d, uv:%d\n", __func__,
				data->eol_state, data->eol_count, (data->eol_flicker / data->eol_flicker_count), data->eol_flicker, data->eol_flicker_count,
				data->eol_awb, data->eol_clear, data->eol_wideband, data->eol_uv);

		if (data->eol_count >= (EOL_COUNT + EOL_SKIP_COUNT) && data->eol_flicker_count >= (EOL_COUNT + EOL_SKIP_COUNT)) {
			test_result.flicker[data->eol_state] = data->eol_flicker / (data->eol_flicker_count - EOL_SKIP_COUNT);
			test_result.awb[data->eol_state] = data->eol_awb / (data->eol_count - EOL_SKIP_COUNT);
			test_result.clear[data->eol_state] = data->eol_clear / (data->eol_count - EOL_SKIP_COUNT);
			test_result.wideband[data->eol_state] = data->eol_wideband / (data->eol_count - EOL_SKIP_COUNT);
			test_result.uv[data->eol_state] = data->eol_uv / (data->eol_count - EOL_SKIP_COUNT);

			printk(KERN_INFO"%s - eol_state = %d, pulse_count = %d, flicker_result = %d\n",
					__func__, data->eol_state, data->eol_pulse_count, test_result.flicker[data->eol_state]);

			data->eol_count = 0;
			data->eol_awb = 0;
			data->eol_clear = 0;
			data->eol_wideband = 0;
			data->eol_flicker = 0;
			data->eol_flicker_count = 0;
			data->eol_pulse_count = 0;
			data->eol_uv = 0;
			data->eol_state++;
		}
	}
}
EXPORT_SYMBOL(als_eol_update_als);

/**
 * als_eol_update_flicker - Record every test value
 *
 * @flicker: current Flicker value detected by sensor.
 */
void als_eol_update_flicker(int Hz)
{
	if (data == NULL || data->eol_enable == false)
		return;

	data->eol_flicker_count++;
	if (Hz != 0 && data->eol_flicker_count > EOL_SKIP_COUNT)
		data->eol_flicker += Hz;
}
EXPORT_SYMBOL(als_eol_update_flicker);

/**
 * als_eol_set_env - set the flickering test environment
 *
 * @torch: whether you use led as torch mode
 * @intensity : led current value in mA.
 *
 * If you edit this function, you should manipulate the intensity to appropriate index value of led modules.
 */
void als_eol_set_env(bool torch, int intensity)
{
	int err;
	err = __als_eol_init_alloc();
	if (err < 0)
		printk(KERN_ERR "%s - test init fail", __func__);

#if IS_ENABLED(CONFIG_LEDS_S2MPB02)
	if (torch) {
		env.gpio_led = gpio_torch;
		env.led_curr = (intensity/20);
		env.led_mode = S2MPB02_TORCH_LED_1;
	} else {
		env.gpio_led = gpio_flash;
		env.led_curr = (intensity-50)/100;
		env.led_mode = S2MPB02_FLASH_LED_1;
	}
	printk(KERN_INFO "%s - gpio:%d intensity:%d(%d) led_mode:%d",
			__func__, env.gpio_led, intensity, env.led_curr, env.led_mode);
#elif IS_ENABLED(CONFIG_LEDS_SM5714)
	env.gpio_led = gpio_torch;
	env.led_curr = (intensity-50)/25;
	printk(KERN_INFO "%s - gpio:%d", __func__, env.gpio_led);
#elif IS_ENABLED(CONFIG_LEDS_KTD2692)
	// KTD don't use gpios
	env.gpio_led = -1;
	env.led_curr = intensity;
	env.led_mode = 4;	//Flicker mode

	printk(KERN_INFO "%s - gpio:%d intensity:%d(%d) led_mode:%d",
			__func__, env.gpio_led, intensity, env.led_curr, env.led_mode);
#elif IS_ENABLED(CONFIG_LEDS_AW36518_FLASH)
	env.gpio_led = gpio_flash;
	env.led_curr = (intensity);
	printk(KERN_INFO "%s - gpio:%d intensity:%d(%d)",
			__func__, env.gpio_led, intensity, env.led_curr);
#else
	printk(KERN_INFO "%s - env not set");
#endif
	env.led_state = 1;
}
EXPORT_SYMBOL(als_eol_set_env);

/**
 * als_eol_mode - start LED flicker loop
 *
 * Return result_data
 * MUST call als_eol_update* functions to notify the sensor output!!
 */
struct result_data* als_eol_mode(void)
{
	int ret;
	ktime_t wait_time = TEST_WAIT_TIME * NSEC_PER_SEC;

	if (data == NULL) {
		printk(KERN_ERR "%s - test environment not configured", __func__);
		goto err;
	}

	ret = __als_flickering_start();		/* start flickerting timer */
	if (ret < 0)
		return ERR_PTR(ret);

	printk(KERN_INFO "%s - flickering started, fail safe timer %lld", __func__, wait_time);
	hrtimer_start(&data->force_stop_timer, wait_time, HRTIMER_MODE_REL);

	/* lock will be released in
	 *  __als_flickering_force_stop_timer : test not finished till error timeout
	 *  __als_flickering_func  : when test finished
	 */
	mutex_lock(&test_lock);
	printk(KERN_INFO "%s - lock released", __func__);

	data->eol_enable = 0;
	if (data->eol_state < EOL_STATE_DONE) {
		test_result.result = -1;
		if (err_handler)
			err_handler();
		goto err;
	}

	hrtimer_cancel(&data->force_stop_timer);
	__als_set_led_mode(0);
#if !IS_ENABLED(CONFIG_LEDS_KTD2692)
	gpio_free(env.gpio_led);
#endif

	test_result.ratio[EOL_STATE_100] = test_result.awb[EOL_STATE_100]*100 / test_result.clear[EOL_STATE_100];
	test_result.ratio[EOL_STATE_120] = test_result.awb[EOL_STATE_120]*100 / test_result.clear[EOL_STATE_120];

	printk(KERN_INFO "%s -RESULT: flicker:%d|%d awb:%d|%d clear:%d|%d wide:%d|%d ratio:%d|%d uv:%d|%d", __func__,
			test_result.flicker[EOL_STATE_100],	test_result.flicker[EOL_STATE_120],
			test_result.awb[EOL_STATE_100],		test_result.awb[EOL_STATE_120],
			test_result.clear[EOL_STATE_100],	test_result.clear[EOL_STATE_120],
			test_result.wideband[EOL_STATE_100],	test_result.wideband[EOL_STATE_120],
			test_result.ratio[EOL_STATE_100],	test_result.ratio[EOL_STATE_120],
			test_result.uv[EOL_STATE_100], test_result.uv[EOL_STATE_120]);

	g_Hz = -1;
err:
	mutex_unlock(&test_lock);

	kfree(data);
	data = NULL;
	return &test_result;
}
EXPORT_SYMBOL(als_eol_mode);


int __als_eol_init_alloc(void)
{
	if (data == NULL) {
		data = (struct test_data*)kzalloc(sizeof(struct test_data), GFP_KERNEL);
		if (data == NULL) {
			printk(KERN_INFO "data alloc err");
			return -ENOMEM;
		}
	}

	data->pulse_duty[0] = 1000000;
	data->pulse_duty[1] = DEFAULT_DUTY_50HZ;
	data->pulse_duty[2] = DEFAULT_DUTY_60HZ;

	hrtimer_init(&data->flicker_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	data->flicker_timer.function = __als_flickering_func;
	hrtimer_init(&data->force_stop_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	data->force_stop_timer.function = __als_flickering_force_stop_func;

	return 0;
}

void __als_set_led_mode(int current_index)
{
// KTD2692 doesn't need this func.
#if IS_ENABLED(CONFIG_LEDS_S2MPB02)
	s2mpb02_led_en(env.led_mode, current_index, S2MPB02_LED_TURN_WAY_GPIO);
#elif IS_ENABLED(CONFIG_LEDS_SM5714)
	/* 50~225mA, 25mA step */
	sm5714_fled_torch_gpio(current_index);
#elif IS_ENABLED(CONFIG_LEDS_AW36518_FLASH)
	if (current_index)
		aw36518_enable_flicker(current_index, true);
	else
		aw36518_enable_flicker(0, false);
#endif
}

int __als_flickering_start(void)
{
	int pulse_duty, ret;
#if !IS_ENABLED(CONFIG_LEDS_KTD2692)
	ret = gpio_request(env.gpio_led, NULL);
	if (ret < 0)
		return ret;
#endif

	data->eol_state = EOL_STATE_INIT;
	data->eol_enable = 1;

#if IS_ENABLED(CONFIG_LEDS_S2MPB02)
	__als_set_led_mode(0);
#endif
	__als_set_led_mode(env.led_curr);

	if (g_Hz != -1) {
		pulse_duty= (1000000/g_Hz)/2;
		data->pulse_duty[0] = pulse_duty * NSEC_PER_USEC;
		data->pulse_duty[1] = pulse_duty * NSEC_PER_USEC;
		data->pulse_duty[2] = pulse_duty * NSEC_PER_USEC;
		printk(KERN_INFO "%s - cur pulse_duty %d g_Hz %d", __func__, pulse_duty, g_Hz);
	}
	printk(KERN_INFO "%s - duty %lld %lld", __func__, data->pulse_duty[1], data->pulse_duty[2]);

	printk(KERN_INFO"%s - eol_loop start", __func__);

	mutex_lock(&test_lock);
	hrtimer_start(&data->flicker_timer, data->pulse_duty[data->eol_state + 1], HRTIMER_MODE_REL);

	return 0;
}

void led_work_func(struct work_struct *work)
{
#if IS_ENABLED(CONFIG_LEDS_S2MPB02) || IS_ENABLED(CONFIG_LEDS_SM5714) || IS_ENABLED(CONFIG_LEDS_AW36518_FLASH)
	gpio_direction_output(env.gpio_led, env.led_state);
#elif IS_ENABLED(CONFIG_LEDS_KTD2692)
	ktd2692_led_mode_ctrl(env.led_mode, env.led_state * env.led_curr);
#endif
	env.led_state ^= 1;
}

DECLARE_WORK(led_work, led_work_func);

enum hrtimer_restart __als_flickering_func(struct hrtimer *timer)
{
	if (data->eol_state >= EOL_STATE_DONE) {
		env.led_state = 0;
		schedule_work(&led_work);

		mutex_unlock(&test_lock);
		return HRTIMER_NORESTART;
	}
	hrtimer_forward_now(timer, data->pulse_duty[data->eol_state + 1]);
	schedule_work(&led_work);

	return HRTIMER_RESTART;
}

enum hrtimer_restart __als_flickering_force_stop_func(struct hrtimer *timer)
{
	printk(KERN_INFO "%s - wait time done! disable test", __func__);
	hrtimer_cancel(&data->flicker_timer);
	mutex_unlock(&test_lock);
	return HRTIMER_NORESTART;
}

int als_eol_parse_dt(void)
{
	struct device_node *np;

	np = of_find_node_by_name(NULL, LED_DT_NODE_NAME);
	if (np == NULL) {
		printk(KERN_ERR "Can't find led node");
		return -ENODEV;
	}
#if IS_ENABLED(CONFIG_LEDS_S2MPB02) || IS_ENABLED(CONFIG_LEDS_SM5714) || IS_ENABLED(CONFIG_LEDS_AW36518_FLASH)
	gpio_torch = of_get_named_gpio(np, "torch-gpio", 0);
	gpio_flash = of_get_named_gpio(np, "flash-gpio", 0);
#endif
	printk(KERN_INFO "torch : %d flash : %d", gpio_torch, gpio_flash);

	return 0;
}

int als_eol_init(void)
{
	int ret = 0;
	printk(KERN_INFO "EOL_TEST Module init\n");
	data = NULL;

	ret = als_eol_parse_dt();
	if (ret < 0) {
		printk(KERN_ERR "dt parse fail!");
		return ret;
	}

	mutex_init(&test_lock);

	return ret;
}

void als_eol_exit(void)
{
	printk(KERN_INFO "EOL_TEST Module exit\n");
	kfree(data);
}

module_init(als_eol_init);
module_exit(als_eol_exit);

MODULE_LICENSE("GPL");
