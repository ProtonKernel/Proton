#ifndef SENSORS_H
#define SENSORS_H
#include "../ssp.h"

#define ITEM_MAX 10

#define	PR_ABS_MAX	8388607		/* 24 bit 2'compl */
#define	PR_ABS_MIN	-8388608

struct sensor_manager {
    char key[ITEM_MAX][256];
    void* item[ITEM_MAX];
    bool found;
    int size;
    int index;
};

void push_back(struct sensor_manager *sm, const char *key, void* item);
void* get_sensor_ptr(struct ssp_data *data, struct sensor_manager *sm, int type);

ssize_t sensor_default_show(struct device *dev, char *buf);
ssize_t sensor_default_store(struct device *dev, const char *buf, size_t size);
void sensor_default_initialize(struct ssp_data *);
int sensor_default_get_data(void);
int sensor_default_check_func(struct ssp_data *, int);

typedef struct accelerometer_t {
    const char *name;
    const char *vendor;
	ssize_t (*get_accel_calibration)(struct device *, char *);
	ssize_t (*set_accel_calibration)(struct device *, const char *, size_t);
	ssize_t (*get_accel_reactive_alert)(struct device *, char *);
	ssize_t (*set_accel_reactive_alert)(struct device *, const char *, size_t);
	ssize_t (*get_accel_selftest)(struct device *, char *);
	ssize_t (*set_accel_lowpassfilter)(struct device *, const char *, size_t);
} accel;

struct accel_bigdata_info {
	u16 version;
	u16 elapsed_time;
	u8 updated_index;
	u16 accuracy;
	s16 bias[3];	//x, y, z
	u16 cal_norm;
	u16 uncal_norm;
} __attribute__((__packed__));

#ifdef CONFIG_SENSORS_ICM42632M
struct accelerometer_t* get_accel_icm42632m(void);
#endif
#ifdef CONFIG_SENSORS_LSM6DSO
struct accelerometer_t* get_accel_lsm6dso(void);
#endif

int accel_open_calibration(struct ssp_data *data);
int set_accel_cal(struct ssp_data *data);
int enable_accel_for_cal(struct ssp_data *data);
void disable_accel_for_cal(struct ssp_data *data, int iDelayChanged);
int accel_do_calibrate(struct ssp_data *data, int iEnable, const int max_accel_1g);

typedef struct gyroscope_t {
    const char *name;
    const char *vendor;
	ssize_t (*get_gyro_power_off)(struct device *, char *);
	ssize_t (*get_gyro_power_on)(struct device *, char *);
	ssize_t (*get_gyro_temperature)(struct device *, char *);
	ssize_t (*get_gyro_selftest)(struct device *, char *);
	ssize_t (*get_gyro_selftest_dps)(struct device *, char *);
	ssize_t (*set_gyro_selftest_dps)(struct device *, const char *, size_t);
} gyro;

struct gyro_bigdata_info {
	u16 version;
	u16 updated_index;
	s32 bias[3];	//x, y, z
	s8 temperature;
} __attribute__((__packed__));

#ifdef CONFIG_SENSORS_ICM42632M
struct gyroscope_t* get_gyro_icm42632m(void);
#endif

#ifdef CONFIG_SENSORS_LSM6DSO
struct gyroscope_t* get_gyro_lsm6dso(void);
#endif

int gyro_open_calibration(struct ssp_data *data);
int save_gyro_caldata(struct ssp_data *data, s32 *iCalData);
int set_gyro_cal(struct ssp_data *data);
short get_gyro_temperature(struct ssp_data *data);

typedef struct magnetometer_t {
    const char *name;
    const char *vendor;
    void (*initialize)(struct ssp_data *);
    int (*check_data_spec)(struct ssp_data *, int);
	ssize_t (*get_magnetic_asa)(struct device *, char *);
	ssize_t (*get_magnetic_matrix)(struct device *, char *);
	ssize_t (*set_magnetic_matrix)(struct device *, const char *, size_t);
	ssize_t (*get_magnetic_selftest)(struct device *, char *);
    ssize_t (*get_magnetic_si_param)(struct device *, char *);
} mag;

#ifdef CONFIG_SENSORS_AK09918C
struct magnetometer_t* get_mag_ak09918c(void);
#endif

typedef struct light_t {
    const char *name;
    const char *vendor;
	ssize_t (*get_light_circle)(struct device *, char *);
} light;

#ifdef CONFIG_SENSORS_TMD4907
struct light_t* get_light_tmd4907(void);
#endif

#ifdef CONFIG_SENSORS_TMD4912
struct light_t* get_light_tmd4912(void);
#endif

#ifdef CONFIG_SENSORS_STK33910
struct light_t* get_light_stk33910(void);
#endif

typedef struct proximity_t {
    const char *name;
    const char *vendor;
    ssize_t (*get_prox_position)(struct device *, char *);
} prox;

#ifdef CONFIG_SENSORS_TMD4907
struct proximity_t* get_prox_tmd4907(void);
#endif

#ifdef CONFIG_SENSORS_TMD4912
struct proximity_t* get_prox_tmd4912(void);
#endif

#ifdef CONFIG_SENSORS_STK33910
struct proximity_t* get_prox_stk33910(void);
#endif

typedef struct barometer_t {
	const char *name;
	const char *vendor;
	ssize_t (*get_baro_calibration)(struct device *, char *);
	ssize_t (*set_baro_calibration)(struct device *, const char *, size_t);
	ssize_t (*get_baro_temperature)(struct device *, char *);
} baro;

#ifdef CONFIG_SENSORS_LPS25H
struct barometer_t* get_baro_lps25hhtr(void);
#endif

#ifdef CONFIG_SENSORS_LPS22H
struct barometer_t* get_baro_lps22hhtr(void);
#endif
#endif