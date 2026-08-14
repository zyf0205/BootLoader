#ifndef __FLASH_H
#define __FLASH_H

#include "stm32f4xx.h"
#include "flash_config.h"

/* ================================================================== *
 * 内部 Flash 驱动: APP 区擦写 + 固件元数据管理
 * ================================================================== */

typedef enum {
    FLASH_OK = 0,
    FLASH_ERR_ERASE,
    FLASH_ERR_WRITE,
    FLASH_ERR_VERIFY,
    FLASH_ERR_PARAM,
} flash_err_t;

/* 固件元数据 (16 字节, 存于 Sector 6) */
typedef struct {
    uint32_t magic;       /* META_MAGIC 表示有效 */
    uint32_t size;        /* 固件大小 (bytes) */
    uint32_t crc32;       /* 固件 CRC32 (MPEG-2) */
    uint16_t version;     /* 固件版本 (来自文件名解析) */
    uint16_t reserved;
} fw_meta_t;

void Flash_Init(void);

/* 擦除 APP 区 (Sector 1-5) */
flash_err_t Flash_EraseApp(void);

/* 写入数据 (4 字节对齐效率最高), 带写回读验证 */
flash_err_t Flash_Write(uint32_t addr, const uint8_t *buf, uint32_t len);

/* 检查 APP 是否有效: 元数据 + MSP + Reset 向量三重校验 */
uint8_t Flash_IsAppValid(void);

/* 计算 APP 区固件 CRC32 */
uint32_t Flash_CalcAppCrc32(uint32_t size);

/* 保存/读取元数据 */
flash_err_t Flash_SaveMeta(const fw_meta_t *meta);
uint8_t     Flash_LoadMeta(fw_meta_t *meta);

#endif /* __FLASH_H */
