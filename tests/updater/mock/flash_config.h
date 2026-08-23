#ifndef __FLASH_CONFIG_MOCK_H
#define __FLASH_CONFIG_MOCK_H

/* 主机测试版分区配置: 数值与固件侧 bootloader/config/flash_config.h 保持一致 */
#include <stdint.h>

#define SRAM_BASE_ADDR      0x20000000
#define SRAM_SIZE           0x20000

#define APP_ADDR            0x08004000UL
#define APP_SIZE            0x3C000UL
#define APP_END_ADDR        (APP_ADDR + APP_SIZE)

#define META_ADDR           0x08040000
#define META_MAGIC          0x4D455441

#endif
