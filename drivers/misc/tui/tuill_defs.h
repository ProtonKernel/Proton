/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * TUI LL device driver definitions header
 * This copy is for tuihwll driver
 *
 * There are two other copies of this file in
 * /TuiService/tui_service_jni/jni/tuill_defs.h and
 * /tzsl/src/tuill/common/tuill_defs.h
 */

#pragma once

#include <linux/types.h>

typedef void		*void_ptr;  /**< a pointer to anything. */
typedef void_ptr	addr_t;     /**< an address, can be physical or virtual */
#define containerof(ptr, type, member) \
	((type *)((addr_t)(ptr) - offsetof(type, member)))

#define USEC_IN_MSEC  1000
#define MSEC_IN_SEC   1000
#define NSEC_IN_MSEC  1000000

#define OS_SWD_SOCKET_NAME  "tuill_swd_server"
#define OS_IWD_SOCKET_NAME  "tuill_iwd_server"
#define TOUCH_DRIVER_NAME   "tuill_touchdrv"
#define DISPLAY_DRIVER_NAME "tuill_dispdrv"
#define OS_DRIVER_NAME      "tuill_osdrv"

#define TUILLD_OSDRV_TA_NAME    "/dev/"OS_DRIVER_NAME
#define TUILLD_DISPDRV_TA_NAME  "/dev/"DISPLAY_DRIVER_NAME
#define TUILLD_TOUCHDRV_TA_NAME "/dev/"TOUCH_DRIVER_NAME

#define TUILL_SERVER_TEMPLATE "/service/%s"

#define TUILL_API_VERSION  1

#define MAX_DISPLAY 1

//--- Internal TUI LL spec types definition ---------------------------------
//these three constants weren't defined in TUI LL spec
//and shouldn't be visible for user, but we need them
#define TEE_PERIPHERAL_DISPLAY 0xABABABAB
#define TEE_PERIPHERAL_SERVICE 0xABABABAC
#define TEE_PERIPHERAL_TUIHW   0xABABABAD

#define TUILL_INVALID_HANDLE_VALUE (-1)
#define TUILL_OPENED_HANDLE_VALUE    1

typedef uint32_t tuill_handle_t;

struct __TEE_EventQueueHandle {
	tuill_handle_t hndl;
};

struct __TEE_EventSourceHandle {
	tuill_handle_t hndl;
};

struct __TEE_PeripheralHandle {
	tuill_handle_t hndl;
};

//--- Configuration enums definition ----------------------------------------
enum TUILL_IO_SOCKET {
	TUILLDRV_SWD_SOCK,
	TUILLDRV_IWD_SOCK,
	TUILLDRV_SOCK_MAX
};

enum tuill_swd_components {
	//enum order must be the same as in tuill_drivers
	TUILL_TOUCH,
	TUILL_DISPLAY,
	TUILL_SWD_MAX,
};

enum tuill_iwd_components {
	TUILL_TUIHW,
	TUILL_SERVICE,
	TUILL_IWD_MAX,
};

enum tuill_drivers {
	//used as TEE_PeripheralId
	TUILL_OS_DRV,
	//enum order must be the same as in tuill_swd_components
	TUILL_TOUCH_DRV,
	TUILL_DISPLAY_DRV,
	TUILL_DRV_MAX,
};

typedef struct tuilldrv_touch_data {
	uint32_t display;
	uint32_t action;
	uint32_t finger;
	uint32_t pressure;
	uint32_t x;
	uint32_t y;
} __attribute__((packed, aligned(4))) tuilldrv_touch_data_t;

typedef struct tuilldrv_tee_data {
	uint32_t event;
} __attribute__((packed, aligned(4))) tuilldrv_tee_data_t;

typedef struct tuilldrv_send_event_data {
	uint32_t periph_id;
	uint32_t event_type;
	uint32_t ret_code;
	union {
		tuilldrv_touch_data_t touch;
		tuilldrv_tee_data_t   tee;
	} u;
} __attribute__((packed, aligned(4))) tuilldrv_send_event_data_t;

//--- Socket commands and structures definition -----------------------------
//must match enum TUIInternalCommand in TUICmdWrapper.java
enum tuill_internal_commands {
	TUILL_ICMD_PING,
	//peripheral API
	TUILL_ICMD_GET_PERIPHERAL_LIST,
	TUILL_ICMD_CLOSE_PERIPHERAL, //single and multiple
	TUILL_ICMD_OPEN_PERIPHERAL,  //single and multiple
	//event API
	TUILL_ICMD_ADD_SOURCES,
	TUILL_ICMD_CANCEL_SOURCES,
	TUILL_ICMD_CLOSE_CLIENT_QUEUE,
	TUILL_ICMD_DROP_SOURCES,
	TUILL_ICMD_LIST_SOURCES,
	TUILL_ICMD_OPEN_CLIENT_QUEUE,
	TUILL_ICMD_WAIT_EVENT,
	//tui API
	TUILL_ICMD_GET_DISPLAY_INFO,
	TUILL_ICMD_TUI_INIT_SESSION_LOW,
	TUILL_ICMD_TUI_CLOSE_SESSION,
	TUILL_ICMD_BLIT_DISPLAY_SURFACE,
	//internal API
	TUILL_ICMD_SET_DRV_STATE,
	TUILL_ICMD_TOUCH_EVENT,
	TUILL_ICMD_OPEN_DRIVER,
	TUILL_ICMD_CLOSE_DRIVER,
	TUILL_ICMD_REBOOT_PHONE,
	TUILL_ICMD_CANCEL_TUI,
	TUILL_ICMD_DRIVER_CLOSED,

#ifdef BUILD_TYPE_debug
	//os_drv debug commands
	TUILL_ICMD_SEND_EVENT,
	TUILL_ICMD_REBOOT,
#endif //BUILD_TYPE_debug

	TUILL_ICMD_MAX,
};

#define RESPONSE_FLAG     0x80000000
#define INJECT_ERR_FLAG   0x40000000 //for debugging, injects error code
#define MAKE_TIMEOUT_FLAG 0x20000000 //for debugging, makes timeout error

typedef struct FB_Data {
	//we need to return this data in case if we opened display
	uint32_t width;
	uint32_t height;
	uint64_t fb_physical;
	uint64_t fb_virtual;
	uint64_t fb_size;
	uint64_t wb_physical;
	uint64_t wb_virtual;
	uint64_t wb_size;
	uint64_t disp_physical;
	uint64_t disp_size;
	uint32_t touch_type;
} __attribute__((packed, aligned(4))) FB_Data_t;

typedef struct OpenPeripheral_cmd {
	uint32_t num;
	uint32_t peripheral_id[TUILL_DRV_MAX];
	uint32_t flags;
	FB_Data_t FB;
} __attribute__((packed, aligned(4))) OpenPeripheral_cmd_t;

typedef struct OpenPeripheral_rsp {
	FB_Data_t FB;
} __attribute__((packed, aligned(4))) OpenPeripheral_rsp_t;

typedef struct ClosePeripheral_cmd {
	uint32_t num;
	uint32_t peripheral_id[TUILL_DRV_MAX];
} __attribute__((packed, aligned(4))) ClosePeripheral_cmd_t;

typedef OpenPeripheral_cmd_t  OpenDrivers_cmd_t;
typedef OpenPeripheral_rsp_t  OpenDrivers_rsp_t;
typedef ClosePeripheral_cmd_t CloseDrivers_cmd_t;

#define TEE_MAX_EVENT_PAYLOAD_SIZE 32
#define TEE_MAX_EVENT_NUMBER       4

typedef struct {
	uint32_t       eventType;
	uint64_t       timestamp;
	tuill_handle_t event_handle;
	uint8_t        payload[TEE_MAX_EVENT_PAYLOAD_SIZE];
} __TEE_Event_V1;

typedef struct {
	uint32_t version;
	uint32_t peripheral_id;
	union {
		__TEE_Event_V1 v1;
	} u;
} __TEE_Event;

//event commands start
typedef struct AddSources_cmd {
	tuill_handle_t queue_handle;
	uint32_t       num_sources;
	tuill_handle_t sources[TUILL_DRV_MAX];
} __attribute__((packed, aligned(4))) AddSources_cmd_t;

typedef struct CancelSources_cmd {
	tuill_handle_t queue_handle;
	uint32_t       num_sources;
	tuill_handle_t sources[TUILL_DRV_MAX];
} __attribute__((packed, aligned(4))) CancelSources_cmd_t;

typedef struct CloseQueue_cmd {
	tuill_handle_t queue_handle;
} __attribute__((packed, aligned(4))) CloseQueue_cmd_t;

typedef struct DropSources_cmd {
	tuill_handle_t queue_handle;
	uint32_t       num_sources;
	tuill_handle_t sources[TUILL_DRV_MAX];
} __attribute__((packed, aligned(4))) DropSources_cmd_t;

typedef struct ListSources_cmd {
	tuill_handle_t queue_handle;
	uint32_t       num_sources;
} __attribute__((packed, aligned(4))) ListSources_cmd_t;

typedef struct ListSources_rsp {
	uint32_t       num_sources;
	tuill_handle_t sources[TUILL_DRV_MAX];
} __attribute__((packed, aligned(4))) ListSources_rsp_t;

typedef struct OpenQueue_cmd {
	uint32_t       timeout_ms;
	uint32_t       num_sources;
	tuill_handle_t sources[TUILL_DRV_MAX];
} __attribute__((packed, aligned(4))) OpenQueue_cmd_t;

typedef struct OpenQueue_rsp {
	tuill_handle_t queue_handle;
} __attribute__((packed, aligned(4))) OpenQueue_rsp_t;

typedef struct WaitEvent_cmd {
	tuill_handle_t queue_handle;
	uint32_t       num_events;
	uint32_t       timeout_ms;
} __attribute__((packed, aligned(4))) WaitEvent_cmd_t;

typedef struct WaitEvent_rsp {
	uint32_t num_events;
	uint32_t dropped;
	__TEE_Event events[TEE_MAX_EVENT_NUMBER];
} __attribute__((packed, aligned(4))) WaitEvent_rsp_t;

typedef struct TouchEvent_cmd {
	uint32_t display;
	uint32_t action;
	uint32_t finger;
	uint32_t pressure;
	uint32_t x;
	uint32_t y;
} __attribute__((packed, aligned(4))) TouchEvent_cmd_t;

typedef struct OsEvent_cmd {
	uint32_t periph_id;
	uint32_t event_type;
	uint32_t event;
} __attribute__((packed, aligned(4))) OsEvent_cmd_t;

//event commands end

typedef struct SetDrvInfo_cmd {
	uint32_t drv_tui_mode;
	uint32_t index; //from tuill_iwd_components
} __attribute__((packed, aligned(4))) SetDrvInfo_cmd_t;

typedef struct GetDisplayInfo_cmd {
	uint32_t version;
	uint32_t displayNumber;
} __attribute__((packed, aligned(4))) GetDisplayInfo_cmd_t;

typedef struct GetDisplayInfo_rsp {
	uint32_t physical_width;
	uint32_t physical_height;
	uint32_t pixel_width;
	uint32_t pixel_height;
	uint32_t bit_depth;
	uint32_t flags;
	uint32_t num_periph;
	uint32_t associatedPeripherals[TUILL_DRV_MAX];
} __attribute__((packed, aligned(4))) GetDisplayInfo_rsp_t;

typedef struct peripheral_info {
	uint32_t type;
	uint32_t id;
} __attribute__((packed, aligned(4))) peripheral_info_t;

typedef struct GetPeripheralList_rsp {
	uint32_t num;
	peripheral_info_t list[TUILL_DRV_MAX];
} __attribute__((packed, aligned(4))) GetPeripheralList_rsp_t;

typedef struct InitTUISession_cmd {
	uint32_t flags;
	uint32_t timeout_ms;
	uint32_t peripheral_id[TUILL_DRV_MAX];
	uint32_t num;
} __attribute__((packed, aligned(4))) InitTUISession_cmd_t;

typedef struct CancelTUI_cmd {
	uint32_t event;
} __attribute__((packed, aligned(4))) CancelTUI_cmd_t;

typedef struct tuill_internal_command {
	uint32_t cmd;
	uint32_t ret_code;
	uint32_t version;
	uint32_t task_state; //duplicates field in long_cmd_t
	uint32_t task_id;    //duplicates field in long_cmd_t
	union {
		//event commands start
		AddSources_cmd_t    AddSources_cmd;
		CancelSources_cmd_t CancelSources_cmd;
		CloseQueue_cmd_t    CloseQueue_cmd;
		DropSources_cmd_t   DropSources_cmd;
		ListSources_cmd_t   ListSources_cmd;
		ListSources_rsp_t   ListSources_rsp;
		OpenQueue_cmd_t     OpenQueue_cmd;
		OpenQueue_rsp_t     OpenQueue_rsp;
		WaitEvent_cmd_t     WaitEvent_cmd;
		WaitEvent_rsp_t     WaitEvent_rsp;
		TouchEvent_cmd_t    TouchEvent_cmd;
		OsEvent_cmd_t       OsEvent_cmd;
		//event commands end
		//peripheral commands start
		GetPeripheralList_rsp_t GetPeripheralList_rsp;
		OpenPeripheral_cmd_t    OpenPeripheral_cmd;
		OpenPeripheral_rsp_t    OpenPeripheral_rsp;
		ClosePeripheral_cmd_t   ClosePeripheral_cmd;
		//peripheral commands end
		//tui commands start
		GetDisplayInfo_cmd_t    GetDisplayInfo_cmd;
		GetDisplayInfo_rsp_t    GetDisplayInfo_rsp;
		InitTUISession_cmd_t    InitTUISession_cmd;
		//tui commands end
		//other commands start
		SetDrvInfo_cmd_t        SetDrvInfo_cmd;
		OpenDrivers_cmd_t       OpenDrivers_cmd;
		OpenDrivers_rsp_t       OpenDrivers_rsp;
		CloseDrivers_cmd_t      CloseDrivers_cmd;
		CancelTUI_cmd_t         CancelTUI_cmd;
		//other commands end
	};
} __attribute__((packed, aligned(4))) tuill_internal_command_t;

//--- Engine structures definition ------------------------------------------

typedef struct tuill_buffer {
	int32_t data_len;
	char    data[sizeof(tuill_internal_command_t)];
} __attribute__((packed, aligned(4))) tuill_buffer_t;

typedef int32_t (*receive_t)(int32_t fd, tuill_buffer_t *data);
typedef void (*process_t)(int32_t fd);

typedef struct tuill_callback {
	receive_t process_input;
	process_t process_hangup;
	process_t process_handhake;
} tuill_callback_t;

typedef enum TUILL_CLIENT_TYPE {
	TUILLDRV_CLIENT_SWD_DRV,
	TUILLDRV_CLIENT_SWD_CLIENT,
	TUILLDRV_CLIENT_IWD_CLIENT,
} TUILL_CLIENT_TYPE_t;

//typedef struct tuill_socket_params {
//	int32_t sfd; //socket fd
//	int32_t efd; //epoll fd
//} tuill_socket_params_t;

//common drv information
typedef struct tuill_drv_ctx {
	uint32_t index;          //in drv_arr
	uint32_t peripheral_type;//GP enum values
	uint32_t peripheral_id;  //internal enum values
	uint32_t tuill_state;    //TEE_PERIPHERAL_STATE_FLAGS
} tuill_drv_ctx_t;

//----- server side contexts for connected entities -------
typedef struct tuill_drv_entity_ctx {
	tuill_drv_ctx_t drv_ctx;//shows that driver is connected to system
} tuill_drv_entity_ctx_t;

//used to indicate opened state for drivers
typedef struct tuill_opened {
	bool  tui; //opened by TEE_TUI_InitSessionLow
	bool  interrupted; //message about this driver interrupt was received
	tuill_handle_t periph_hndl; //indicates session is opened
	uint32_t peripheral_type;
	uint32_t peripheral_id;
} tuill_opened_t;

enum task_type {
	TASK_NONE,
	TASK_OPEN,
	TASK_CLOSE,
	TASK_DISPLAYINFO,
	TASK_CLOSE_NWD
};

enum task_cmd_state {
	TASK_CMD_VOID,
	TASK_CMD_NEEDED,
	TASK_CMD_SENT,
	TASK_CMD_DONE,
	TASK_CMD_ERROR,
	TASK_CMD_UNDO_SENT,
	TASK_CMD_UNDONE,
};

enum task_state {
	TASK_STATE_VOID,
	TASK_STATE_DOING,
	TASK_STATE_UNDOING,
};



typedef struct tuill_iwd_ctx {
	uint32_t index; //from tuill_iwd_components
	uint32_t tui_mode;
} tuill_iwd_entity_ctx_t;
//---------------------------------------------------------
