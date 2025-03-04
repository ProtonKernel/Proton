/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - EL2 module
 * Author: Junho Choi <junhosj.choi@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_EL2_H
#define __EXYNOS_EL2_H

/* EL2 crash buffer size for each CPU */
#define EL2_CRASH_BUFFER_SIZE			(0x2000)
#define EL2_LOG_BUFFER_CLEAR			(1)

/* Retry count for allocate EL2 crash buffer */
#define MAX_RETRY_COUNT				(8)

/* EL2 log dump name */
#define EL2_LOG_DSS_NAME			"log_harx"

/* EL2 dynamic level3 table buffer size */
#define EL2_LV3_TABLE_BUF_SIZE_4MB		(0x400000)
#define EL2_DYNAMIC_LV3_TABLE_BUF_256K		(0x40000)
#define EL2_LV3_BUF_MAX				(0x80)

/* EL2 Stage 2 enable check */
#define S2_BUF_SIZE				(30)

#ifndef __ASSEMBLY__

#endif	/* __ASSEMBLY__ */

#endif	/* __EXYNOS_EL2_H */
