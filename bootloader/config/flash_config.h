#ifndef __FLASH_CONFIG_H
#define __FLASH_CONFIG_H

/* ================================================================== *
 * Flash 分区表 - STM32F411CEU6 (512KB Flash / 128KB SRAM)
 *
 *  +----------------+----------+----------+----------------------+
 *  | 区域           | 地址      | 大小     | 用途                 |
 *  +----------------+----------+----------+----------------------+
 *  | Bootloader     | 0x08000000|  16KB    | Sector 0, 只读      |
 *  | APP            | 0x08004000| 240KB    | Sector 1-5, 可擦写  |
 *  | Metadata       | 0x08040000| 128KB    | Sector 6, 只存前16B  |
 *  | Reserved       | 0x08060000| 128KB    | Sector 7, 预留      |
 *  +----------------+----------+----------+----------------------+
 *
 * 注意: APP 分区地址与 modules/bootapi/boot_api.h 契约保持一致,
 *       修改分区时两个工程必须同步更新链接脚本。
 * ================================================================== */

#include "stm32f4xx.h"
#include "boot_api.h"

/* ---- SRAM 范围 (用于 MSP 校验) ---- */
#define SRAM_BASE_ADDR      0x20000000
#define SRAM_SIZE           0x20000     /* 128KB */

/* ---- APP (Sector 1-5, 以 boot_api.h 契约为准) ---- */
#define APP_ADDR            BOOT_APP_BASE_ADDR
#define APP_SIZE            BOOT_APP_MAX_SIZE
#define APP_END_ADDR        (APP_ADDR + APP_SIZE)

/* ---- Metadata (Sector 6) ---- */
#define META_ADDR           0x08040000
#define META_SECTOR         FLASH_Sector_6
#define META_MAGIC          0x4D455441  /* "META" 小端 */

/* ---- 串口接收缓冲 ---- */
#define USART_RX_BUF_SIZE   2048        /* DMA 环形缓冲, 需 >= 1024 + 余量 */
#define USART_TX_BUF_SIZE   256         /* TX DMA 缓冲 */

#endif /* __FLASH_CONFIG_H */
