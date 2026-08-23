#ifndef __FLASH_MOCK_H
#define __FLASH_MOCK_H

/* 主机测试版 flash.h: API/枚举/结构体与固件侧完全一致,
 * 实现在 test_updater.c 中 (模拟 Flash, 记录操作顺序) */
#include <stdint.h>
#include "flash_config.h"

typedef enum {
    FLASH_OK = 0,
    FLASH_ERR_ERASE,
    FLASH_ERR_WRITE,
    FLASH_ERR_VERIFY,
    FLASH_ERR_PARAM,
} flash_err_t;

typedef struct {
    uint32_t magic;
    uint32_t size;
    uint32_t crc32;
    uint16_t version;
    uint16_t reserved;
} fw_meta_t;

void Flash_Init(void);
flash_err_t Flash_EraseApp(void);
flash_err_t Flash_Write(uint32_t addr, const uint8_t *buf, uint32_t len);
uint8_t Flash_IsAppValid(void);
uint32_t Flash_CalcAppCrc32(uint32_t size);
flash_err_t Flash_InvalidateMeta(void);
flash_err_t Flash_SaveMeta(const fw_meta_t *meta);
uint8_t Flash_LoadMeta(fw_meta_t *meta);

#endif
