/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos IS Metadata structure
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_METADATA_VENDOR_H_
#define IS_METADATA_VENDOR_H_

#include "is-config.h"
#include "is-framemgr.h"
#include "is-common-enum.h"

#ifndef _LINUX_TYPES_H
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
#endif

struct rational {
	int32_t num;
	int32_t den;
};

#define CAMERA2_MAX_AVAILABLE_MODE		21
#define CAMERA2_MAX_FACES			16
#define CAMERA2_MAX_VENDER_LENGTH		400
#define CAMERA2_AWB_VENDER_LENGTH		415
#define CAMERA2_MAX_IPC_VENDER_LENGTH		3130
#define CAMERA2_MAX_PDAF_MULTIROI_COLUMN	13
#define CAMERA2_MAX_PDAF_MULTIROI_ROW		9
#define CAMERA2_MAX_UCTL_VENDER_LENGTH		32

#define CAMERA2_MAX_UCTL_VENDOR2_LENGTH		400
#define CAMERA2_MAX_UDM_VENDOR2_LENGTH		32

#define CAMERA2_MAX_STRIPE_REGION_NUM		5

#define EV_LIST_SIZE						24
#define MANUAL_LIST_SIZE                                        5

#define OPEN_MAGIC_NUMBER			0x20202102
#define SHOT_MAGIC_NUMBER			0x56789234

#define CHK_MODECHANGE_SCN(captureIntent)      \
	(((captureIntent == AA_CAPTURE_INTENT_STILL_CAPTURE_REMOSAIC_SINGLE) \
	|| (captureIntent == AA_CAPTURE_INTENT_STILL_CAPTURE_REMOSAIC_SINGLE_FLASH) \
	|| (captureIntent == AA_CAPTURE_INTENT_STILL_CAPTURE_REMOSAIC_DYNAMIC_SHOT) \
	|| (captureIntent == AA_CAPTURE_INTENT_STILL_CAPTURE_REMOSAIC_MFHDR_DYNAMIC_SHOT)       \
	|| (captureIntent == AA_CAPTURE_INTENT_STILL_CAPTURE_REMOSAIC_EXPOSURE_DYNAMIC_SHOT)) ? 1 : 0)

enum is_subscenario_id {
	ISS_SUB_SCENARIO_STILL_PREVIEW = 0,                                         /* 0: Single preview (HDR Auto/Off) */
	ISS_SUB_SCENARIO_STILL_PREVIEW_WDR_ON = 1,                                  /* 1: Single preview (HDR On) */
	ISS_SUB_SCENARIO_STILL_FULL_PREVIEW_WDR_AUTO = 2,                           /* TODO: 2: Full resolution Preview (HDR Auto/Off) */
	ISS_SUB_SCENARIO_SUPER_NIGHT_SHOT_PREVIEW = 3,                              /* Super night Preview (HDR Auto) */
	ISS_SUB_SCENARIO_STILL_PREVIEW_GALAXY_RAW = 4,                              /* 4: galaxy raw preview */

	ISS_SUB_SCENARIO_STILL_CAPTURE_WDR_AUTO = 10,                               /* 10: Single/Burst/Snapshot capture (HDR Auto/Off) */
	ISS_SUB_SCENARIO_STILL_CAPTURE = 11,                                        /* 11: Single capture (HDR Off) : Pro mode	 */
	ISS_SUB_SCENARIO_STILL_CAPTURE_LONG = 12,                                   /* 12: Single long capture (HDR Off) : Pro mode	 */
	ISS_SUB_SCENARIO_STILL_CAPTURE_DNG_REPROCESSING = 13,                       /* TODO: 13: DNG capture (HDR Off) : Pro m */
	ISS_SUB_SCENARIO_FULL_CAPTURE = 14,                                         /* TODO: 14: Full resolution Capture (HDR Auto/Off) */
	ISS_SUB_SCENARIO_MERGED_STILL_CAPTURE_MFNR = 15,                            /* TODO: 15: MFNR capture (HDR Auto/Off) */
	ISS_SUB_SCENARIO_MERGED_STILL_CAPTURE_MFHDR = 16,                           /* 16: MFHDR capture (HDR Auto/Off) */
	ISS_SUB_SCENARIO_SUPER_NIGHT_SHOT = 17,                                     /* 17: Super night capture (HDR Auto) */
	ISS_SUB_SCENARIO_STILL_CAPTURE_LLS = 18,                                    /* 18: Flash LLS capture (HDR Auto/Off) */
	ISS_SUB_SCENARIO_STILL_FLASH_LLS = ISS_SUB_SCENARIO_STILL_CAPTURE_LLS,
	ISS_SUB_SCENARIO_STILL_CAPTURE_ZOOM = 19,                                   /* 19: SR Zoom capture ( HDR Auto/Off ) */
	ISS_SUB_SCENARIO_FULL_RESOLUTION_LOW_LIGHT_CAPTURE = 20,                    /* 20: Full resolution Low Light Capture  (HDR Auto/Off) */
	ISS_SUB_SCENARIO_SUPER_NIGHT_CAPTURE_LONG_EXPOSURE = 21,                    /* 21: Super night capture Long Exposure (HDR Auto) */
	ISS_SUB_SCENARIO_MERGED_STILL_CAPTURE_SHORT_REF_LLHDR = 22,                 /* 22: Short Ref LLHDR capture (HDR Auto/Off) */
	ISS_SUB_SCENARIO_MERGED_STILL_REMOSAIC_CAPTURE_MFNR = 23,                   /* 23: Remosaic TNR capture (HDR Auto/Off) */
	ISS_SUB_SCENARIO_MERGED_STILL_FUSION_CAPTURE = 24,                          /* 24: fusion capture (HDR Auto/Off) */
	ISS_SUB_SCENARIO_MERGED_STILL_CAPTURE_GFHDR_SHORT = 25,                     /* 25: GFHDR Short capture (HDR Auto/Off) */
	ISS_SUB_SCENARIO_MERGED_STILL_CAPTURE_GFHDR_LONG = 26,                      /* 26: GFHDR Long capture (HDR Auto/Off) */
	ISS_SUB_SCENARIO_MERGED_STILL_CAPTURE_GALAXY_RAW = 27,                      /* 27: Galaxy Raw still capture */

	ISS_SUB_SCENARIO_VIDEO = 30,                                                /* 30: FHD 30fps (HDR Off) */
	ISS_SUB_SCENARIO_VIDEO_WDR_AUTO = 31,                                       /* 31: FHD 30fps (HDR Auto) */
	ISS_SUB_SCENARIO_VIDEO_WDR_ON = 32,                                         /* 32: FHD 30fps (HDR On) */
	ISS_SUB_SCENARIO_VIDEO_SUPER_STEADY = 33,                                   /* 33: Super Steady : FHD 30fps ( HDR Off ) */
	ISS_SUB_SCENARIO_VIDEO_SUPER_STEADY_WDR_AUTO = 34,                          /* 34: Super Steady : FHD 30fps ( HDR Auto ) */
	ISS_SUB_SCENARIO_VIDEO_SUPER_STEADY_WDR_ON = 35,                            /* 35: Super Steady : FHD 30fps ( HDR On ) */
	ISS_SUB_SCENARIO_FHD_60FPS = 36,                                            /* 36: FHD 60fps (HDR Off) */
	ISS_SUB_SCENARIO_FHD_60FPS_WDR_AUTO = 37,                                   /* 37: FHD 60fps (HDR Auto) */
	ISS_SUB_SCENARIO_FHD_60FPS_WDR_ON = 38,                                     /* 38: FHD 60fps (HDR On) */
	ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED = 39,                                     /* 39: FHD 120fps, HD 240fps, HD 960fps (HDR Off) */
	ISS_SUB_SCENARIO_UHD_30FPS = 40,                                            /* 40: UHD 30fps (HDR Off) */
	ISS_SUB_SCENARIO_UHD_30FPS_WDR_AUTO = 41,                                   /* 41: UHD 30fps (HDR Auto) */
	ISS_SUB_SCENARIO_UHD_30FPS_WDR_ON = 42,                                     /* 42: UHD 30fps (HDR On) */
	ISS_SUB_SCENARIO_UHD_60FPS = 43,                                            /* 43: UHD 60fps (HDR Off) */
	ISS_SUB_SCENARIO_UHD_60FPS_WDR_AUTO = 44,                                   /* 44: UHD 60fps (HDR Auto) */
	ISS_SUB_SCENARIO_UHD_60FPS_WDR_ON = 45,                                     /* 45: UHD 60fps (HDR On)0:fps */
	ISS_SUB_SCENARIO_FHD_VIDEO_HDR10_WDR_AUTO = 46,                             /* 46: HDR10+ video (HDR Auto) : FHD 30fps	 */
	ISS_SUB_SCENARIO_UHD_VIDEO_HDR10_WDR_AUTO = 47,                             /* 47: HDR10+ video (HDR Auto) : UHD 30fps */
	ISS_SUB_SCENARIO_8K_WDR_AUTO = 48,                                          /* TODO: 48: 8K Video 30fps (HDR Auto) */
	ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED_960FPS = 49,
	ISS_SUB_SCENARIO_VIDEO_MERGED_HDR_AUTO = 50,                                /* 50: FHD 30fps (Video HDR Auto - AEB mode) */
	ISS_SUB_SCENARIO_FHD_VIDEO_HDR10_WDR_AUTO_60FPS = 51,                       /* 51: FHD 60fps (Video HDR 10) */
	ISS_SUB_SCENARIO_UHD_VIDEO_HDR10_WDR_AUTO_60FPS = 52,                       /* 52: UHD 60fps (Video HDR 10) */
	ISS_SUB_SCENARIO_UHD_120FPS_SLOW_MOTION = 53,                               /* 53: UHD 120fps (Video slow motion) */
	ISS_SUB_SCENARIO_VIDEO_FHD_AUTO_FPS = 54,                                   /* 54: FHD auto fps */
	ISS_SUB_SCENARIO_MULTIVIEW_RECORDING = 55,                                  /* 55: MultiView recording */

	ISS_SUB_SCENARIO_LIVE_OUTFOCUS_PREVIEW = 60,                                /* 60: Bokeh preview (HDR Off) */
	ISS_SUB_SCENARIO_LIVE_OUTFOCUS_PREVIEW_WDR_AUTO = 61,                       /* 61: Bokeh preview (HDR Auto) */
	ISS_SUB_SCENARIO_LIVE_OUTFOCUS_PREVIEW_WDR_ON = 62,                         /* 62: Bokeh preview (HDR On) */
	ISS_SUB_SCENARIO_LIVE_OUTFOCUS_CAPTURE = 63,                                /* 63: Bokeh capture (HDR Off) */
	ISS_SUB_SCENARIO_LIVE_OUTFOCUS_CAPTURE_WDR_AUTO = 64,                       /* 64: Bokeh capture (HDR Auto) */
	ISS_SUB_SCENARIO_LIVE_OUTFOCUS_CAPTURE_WDR_ON = 65,                         /* 65: Bokeh capture (HDR On) */
	ISS_SUB_SCENARIO_VT = 66,                                                   /* TODO: 66: VT call (HDR Off) */

	ISS_SUB_SCENARIO_STILL_PREVIEW_3RD_PARTY_WDR_AUTO = 67,                     /* 67: 3rd Party Preview (HDR Auto) */
	ISS_SUB_SCENARIO_VIDEO_3RD_PARTY_WDR_AUTO = 68,                             /* 68: 3rd Party Video (HDR Auto) */
	ISS_SUB_SCENARIO_STILL_CAPTURE_3RD_PARTY_WDR_AUTO = 69,                     /* 69: 3rd Party Capture (HDR Auto) */
	ISS_SUB_SCENARIO_FRONT_SMART_STAY = 70,                                     /* 70: Smart Stay */
	ISS_SUB_SCENARIO_FRONT_3RD_PARY_VT = 71,                                    /* 3rd Party VT call (HDR Auto) */
	ISS_SUB_SCENARIO_VIDEO_PRO_HIGH_SPEED = 72,                                 /* 72 : Pro video 120fps */

	ISS_SUB_SCENARIO_VIDEO_ISPLP_TNR = 110,                                     /* TODO: 110: ISPLT_TNR */
	ISS_SUB_END,
};

#define IS_VIDEO_SCENARIO(setfile)					\
	(((setfile) == ISS_SUB_SCENARIO_VIDEO)				\
	|| ((setfile) == ISS_SUB_SCENARIO_VIDEO_WDR_AUTO)		\
	|| ((setfile) == ISS_SUB_SCENARIO_VIDEO_WDR_ON)			\
	|| ((setfile) == ISS_SUB_SCENARIO_FHD_60FPS)			\
	|| ((setfile) == ISS_SUB_SCENARIO_FHD_60FPS_WDR_AUTO)		\
	|| ((setfile) == ISS_SUB_SCENARIO_UHD_30FPS)			\
	|| ((setfile) == ISS_SUB_SCENARIO_UHD_30FPS_WDR_AUTO)		\
	|| ((setfile) == ISS_SUB_SCENARIO_UHD_30FPS_WDR_ON)		\
	|| ((setfile) == ISS_SUB_SCENARIO_UHD_60FPS)			\
	|| ((setfile) == ISS_SUB_SCENARIO_UHD_60FPS_WDR_AUTO)		\
	|| ((setfile) == ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED)		\
	|| ((setfile) == ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED_960FPS)	\
	|| ((setfile) == ISS_SUB_SCENARIO_VIDEO_MERGED_HDR_AUTO))

#define IS_VIDEO_HDR_SCENARIO(setfile)				\
	(((setfile) == ISS_SUB_SCENARIO_VIDEO_WDR_AUTO)		\
	|| ((setfile) == ISS_SUB_SCENARIO_VIDEO_WDR_ON)		\
	|| ((setfile) == ISS_SUB_SCENARIO_VIDEO_MERGED_HDR_AUTO))

enum is_scenario_is {
	IS_SCENARIO_SWVDIS = 1,
	IS_SCENARIO_COLOR_IRIS = 2,
	IS_SCENARIO_AUTO_DUAL = 3,
	IS_SCENARIO_FULL_SIZE = 4,
	IS_SCENARIO_HIGH_SPEED_DUALFPS = 5, /* FPS is changed from normal to high speed */
	IS_SCENARIO_SECURE = 6,
	IS_SCENAIRO_REMOSAIC = 7,
	IS_SCENAIRO_FACTORY_RAW = 8,
	IS_SCENARIO_PRO_VIDEO = 9,
};

#define IS_DVFS_SCENARIO_COMMON_MODE_SHIFT 0
#define IS_DVFS_SCENARIO_COMMON_MODE_MASK 0x3
#define IS_DVFS_SCENARIO_VENDOR_SHIFT 16
#define IS_DVFS_SCENARIO_VENDOR_MASK 0xffff

enum is_dvfs_scenario_common_mode {
	IS_DVFS_SCENARIO_COMMON_MODE_PHOTO = 1,
	IS_DVFS_SCENARIO_COMMON_MODE_VIDEO = 2,
};

enum is_dvfs_scenario_vendor {
	IS_DVFS_SCENARIO_VENDOR_VT = 1,
	IS_DVFS_SCENARIO_VENDOR_SSM = 2,
	IS_DVFS_SCENARIO_VENDOR_VIDEO_HDR = 3,
	IS_DVFS_SCENARIO_VENDOR_SUPER_STEADY = 4,
	IS_DVFS_SCENARIO_VENDOR_LOW_POWER = 5,
};

#define IS_DVFS_SCENARIO_WIDTH_SHIFT        0
#define IS_DVFS_SCENARIO_HEIGHT_SHIFT       16

/*
 *controls/dynamic metadata
 */

/* android.request */

enum metadata_mode {
	METADATA_MODE_NONE,
	METADATA_MODE_FULL
};

enum available_capabilities {
	REQUEST_AVAILABLE_CAPABILITIES_BACKWARD_COMPATIBLE = 0,
	REQUEST_AVAILABLE_CAPABILITIES_MANUAL_SENSOR,
	REQUEST_AVAILABLE_CAPABILITIES_MANUAL_POST_PROCESSING,
	REQUEST_AVAILABLE_CAPABILITIES_RAW,
	REQUEST_AVAILABLE_CAPABILITIES_PRIVATE_REPROCESSING,
	REQUEST_AVAILABLE_CAPABILITIES_READ_SENSOR_SETTINGS,
	REQUEST_AVAILABLE_CAPABILITIES_BURST_CAPTURE,
	REQUEST_AVAILABLE_CAPABILITIES_YUV_REPROCESSING,
	REQUEST_AVAILABLE_CAPABILITIES_DEPTH_OUTPUT,
	REQUEST_AVAILABLE_CAPABILITIES_CONSTRAINED_HIGH_SPEED_VIDEO,
};

struct camera2_request_ctl {
	uint32_t		frameCount;
	uint32_t		id;
	enum metadata_mode	metadataMode;
};

struct camera2_request_dm {
	uint32_t		frameCount;
	uint32_t		id;
	uint8_t			pipelineDepth;
};

struct camera2_entry_ctl {
	/** \brief
	  per-frame control for entry control
	  \remarks
	  low parameter is 0bit ~ 31bit flag
	  high parameter is 32bit ~ 63bit flag
	 */
	uint32_t		lowIndexParam;
	uint32_t		highIndexParam;
	uint32_t		parameter[2048];
};

struct camera2_entry_dm {
	uint32_t		lowIndexParam;
	uint32_t		highIndexParam;
};

/* android.lens */

enum optical_stabilization_mode {
	OPTICAL_STABILIZATION_MODE_OFF = 0,
	OPTICAL_STABILIZATION_MODE_ON = 1,

	/* vendor feature */
	OPTICAL_STABILIZATION_MODE_STILL = 100, // Still mode
	OPTICAL_STABILIZATION_MODE_STILL_ZOOM,  // Still Zoom mode
	OPTICAL_STABILIZATION_MODE_VIDEO,       // Recording mode
	OPTICAL_STABILIZATION_MODE_SINE_X,      // factory mode x
	OPTICAL_STABILIZATION_MODE_SINE_Y,      // factory mode y
	OPTICAL_STABILIZATION_MODE_CENTERING,   // Centering mode
	OPTICAL_STABILIZATION_MODE_HOLD,        // hold mode
	OPTICAL_STABILIZATION_MODE_VDIS,        // VDIS mode
	OPTICAL_STABILIZATION_MODE_VIDEO_RATIO_4_3, // Recording mode(VGA)
	OPTICAL_STABILIZATION_MODE_VDIS_ASR, // VDIS mode with ASR applied
};

enum lens_state {
	LENS_STATE_STATIONARY = 0,
	LENS_STATE_MOVING,
};

enum lens_focus_distance_calibration {
	LENS_INFO_FOCUS_DISTANCE_CALIBRATION_UNCALIBRATED = 0,
	LENS_INFO_FOCUS_DISTANCE_CALIBRATION_APPROXIMATE,
	LENS_INFO_FOCUS_DISTANCE_CALIBRATION_CALIBRATED,
};

enum lens_facing {
	LENS_FACING_BACK,
	LENS_FACING_FRONT,
	LENS_FACING_EXTERNAL,
};

struct camera2_lens_ctl {
	int32_t					aperture;
	float					filterDensity;
	float					focalLength;
	float					focusDistance;
	enum optical_stabilization_mode		opticalStabilizationMode;
};

struct camera2_lens_dm {
	int32_t					aperture;
	float					focusDistance;
	float					focusRange[2];
	enum lens_state				state;
};

/* android.sensor */

enum sensor_test_pattern_mode {
	SENSOR_TEST_PATTERN_MODE_OFF = 1,
	SENSOR_TEST_PATTERN_MODE_SOLID_COLOR,
	SENSOR_TEST_PATTERN_MODE_COLOR_BARS,
	SENSOR_TEST_PATTERN_MODE_COLOR_BARS_FADE_TO_GRAY,
	SENSOR_TEST_PATTERN_MODE_PN9,
	SENSOR_TEST_PATTERN_MODE_BLACK,
	SENSOR_TEST_PATTERN_MODE_CUSTOM1 = 257,
};

enum sensor_colorfilterarrangement {
	SENSOR_COLORFILTERARRANGEMENT_RGGB,
	SENSOR_COLORFILTERARRANGEMENT_GRBG,
	SENSOR_COLORFILTERARRANGEMENT_GBRG,
	SENSOR_COLORFILTERARRANGEMENT_BGGR,
	SENSOR_COLORFILTERARRANGEMENT_RGB
};

enum sensor_timestamp_calibration {
	SENSOR_INFO_TIMESTAMP_CALIBRATION_UNCALIBRATED = 0,
	SENSOR_INFO_TIMESTAMP_CALIBRATION_CALIBRATED,
};

enum sensor_lensshading_applied {
	SENSOR_INFO_LENS_SHADING_APPLIED_FALSE = 0,
	SENSOR_INFO_LENS_SHADING_APPLIED_TRUE,
};

enum sensor_ref_illuminant {
	SENSOR_ILLUMINANT_DAYLIGHT = 1,
	SENSOR_ILLUMINANT_FLUORESCENT = 2,
	SENSOR_ILLUMINANT_TUNGSTEN = 3,
	SENSOR_ILLUMINANT_FLASH = 4,
	SENSOR_ILLUMINANT_FINE_WEATHER = 9,
	SENSOR_ILLUMINANT_CLOUDY_WEATHER = 10,
	SENSOR_ILLUMINANT_SHADE = 11,
	SENSOR_ILLUMINANT_DAYLIGHT_FLUORESCENT = 12,
	SENSOR_ILLUMINANT_DAY_WHITE_FLUORESCENT = 13,
	SENSOR_ILLUMINANT_COOL_WHITE_FLUORESCENT = 14,
	SENSOR_ILLUMINANT_WHITE_FLUORESCENT = 15,
	SENSOR_ILLUMINANT_STANDARD_A = 17,
	SENSOR_ILLUMINANT_STANDARD_B = 18,
	SENSOR_ILLUMINANT_STANDARD_C = 19,
	SENSOR_ILLUMINANT_D55 = 20,
	SENSOR_ILLUMINANT_D65 = 21,
	SENSOR_ILLUMINANT_D75 = 22,
	SENSOR_ILLUMINANT_D50 = 23,
	SENSOR_ILLUMINANT_ISO_STUDIO_TUNGSTEN = 24
};

struct camera2_sensor_ctl {
	uint64_t			exposureTime;
	uint64_t			frameDuration;
	uint32_t			sensitivity;
	int32_t				testPatternData[4];
	enum sensor_test_pattern_mode	testPatternMode;
	enum sensor_colorfilterarrangement sensorColorFilter;
};

struct camera2_sensor_dm {
	uint64_t			exposureTime;
	uint64_t			frameDuration;
	uint32_t			sensitivity;
	uint64_t			timeStamp;
	float				temperature;
	struct rational			neutralColorPoint[3];
	double				noiseProfile[4][2];
	float				profileHueSatMap[2][2][2][3];
	float				profileToneCurve[32][2];
	float				greenSplit;
	uint64_t			rollingShutterSkew;
};

/* android.flash */

enum flash_mode {
	CAM2_FLASH_MODE_NONE = 0,
	CAM2_FLASH_MODE_OFF = 1,
	CAM2_FLASH_MODE_SINGLE,
	CAM2_FLASH_MODE_TORCH,

	/* vendor feature */
	CAM2_FLASH_MODE_BEST = 100,
	CAM2_FLASH_MODE_LCD = 101,
};

enum flash_state {
	FLASH_STATE_UNAVAILABLE = 0,
	FLASH_STATE_CHARGING,
	FLASH_STATE_READY,
	FLASH_STATE_FIRED,
	FLASH_STATE_PARTIAL,
};

// TODO: [API32] not used
enum capture_state {
	CAPTURE_STATE_NONE = 0,
	CAPTURE_STATE_FLASH = 1,
	CAPTURE_STATE_HDR_DARK = 12,
	CAPTURE_STATE_HDR_NORMAL = 13,
	CAPTURE_STATE_HDR_BRIGHT = 14,
	CAPTURE_STATE_ZSL_LIKE = 20,
	CAPTURE_STATE_STREAM_ON_CAPTURE = 30,
	CAPTURE_STATE_RAW_CAPTURE = 100,
	CAPTURE_STATE_VIDEO_RECORD_CHANGE_FPS = 200,
};

enum flash_info_available {
	FLASH_INFO_AVAILABLE_FALSE = 0,
	FLASH_INFO_AVAILABLE_TRUE,
};

struct camera2_flash_ctl {
	uint32_t		firingPower;
	uint64_t		firingTime;
	enum flash_mode		flashMode;
};

struct camera2_flash_dm {
	uint32_t		firingPower;
	uint64_t		firingTime;
	enum flash_mode		flashMode;
	enum flash_state	flashState;

	/* vendor feature */
	uint32_t		vendor_firingStable;
	uint32_t		vendor_decision;
	uint32_t		vendor_flashReady;
	uint32_t		vendor_flashOffReady;
};

/* android.hotpixel */

enum processing_mode {
	PROCESSING_MODE_OFF = 1,
	PROCESSING_MODE_FAST,
	PROCESSING_MODE_HIGH_QUALITY,
	PROCESSING_MODE_MINIMAL,
	PROCESSING_MODE_ZERO_SHUTTER_LAG,

	/* vendor feature */
	PROCESSING_MODE_MANUAL = 100,
	PROCESSING_MODE_SOFT = 101,
};

struct camera2_hotpixel_ctl {
	enum processing_mode	mode;
};

/* android.demosaic */

enum demosaic_processing_mode {
	DEMOSAIC_PROCESSING_MODE_FAST = 1,
	DEMOSAIC_PROCESSING_MODE_HIGH_QUALITY
};

struct camera2_demosaic_ctl {
	enum demosaic_processing_mode	mode;
};

/* android.noiseReduction */

struct camera2_noisereduction_ctl {
	enum processing_mode	mode;
	uint8_t			strength;
};

/* android.shading */

struct camera2_shading_ctl {
	enum processing_mode	mode;
	uint8_t			strength;
};

/* android.colorCorrection */

enum colorcorrection_mode {
	COLORCORRECTION_MODE_TRANSFORM_MATRIX = 1,
	COLORCORRECTION_MODE_FAST,
	COLORCORRECTION_MODE_HIGH_QUALITY,
};

struct camera2_colorcorrection_ctl {
	enum colorcorrection_mode	mode;
	struct rational			transform[9];
	float				gains[4];
	enum processing_mode		aberrationCorrectionMode;

	/* vendor feature */
	uint32_t			vendor_hue;
	uint32_t			vendor_saturation;
	uint32_t			vendor_brightness;
	uint32_t			vendor_contrast;
};

struct camera2_colorcorrection_dm {
	struct rational			transform[9];
	float				gains[4];
};

/* android.tonemap */

enum tonemap_mode {
	TONEMAP_MODE_CONTRAST_CURVE = 1,
	TONEMAP_MODE_FAST,
	TONEMAP_MODE_HIGH_QUALITY,
	TONEMAP_MODE_GAMMA_VALUE,
	TONEMAP_MODE_PRESET_CURVE,
};

enum tonemap_presetCurve {
	TONEMAP_PRESET_CURVE_SRGB,
	TONEMAP_PRESET_CURVE_REC709,
};

struct camera2_tonemap_ctl {
	float				curveBlue[64];
	float				curveGreen[64];
	float				curveRed[64];
	float				curve;
	enum tonemap_mode		mode;
	float				gamma;
	enum tonemap_presetCurve  	presetCurve;
};

/* android.edge */

struct camera2_edge_ctl {
	enum processing_mode	mode;
	uint8_t			strength;
};

/* android.scaler */

struct camera2_scaler_ctl {
	uint32_t	cropRegion[4];
};

struct camera2_scaler_dm {
	uint32_t	cropRegion[4];
};

enum available_stream_configurations {
	SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT = 0,
	SCALER_AVAILABLE_STREAM_CONFIGURATIONS_INPUT,
};

enum scaler_cropping_type {
	SCALER_CROPPING_TYPE_CENTER_ONLY = 0,
	SCALER_CROPPING_TYPE_FREEFORM,
};

/* android.jpeg */
struct camera2_jpeg_ctl {
	uint8_t		gpsLocation;
	double		gpsCoordinates[3];
	uint8_t		gpsProcessingMethod[33];
	uint64_t	gpsTimestamp;
	uint32_t	orientation;
	uint8_t		quality;
	uint8_t		thumbnailQuality;
	uint32_t	thumbnailSize[2];
};

/* android.statistics */

enum facedetect_mode {
	FACEDETECT_MODE_OFF = 1,
	FACEDETECT_MODE_SIMPLE,
	FACEDETECT_MODE_FULL
};

enum stats_mode {
	STATS_MODE_OFF = 1,
	STATS_MODE_ON
};

enum stats_scene_flicker {
	STATISTICS_SCENE_FLICKER_NONE = 1,
	STATISTICS_SCENE_FLICKER_50HZ,
	STATISTICS_SCENE_FLICKER_60HZ,
};

enum stats_lowlightmode {
	STATE_LLS_LEVEL_ZSL = 0,
	STATE_LLS_LEVEL_LOW = 1,
	STATE_LLS_LEVEL_HIGH = 2,
	STATE_LLS_LEVEL_SIS = 3,
	STATE_LLS_LEVEL_ZSL_LIKE = 4,
	STATE_LLS_LEVEL_ZSL_LIKE1 = 7,
	STATE_LLS_LEVEL_SHARPEN_SINGLE = 8,
	STATE_LLS_MANUAL_ISO = 9,
	STATE_LLS_LEVEL_SHARPEN_DR = 10,
	STATE_LLS_LEVEL_SHARPEN_IMA = 11,
	STATE_LLS_LEVEL_STK = 12,
	STATE_LLS_LEVEL_FLASH = 16,
	STATE_LLS_LEVEL_MULTI_MERGE_2 = 18,
	STATE_LLS_LEVEL_MULTI_MERGE_3 = 19,
	STATE_LLS_LEVEL_MULTI_MERGE_4 = 20,
	STATE_LLS_LEVEL_MULTI_PICK_2 = 34,
	STATE_LLS_LEVEL_MULTI_PICK_3 = 35,
	STATE_LLS_LEVEL_MULTI_PICK_4 = 36,
	STATE_LLS_LEVEL_MULTI_MERGE_INDICATOR_2 = 50,
	STATE_LLS_LEVEL_MULTI_MERGE_INDICATOR_3 = 51,
	STATE_LLS_LEVEL_MULTI_MERGE_INDICATOR_4 = 52,
	STATE_LLS_LEVEL_FLASH_2 = 66,
	STATE_LLS_LEVEL_FLASH_3 = 67,
	STATE_LLS_LEVEL_FLASH_4 = 68,
	STATE_LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_2 = 82,
	STATE_LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_3 = 83,
	STATE_LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_4 = 84,
	STATE_LLS_LEVEL_FLASH_LOW_2 = 98,
	STATE_LLS_LEVEL_FLASH_LOW_3 = 99,
	STATE_LLS_LEVEL_FLASH_LOW_4 = 100,
	STATE_LLS_LEVEL_DUMMY = 150,
};

enum stats_wdrAutoState {
	STATE_WDR_AUTO_OFF = 1,
	STATE_WDR_AUTO_REQUIRED = 2,
};

struct PersonMeta_t {
	unsigned int face_Yover200;
	unsigned int face_Yover230;
	unsigned int face_Yavg;
	unsigned int face_pixelRatio[3];
	unsigned int hAvg;
	unsigned int sAvg;
	unsigned int vAvg;
	unsigned int faceIdx;
	unsigned int faceRegion[4];
};

struct PersonWholeMeta_t {
	unsigned int numStat;
	unsigned int humanRatio;
	struct PersonMeta_t humanStat[5];
};

struct camera2_stats_ctl {
	enum facedetect_mode	faceDetectMode;
	enum stats_mode		histogramMode;
	enum stats_mode		sharpnessMapMode;
	enum stats_mode		hotPixelMapMode;
	enum stats_mode		lensShadingMapMode;
	struct PersonWholeMeta_t metaForPerson;
};

struct camera2_stats_dm {
	enum facedetect_mode		faceDetectMode;
	uint32_t			faceIds[CAMERA2_MAX_FACES];
	uint32_t			faceLandmarks[CAMERA2_MAX_FACES][6];
	uint32_t			faceRectangles[CAMERA2_MAX_FACES][4];
	uint8_t 			faceScores[CAMERA2_MAX_FACES];
	uint32_t			faceSrcImageSize[2];
	uint32_t			faces[CAMERA2_MAX_FACES - 2];
	uint32_t			histogram[3 * 256];
	int32_t 			sharpnessMap[2][2][3];
	uint8_t 			lensShadingCorrectionMap;
	float				lensShadingMap[17 * 13 * 4]; // grid width * grid height * 4 channel
	enum stats_scene_flicker	sceneFlicker;
	int32_t 			hotPixelMap[CAMERA2_MAX_AVAILABLE_MODE][2];

	/* vendor feature */
	uint32_t			vendor_lls_tuning_set_index;
	uint32_t			vendor_lls_brightness_index;
	enum stats_wdrAutoState		vendor_wdrAutoState;
	uint32_t			vendor_rgbAvgSamples[2][4];
};

/* android.control */

enum aa_capture_intent {
	AA_CAPTURE_INTENT_CUSTOM = 0,
	AA_CAPTURE_INTENT_PREVIEW,
	AA_CAPTURE_INTENT_STILL_CAPTURE,
	AA_CAPTURE_INTENT_VIDEO_RECORD,
	AA_CAPTURE_INTENT_VIDEO_SNAPSHOT,
	AA_CAPTURE_INTENT_ZERO_SHUTTER_LAG,
	AA_CAPTURE_INTENT_MANUAL,
	AA_CAPTURE_INTENT_MOTION_TRACKING,

	/* vendor feature */
	AA_CAPTURE_INTENT_STILL_CAPTURE_OIS_SINGLE                      = 100,
	AA_CAPTURE_INTENT_STILL_CAPTURE_OIS_MULTI                       = 101,
	AA_CAPTURE_INTENT_STILL_CAPTURE_OIS_BEST                        = 102,
	AA_CAPTURE_INTENT_STILL_CAPTURE_COMP_BYPASS                     = 103,
	AA_CAPTURE_INTENT_STILL_CAPTURE_RAWDUMP = AA_CAPTURE_INTENT_STILL_CAPTURE_COMP_BYPASS,
	AA_CAPTURE_INTENT_STILL_CAPTURE_OIS_DEBLUR                      = 104,
	AA_CAPTURE_INTENT_STILL_CAPTURE_DEBLUR_DYNAMIC_SHOT             = 105,
	AA_CAPTURE_INTENT_STILL_CAPTURE_OIS_DYNAMIC_SHOT                = 106,
	AA_CAPTURE_INTENT_STILL_CAPTURE_EXPOSURE_DYNAMIC_SHOT           = 107,
	AA_CAPTURE_INTENT_STILL_CAPTURE_MFHDR_DYNAMIC_SHOT              = 108,
	AA_CAPTURE_INTENT_STILL_CAPTURE_LLHDR_DYNAMIC_SHOT              = 109,
	AA_CAPTURE_INTENT_STILL_CAPTURE_SUPER_NIGHT_SHOT_HANDHELD       = 110,
	AA_CAPTURE_INTENT_STILL_CAPTURE_SUPER_NIGHT_SHOT_TRIPOD         = 111,
	AA_CAPTURE_INTENT_STILL_CAPTURE_CANCEL                          = 112,
	AA_CAPTURE_INTENT_STILL_CAPTURE_NORMAL_FLASH                    = 113,
	AA_CAPTURE_INTENT_STILL_CAPTURE_REMOSAIC_SINGLE                 = 114,
	AA_CAPTURE_INTENT_STILL_CAPTURE_REMOSAIC_DYNAMIC_SHOT           = 115,
	AA_CAPTURE_INTENT_STILL_CAPTURE_REMOSAIC_MFHDR_DYNAMIC_SHOT     = 116,
	AA_CAPTURE_INTENT_STILL_CAPTURE_LLHDR_VEHDR_DYNAMIC_SHOT        = 117,
	AA_CAPTURE_INTENT_STILL_CAPTURE_VENR_DYNAMIC_SHOT               = 118,
	AA_CAPTURE_INTENT_STILL_CAPTURE_LLS_FLASH                       = 119,
	AA_CAPTURE_INTENT_STILL_CAPTURE_SUPER_NIGHT_SHOT_HANDHELD_FAST          = 120,    // 1st frame for JPEG+Thumbnail
	AA_CAPTURE_INTENT_STILL_CAPTURE_SUPER_NIGHT_SHOT_TRIPOD_FAST            = 121,      // 1st frame for JPEG+Thumbnail
	AA_CAPTURE_INTENT_STILL_CAPTURE_SUPER_NIGHT_SHOT_TRIPOD_LE_FAST         = 122,   // 1st frame for JPEG+Thumbnail
	AA_CAPTURE_INTENT_STILL_CAPTURE_CROPPED_REMOSAIC_DYNAMIC_SHOT           = 123,
	AA_CAPTURE_INTENT_STILL_CAPTURE_REMOSAIC_SINGLE_FLASH                   = 124,
	AA_CAPTURE_INTENT_STILL_CAPTURE_CROPPED_REMOSAIC_SINGLE                 = 125,
	AA_CAPTURE_INTENT_STILL_CAPTURE_SHORT_REF_LLHDR_DYNAMIC_SHOT            = 126,
	AA_CAPTURE_INTENT_STILL_CAPTURE_PROCESSED_REMOSAIC_SINGLE               = 127,
	AA_CAPTURE_INTENT_STILL_CAPTURE_PROCESSED_REMOSAIC_DYNAMIC_SHOT         = 128,
	AA_CAPTURE_INTENT_STILL_CAPTURE_PROCESSED_REMOSAIC_MFHDR_DYNAMIC_SHOT   = 129,
	AA_CAPTURE_INTENT_STILL_CAPTURE_PROCESSED_REMOSAIC_SINGLE_FLASH         = 130,
	AA_CAPTURE_INTENT_STILL_CAPTURE_AIF_DYNAMIC_SHOT                        = 131,
	AA_CAPTURE_INTENT_STILL_CAPTURE_GFHDR_DYNAMIC_SHOT                      = 132,
	AA_CAPTURE_INTENT_STILL_CAPTURE_SUPER_NIGHT_SHOT_EXTREME_DARK           = 133,
	AA_CAPTURE_INTENT_STILL_CAPTURE_SUPER_NIGHT_SHOT_HANDHELD_MAX           = 134,
	AA_CAPTURE_INTENT_STILL_CAPTURE_SUPER_NIGHT_SHOT_TRIPOD_MAX             = 135,
	AA_CAPTURE_INTENT_STILL_CAPTURE_SUPER_NIGHT_SHOT_EXTREME_DARK_MAX       = 136,
	AA_CAPTURE_INTENT_STILL_CAPTURE_SR_HDR_DYNAMIC_SHOT                     = 137,
	AA_CAPTURE_INTENT_STILL_CAPTURE_GALAXY_RAW_DYNAMIC_SHOT                 = 138,
	AA_CAPTURE_INTENT_STILL_CAPTURE_HYBRID_MFHDR_DYNAMIC_SHOT               = 160,
	AA_CAPTURE_INTENT_STILL_CAPTURE_HYBRID_LLHDR_DYNAMIC_SHOT               = 161,
    AA_CAPTURE_INTENT_STILL_CAPTURE_EXECUTOR_NIGHT_SHOT                     = 162,
    AA_CAPTURE_INTENT_STILL_CAPTURE_LIGHT_TRAIL_SHOT                        = 163,
	AA_CAPTURE_INTENT_STILL_CAPTURE_SUPER_NIGHT_SHOT_HANDHELD_POST_LIB      = 164,
	AA_CAPTURE_INTENT_STILL_CAPTURE_SUPER_NIGHT_SHOT_TRIPOD_POST_LIB        = 165,
	AA_CAPTURE_INTENT_STILL_CAPTURE_ASTRO_SHOT                              = 166,

	AA_CAPTURE_INTENT_VIDEO_RECORD_CHANGE_FPS                               = 500,

	AA_CAPTURE_INTENT_STILL_CAPTURE_SPORT_MOTIONLEVEL0 = 90000,
	AA_CAPTURE_INTENT_STILL_CAPTURE_SPORT_MOTIONLEVEL1 = 90001,
	AA_CAPTURE_INTENT_STILL_CAPTURE_SPORT_MOTIONLEVEL2 = 90002,
	AA_CAPTURE_INTENT_STILL_CAPTURE_SUPER_MOON,
	AA_CAPTURE_INTENT_STILL_CAPTURE_REMOSAIC_EXPOSURE_DYNAMIC_SHOT,
};

enum aa_mode {
	AA_CONTROL_OFF = 1,
	AA_CONTROL_AUTO,
	AA_CONTROL_USE_SCENE_MODE,
	AA_CONTROL_OFF_KEEP_STATE,
};

enum aa_scene_mode {
	AA_SCENE_MODE_DISABLED = 1,
	AA_SCENE_MODE_FACE_PRIORITY,
	AA_SCENE_MODE_ACTION,
	AA_SCENE_MODE_PORTRAIT,
	AA_SCENE_MODE_LANDSCAPE,
	AA_SCENE_MODE_NIGHT,
	AA_SCENE_MODE_NIGHT_PORTRAIT,
	AA_SCENE_MODE_THEATRE,
	AA_SCENE_MODE_BEACH,
	AA_SCENE_MODE_SNOW,
	AA_SCENE_MODE_SUNSET,
	AA_SCENE_MODE_STEADYPHOTO,
	AA_SCENE_MODE_FIREWORKS,
	AA_SCENE_MODE_SPORTS,
	AA_SCENE_MODE_PARTY,
	AA_SCENE_MODE_CANDLELIGHT,
	AA_SCENE_MODE_BARCODE,
	AA_SCENE_MODE_HIGH_SPEED_VIDEO,
	AA_SCENE_MODE_HDR,
	AA_SCENE_MODE_FACE_PRIORITY_LOW_LIGHT,

	AA_SCENE_MODE_MANUAL_MFHDR,
	/* vendor feature */
	AA_SCENE_MODE_NIGHT_CAPTURE    = 100,
	AA_SCENE_MODE_ANTISHAKE        = 101,
	AA_SCENE_MODE_LLS              = 102,
	AA_SCENE_MODE_FDAE             = 103,
	AA_SCENE_MODE_DUAL             = 104,
	AA_SCENE_MODE_DRAMA            = 105,
	AA_SCENE_MODE_ANIMATED         = 106,
	AA_SCENE_MODE_PANORAMA         = 107,
	AA_SCENE_MODE_GOLF             = 108,
	AA_SCENE_MODE_PREVIEW          = 109,
	AA_SCENE_MODE_VIDEO            = 110,
	AA_SCENE_MODE_SLOWMOTION_2     = 111,
	AA_SCENE_MODE_SLOWMOTION_4_8   = 112,
	AA_SCENE_MODE_DUAL_PREVIEW     = 113,
	AA_SCENE_MODE_DUAL_VIDEO       = 114,
	AA_SCENE_MODE_120_PREVIEW      = 115,
	AA_SCENE_MODE_LIGHT_TRACE      = 116,
	AA_SCENE_MODE_FOOD             = 117,
	AA_SCENE_MODE_AQUA             = 118,
	AA_SCENE_MODE_THERMAL          = 119,
	AA_SCENE_MODE_VIDEO_COLLAGE    = 120,
	AA_SCENE_MODE_PRO_MODE         = 121,
	AA_SCENE_MODE_COLOR_IRIS       = 122,
	AA_SCENE_MODE_FACE_LOCK        = 123,
	AA_SCENE_MODE_LIVE_OUTFOCUS    = 124,
	AA_SCENE_MODE_REMOSAIC         = 125,
	AA_SCENE_MODE_SUPER_SLOWMOTION = 126,
	AA_SCENE_MODE_HYPERLAPSE       = 127,
	AA_SCENE_MODE_FACTORY_LN2      = 128,
	AA_SCENE_MODE_FACTORY_LN4      = 129,
	AA_SCENE_MODE_LABS             = 130,
	AA_SCENE_MODE_REMOSAIC_PURE_BAYER_ONLY = 131,
	AA_SCENE_MODE_REMOSAIC_MFHDR_PURE_BAYER_ONLY = 132,

	AA_SCENE_MODE_SELFI_FOCUS      = 133,
	AA_SCENE_MODE_STICKER          = 134,
	AA_SCENE_MODE_INSTAGRAM        = 135,
	AA_SCENE_MODE_FAST_AE          = 136,
	AA_SCENE_MODE_ILLUMINANCE      = 137,
	AA_SCENE_MODE_SUPER_NIGHT      = 138,
	AA_SCENE_MODE_BOKEH_VIDEO      = 139,
	AA_SCENE_MODE_SINGLE_TAKE      = 140,
	AA_SCENE_MODE_DIRECTORS_VIEW   = 141,
	AA_SCENE_MODE_PORTRAIT_NIGHT   = 142,
	AA_SCENE_MODE_PORTRAIT_VERY_NIGHT = 143,
	AA_SCENE_MODE_FHD_AUTO         = 144,
	AA_SCENE_MODE_AI_HIGHRES       = 146,
	AA_SCENE_MODE_EXECUTOR_TOOL    = 147,
	AA_SCENE_MODE_LIGHT_TRAIL      = 148,
	AA_SCENE_MODE_ASTRO            = 149,
};

enum aa_effect_mode {
	AA_EFFECT_OFF = 1,
	AA_EFFECT_MONO,
	AA_EFFECT_NEGATIVE,
	AA_EFFECT_SOLARIZE,
	AA_EFFECT_SEPIA,
	AA_EFFECT_POSTERIZE,
	AA_EFFECT_WHITEBOARD,
	AA_EFFECT_BLACKBOARD,
	AA_EFFECT_AQUA,

	/* vendor feature */
	AA_EFFECT_EMBOSS = 100,
	AA_EFFECT_EMBOSS_MONO,
	AA_EFFECT_SKETCH,
	AA_EFFECT_RED_YELLOW_POINT,
	AA_EFFECT_GREEN_POINT,
	AA_EFFECT_BLUE_POINT,
	AA_EFFECT_MAGENTA_POINT,
	AA_EFFECT_WARM_VINTAGE,
	AA_EFFECT_COLD_VINTAGE,
	AA_EFFECT_WASHED,
	AA_EFFECT_BEAUTY_FACE
};

enum aa_ae_lock {
	AA_AE_LOCK_OFF = 1,
	AA_AE_LOCK_ON
};

enum aa_aemode {
	AA_AEMODE_OFF = 1,
	AA_AEMODE_ON,
	AA_AEMODE_ON_AUTO_FLASH,
	AA_AEMODE_ON_ALWAYS_FLASH,
	AA_AEMODE_ON_AUTO_FLASH_REDEYE,
	AA_AEMODE_ON_EXTERNAL_FLASH,

	/* vendor feature */
	AA_AEMODE_CENTER = 100,
	AA_AEMODE_AVERAGE,
	AA_AEMODE_MATRIX,
	AA_AEMODE_SPOT,
	AA_AEMODE_CENTER_TOUCH,
	AA_AEMODE_AVERAGE_TOUCH,
	AA_AEMODE_MATRIX_TOUCH,
	AA_AEMODE_SPOT_TOUCH,
	AA_AEMODE_EMUL,
	UNKNOWN_AA_AE_MODE
};

enum aa_ae_flashmode {
	/*all flash control stop*/
	AA_FLASHMODE_OFF = 1,
	/*flash start*/
	AA_FLASHMODE_START,
	/*flash cancle*/
	AA_FLASHMODE_CANCEL,
	/*internal 3A can control flash*/
	AA_FLASHMODE_ON,
	/*internal 3A can do auto flash algorithm*/
	AA_FLASHMODE_AUTO,
	/*internal 3A can fire flash by auto result*/
	AA_FLASHMODE_CAPTURE,
	/*internal 3A can control flash forced*/
	AA_FLASHMODE_ON_ALWAYS
};

enum aa_ae_antibanding_mode {
	AA_AE_ANTIBANDING_OFF = 1,
	AA_AE_ANTIBANDING_50HZ,
	AA_AE_ANTIBANDING_60HZ,
	AA_AE_ANTIBANDING_AUTO,

	/* vendor feature */
	AA_AE_ANTIBANDING_AUTO_50HZ = 100,   /*50Hz + Auto*/
	AA_AE_ANTIBANDING_AUTO_60HZ	/*60Hz + Auto*/
};

enum aa_awb_lock {
	AA_AWB_LOCK_OFF = 1,
	AA_AWB_LOCK_ON,
};

enum aa_awbmode {
	AA_AWBMODE_OFF = 1,
	AA_AWBMODE_WB_AUTO,
	AA_AWBMODE_WB_INCANDESCENT,
	AA_AWBMODE_WB_FLUORESCENT,
	AA_AWBMODE_WB_WARM_FLUORESCENT,
	AA_AWBMODE_WB_DAYLIGHT,
	AA_AWBMODE_WB_CLOUDY_DAYLIGHT,
	AA_AWBMODE_WB_TWILIGHT,
	AA_AWBMODE_WB_SHADE,
	AA_AWBMODE_WB_CUSTOM_K
};

enum aa_ae_precapture_trigger {
	AA_AE_PRECAPTURE_TRIGGER_IDLE = 0,
	AA_AE_PRECAPTURE_TRIGGER_START,
	AA_AE_PRECAPTURE_TRIGGER_CANCEL,
};

enum aa_afmode {
	AA_AFMODE_INVALID = 0,
	AA_AFMODE_OFF = 1,
	AA_AFMODE_AUTO,
	AA_AFMODE_MACRO,
	AA_AFMODE_CONTINUOUS_VIDEO,
	AA_AFMODE_CONTINUOUS_PICTURE,
	AA_AFMODE_EDOF,
};

enum aa_afmode_option_bit {
	AA_AFMODE_OPTION_BIT_VIDEO = 0,
	AA_AFMODE_OPTION_BIT_MACRO = 1,
	AA_AFMODE_OPTION_BIT_FACE = 2,
	AA_AFMODE_OPTION_BIT_DELAYED = 3,
	AA_AFMODE_OPTION_BIT_OUT_FOCUSING = 4,
	AA_AFMODE_OPTION_BIT_OBJECT_TRACKING = 5,
	AA_AFMODE_OPTION_BIT_AF_ROI_NO_CONV = 6,
	AA_AFMODE_OPTION_BIT_MULTI_AF = 7,
	AA_AFMODE_OPTION_BIT_AUTO_FRAMING    = 8,
};

enum aa_afmode_ext {
	AA_AFMODE_EXT_OFF = 1,
	/* Increase macro range for special app */
	AA_AFMODE_EXT_ADVANCED_MACRO_FOCUS = 2,
	/* Set AF region for OCR */
	AA_AFMODE_EXT_FOCUS_LOCATION = 3,
	/* Set fixed face */
	AA_AFMODE_EXT_FIXED_FACE = 4,
	AA_AFMODE_EXT_FULLSWEEP_MODE = 5,
};

enum aa_af_trigger {
	AA_AF_TRIGGER_IDLE = 0,
	AA_AF_TRIGGER_START,
	AA_AF_TRIGGER_CANCEL,
};

enum aa_afstate {
	AA_AFSTATE_INACTIVE = 1,
	AA_AFSTATE_PASSIVE_SCAN,
	AA_AFSTATE_PASSIVE_FOCUSED,
	AA_AFSTATE_ACTIVE_SCAN,
	AA_AFSTATE_FOCUSED_LOCKED,
	AA_AFSTATE_NOT_FOCUSED_LOCKED,
	AA_AFSTATE_PASSIVE_UNFOCUSED,
	AA_AFSTATE_FIXED_FOCUSED_INACTIVE = 102,
};

enum ae_state {
	AE_STATE_INACTIVE = 1,
	AE_STATE_SEARCHING,
	AE_STATE_CONVERGED,
	AE_STATE_LOCKED,
	AE_STATE_FLASH_REQUIRED,
	AE_STATE_PRECAPTURE,
	AE_STATE_LOCKED_CONVERGED = 10,
	AE_STATE_LOCKED_FLASH_REQUIRED,
	AE_STATE_SEARCHING_FLASH_REQUIRED,
};

enum awb_state {
	AWB_STATE_INACTIVE = 1,
	AWB_STATE_SEARCHING,
	AWB_STATE_CONVERGED,
	AWB_STATE_LOCKED
};

enum aa_videostabilization_mode {
	VIDEO_STABILIZATION_MODE_OFF = 0,
	VIDEO_STABILIZATION_MODE_ON,
	VIDEO_STABILIZATION_MODE_SWVDIS = 100,
	VIDEO_STABILIZATION_MODE_SUPERSTEADY,
};

enum aa_isomode {
	AA_ISOMODE_AUTO = 1,
	AA_ISOMODE_MANUAL,
};

enum aa_cameraid {
	AA_CAMERAID_FRONT = 1,
	AA_CAMERAID_REAR,
};

enum aa_cameraMode {
	AA_CAMERAMODE_SINGLE = 1,
	AA_CAMERAMODE_DUAL_SYNC = 2,
	AA_CAMERAMODE_DUAL_ASYNC = 3,
	AA_CAMERAMODE_DUAL_ONE_SENSOR_OFF = 4,
	AA_CAMERAMODE_DUAL_SYNC_SAME_TIME_CONTROL = 5,
};

enum aa_sensorPlace {
	AA_SENSORPLACE_REAR = 0,
	AA_SENSORPLACE_FRONT = 1,
	AA_SENSORPLACE_REAR2 = 2,
	AA_SENSORPLACE_FRONT2 = 3,
	AA_SENSORPLACE_REAR3 = 4,
	AA_SENSORPLACE_FRONT3 = 5,
	AA_SENSORPLACE_REAR4 = 6,
	AA_SENSORPLACE_FRONT4 = 7,
	AA_SENSORPLACE_END,
};

enum aa_fallback {
	AA_FALLBACK_INACTIVE = 0,
	AA_FALLBACK_ACTIVE = 1,
};

enum aa_cameratype {
	AA_CAMERATYPE_SINGLE = 1,
	AA_CAMERATYPE_WIDE,
	AA_CAMERATYPE_TELE,
};

enum aa_videomode {
	AA_VIDEOMODE_OFF = 0,
	AA_VIDEOMODE_ON_NORMAL,
	AA_VIDEOMODE_ON_HDR10,
};

enum aa_ae_facemode {
	AA_AE_FACEMODE_OFF = 0,
	AA_AE_FACEMODE_ON,
};

enum aa_ae_lockavailable {
	AE_LOCK_AVAILABLE_FALSE = 0,
	AE_LOCK_AVAILABLE_TRUE,
};

enum aa_supernightmode {
	AA_SUPER_NIGHT_MODE_OFF = 0,
	AA_SUPER_NIGHT_MODE_ON,
	AA_SUPER_NIGHT_MODE_MAX_ON,
};

enum aa_awb_lockavailable {
	AWB_LOCK_AVAILABLE_FALSE = 0,
	AWB_LOCK_AVAILABLE_TRUE,
};

enum aa_available_mode {
	AA_OFF = 0,
	AA_AUTO,
	/* AA_USE_SCENE_MODE,
	 * AA_OFF_KEEP_STATE, */
};

enum aa_af_scene_change {
	AA_AF_NOT_DETECTED = 0,
	AA_AF_DETECTED,
};

enum aa_aemode_state {
	AA_AE_NORMAL = 0,
	AA_AE_FACE,
	AA_AE_TOUCH,
};

enum aa_dynamic_bds_mode {
	AA_DYNAMIC_BDS_MODE_OFF = 0,
	AA_DYNAMIC_BDS_MODE_LOW_LIGHT_ON = 1,	/* IQ controlled */
	AA_DYNAMIC_BDS_MODE_SYSTEM_ON  = 2,	/* HAL controlled */
	AA_DYNAMIC_BDS_MODE_VT_CALL_LOW_POWER  = 3, 	/* HAL controlled */
};

enum aa_enable_dynamicshot {
	AA_DYNAMICSHOT_SIMPLE = 0,
	AA_DYNAMICSHOT_FULL,
	AA_DYNAMICSHOT_HDR_ONLY,
	AA_DYNAMICSHOT_LLS_ONLY,
	AA_DYNAMICSHOT_SINGLE_ONLY,
};

enum aa_night_timelaps_mode {
  AA_NIGHT_TIMELAPS_MODE_OFF = 0,
  AA_NIGHT_TIMELAPS_MODE_ON_45X,
  AA_NIGHT_TIMELAPS_MODE_ON_15X,
  AA_NIGHT_TIMELAPS_MODE_ON_300X,
};

enum aa_capture_hint {
	AA_CAPTURE_HINT_NONE = 0,
	AA_CAPTURE_HINT_BURST,
	AA_CAPTURE_HINT_AGIF,
	AA_CAPTURE_HINT_SINGLE_TAKE,
	AA_CAPTURE_HINT_FACTORY_MAIN = 100,
	AA_CAPTURE_HINT_FACTORY_SECONDARY = 101,
};

enum aa_short_ref_hdr_mode {
	AA_SHORTREF_HDR_OFF = 0,
	AA_SHORTREF_HDR_ON,
};

enum aa_control_ssrm_hint {
	AA_SSRM_HINT_NONE = 0,
	AA_SSRM_HINT_LOW_POWER_MODE_L1 = 1 << 0,
	AA_SSRM_HINT_LOW_POWER_MODE_L2 = 1 << 1,
	AA_SSRM_HINT_LOW_POWER_MODE_L3 = 1 << 2,
};

enum aa_ae_extra_mode {
	AA_AE_EXTRA_MODE_AUTO             = 0,
	AA_AE_EXTRA_MODE_SHUTTER_PRIORITY = 1,
	AA_AE_EXTRA_MODE_ISO_PRIORITY     = 2,
};

enum aa_transient_capture_action {
	AA_TRANSIENT_CAPTURE_ACTION_OFF = 0,
	AA_TRANSIENT_CAPTURE_ACTION_FAST_CAPTURE = 1,
};

enum aa_night_indicator {
	AA_NIGHT_INDICATOR_NONE = 0,
	AA_NIGHT_INDICATOR_ON,
	AA_NIGHT_INDICATOR_OFF,
};

struct camera2_video_output_size {
	uint16_t			width;
	uint16_t			height;
};

struct tof_info {
	uint16_t			fps;
	uint16_t			exposureTime;
};

struct depth_info {
	int32_t			AIFCaptureNum;
};

struct camera2_aa_ctl {
	enum aa_ae_antibanding_mode	aeAntibandingMode;
	int32_t				aeExpCompensation;
	enum aa_ae_lock			aeLock;
	enum aa_aemode			aeMode;
	uint32_t			aeRegions[5];
	uint32_t			aeTargetFpsRange[2];
	enum aa_ae_precapture_trigger	aePrecaptureTrigger;
	enum aa_afmode			afMode;
	uint32_t			afRegions[5];
	enum aa_af_trigger		afTrigger;
	enum aa_awb_lock		awbLock;
	enum aa_awbmode			awbMode;
	uint32_t			awbRegions[5];
	enum aa_capture_intent		captureIntent;
	enum aa_effect_mode		effectMode;
	enum aa_mode			mode;
	enum aa_scene_mode		sceneMode;
	enum aa_videostabilization_mode videoStabilizationMode;

	/* vendor feature */
	float				vendor_aeExpCompensationStep;
	uint32_t			vendor_afmode_option;
	enum aa_afmode_ext		vendor_afmode_ext;
	enum aa_ae_flashmode		vendor_aeflashMode;
	enum aa_isomode			vendor_isoMode;
	uint32_t			vendor_isoValue;
	int32_t				vendor_awbValue;
	enum aa_cameraid		vendor_cameraId;
	enum aa_videomode		vendor_videoMode;
	enum aa_ae_facemode		vendor_aeFaceMode;
	int32_t				vendor_exposureValue;
	uint32_t			vendor_captureCount;
	uint32_t			vendor_captureExposureTime;
	int32_t				vendor_colorTempKelvin;
	enum aa_enable_dynamicshot	vendor_enableDynamicShotDm;
	float				vendor_expBracketing[15];      // Used in mass
	float				vendor_expBracketingCapture;
	enum aa_supernightmode		vendor_superNightShotMode;
	struct camera2_video_output_size vendor_videoOutputSize;
	enum aa_night_timelaps_mode	vendor_nightTimelapsMode;
	uint32_t			vendor_personalPresetIndex;
	struct tof_info			vendor_TOFInfo;
	uint32_t			vendor_captureHint;
	int32_t				vendor_captureEV;
	uint32_t			vendor_ssrmHint;
	enum aa_short_ref_hdr_mode	vendor_shortRefHdrEnable;
	int32_t				vendor_memTotal;
	struct depth_info		vendor_depthInfo;
	enum aa_ae_extra_mode		vendor_aeExtraMode;
	enum aa_dynamic_bds_mode	vendor_enableDynamicBds;
	enum aa_transient_capture_action	vendor_transientCaptureAction;
	uint32_t			ispHwTargetFpsRange[2];
	uint32_t			vendor_memAvailable;
	int32_t 			vendor_fpsHintResult;
	uint32_t			vendor_currentHyperlapseMode;   // 0 : auto, value : speed
	char				vendor_multiFrameEvList[EV_LIST_SIZE];
	uint32_t			vendor_multiFrameIsoList[MANUAL_LIST_SIZE];
	uint32_t			vendor_multiFrameExposureList[MANUAL_LIST_SIZE];
	uint32_t			vendor_zoomLockState;
	uint64_t			vendor_specialImageQualityPolicy;
	uint32_t 			vendor_exposureTableType;
    uint32_t 			vendor_fastCaptureOption;
	uint32_t			vendor_reserved[14];
};

struct aa_apexInfo {
	int32_t av;
	int32_t sv;
	int32_t tv;
	int32_t ev;
	int32_t bv;
};

struct sensorInfo {
	uint32_t analogGain;
	uint32_t digitalGain;
	uint64_t longExposureTime;
	uint64_t midExposureTime;
	uint64_t shortExposureTime;
	uint32_t longAnalogGain;
	uint32_t midAnalogGain;
	uint32_t shortAnalogGain;
	uint32_t longDigitalGain;
	uint32_t midDigitalGain;
	uint32_t shortDigitalGain;
};

struct osdInfo {
	struct sensorInfo sensorInfo;
	/* awb,af TBD  */
	uint32_t          reserved[10];
};

struct oisHallInfo {
	uint64_t readTimeStamp;
	uint32_t counter;
	int16_t X_AngVel[4];
	int16_t Y_AngVel[4];
	int16_t Z_AngVel[4];
};

struct camera2_aa_dm {
	int32_t				aeExpCompensation;
	uint32_t			aeRegions[5];
	enum ae_state			aeState;
	uint32_t			afRegions[5];
	enum aa_afstate			afState;
	uint32_t			awbRegions[5];
	enum awb_state			awbState;
	enum aa_scene_mode		sceneMode;
	enum aa_af_scene_change		afSceneChange;

	/* vendor feature */
	uint32_t			vendor_isoValue;
	int32_t				vendor_exposureValue;
	uint32_t			vendor_touchAeDone;
	uint32_t			vendor_touchBvChange;
	float				vendor_objectDistanceCm;
	int32_t				vendor_colorTempKelvin;
	int32_t				vendor_dynamicShotValue[3];
	int32_t				vendor_lightConditionValue;
	int32_t				vendor_dynamicShotExtraInfo;
	struct aa_apexInfo		vendor_apexInfo;
	struct osdInfo			vendor_osdInfo;
	int32_t				vendor_drcRatio;
	uint32_t			vendor_colorTempIndex;  // Used for tune
	uint32_t			vendor_luxIndex;        // Used for tune
	float				vendor_luxStandard;     // Used for tune
	int32_t				vendor_multiFrameEv;
	uint32_t			vendor_faceToneWeight;
	float				vendor_noiseIndex;

	enum aa_aemode_state		vendor_aeModeState;         // Used for tune
	uint32_t			vendor_linearExposureIndex; // Used for tune
	uint32_t			ispDigitalGain;             // Used for tune
	uint32_t			vendor_previewSkipFrame;    // Used in mass
	uint32_t			vendor_aeDrcGain;           // Used in mass
	int32_t				vendor_aeStats4VO[8];
	int32_t				vendor_dynamicShotCaptureDuration;
	struct oisHallInfo		vendor_oisHallData;
	int32_t				vendor_fpsHint;
	enum aa_dynamic_bds_mode	vendor_dynamicBdsInfo;
	int32_t				vendor_gfHdrEv[2];
	uint32_t			ispHwTargetFpsRange[2];
	uint32_t			vendor_aeDarkBoostGain;
	uint32_t 			vendor_skipAFStateCapture;       // Skip checking af state before capturing. (0 / 1)
	uint32_t			vendor_nightModeSuggest;    // 0(off), 1(on)
	enum				aa_night_indicator vendor_nightIndicator;
	uint32_t			vendor_reserved[30];

	// For dual
	uint32_t			vendor_wideTeleConvEv;
	uint32_t			vendor_fusionCaptureAeInfo;
	uint32_t			vendor_fusionCaptureAfInfo;

	uint32_t			vendor_reserved_dual[9];
};

/* android.led */

enum led_transmit {
	TRANSMIT_OFF = 0,
	TRANSMIT_ON,
};

struct camera2_led_ctl {
	enum led_transmit		transmit;
};

struct camera2_led_dm {
	enum led_transmit		transmit;
};

/* android.info */

enum info_supported_hardware_level {
	INFO_SUPPORTED_HARDWARE_LEVEL_LIMITED = 0,
	INFO_SUPPORTED_HARDWARE_LEVEL_FULL,
	INFO_SUPPORTED_HARDWARE_LEVEL_LEGACY,
};

/* android.blacklevel */

enum blacklevel_lock {
	BLACK_LEVEL_LOCK_OFF = 0,
	BLACK_LEVEL_LOCK_ON,
};

struct camera2_blacklevel_ctl {
	enum blacklevel_lock		lock;
};

struct camera2_blacklevel_dm {
	enum blacklevel_lock		lock;
	float				dynamicBlackLevel[4];  // same order as CFA(Color Filter Arrangement)
};

/* android.reprocess */

struct camera2_reprocess_ctl {
	float	effectiveExposureFactor;
};

/* android.depth */

enum depth_available_depth_stream_config {
	DEPTH_AVAILABLE_DEPTH_STREAM_CONFIGURATIONS_OUTPUT,
	DEPTH_AVAILABLE_DEPTH_STREAM_CONFIGURATIONS_INPUT,
};

enum depth_depth_is_exclusive {
	DEPTH_DEPTH_IS_EXCLUSIVE_FALSE,
	DEPTH_DEPTH_IS_EXCLUSIVE_TRUE,
};

/* android.sync */

enum sync_frame_number {
	SYNC_FRAME_NUMBER_CONVERGING	= -1,
	SYNC_FRAME_NUMBER_UNKNOWN	= -2,
};

enum sync_max_latency {
	SYNC_MAX_LATENCY_PER_FRAME_CONTROL	= 0,
	SYNC_MAX_LATENCY_UNKNOWN		= -1,
};

struct camera2_sync_ctl {
	int64_t				frameNumber;
};

struct camera2_sync_dm {
	int32_t				maxLatency;
};

struct camera2_ctl {
	struct camera2_colorcorrection_ctl	color;
	struct camera2_aa_ctl			aa;
	struct camera2_demosaic_ctl		demosaic;
	struct camera2_edge_ctl			edge;
	struct camera2_flash_ctl		flash;
	struct camera2_hotpixel_ctl		hotpixel;
	struct camera2_jpeg_ctl			jpeg;
	struct camera2_lens_ctl			lens;
	struct camera2_noisereduction_ctl	noise;
	struct camera2_request_ctl		request;
	struct camera2_scaler_ctl		scaler;
	struct camera2_sensor_ctl		sensor;
	struct camera2_shading_ctl		shading;
	struct camera2_stats_ctl		stats;
	struct camera2_tonemap_ctl		tonemap;
	struct camera2_led_ctl			led;
	struct camera2_blacklevel_ctl		blacklevel;
	struct camera2_sync_ctl			sync;
	struct camera2_reprocess_ctl		reprocess;

	/* vendor feature */
	struct camera2_entry_ctl		vendor_entry;
};

struct camera2_dm {
	struct camera2_colorcorrection_dm	color;
	struct camera2_aa_dm			aa;
	struct camera2_flash_dm			flash;
	struct camera2_lens_dm			lens;
	struct camera2_request_dm		request;
	struct camera2_scaler_dm		scaler;
	struct camera2_sensor_dm		sensor;
	struct camera2_stats_dm			stats;
	struct camera2_led_dm			led;
	struct camera2_blacklevel_dm		blacklevel;
	struct camera2_sync_dm			sync;

	/* vendor feature */
	struct camera2_entry_dm			vendor_entry;
};

struct camera2_obj_af_info {
	int32_t focusState;
	int32_t focusROILeft;
	int32_t focusROIRight;
	int32_t focusROITop;
	int32_t focusROIBottom;
	int32_t focusWeight;
	int32_t w_movement;
	int32_t h_movement;
	int32_t w_velocity;
	int32_t h_velocity;
};

struct camera2_hrm_sensor_info {
	uint32_t	visible_data;
	uint32_t	ir_data;
	int32_t		flicker_data; // 0: No flicker detect, 100: 50Hz, 120: 60Hz, -3/-4/-5: reporting mode
	int32_t		status;
	uint32_t	visible_cdata;
	uint32_t	visible_rdata;
	uint32_t	visible_gdata;
	uint32_t	visible_bdata;
	uint32_t	ir_gain;
	uint32_t	ir_exptime;
};

struct camera2_illuminaion_sensor_info {
	uint16_t	visible_cdata;
	uint16_t	visible_rdata;
	uint16_t	visible_gdata;
	uint16_t	visible_bdata;
	uint16_t	visible_gain;
	uint16_t	visible_exptime;
	uint16_t	ir_north;
	uint16_t	ir_south;
	uint16_t	ir_east;
	uint16_t	ir_west;
	uint16_t	ir_gain;
	uint16_t	ir_exptime;
};

enum sensor_state {
	SENSOR_STATE_STATIONARY = 0,
	SENSOR_STATE_MOVING,
};

struct camera2_gyro_sensor_info {
	float x;
	float y;
	float z;
	enum sensor_state state;
};

struct camera2_gyro_sensor_history_info {
	bool valid;

	struct camera2_gyro_sensor_info gyroData[20];
	uint64_t gyroTimeStamp[20];
};

struct camera2_accelerometer_sensor_info {
	float x;
	float y;
	float z;
};

struct camera2_proximity_sensor_info {
	int32_t flicker_data; // 0: No flicker detect, 100: 50Hz, 120: 60Hz
};

struct camera2_temperature_info {
	int32_t usb_thermal;
};

struct camera2_aa_uctl {
	struct camera2_obj_af_info af_data;
	struct camera2_hrm_sensor_info hrmInfo;
	struct camera2_illuminaion_sensor_info illuminationInfo;
	struct camera2_gyro_sensor_info gyroInfo;
	struct camera2_gyro_sensor_history_info     gyroHistoryInfo;
	struct camera2_accelerometer_sensor_info accInfo;
	struct camera2_proximity_sensor_info		proximityInfo;
	struct camera2_temperature_info		temperatureInfo;
};

struct camera2_aa_udm {
	struct camera2_gyro_sensor_info gyroInfo;
};

/** \brief
  User-defined control for lens.
 */
struct camera2_lens_uctl {
	uint32_t	pos;
	uint32_t	posSize;
	uint32_t	direction;
	uint32_t	slewRate;
	uint32_t	oisCoefVal;
};

struct camera2_lens_udm {
	uint32_t	pos;
	uint32_t	posSize;
	uint32_t	direction;
	uint32_t	slewRate;
	uint32_t	oisCoefVal;
};

struct camera2_ae_udm {
	uint32_t	vsLength;
	uint32_t	vendorSpecific[CAMERA2_MAX_VENDER_LENGTH];
};

struct camera2_awb_udm {
	uint32_t	vsLength;
	uint32_t	vendorSpecific[CAMERA2_AWB_VENDER_LENGTH];
};

struct camera2_af_udm {
	uint32_t	vsLength;
	uint32_t	vendorSpecific[CAMERA2_MAX_VENDER_LENGTH];
	int32_t		lensPositionInfinity;
	int32_t		lensPositionMacro;
	int32_t		lensPositionCurrent;

	/** vendor specific2 length */
	uint32_t	vs2Length;
	/** vendor specific2 data array */
	uint32_t	vendorSpecific2[CAMERA2_MAX_UDM_VENDOR2_LENGTH - 1];

	/* Contrast Factor for MF capture best frame selection */
	uint32_t	contrastFactor;
};

struct camera2_ipc_udm {
	uint32_t vsLength;
	uint32_t vendorSpecific[CAMERA2_MAX_IPC_VENDER_LENGTH];
};

/** \brief
  User-defined metadata for rta debug info.
 */
struct camera2_rta_udm {
	uint32_t vsLength;
	uint32_t vendorSpecific[65];

	uint32_t vs2Length;
	uint32_t vendorSpecific2[73];
};

struct camera2_drc_udm {
	uint16_t globalToneMap[32];
};

struct camera2_rgbGamma_udm {
	uint16_t xTable[32];
	uint16_t yTable[3 * 32]; // R [0~31], G [32~63], B [64~95]
};

struct camera2_ccm_udm {
	int16_t ccmVectors9[9 * 3];
};

struct camera2_internal_udm {
	/** vendor specific data array */
	uint32_t ProcessedFrameInfo;
	uint32_t vendorSpecific[4];
    /*
       vendorSpecific2[0] : 3aaIspSircSdk
       vendorSpedific2[1] : IspTpuSirdSdk
       vendorSpecific2[100] : exposure
       vendorSpecific2[101] : iso(gain)
       vendorSpecific2[102] : Bv
       vendorSpecific2[103] : Tv
     */
};

struct camera2_sensor_uctl {
	uint64_t	dynamicFrameDuration;
	uint32_t	analogGain;
	uint32_t	digitalGain;
	uint64_t	longExposureTime; /* For supporting WDR */
	uint64_t	midExposureTime;
	uint64_t	shortExposureTime;
	uint32_t	longAnalogGain;
	uint32_t	midAnalogGain;
	uint32_t	shortAnalogGain;
	uint32_t	longDigitalGain;
	uint32_t	midDigitalGain;
	uint32_t	shortDigitalGain;
	uint64_t	exposureTime;
	uint32_t	frameDuration;
	uint32_t	sensitivity;
	uint32_t	CtrlFrameDuration;
};

struct camera2_sensor_udm {
	uint64_t	dynamicFrameDuration;
	uint32_t	analogGain;
	uint32_t	digitalGain;
	uint64_t	longExposureTime;
	uint64_t	midExposureTime;
	uint64_t	shortExposureTime;
	uint32_t	longAnalogGain;
	uint32_t	midAnalogGain;
	uint32_t	shortAnalogGain;
	uint32_t	longDigitalGain;
	uint32_t	midDigitalGain;
	uint32_t	shortDigitalGain;
	uint32_t	shortWdrExposureTime;
	uint32_t	longWdrExposureTime;
	uint32_t	shortWdrSensitivity;
	uint32_t	longWdrSensitivity;
	uint64_t	timeStampBoot;
	uint32_t	multiLuminances[9];
};

enum mcsc_port {
	MCSC_PORT_NONE = -1,
	MCSC_PORT_0,
	MCSC_PORT_1,
	MCSC_PORT_2,
	MCSC_PORT_3,
	MCSC_PORT_4,
	MCSC_PORT_MAX,
};

enum mcsc_interface_type {
	INTERFACE_TYPE_YSUM = 0,
	INTERFACE_TYPE_DS,
	INTERFACE_TYPE_MAX,
};

struct ysum_data {
	uint32_t lower_ysum_value;
	uint32_t higher_ysum_value;
};

struct camera2_scaler_uctl {
	uint32_t orientation;
	enum mcsc_port mcsc_sub_blk_port[INTERFACE_TYPE_MAX];
};

struct camera2_scaler_udm {
	struct ysum_data ysumdata;
};

struct camera2_flash_uctl {
	uint32_t		firingPower;
	uint64_t		firingTime;
	enum flash_mode		flashMode;
};

enum camera2_wdr_mode {
	CAMERA_WDR_OFF = 1,
	CAMERA_WDR_ON = 2,
	CAMERA_WDR_AUTO = 3,
	CAMERA_WDR_AUTO_LIKE = 4,
	CAMERA_WDR_AUTO_3P = 5,
	CAMERA_WDR_AUTO_3P_VIDEO = 6,
	TOTALCOUNT_CAMERA_WDR,
	CAMERA_WDR_UNKNOWN,
};

enum camera2_paf_mode {
	CAMERA_PAF_OFF = 1,
	CAMERA_PAF_ON,
};

enum camera2_disparity_mode {
	CAMERA_DISPARITY_OFF = 1,
	CAMERA_DISPARITY_SAD,
	CAMERA_DISPARITY_CENSUS_MEAN,
	CAMERA_DISPARITY_CENSUS_CENTER,		/* disparity mode default */
};

enum camera_flash_mode {
	CAMERA_FLASH_MODE_OFF = 0,
	CAMERA_FLASH_MODE_AUTO,
	CAMERA_FLASH_MODE_ON,
	CAMERA_FLASH_MODE_RED_EYE,
	CAMERA_FLASH_MODE_TORCH
};

enum camera_op_mode {
	CAMERA_OP_MODE_GED = 0,   // default
	CAMERA_OP_MODE_TW,
	CAMERA_OP_MODE_HAL3_GED,
	CAMERA_OP_MODE_HAL3_TW,
	CAMERA_OP_MODE_FAC,
	CAMERA_OP_MODE_HAL3_FAC,
	CAMERA_OP_MODE_HAL3_SDK,  // UNIHAL default
	CAMERA_OP_MODE_HAL3_CAMERAX,
	CAMERA_OP_MODE_HAL3_UNIHAL_VIP,
	CAMERA_OP_MODE_HAL3_SDK_VIP,
	CAMERA_OP_MODE_HAL3_ATTACH,
	CAMERA_OP_MODE_HAL3_UNIHAL_VIDEO,
};

enum camera2_sensor_hdr_mode {
	CAMERA_SENSOR_HDR_MODE_SINGLE,
	CAMERA_SENSOR_HDR_MODE_2HDR,
	CAMERA_SENSOR_HDR_MODE_3HDR,
	CAMERA_SENSOR_HDR_MODE_2AEB,
};

enum camera2_range_sensor_mode {
	CAMERA_RANGE_SENSOR_MODE_AUTO,
	CAMERA_RANGE_SENSOR_MODE_ON,
	CAMERA_RANGE_SENSOR_MODE_OFF,
};

struct camera2_is_mode_uctl {
	enum camera2_sensor_hdr_mode	sensor_hdr_mode;
	enum camera2_wdr_mode		wdr_mode;
	enum camera2_paf_mode		paf_mode;
	enum camera2_disparity_mode	disparity_mode;
	enum camera2_range_sensor_mode	range_sensor_mode;
};

struct camera2_pdaf_single_result {
	uint16_t	mode;
	uint16_t	goalPos;
	uint16_t	reliability;
	uint16_t	currentPos;
};

struct camera2_pdaf_multi_result {
	uint16_t	mode;
	uint16_t	goalPos;
	uint16_t	reliability;
	uint16_t	focused;
};

struct camera2_pdaf_udm {
	uint16_t				numCol;
	uint16_t				numRow;
	struct camera2_pdaf_multi_result	multiResult[CAMERA2_MAX_PDAF_MULTIROI_ROW][CAMERA2_MAX_PDAF_MULTIROI_COLUMN];
	struct camera2_pdaf_single_result	singleResult;
	float					singleResultPhaseDifference;
	uint16_t				lensPosResolution;
};

struct camera2_fd_uctl {
	enum facedetect_mode	faceDetectMode;
	uint32_t		faceIds[CAMERA2_MAX_FACES];
	uint32_t		faceLandmarks[CAMERA2_MAX_FACES][6];
	uint32_t		faceRectangles[CAMERA2_MAX_FACES][4];
	uint8_t			faceScores[CAMERA2_MAX_FACES];
	uint32_t		faces[CAMERA2_MAX_FACES];

	uint32_t		vendorSpecific[CAMERA2_MAX_UCTL_VENDER_LENGTH];
/* ---------------------------------------------------------
	vendorSpecific[0] = fdMapAddress[0];
	vendorSpecific[1] = fdMapAddress[1];
	vendorSpecific[2] = fdMapAddress[2];
	vendorSpecific[4] = fdMapAddress[4];
	vendorSpecific[5] = fdMapAddress[5];
	vendorSpecific[6] = fdMapAddress[6];
	vendorSpecific[7] = fdMapAddress[7];
	vendorSpecific[8] = fdYMapAddress;
	vendorSpecific[9] = fdCoefK;
	vendorSpecific[10] = fdUp;
	vendorSpecific[11] = fdShift;
	vendorSpecific[12] ~ vendorSpecific[31] : reserved
	---------------------------------------------------------
*/
};

struct camera2_fd_udm {
	uint32_t	faceCount;
};

struct camera2_ni_udm {
	uint32_t currentFrameNoiseIndex; /* Noise Index for N */
	uint32_t nextFrameNoiseIndex; /* Noise Index for N+1 */
	uint32_t nextNextFrameNoiseIndex; /* Noise Index for N+2 */
};

enum camera2_drc_mode {
	DRC_OFF = 1,
	DRC_ON,
};

struct camera2_drc_uctl {
	enum camera2_drc_mode uDrcEn;
};

enum camera2_dcp_process_mode {
	DCP_PROCESS_OFF = 0,
	DCP_PROCESS_ON,
};

struct camera2_tnr_uctl {
	uint32_t	referenceFrameNumber;
	uint32_t	candidateFrameNumber;

	uint32_t	numberOfCandidateFrames;
	uint32_t	minOfMergeFrames;

	uint32_t	reserved[8];
};

enum camera2_scene_index {
	SCENE_INDEX_OFF         	= -2,
	SCENE_INDEX_SCANNING    	= -1,
	SCENE_INDEX_INVALID     	= 0,
	SCENE_INDEX_FOOD        	= 1,
	SCENE_INDEX_TEXT        	= 2,
	SCENE_INDEX_PERSON      	= 3,
	SCENE_INDEX_FLOWER      	= 4,
	SCENE_INDEX_TREE        	= 5,
	SCENE_INDEX_MOUNTAIN    	= 6,
	SCENE_INDEX_MOUNTAIN_GREEN  	= 7,
	SCENE_INDEX_MOUNTAIN_FALL   	= 8,
	SCENE_INDEX_ANIMAL      	= 9,
	SCENE_INDEX_SUNSET_SUNRISE  	= 10,
	SCENE_INDEX_BEACH       	= 11,
	SCENE_INDEX_SKY         	= 12,
	SCENE_INDEX_SNOW        	= 13,
	SCENE_INDEX_NIGHTVIEW   	= 14,
	SCENE_INDEX_WATERFALL   	= 15,
	SCENE_INDEX_BIRD        	= 16,
	SCENE_INDEX_CITYSTREET  	= 17,
	SCENE_INDEX_HOMEINDOOR  	= 18,
	SCENE_INDEX_WATERSIDE   	= 19,
	SCENE_INDEX_SCENERY     	= 20,
	SCENE_INDEX_GREENERY    	= 21,
	SCENE_INDEX_BABY        	= 22,
	SCENE_INDEX_CAT         	= 23,
	SCENE_INDEX_DOG         	= 24,
	SCENE_INDEX_CLOTHING    	= 25,
	SCENE_INDEX_DRINK       	= 26,
	SCENE_INDEX_PEOPLE      	= 27,
	SCENE_INDEX_RESTAURANT_INDOOR 	= 28,
	SCENE_INDEX_STAGE       	= 29,
	SCENE_INDEX_VEHICLE     	= 30,
	SCENE_INDEX_TREE_GREEN  	= 31,
	SCENE_INDEX_SKY_BLUE    	= 32,
	SCENE_INDEX_SKY_GREY    	= 33,
	SCENE_INDEX_SKYSCRAPER  	= 34,
	SCENE_INDEX_CITY        	= 35,
	SCENE_INDEX_SHOE_DISP   	= 36,
	SCENE_INDEX_SHOE_ON     	= 37,
	SCENE_INDEX_FACE        	= 38,
	SCENE_INDEX_DAY_HDR     	= 10000, // The enums which are same as or more than 1000 are set by AE result
	SCENE_INDEX_NIGHT_HDR   	= 10001,
	SCENE_INDEX_MOTION_BLUR_REMOVAL = 10002
};

enum {
	RECT_IDX_LEFT,
	RECT_IDX_TOP,
	RECT_IDX_RIGHT,
	RECT_IDX_BOTTOM,
	RECT_IDX_MAX,
};

struct camera2_scene_detect_uctl
{
	uint64_t			timeStamp;
	enum camera2_scene_index	scene_index;
	uint32_t			confidence_score;
	uint32_t			object_roi[RECT_IDX_MAX];  /* left, top, right, bottom */
};

struct score_info
{
	uint32_t frameCount;
	uint32_t score;
	uint32_t motionIndex;
	uint32_t localMotionIndex;
	uint32_t gmeConfidence;
};

struct camera2_mfstill_uctl
{
	struct score_info sinfo[15];
	uint32_t ref_frameCount;
	uint32_t rej_frameCount[15];
};

enum camera_vt_mode {
	VT_MODE_OFF = 0,
	VT_MODE_1,   /* qcif ~ qvga */
	VT_MODE_2,   /* qvga ~ vga*/
	VT_MODE_3,   /* reserved : smart stay */
	VT_MODE_4,   /* vga ~ hd  */
	VT_MODE_5,   /* vga ~ hd 20Fps */
};

#define CAMERA2_MAX_ME_MV 200

struct camera2_me_udm {
	uint32_t	motion_vector[CAMERA2_MAX_ME_MV];	/* for (n-2)th frame */
	uint32_t	current_patch[CAMERA2_MAX_ME_MV];	/* for (n-1)th frame */
};

struct camera2_tnr_udm {
	uint32_t	bestTnrScore;
	uint32_t	mergeFrameNum;
	uint32_t	mergeFrameOrder[15];
	uint32_t	meConfidence;
	uint32_t	reserved[9];
};

struct camera2_frame_info_udm {
	uint32_t	sensor_binning[2];
	uint32_t	sensor_size[2];
	uint32_t	sensor_crop_region[4];
	uint32_t	taa_in_crop_region[4];
};

struct camera2_gmv_uctl {
	int16_t gmX;
	int16_t gmY;
};

enum camera_motion_state {
	CAMERA_MOTION_UNKNOWN = 0,
	CAMERA_MOTION_TRIPOD,
	CAMERA_MOTION_STATIONARY,
	CAMERA_MOTION_MOVING,
};

enum camera_client_index {
	CAMERA_APP_CATEGORY_NOT_READ           = -1,
	CAMERA_APP_CATEGORY_NONE               = 0,
	CAMERA_APP_CATEGORY_FACEBOOK           = 1,
	CAMERA_APP_CATEGORY_WECHAT             = 2,
	CAMERA_APP_CATEGORY_SNAPCHAT           = 3,
	CAMERA_APP_CATEGORY_TWITTER            = 4,
	CAMERA_APP_CATEGORY_INSTAGRAM          = 5,
	CAMERA_APP_CATEGORY_3P_VT              = 6,
	CAMERA_APP_CATEGORY_VAULT              = 7,
	CAMERA_APP_CATEGORY_FACEBOOK_MASSENGER = 8,
	CAMERA_APP_CATEGORY_WHATSAPP           = 9,
	CAMERA_APP_CATEGORY_ULIKE              = 10,
	CAMERA_APP_CATEGORY_WEIBO              = 11,
	CAMERA_APP_CATEGORY_MEITU              = 12,
	CAMERA_APP_CATEGORY_KAKAOBANK          = 13,
	CAMERA_APP_CATEGORY_CAMCARD            = 14,
	CAMERA_APP_CATEGORY_CAMCARD_FREE       = 15,
	CAMERA_APP_CATEGORY_SNOW               = 16,
	CAMERA_APP_CATEGORY_B612               = 17,
	CAMERA_APP_CATEGORY_SODA               = 18,
	CAMERA_APP_CATEGORY_FOODIE             = 19,
	CAMERA_APP_CATEGORY_ZOOM               = 20,
	CAMERA_APP_CATEGORY_TIKTOK             = 21,
    CAMERA_APP_CATEGORY_SMART_STAY         = 22,
    CAMERA_APP_CATEGORY_SABC               = 23,
	CAMERA_APP_CATEGORY_MAX
};

enum remosaic_oper_mode {
	REMOSAIC_OPER_MODE_NONE = 0,
	REMOSAIC_OPER_MODE_SINGLE = 1,
	REMOSAIC_OPER_MODE_MFHDR = 2,
	REMOSAIC_OPER_MODE_SINGLE_4_3H = 3,
};
enum camera_adaptive_lens_condition
{
    CAMERA_ADAPTIVE_LENS_UNAVAILABLE = 0,
    CAMERA_ADAPTIVE_LENS_UW_AUTO_AVAILABLE = 1,
    CAMERA_ADAPTIVE_LENS_CONDITION_MAX,
};

enum camera_adaptive_lens_mode_state
{
    CAMERA_ADAPTIVE_LENS_MODE_STATE_NONE = 0,
    CAMERA_ADAPTIVE_LENS_MODE_STATE_UW_AUTO_DONE = 1,
    CAMERA_ADAPTIVE_LENS_MODE_STATE_MAX,
};

struct camera2_meds_common_udm {
	uint32_t chainConfigChanged;
	uint16_t sensorCalibratedWidth;
	uint16_t sensorCalibratedHeight;
	uint16_t binningX;
	uint16_t binningY;
	uint16_t cropOffsetX;
	uint16_t cropOffsetY;
	uint16_t cropWidth;
	uint16_t cropHeight;
	uint16_t taaScScaleFactor;
};
struct camera2_orbds_udm {
	uint32_t yDsScaleFactor;
	uint16_t yDsOutputWidth;
	uint16_t yDsOutputHeight;
};
struct camera2_lmeds_udm {
	uint32_t yDsScaleFactor;
	uint16_t yDsOutputWidth;
	uint16_t yDsOutputHeight;
};

enum camera_flip_mode {
	CAM_FLIP_MODE_NORMAL = 0,
	CAM_FLIP_MODE_HORIZONTAL,
	CAM_FLIP_MODE_VERTICAL,
	CAM_FLIP_MODE_HORIZONTAL_VERTICAL,
	CAM_FLIP_MODE_MAX,
};
enum camera_external_lens_mask {
	CAMERA_EXTERNAL_LENS_NONE           = 0,
	CAMERA_EXTERNAL_LENS_DOF            = 1 << 0, /* bit 0 */
	CAMERA_EXTERNAL_LENS_ANAMORPHIC     = 1 << 1, /* bit 1 */
};

struct camera2_object_detect_uctl {
	uint16_t detected;
	uint16_t confidence_score;
	uint16_t object_roi[RECT_IDX_MAX]; /* RECT_IDX_LEFT, RECT_IDX_TOP, RECT_IDX_WIDTH, RECT_IDX_HEIGHT */
};

/** \brief
  User-defined control area.
  \remarks
  sensor, lens, flash category is empty value.
  It should be filled by a5 for SET_CAM_CONTROL command.
  Other category is filled already from host.
 */
struct camera2_uctl {
	/** \brief
	  Set sensor, lens, flash control for next frame.
	  \remarks
	  This flag can be combined.
	  [0 bit] lens
	  [1 bit] sensor
	  [2 bit] flash
	 */
	uint32_t			uUpdateBitMap;

	/** For debugging */
	uint32_t			uFrameNumber;

	struct camera2_aa_uctl		aaUd;
	struct camera2_lens_uctl	lensUd;
	struct camera2_sensor_uctl	sensorUd;
	struct camera2_flash_uctl	flashUd;
	struct camera2_scaler_uctl	scalerUd;
	struct camera2_is_mode_uctl	isModeUd;

	struct camera2_fd_uctl		fdUd;

	/** ispfw specific control(user-defined) of drc. */
	struct camera2_drc_uctl		drcUd;

	/** ispfw specific control(user-defined) of dcp. */
	struct camera2_tnr_uctl   tnrUd;
	struct camera2_scene_detect_uctl sceneDetectInfoUd;
	enum camera_vt_mode		vtMode;
	float				zoomRatio;
	enum camera_flash_mode		flashMode;
	enum camera_op_mode		opMode;
	uint32_t			statsRoi[4];
	enum aa_cameraMode		cameraMode;
	enum aa_sensorPlace		masterCamera;
	struct camera2_gmv_uctl		gmvUd;
	int32_t				productColorInfo;
	uint8_t				countryCode[4];
	enum camera_motion_state	motionState;
	enum camera_client_index	cameraClientIndex;
	uint32_t			remosaicOperMode;
	struct camera2_mfstill_uctl	mfInfoUd;
	struct camera2_object_detect_uctl moonDetectInfoUd;
	uint32_t			drcGridInfo[6];       // Used in HAL-DDK interface
	uint32_t			svHistInfo[6];        // Used in HAL-DDK interface
	enum camera_adaptive_lens_condition adaptiveLensCondition;
	enum camera_adaptive_lens_mode_state adaptiveLensModeState;
	struct camera2_object_detect_uctl sunDetectInfoUd;
	enum camera_flip_mode			sensorFlip;
	enum camera_external_lens_mask	externalLensType;
	uint32_t						textDetectionInfo;
	uint32_t						reserved[14];
};

struct camera2_udm {
	struct camera2_aa_udm		aa;
	struct camera2_lens_udm		lens;
	struct camera2_sensor_udm	sensor;
	struct camera2_ae_udm		ae;
	struct camera2_awb_udm		awb;
	struct camera2_af_udm		af;
	struct camera2_ipc_udm		ipc;
	struct camera2_rta_udm		rta;
	struct camera2_internal_udm	internal;
	struct camera2_scaler_udm	 scaler;
	struct camera2_pdaf_udm		pdaf;
	struct camera2_fd_udm		fd;
	struct camera2_me_udm		me;
	float				zoomRatio;
	struct camera2_ni_udm		ni;
	struct camera2_drc_udm		drc;
	struct camera2_rgbGamma_udm	rgbGamma;
	struct camera2_ccm_udm		ccm;
	enum aa_cameraMode		cameraMode;
	enum aa_sensorPlace		masterCamera;
	enum aa_fallback		fallback;
	uint8_t				frame_id[32];
	enum camera2_scene_index	scene_index;
	uint32_t			flicker_detect;
	uint32_t			reserved1[2];
	uint32_t			drcGridInfo[6];
	uint32_t			svHistInfo[6];
	uint32_t			reserved2[9];
	struct camera2_tnr_udm		tnr;
	uint32_t			motionIndex;
	uint32_t			localMotionIndex;
	struct camera2_meds_common_udm	meds_common;       // Used in HAL-DDK interface
	struct camera2_orbds_udm	orbds;                // Used in HAL-DDK interface
	struct camera2_lmeds_udm	lmeds;                // Used in HAL-DDK interface
	struct camera2_frame_info_udm frame_info;
	uint32_t			reserved[2];
};

struct camera2_shot {
	/*google standard area*/
	struct camera2_ctl	ctl;
	struct camera2_dm	dm;
	/*user defined area*/
	struct camera2_uctl	uctl;
	struct camera2_udm	udm;
	/*magic : 23456789*/
	uint32_t		magicNumber;
};

struct camera2_node_input {
	/**	\brief
	  intput crop region
	  \remarks
	  [0] x axis
	  [1] y axie
	  [2] width
	  [3] height
	 */
	uint32_t	cropRegion[4];
};

struct camera2_node_output {
	/**	\brief
	  output crop region
	  \remarks
	  [0] x axis
	  [1] y axie
	  [2] width
	  [3] height
	 */
	uint32_t	cropRegion[4];
};

enum camera_node_mode {
	CAMERA_NODE_NORMAL = 0,
	CAMERA_NODE_LOGICAL,
};

struct camera2_node {
	/**	\brief
	  video node id
	  \remarks
	  [x] video node id
	 */
	uint32_t			vid;

	/**	\brief
	  stream control
	  \remarks
	  [0] disable stream out
	  [1] enable stream out
	 */
	uint32_t			request;

	struct camera2_node_input	input;
	struct camera2_node_output	output;
	/*
	 * mode: 0 (nomal node), 1 (logical node)
	 * pixelformat : per-frame format
	 * pixelsize   : per-frame pixel_size
	 * width   :  per-frame width (for stride)
	 * height   : per-frame height (for stride)
	 * byteperline : bytes per line
	 * quantization : for wide/narrow color range
	 * flip : NORMAL or X-MIRROR or Y- MIRROR or XY- MIRROR
	 */
	uint32_t			mode;
	uint32_t			pixelformat;
	uint32_t			pixelsize;
	uint32_t			flags;
	uint32_t			width;
	uint32_t			height;
	uint32_t			bytesperline[VIDEO_MAX_PLANES];
	enum v4l2_quantization		quantization;
	uint32_t			flip;

	uint32_t			result;
	struct v4l2_buffer              buf;
};

struct camera2_node_group {
	/**	\brief
	  output device node
	  \remarks
	  this node can pull in image
	 */
	struct camera2_node		leader;

	/**	\brief
	  capture node list
	  \remarks
	  this node can get out image
	  3AAC, 3AAP, SCC, SCP, VDISC
	 */
	struct camera2_node		capture[CAPTURE_NODE_MAX];
};

typedef struct hfd_meta hfd_meta_t;
struct hfd_meta {
	uint32_t		hfd_enable;
	uint32_t		faceIds[CAMERA2_MAX_FACES];
	uint32_t		faceLandmarks[CAMERA2_MAX_FACES][6];
	uint32_t		faceRectangles[CAMERA2_MAX_FACES][4];
	uint32_t		score[CAMERA2_MAX_FACES];
	uint32_t		is_rot[CAMERA2_MAX_FACES];
	uint32_t		is_yaw[CAMERA2_MAX_FACES];
	uint32_t		rot[CAMERA2_MAX_FACES];
	uint32_t		mirror_x[CAMERA2_MAX_FACES];
	uint32_t		hw_rot_mirror[CAMERA2_MAX_FACES];
};

enum camera_thermal_mode {
	CAM_THERMAL_NORMAL = 0,
	CAM_THERMAL_THROTTLING,
	CAM_THERMAL_TRIPPING,
	CAM_THERMAL_MAX,
};

enum camera_crc_index {
	CAMERA_CRC_INDEX_DUAL = 0,
	CAMERA_CRC_INDEX_PDAF,
	CAMERA_CRC_INDEX_AWB,
	CAMERA_CRC_INDEX_AF,
	CAMERA_CRC_INDEX_MNF,
	CAMERA_CRC_INDEX_MAX,
};

enum sensor_gyro_info_state {
	SENSOR_GYRO_INFO_STATE_BASE = 0,
	SENSOR_GYRO_INFO_STATE_SUCCESS,
	SENSOR_GYRO_INFO_STATE_FAIL,
	SENSOR_GYRO_INFO_STATE_MAX,
};

struct facial_score {
	int32_t                 left_eye;
	int32_t                 right_eye;
	int32_t                 mouth;
	int32_t                 smile;
	int32_t                 left_blink;
	int32_t                 right_blink;
};

struct facial_angle {
	uint32_t                yaw;
	uint32_t                roll;
};

struct vra_ext_meta {
	struct facial_score     facialScore[CAMERA2_MAX_FACES];
	struct facial_angle     facialAngle[CAMERA2_MAX_FACES];
	uint32_t                reserved[10];
};

struct ddk_setfile_ver
{
        uint32_t        header1; // 0xF85A20B4
        uint32_t        header2; // 0xCA539ADF
        char            ddk_version[128];
        char            setfile_version[128];
};

struct sensor_gyro_info {
        int32_t x;
        int32_t y;
        int32_t z;
        enum sensor_gyro_info_state state;
};

struct camera2_shot_ext_user {
        int             crc_result[CAMERA_CRC_INDEX_MAX];
        int             focus_actual_pos;
        int             focus_target_pos;

        struct ddk_setfile_ver  ddk_version;

        struct sensor_gyro_info gyro_info;
};

/** \brief
  stream structure for scaler.
 */
struct camera2_stream {
	/**	\brief
	  this address for verifying conincidence of index and address
	  \remarks
	  [X] kernel virtual address for this buffer
	 */
	uint32_t		address;

	/**	\brief
	  this frame count is from FLITE through dm.request.fcount,
	  this count increases every frame end. initial value is 1.
	  \remarks
	  [X] frame count
	 */
	uint32_t		fcount;

	/**	\brief
	  this request count is from HAL through ctl.request.fcount,
	  this count is the unique.
	  \remarks
	  [X] request count
	 */
	uint32_t		rcount;

	/**	\brief
	  frame index of isp framemgr.
	  this value is for driver internal debugging
	  \remarks
	  [X] frame index
	 */
	uint32_t		findex;

	/**	\brief
	  frame validation of isp framemgr.
	  this value is for driver and HAL internal debugging
	  \remarks
	  [X] frame valid
	 */
	uint32_t		fvalid;

	/**	\brief
	  output crop region
	  this value mean the output image places the axis of  memory space
	  \remarks
	  [0] crop x axis
	  [1] crop y axis
	  [2] width
	  [3] height
	 */
	uint32_t		input_crop_region[4];
	uint32_t		output_crop_region[4];

	uint32_t		pixelformat;
	uint32_t		pixelsize;

	/**
	  stripe region horizontal pixel nums
	  this value indicates each width pixel size of stripe region
	 */
	uint32_t                stripe_region_num;
	uint32_t		stripe_h_pix_nums[CAMERA2_MAX_STRIPE_REGION_NUM];
};

/** \brief
  Structure for interfacing between HAL and driver.
 */
struct camera2_shot_ext {
	/*
	 * ---------------------------------------------------------------------
	 * HAL Control Part
	 * ---------------------------------------------------------------------
	 */

	struct camera2_stream		reserved_stream;

	/**	\brief
	  setfile change
	  \remarks
	  [x] mode for setfile
	 */
	uint32_t			setfile;

	/**	\brief
	  node group control
	  \remarks
	  per frame control
	 */
	struct camera2_node_group	node_group;

	/**	\brief
	  TNR processing mode control
	  \remarks
	  [0] normal mode
	  [1] merge mode
	  [2] TNR only mode
	  [3] merge with NR mode
	 */
	uint32_t			tnr_mode;

	/**	\brief
	  post processing control(FD)
	  \remarks
	  [0] bypass off
	  [1] bypass on
	 */
	uint32_t			fd_bypass;

	/*
	 * ---------------------------------------------------------------------
	 * DRV Control Part
	 * ---------------------------------------------------------------------
	 */

	/**	\brief
	  requested frames state.
	  driver return the information everytime
	  when dequeue is requested.
	  \remarks
	  [X] count
	 */
	uint32_t			free_cnt;
	uint32_t			request_cnt;
	uint32_t			process_cnt;
	uint32_t			complete_cnt;

	/**	\brief
	  shot validation
	  \remarks
	  [0] valid
	  [1] invalid
	 */
	uint32_t			invalid;

	struct hfd_meta			hfd;

	uint16_t			binning_ratio_x;
	uint16_t			binning_ratio_y;
	uint32_t			crop_taa_x;
	uint32_t			crop_taa_y;
	uint32_t			bds_ratio_x;
	uint32_t			bds_ratio_y;
	uint32_t			remosaic_rotation;

	enum camera_flip_mode           mcsc_flip[MCSC_PORT_MAX];
	enum camera_flip_mode           mcsc_flip_result[MCSC_PORT_MAX];

	struct vra_ext_meta             vra_ext;

	enum camera_thermal_mode        thermal;

	/* Use user vendor */
	struct camera2_shot_ext_user    user;

	/* reserved for future */
	uint32_t			reserved[7];

	/**	\brief
	  processing time debugging
	  \remarks
	  taken time(unit : struct timeval)
	  [0][x] CSIS start
	  [1][x] CSIS end
	  [2][x] DRV Shot
	  [3][x] DRV Shot done
	  [4][x] DRV Meta done
	 */
	uint32_t			timeZone[10][2];

	/*
	 * ---------------------------------------------------------------------
	 * Camera API
	 * ---------------------------------------------------------------------
	 */

	struct camera2_shot		shot;
};

#define CAM_LENS_CMD		(0x1 << 0x0)
#define CAM_SENSOR_CMD		(0x1 << 0x1)
#define CAM_FLASH_CMD		(0x1 << 0x2)

/* typedefs below are for firmware sources */

typedef enum metadata_mode metadata_mode_t;
typedef struct camera2_request_ctl camera2_request_ctl_t;
typedef struct camera2_request_dm camera2_request_dm_t;
typedef enum optical_stabilization_mode optical_stabilization_mode_t;
typedef enum lens_facing lens_facing_t;
typedef struct camera2_entry_ctl camera2_entry_ctl_t;
typedef struct camera2_entry_dm camera2_entry_dm_t;
typedef struct camera2_lens_ctl camera2_lens_ctl_t;
typedef struct camera2_lens_dm camera2_lens_dm_t;
typedef enum sensor_colorfilterarrangement sensor_colorfilterarrangement_t;
typedef enum sensor_lensshading_applied sensor_lensshading_applied_t;
typedef enum sensor_ref_illuminant sensor_ref_illuminant_t;
typedef struct camera2_sensor_ctl camera2_sensor_ctl_t;
typedef struct camera2_sensor_dm camera2_sensor_dm_t;
typedef enum flash_mode flash_mode_t;
typedef struct camera2_flash_ctl camera2_flash_ctl_t;
typedef struct camera2_flash_dm camera2_flash_dm_t;
typedef enum processing_mode processing_mode_t;
typedef struct camera2_hotpixel_ctl camera2_hotpixel_ctl_t;

typedef struct camera2_demosaic_ctl camera2_demosaic_ctl_t;
typedef struct camera2_noisereduction_ctl camera2_noisereduction_ctl_t;
typedef struct camera2_shading_ctl camera2_shading_ctl_t;
typedef enum colorcorrection_mode colorcorrection_mode_t;
typedef struct camera2_colorcorrection_ctl camera2_colorcorrection_ctl_t;
typedef struct camera2_colorcorrection_dm camera2_colorcorrection_dm_t;
typedef enum tonemap_mode tonemap_mode_t;
typedef enum tonemap_presetCurve tonemap_presetCurve_t;
typedef struct camera2_tonemap_ctl camera2_tonemap_ctl_t;

typedef struct camera2_edge_ctl camera2_edge_ctl_t;
typedef struct camera2_scaler_ctl camera2_scaler_ctl_t;
typedef struct camera2_scaler_dm camera2_scaler_dm_t;
typedef struct camera2_jpeg_ctl camera2_jpeg_ctl_t;
typedef enum facedetect_mode facedetect_mode_t;
typedef enum stats_mode stats_mode_t;
typedef struct camera2_stats_ctl camera2_stats_ctl_t;
typedef struct camera2_stats_dm camera2_stats_dm_t;
typedef enum aa_capture_intent aa_capture_intent_t;
typedef enum aa_mode aa_mode_t;
typedef enum aa_scene_mode aa_scene_mode_t;
typedef enum aa_effect_mode aa_effect_mode_t;
typedef enum aa_aemode aa_aemode_t;
typedef enum aa_ae_antibanding_mode aa_ae_antibanding_mode_t;
typedef enum aa_awbmode aa_awbmode_t;
typedef enum aa_afmode aa_afmode_t;
typedef enum aa_afstate aa_afstate_t;
typedef enum aa_ae_lockavailable aa_ae_lockavailable_t;
typedef enum aa_awb_lockavailable aa_awb_lockavailable_t;
typedef enum aa_available_mode aa_available_mode_t;
typedef struct camera2_aa_ctl camera2_aa_ctl_t;
typedef struct camera2_aa_dm camera2_aa_dm_t;
typedef struct camera2_ctl camera2_ctl_t;
typedef struct camera2_uctl camera2_uctl_t;
typedef struct camera2_dm camera2_dm_t;

typedef struct camera2_reprocess_ctl camera2_reprocess_ctl_t;
typedef enum depth_available_depth_stream_config depth_available_depth_stream_config_t;
typedef enum depth_depth_is_exclusive depth_depth_is_exclusive_t;

typedef struct camera2_scaler_uctl camera2_scaler_uctl_t;

typedef struct camera2_fd_uctl camera2_fd_uctl_t;
typedef struct camera2_fd_udm camera2_fd_udm_t;

typedef struct camera2_sensor_uctl camera2_sensor_uctl_t;
typedef struct camera2_sensor_udm camera2_sensor_udm_t;

typedef struct camera2_aa_uctl camera2_aa_uctl_t;
typedef struct camera2_aa_udm camera2_aa_udm_t;

typedef struct camera2_lens_uctl camera2_lens_uctl_t;
typedef struct camera2_lens_udm camera2_lens_udm_t;

typedef struct camera2_ae_udm camera2_ae_udm_t;
typedef struct camera2_awb_udm camera2_awb_udm_t;
typedef struct camera2_af_udm camera2_af_udm_t;
typedef struct camera2_ipc_udm camera2_ipc_udm_t;
typedef struct camera2_udm camera2_udm_t;

typedef struct camera2_rta_udm camera2_rta_udm_t;
typedef struct camera2_internal_udm camera2_internal_udm_t;

typedef struct camera2_flash_uctl camera2_flash_uctl_t;
typedef struct camera2_shot camera2_shot_t;

typedef struct camera2_ni_udm camera2_ni_udm_t;

typedef struct camera2_drc_udm      camera2_drc_udm_t;
typedef struct camera2_rgbGamma_udm camera2_rgbGamma_udm_t;
typedef struct camera2_ccm_udm      camera2_ccm_udm_t;

typedef struct camera2_me_udm camera2_me_udm_t;
#endif
