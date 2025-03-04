/*
 *
 * $Id: stk3a8x.h
 *
 * Copyright (C) 2012~2020 Taka Chiu    <taka_chiu@sensortek.com.tw>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */
#ifndef __STK3A8X_H__
#define __STK3A8X_H__

#include <linux/pinctrl/consumer.h>

#define DRIVER_VERSION  "2.2.4"

/* Driver Settings */
#define STK_ALS_CALI
//#define STK_ALS_FIR
#define STK_FIFO_ENABLE
//#define SUPPORT_SENSOR_CLASS
//#define STK_CHECK_LIB
#define STK_FFT_FLICKER
#define STK_SW_AGC
#define CONFIG_AMS_ALS_COMPENSATION_FOR_AUTO_BRIGHTNESS

/* Define Register Map */
#define STK3A8X_REG_STATE                   0x00
#define STK3A8X_REG_ALSCTRL1                0x02
#define STK3A8X_REG_INTCTRL1                0x04
#define STK3A8X_REG_WAIT                    0x05
#define STK3A8X_REG_THDH1_ALS               0x0A
#define STK3A8X_REG_THDH2_ALS               0x0B
#define STK3A8X_REG_THDL1_ALS               0x0C
#define STK3A8X_REG_THDL2_ALS               0x0D
#define STK3A8X_REG_FLAG                    0x10
#define STK3A8X_REG_DATA1_F                 0x13
#define STK3A8X_REG_DATA2_F                 0x14
#define STK3A8X_REG_DATA1_IR                0x15
#define STK3A8X_REG_DATA2_IR                0x16
#define STK3A8X_REG_PDT_ID                  0x3E
#define STK3A8X_REG_RSRVD                   0x3F
#define STK3A8X_REG_GAINCTRL                0x4E
#define STK3A8X_REG_FIFOCTRL1               0x60
#define STK3A8X_REG_THD1_FIFO_FCNT          0x61
#define STK3A8X_REG_THD2_FIFO_FCNT          0x62
#define STK3A8X_REG_FIFOCTRL2               0x63
#define STK3A8X_REG_FIFOFCNT1               0x64
#define STK3A8X_REG_FIFOFCNT2               0x65
#define STK3A8X_REG_FIFO_OUT                0x66
#define STK3A8X_REG_FIFO_FLAG               0x67
#define STK3A8X_REG_ALSCTRL2                0x6F
#define STK3A8X_REG_SW_RESET                0x80
#define STK3A8X_REG_AGAIN                   0xDB
#define STK3A8X_REG_FIFOCTRL3               0xFF

/* Define state reg */
#define  STK3A8X_STATE_EN_WAIT_SHIFT            2
#define  STK3A8X_STATE_EN_ALS_SHIFT             1
#define  STK3A8X_STATE_EN_WAIT_MASK             0x04
#define  STK3A8X_STATE_EN_ALS_MASK              0x02

/* Define ALS ctrl reg */
#define  STK3A8X_ALS_PRS_SHIFT           6
#define  STK3A8X_ALS_GAIN_SHIFT          4
#define  STK3A8X_ALS_IT_SHIFT            0
#define  STK3A8X_ALS_PRS_MASK            0xC0
#define  STK3A8X_ALS_GAIN_MASK           0x30
#define  STK3A8X_ALS_IT_MASK             0x0F

/* Define interrupt reg */
#define  STK3A8X_INT_ALS_SHIFT           3
#define  STK3A8X_INT_ALS_MASK            0x08
#define  STK3A8X_INT_ALS                 0x08

/* Define flag reg */
#define  STK3A8X_FLG_ALSDR_SHIFT            7
#define  STK3A8X_FLG_ALSINT_SHIFT           5
#define  STK3A8X_FLG_ALSSAT_SHIFT           2

#define  STK3A8X_FLG_ALSDR_MASK          0x80
#define  STK3A8X_FLG_ALSINT_MASK         0x20
#define  STK3A8X_FLG_ALSSAT_MASK         0x04

/* Define ALS parameters */
#define  STK3A8X_ALS_PRS1               0x00
#define  STK3A8X_ALS_PRS2               0x40
#define  STK3A8X_ALS_PRS4               0x80
#define  STK3A8X_ALS_PRS8               0xC0

#define  STK3A8X_ALS_GAIN1              0x00
#define  STK3A8X_ALS_GAIN4              0x10
#define  STK3A8X_ALS_GAIN16             0x20
#define  STK3A8X_ALS_GAIN64             0x30
#define  STK3A8X_ALS_GAIN128            0x02
#define  STK3A8X_ALS_DGAIN128_MASK      0x02
#define  STK3A8X_ALS_DGAIN128_SHIFT     1
#define  STK3A8X_ALS_GAIN_C_MASK        0x30
#define  STK3A8X_ALS_GAIN128_C_MASK     0x04

#define  STK3A8X_ALS_IT25               0x03
#define  STK3A8X_ALS_IT50               0x04
#define  STK3A8X_ALS_IT100              0x05
#define  STK3A8X_ALS_IT200              0x06

#define  STK3A8X_ALS_IR_GAIN1           0x00
#define  STK3A8X_ALS_IR_GAIN4           0x10
#define  STK3A8X_ALS_IR_GAIN16          0x20
#define  STK3A8X_ALS_IR_GAIN64          0x30
#define  STK3A8X_ALS_IR_GAIN128         0x04

#define  STK3A8X_WAIT20                 0x0C
#define  STK3A8X_WAIT50                 0x20
#define  STK3A8X_WAIT100                0x40

/** sw reset value */
#define STK_STK3A8X_SWRESET             0x00

/** Off to idle time */
#define STK3A8X_OFF_TO_IDLE_MS          10  //ms

/** ALS threshold */
#define STK3A8X_ALS_THD_ADJ             0.05
#define STK3A8X_NUM_AXES                3
#define STK3A8X_MAX_MIN_DIFF            200
#define STK3A8X_LT_N_CT                 1700
#define STK3A8X_HT_N_CT                 2200

#define STK3A8X_ALS_DATA_READY_TIME     60
#define STK3A8X_ALS_THRESHOLD           30

#ifdef STK_ALS_CALI
	#define STK3A8X_ALS_CALI_DATA_READY_US  55000000
	#define STK3A8X_ALS_CALI_TARGET_LUX     500.0
#endif

#define  STK3A8X_IT_ALS_SEL_MASK            0x80
#define  STK3A8X_IT2_ALS_MASK               0x1F
#define  STK3A8X_ALS_IT192                  0x00
#define  STK3A8X_ALS_IT288                  0x01
#define  STK3A8X_ALS_IT384                  0x02
#define  STK3A8X_ALS_IT480                  0x03
#define  STK3A8X_ALS_IT576                  0x04
#define  STK3A8X_ALS_IT672                  0x05
#define  STK3A8X_ALS_IT768                  0x06
#define  STK3A8X_ALS_IT1056                 0x09
#define  STK3A8X_ALS_IT1152                 0x0A
#define  STK3A8X_ALS_IT1248                 0x0B
#define  STK3A8X_ALS_IT1344                 0x0C
#define  STK3A8X_ALS_IT1440                 0x0D
#define  STK3A8X_ALS_IT1536                 0x0E
#define  STK3A8X_ALS_IT2112                 0x14

#ifdef STK_FIFO_ENABLE
	#define STK_FIFO_I2C_READ_FRAME             64  //get fifo data one time
	#define STK_FIFO_I2C_READ_FRAME_TARGET      64  //for fifo interrupt
	#define STK_FIFO_GET_MAX_FRAME              128 //get fifo frame limitation

	#define  STK3A8X_FIFO_SEL_DATA_FLICKER      0x00
	#define  STK3A8X_FIFO_SEL_DATA_IR           0x01
	#define  STK3A8X_FIFO_SEL_DATA_FLICKER_IR   0x02
	#define  STK3A8X_FIFO_SEL_MASK              0x03

	#define  STK3A8X_FIFO_MODE_OFF              0x00
	#define  STK3A8X_FIFO_MODE_BYPASS           0x10
	#define  STK3A8X_FIFO_MODE_NORMAL           0x20
	#define  STK3A8X_FIFO_MODE_STREAM           0x30

	#define  STK3A8X_FOVR_EN_MASK               0x04
	#define  STK3A8X_FWM_EN_MASK                0x02
	#define  STK3A8X_FFULL_EN_MASK              0x01

	#define  STK3A8X_FLG_FIFO_OVR_MASK          0x80
	#define  STK3A8X_FLG_FIFO_WM_MASK           0x10
	#define  STK3A8X_FLG_FIFO_FULL_MASK         0x01
#endif

#ifdef STK_SW_AGC
	#define STK3A8X_SW_AGC_H_THD                88
	#define STK3A8X_SW_AGC_L_THD                22
	#define STK3A8X_SW_AGC_DATA_BLOCK           128
	#define STK3A8X_SW_AGC_SAT_GOLDEN           172
	#define STK3A8X_SW_AGC_SAT_STEP             86
	#define STK3A8X_SW_AGC_ALS_IT_HTD           50000
	#define STK3A8X_SW_AGC_ALS_IT_LTD           10000
	#ifdef STK_FIFO_ENABLE
		#define STK3A8X_SW_AGC_AG_LOCK          false
	#else
		#define STK3A8X_SW_AGC_AG_LOCK          true
	#endif
#endif

#define STK_ALSPS_TIMER_MS                      90

//#define DEVICE_NAME                     "stk_alps"
#define DEVICE_NAME                     "STK3A8X"
//#define ALS_NAME                        "lightsensor-level"
#define ALS_NAME						"als_rear"
#define STK3A8X_CALI_FILE               "/persist/sensors/stkalpscali.conf"
#define STK3A8X_PID_LIST_NUM            4
#define STK_CHANNEL_NUMBER              3
#define STK_CHANNEL_OFFSET              3
#define STK_CHANNEL_GOLDEN              10
#define STK3A8X_REG_READ(stk, reg)  ((stk)->bops->read((stk), reg))
#define STK3A8X_REG_WRITE(stk, reg, val)    ((stk)->bops->write((stk), reg, val))
#define STK3A8X_REG_WRITE_BLOCK(stk, reg, val, len)    ((stk)->bops->write_block((stk), reg, val, len))
#define STK3A8X_REG_READ_MODIFY_WRITE(stk, reg, val, mask)  ((stk)->bops->read_modify_write((stk), reg, val, mask))
#define STK3A8X_REG_BLOCK_READ(stk, reg, count, buf)    ((stk)->bops->read_block((stk), reg, count, buf))
#define STK_ABS(x)              ((x < 0)? (-x):(x))

#ifdef STK_CHECK_LIB
	#include "stk_otp_encrypt.h"
#endif
#ifdef STK_FFT_FLICKER
#define FFT_BLOCK_SIZE 128

// sec_fft
#define SEC_FIFO_SIZE       128
#define SEC_SAMPLING_TIME   781  /* usec */
#define SEC_SAMPLING_FREQ   1000000 / SEC_SAMPLING_TIME  /* Hz */

// calc_thd
#define FLICKER_THD_CLEAR       1800000LL    /* 1.8 * 1000 * 1000 */
#define FLICKER_THD_RATIO       3LL       /* 0.003 * 1000 */
#define FLICKER_GAIN_MAX        256LL    /* 256 */
#define FLICKER_THD_RATIO_AUTO  1000000LL    /* 1 * 1000 * 1000 */

#define Q_offset 14
#define FFT_PEAK_THRESHOLD 20000
typedef struct fft_array
{
	int16_t real;
	int16_t imag;
} fft_array;
#endif

/* platform data */
struct stk3a8x_platform_data
{
	uint32_t    als_scale;
	bool        als_is_dri;
	int         int_pin;
	uint32_t    int_flags;
};

#ifdef STK_ALS_MID_FIR
typedef struct
{
	uint16_t raw[STK_ALS_MID_FIR_LEN];
	uint32_t number;
	uint32_t index;
} stk3a8x_data_filter;
#endif

typedef enum
{
	STK3A8X_NONE        = 0x0,
	STK3A8X_ALS         = 0x1,
	STK3A8X_ALL         = 0x2,
} stk3a8x_sensor_type;

typedef enum
{
	STK3A8X_DATA_TIMER_ALPS,
#ifdef STK_FIFO_ENABLE
	STK3A8X_FIFO_RELEASE_TIMER,
#endif
} stk3a8x_timer_type;

typedef struct stk3a8x_register_table
{
	uint8_t address;
	uint8_t value;
	uint8_t mask_bit;
} stk3a8x_register_table;

#ifdef STK_FIFO_ENABLE
typedef enum
{
	STK3A8X_FIFO_PS,
	STK3A8X_FIFO_ALS_C1,
	STK3A8X_FIFO_ALS_C2,
	STK3A8X_FIFO_RGBC,
	STK3A8X_FIFO_ALS_C1_C2_PS,
	STK3A8X_FIFO_UNAVAILABLE
} stk3a8x_frame_type;
#endif

typedef struct stk3a8x_timer_info
{
	bool                        timer_is_active;
	bool                        timer_is_exist;
} stk3a8x_timer_info;

typedef struct stk3a8x_irq_info
{
	bool                        irq_is_active;
	bool                        irq_is_exist;
	bool                        fifo_irq_is_active;
	bool                        fifo_irq_is_exist;
} stk3a8x_irq_info;

#if (defined(STK_ALS_CALI) | defined(STK_PS_CALI))
typedef enum
{
	STK3A8X_CALI_IDLE,
	STK3A8X_CALI_RUNNING,
	STK3A8X_CALI_FAILED,
	STK3A8X_CALI_DONE
} stk3a8x_calibration_status;
#endif

typedef struct stk3a8x_cali_table
{
	uint32_t als_version;
	uint32_t als_scale;
	uint32_t als_bias;
} stk3a8x_cali_table;

typedef struct stk3a8x_cali_info
{
#ifdef STK_ALS_CALI
	stk3a8x_timer_info          timer_status;
	stk3a8x_calibration_status  cali_status;
#endif
	stk3a8x_cali_table          cali_para;
} stk3a8x_cali_info;

typedef struct sec_raw_data
{
	uint32_t                    wideband;
	uint32_t                    ir;
	uint32_t                    clear;
	uint32_t                    flicker;
	uint32_t                    wide_gain;
	uint32_t                    clear_gain;
} sec_raw_data;

typedef struct stk3a8x_als_info
{
	bool                        first_init;
	bool                        cali_enable;
	float                       sampling_rate_hz;
	uint64_t                    sampling_intvl;
	bool                        enable;
	uint8_t                     als_ctrl_reg;
	uint32_t                    scale;
	bool                        is_dri;
	bool                        is_data_ready;
	uint16_t                    als_it;
#ifdef STK_ALS_MID_FIR
	stk3a8x_data_filter         als_data_filter;
#endif
	sec_raw_data                raw_data;
	bool                        is_raw_update;
	uint8_t                     cali_failed_count;
	uint16_t                    last_raw_data[3];
	uint16_t                    als_cali_data;
	uint16_t                    last_lux;
#ifdef STK_SW_AGC
	uint16_t                    als_sat_thd_h;
	uint16_t                    als_sat_thd_l;
	uint16_t                    als_agc_sum;
	uint16_t                    als_cur_dgain;
	uint16_t                    als_cur_again;
	uint8_t                     als_agc_ratio;
	uint8_t                     als_agc_ratio_updated;
	uint8_t                     als_gain_level;
	uint8_t                     last_als_gain_level;
	uint8_t                     als_it2_reg;
	bool                        is_gain_changing;
	bool                        is_data_saturated;
#endif
} stk3a8x_als_info;

#ifdef STK_FIFO_ENABLE
typedef struct stk3a8x_fifo_info
{
	bool                        enable;
	bool                        is_dri;
	uint16_t                    *fifo_data0;
	uint16_t                    *fifo_data1;
	uint16_t                    target_frame_count;
	uint16_t                    read_frame;
	uint16_t                    read_max_data_byte;
	uint8_t                     latency_status;
	bool                        update_latency;
	stk3a8x_frame_type          data_type;
	bool                        sel_mode;
	uint8_t                     frame_byte;
	uint8_t                     frame_data;
	uint16_t                    last_frame_count;
	bool                        fifo_reading;
	uint8_t                     block_size;
#ifdef STK_FFT_FLICKER
	uint16_t                    fft_buf[FFT_BLOCK_SIZE];
	uint16_t                    last_flicker_freq;
#endif
} stk3a8x_fifo_info;
#endif

typedef struct stk3a8x_data
{
	struct      i2c_client *client;
	struct      device *dev;
	struct      device *sensor_dev;
	struct      stk3a8x_platform_data *pdata;
	const struct stk3a8x_bus_ops *bops;
#ifdef SUPPORT_SENSOR_CLASS
	struct sensors_classdev als_cdev;
#endif
	struct mutex            config_lock;
	struct mutex            io_lock;
	struct mutex            data_info_lock;
#if defined(CONFIG_AMS_ALS_COMPENSATION_FOR_AUTO_BRIGHTNESS)
	struct mutex            enable_lock;
#endif
	struct input_dev        *als_input_dev;
	ktime_t                 alps_poll_delay;
	struct work_struct      stk_alps_work;
	struct hrtimer          alps_timer;
	struct workqueue_struct *stk_alps_wq;
	int                     int_pin;
	int32_t                 irq;
	struct work_struct      stk_irq_work;
	struct workqueue_struct *stk_irq_wq;
	stk3a8x_irq_info        irq_status;
#ifdef STK_ALS_CALI
	struct hrtimer          cali_timer;
	struct workqueue_struct *stk_cali_wq;
	struct work_struct      stk_cali_work;
	ktime_t                 cali_poll_delay;
#endif
	stk3a8x_cali_info       cali_info;
	atomic_t                recv_reg;
	stk3a8x_als_info        als_info;
	stk3a8x_timer_info      alps_timer_info;
	bool                    first_init;
	bool                    saturation;
#ifdef STK_FIFO_ENABLE
	stk3a8x_fifo_info       fifo_info;
	// fifo control timer
	struct hrtimer          fifo_release_timer;
	struct workqueue_struct *stk_fifo_release_wq;
	struct work_struct      stk_fifo_release_work;
	ktime_t                 fifo_release_delay;
	stk3a8x_timer_info      fifo_release_timer_info;
#ifdef STK_FIFO_POLLING_TIMER
	struct hrtimer          fifo_timer;
	struct workqueue_struct *stk_fifo_wq;
	struct work_struct      stk_fifo_work;
	ktime_t                 fifo_poll_delay;
	stk3a8x_timer_info      fifo_timer_info;
#endif
#endif
#ifdef STK_CHECK_LIB
	stk_operation           check_operatiop;
#endif
	struct regulator*       vdd_regulator;
	struct regulator*       vbus_regulator;
	bool                    vdd_1p8_enable;
	bool                    vbus_1p8_enable;
	struct pinctrl          *als_pinctrl;
	struct pinctrl_state    *pins_sleep;
	struct pinctrl_state    *pins_active;
#if IS_ENABLED(CONFIG_SENSORS_FLICKER_SELF_TEST)
	bool                    eol_enabled;
	bool                    recover_state;
#endif
	int                     isTrimmed;
#if defined(CONFIG_AMS_ALS_COMPENSATION_FOR_AUTO_BRIGHTNESS)
	bool als_flag;
	bool flicker_flag;
#endif
} stk3a8x_data;

struct stk3a8x_bus_ops
{
	u16 bustype;
	int (*read)(struct stk3a8x_data *, unsigned char);
	int (*read_block)(struct stk3a8x_data *, unsigned char, int, void *);
	int (*write)(struct stk3a8x_data *, unsigned char, unsigned char);
	int (*write_block)(struct stk3a8x_data *, unsigned char, unsigned char *, unsigned char);
	int (*read_modify_write)(struct stk3a8x_data *, unsigned char, unsigned char, unsigned char);
};

extern int print_debug;
#define dbg_flicker(fmt, arg...)                    \
	do {                                            \
		if (print_debug) {                          \
			printk(KERN_DEFAULT "%s(%d):[DBG]" fmt, \
					__func__, __LINE__, ##arg);     \
		}                                           \
	} while(0)

#define info_flicker(fmt, arg...)       \
	do {                                \
		printk(KERN_DEFAULT "%s:" fmt,  \
				__func__, ##arg);       \
	} while(0)

#define err_flicker(fmt, arg...)                \
	do {                                        \
		printk(KERN_ERR "%s(%d):[ERR]" fmt,     \
				__func__, __LINE__, ##arg);     \
	} while(0)


int stk3a8x_probe(struct i2c_client *client,
		const struct stk3a8x_bus_ops *bops);
int stk3a8x_remove(struct i2c_client *client);
int stk3a8x_suspend(struct device *dev);
int stk3a8x_resume(struct device *dev);

int32_t stk3a8x_cali_als(struct stk3a8x_data *alps_data);
void stk3a8x_get_reg_default_setting(uint8_t reg, uint16_t* value);
int32_t stk3a8x_update_registry(struct stk3a8x_data *alps_data);


int sensors_register(struct device **dev, void *drvdata,
	struct device_attribute *attributes[], char *name);
void sensors_unregister(struct device * const dev,
	struct device_attribute *attributes[]);

#define vfs_read(a, b, c, d)
#define vfs_write(a, b, c, d)

#ifdef STK_FIFO_ENABLE
	void stk3a8x_free_fifo_data(struct stk3a8x_data *alps_data);
	void stk3a8x_alloc_fifo_data(struct stk3a8x_data *alps_data, uint32_t size);
	void stk3a8x_fifo_init(struct stk3a8x_data *alps_data);
	int32_t stk3a8x_enable_fifo(struct stk3a8x_data *alps_data, bool en);
	void stk3a8x_fifo_get_data(struct stk3a8x_data *alps_data, uint16_t frame_num);
	void stk3a8x_avg_data(struct stk3a8x_data *alps_data);
	void stk3a8x_get_fifo_data_polling(struct stk3a8x_data *alps_data);
	void stk3a8x_get_fifo_data_dri(struct stk3a8x_data *alps_data);
#ifdef STK_FFT_FLICKER
	void SEC_fft_entry(struct stk3a8x_data *alps_data);
#endif
#endif
#ifdef STK_SW_AGC
	void stk3a8x_als_agc_check(struct stk3a8x_data *alps_data);
#endif
#endif // __STK3A8X_H__
