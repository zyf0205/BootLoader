#ifndef __FLASH_CONFIG_H
#define __FLASH_CONFIG_H

#include "boot_api.h"

/* ================================================================== *
 * APP 分区配置
 * APP 区定义以 modules/bootapi/boot_api.h 契约为准,
 * 本文件提供工程内使用的别名。
 * ================================================================== */

#define APP_ADDR   BOOT_APP_BASE_ADDR   /* 0x08004000, 与链接脚本一致 */
#define APP_SIZE   BOOT_APP_MAX_SIZE    /* 240KB */

#endif /* __FLASH_CONFIG_H */
