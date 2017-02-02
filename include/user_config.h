#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#include "os_type.h"

#define PLATFORM_DEBUG 1

struct __attribute__((packed, aligned(4))) saved_param {
	char StationSSID[32];
	char StationPwd[64];
	char SoftAPSSID[32];
	char SoftAPPwd[64];
	uint16_t dummy;  // force struct size to be a multiple of 4
	uint16_t crc;
};

#define PARAM_SECTOR	0x7A

#define WIFI_APSSID	"NewTS"
#define WIFI_APPASSWORD	"12345678"
#define WIFI_STA_MAX_CONN_RETRY	15

#define RJ45_YELLOW 2
#define RJ45_YELLOW_MUX PERIPHS_IO_MUX_GPIO2_U
#define RJ45_YELLOW_FUNC FUNC_GPIO2

#define RS485_DIR 4
#define RS485_DIR_MUX PERIPHS_IO_MUX_GPIO4_U
#define RS485_DIR_FUNC FUNC_GPIO4

#define RJ45_GREEN 5
#define RJ45_GREEN_MUX PERIPHS_IO_MUX_GPIO5_U
#define RJ45_GREEN_FUNC FUNC_GPIO5


#endif
