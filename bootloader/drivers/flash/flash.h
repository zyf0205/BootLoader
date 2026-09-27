#ifndef __FLASH_H
#define __FLASH_H

#include "stm32f4xx.h"
#include "flash_config.h"

/* ================================================================== *
 * 内部 Flash 驱动: APP 区擦写 + 固件元数据管理
 * ================================================================== */

/* Flash 操作结果 (驱动层领域错误码) */
typedef enum {
    FLASH_OK = 0,        /* 成功 */
    FLASH_ERR_ERASE,     /* 扇区擦除失败 */
    FLASH_ERR_WRITE,     /* 编程 (写入) 失败 */
    FLASH_ERR_VERIFY,    /* 写入后回读校验不一致 */
    FLASH_ERR_PARAM,     /* 参数非法 (空指针 / 地址越界) */
} flash_err_t;

/* 固件元数据 (16 字节, 存于 Sector 6; 字段全部 4 字节对齐便于按字编程) */
typedef struct {
    uint32_t magic;       /* META_MAGIC 表示有效 (最后写入, 见 Flash_SaveMeta) */
    uint32_t size;        /* 固件大小 (bytes) */
    uint32_t crc32;       /* 固件 CRC32 (MPEG-2) */
    uint16_t version;     /* 固件版本 (来自文件名解析), 0 = 未知 */
    uint16_t reserved;    /* 保留对齐字段, 恒为 0 */
} fw_meta_t;

void Flash_Init(void);

/* 擦除 APP 区 (Sector 1-5) */
flash_err_t Flash_EraseApp(void);

/* 写入数据 (4 字节对齐效率最高), 带写回读验证 */
flash_err_t Flash_Write(uint32_t addr, const uint8_t *buf, uint32_t len);

/* 检查 APP 是否有效: 元数据 + 向量表 + CRC32 */
uint8_t Flash_IsAppValid(void);

/* 计算 APP 区固件 CRC32 */
uint32_t Flash_CalcAppCrc32(uint32_t size);

/* 保存/读取元数据 */
flash_err_t Flash_InvalidateMeta(void);
flash_err_t Flash_SaveMeta(const fw_meta_t *meta);
uint8_t     Flash_LoadMeta(fw_meta_t *meta);

#endif /* __FLASH_H */
