#ifndef IS_VENDOR_CONFIG_RSU_V09_H
#define IS_VENDOR_CONFIG_RSU_V09_H

#define VENDER_PATH
#define USE_CAMERA_SENSOR_RETENTION
#define CAMERA_REAR_DUAL_CAL
#define CAMERA_REAR2
#define CAMERA_REAR2_AF /* related to OIS */
#define CAMERA_REAR2_TILT
#define CAMERA_REAR2_MODULEID
#define CAMERA_REAR3
#define CAMERA_REAR3_AFCAL
#define CAMERA_REAR3_TILT
#define CAMERA_REAR3_MODULEID
#define CAMERA_REAR_PAFCAL

#define USE_CAMERA_2LD_4000X3000
#define USE_HI1336C_SETFILE

#define CAMERA_STANDARD_CAL_ISP_VERSION 'E'

#define IS_MAX_FW_BUFFER_SIZE (4100 * 1024)
#define IS_MAX_CAL_SIZE (64 * 1024)

/* Max width: 4000, Max height: 3000 */
/* 108192128 bytes */
#define IS_MAX_TNRISP_SIZE (0x0672E180)

#define CONFIG_SEC_CAL_ENABLE
#ifdef CONFIG_SEC_CAL_ENABLE
#define USES_STANDARD_CAL_RELOAD
#endif
#define IS_VENDOR_SENSOR_COUNT 4	/* FRONT_0, REAR_0, REAR_1, REAR_2*/
#define IS_FRONT_MAX_CAL_SIZE (64 * 1024)
#define IS_REAR2_MAX_CAL_SIZE (64 * 1024)

#define CAMERA_OIS_GYRO_OFFSET_SPEC 10000

#define CAMERA_REAR2_SENSOR_SHIFT_CROP

#define CAMERA_2ND_OIS

//#define USE_CAMERA_EMBEDDED_HEADER // TEMP_OLYMPUS

#define USE_CAMERA_ADAPTIVE_MIPI
#ifdef USE_CAMERA_ADAPTIVE_MIPI
/*#define USE_CAMERA_ADAPTIVE_MIPI_RUNTIME*/
#endif

/* #define USE_CAMERA_CHECK_SENSOR_REV */

#define USE_CAMERA_HW_BIG_DATA
#ifdef USE_CAMERA_HW_BIG_DATA
/* #define CAMERA_HW_BIG_DATA_FILE_IO */
/* #define CSI_SCENARIO_COMP		(0) This value follows dtsi */
#define CSI_SCENARIO_SEN_FRONT	(1)
#define CSI_SCENARIO_TELE		(2)
#define CSI_SCENARIO_SECURE		(3)
#define CSI_SCENARIO_SEN_REAR	(0)
#endif

#define USE_AF_SLEEP_MODE

/* It should be align with DDK and RTA side */
#define USE_NEW_PER_FRAME_CONTROL

/* define supported aperture level */
// #define ROM_SUPPORT_APERTURE_F2	// Second step of aperture.

/* Tele sensor crop shift and OIS calibration will be applied instead of this feature in this project */
/* #define OIS_CENTERING_SHIFT_ENABLE */
#undef ENABLE_DYNAMIC_MEM

#define USE_SENSOR_LONG_EXPOSURE_SHOT
#define OIS_DUAL_CAL_DEFAULT_VALUE_WIDE 0
#define OIS_DUAL_CAL_DEFAULT_VALUE_TELE 0
#define WIDE_OIS_ROM_ID ROM_ID_REAR
#define TELE_OIS_ROM_ID ROM_ID_REAR3
#define TELE_OIS_TILT_ROM_ID TELE_OIS_ROM_ID

#define USE_OIS_HALL_DATA_FOR_VDIS

#define CONFIG_SECURE_CAMERA_USE 1

#define USE_CAMERA_IOVM_BEST_FIT

#define CAMERA_CSI_A_PHY_CFG (0x8061) /* R9S Front */
#define CAMERA_CSI_C_PHY_CFG (0x8062) /* R9S Wide */

#define CONFIG_LEDS_KTD2692

/* This for dualization between eeprom sensor and otprom sensor */
#define USE_CAMERA_DUALIZED
#ifdef USE_CAMERA_DUALIZED
/* This is UW camera dualized sensor ID*/
#define CAMERA_UWIDE_DUALIZED SENSOR_NAME_HI1336
#endif

#define PDP_FAST_VVALID_THRESHOLD_TIME 1000000 /* 1s */

#endif /* IS_VENDOR_CONFIG_RSU_V09_H */
