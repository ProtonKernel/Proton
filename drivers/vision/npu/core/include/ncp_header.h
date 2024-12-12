
/* Get NCP_VERSION value from platform specific configuration */
#include "npu-config.h"

#if (NCP_VERSION == 9)
#include "ncp_header_v9.h"
#elif (NCP_VERSION == 21)
#include "ncp_header_v21.h"
#elif (NCP_VERSION == 23)
#include "ncp_header_v23.h"
#elif (NCP_VERSION == 24)
#include "ncp_header_v24.h"
#else
#error NCP_VERSION is not defined or not supported
#endif
