#ifndef __FLASH_CONFIG_H
#define __FLASH_CONFIG_H

/* 主机测试版分区配置: 地址映射到一块模拟 Flash 缓冲区。
 * 数值布局与固件侧 bootloader/config/flash_config.h 一致:
 *   APP 起始 0, 240KB; Metadata 偏移 0x3C000 */
#include <stdint.h>

extern uint8_t sim_flash[];

#define APP_ADDR            ((uintptr_t)(sim_flash))
#define APP_SIZE            0x3C000u
#define APP_END_ADDR        (APP_ADDR + APP_SIZE)
#define META_ADDR           ((uintptr_t)(sim_flash + 0x3C000u))
#define META_SECTOR         FLASH_Sector_6
#define META_MAGIC          0x4D455441

#define SRAM_BASE_ADDR      0x20000000u
#define SRAM_SIZE           0x20000u

#endif /* __FLASH_CONFIG_H */
