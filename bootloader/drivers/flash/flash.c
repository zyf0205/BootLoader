#include "flash.h"
#include "crc32.h"
#include <string.h>

/* APP 区扇区列表 */
static const uint32_t s_app_sectors[] = {
    FLASH_Sector_1,
    FLASH_Sector_2,
    FLASH_Sector_3,
    FLASH_Sector_4,
    FLASH_Sector_5,
};

void Flash_Init(void)
{
    Crc32_Init();
}

/* ---- 擦除 APP 区 ---- */
flash_err_t Flash_EraseApp(void)
{
    FLASH_Unlock();

    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR |
                    FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR |
                    FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    for (uint32_t i = 0; i < sizeof(s_app_sectors) / sizeof(s_app_sectors[0]); i++)
    {
        if (FLASH_EraseSector(s_app_sectors[i], VoltageRange_3) != FLASH_COMPLETE)
        {
            FLASH_Lock();
            return FLASH_ERR_ERASE;
        }
    }

    FLASH_Lock();
    return FLASH_OK;
}

/* ---- 擦除元数据区 (Sector 6) ---- */
static flash_err_t Flash_EraseMeta(void)
{
    FLASH_Unlock();

    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR |
                    FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR |
                    FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    FLASH_Status status = FLASH_EraseSector(META_SECTOR, VoltageRange_3);

    FLASH_Lock();
    return (status == FLASH_COMPLETE) ? FLASH_OK : FLASH_ERR_ERASE;
}

/* ---- 写数据 + 回读验证 ---- */
flash_err_t Flash_Write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint32_t i;

    if (buf == NULL || len == 0)
        return FLASH_ERR_PARAM;

    /* 边界检查: 防止写穿到 Bootloader 或越出 APP 区 */
    if (addr < APP_ADDR || (addr + len) > APP_END_ADDR)
        return FLASH_ERR_PARAM;

    FLASH_Unlock();

    /* 逐字写入 (不足 4 字节的尾部用 0xFF 补齐, 与擦除态一致) */
    for (i = 0; i < len; i += 4)
    {
        uint32_t word = 0xFFFFFFFF;

        for (uint32_t j = 0; j < 4 && (i + j) < len; j++)
        {
            word &= ~(0xFFu << (j * 8));
            word |= (uint32_t)buf[i + j] << (j * 8);
        }

        if (FLASH_ProgramWord(addr + i, word) != FLASH_COMPLETE)
        {
            FLASH_Lock();
            return FLASH_ERR_WRITE;
        }
    }

    FLASH_Lock();

    /* 回读验证 */
    for (i = 0; i < len; i++)
    {
        if (*(volatile uint8_t *)(addr + i) != buf[i])
        {
            return FLASH_ERR_VERIFY;
        }
    }

    return FLASH_OK;
}

/* ---- APP 有效性检查 ---- */
uint8_t Flash_IsAppValid(void)
{
    fw_meta_t meta;
    uint32_t  msp;
    uint32_t  reset;

    /* 1. 元数据必须有效 */
    if (!Flash_LoadMeta(&meta))
        return 0;

    if (meta.size == 0 || meta.size > APP_SIZE)
        return 0;

    /* 2. MSP 必须指向 SRAM */
    msp = *(volatile uint32_t *)APP_ADDR;
    if (msp < SRAM_BASE_ADDR || msp > (SRAM_BASE_ADDR + SRAM_SIZE))
        return 0;

    /* 3. Reset 向量必须指向 APP 区 (Thumb 指令, 最低位为 1) */
    reset = *(volatile uint32_t *)(APP_ADDR + 4);
    if (reset < APP_ADDR || reset >= APP_END_ADDR || (reset & 1) == 0)
        return 0;

    /* 4. 启动前对照元数据校验完整固件 */
    if (Flash_CalcAppCrc32(meta.size) != meta.crc32)
        return 0;

    return 1;
}

/* ---- 计算 APP 区 CRC32 ---- */
uint32_t Flash_CalcAppCrc32(uint32_t size)
{
    if (size == 0 || size > APP_SIZE)
        return 0;

    Crc32_Reset();

    const uint32_t *p = (const uint32_t *)APP_ADDR;
    uint32_t word_cnt = size / 4;

    for (uint32_t i = 0; i < word_cnt; i++)
    {
        CRC->DR = p[i];
    }

    /* 尾部不足 4 字节: 高位补 0xFF */
    uint32_t remain = size % 4;
    if (remain > 0)
    {
        uint32_t last_word = 0xFFFFFFFF;
        const uint8_t *pb = (const uint8_t *)(APP_ADDR + word_cnt * 4);
        for (uint32_t j = 0; j < remain; j++)
        {
            last_word &= ~(0xFFu << (j * 8));
            last_word |= (uint32_t)pb[j] << (j * 8);
        }
        CRC->DR = last_word;
    }

    return CRC->DR;
}

/* ---- 使旧固件元数据失效 (擦除 APP 前调用) ---- */
flash_err_t Flash_InvalidateMeta(void)
{
    const fw_meta_t *stored = (const fw_meta_t *)META_ADDR;
    FLASH_Status status;

    if (stored->magic != META_MAGIC)
        return FLASH_OK;

    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR |
                    FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR |
                    FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
    status = FLASH_ProgramWord(META_ADDR, 0);
    FLASH_Lock();

    if (status != FLASH_COMPLETE || *(volatile uint32_t *)META_ADDR != 0)
        return FLASH_ERR_WRITE;
    return FLASH_OK;
}

/* ---- 保存元数据: 内容先写, magic 最后提交 ---- */
flash_err_t Flash_SaveMeta(const fw_meta_t *meta)
{
    const uint32_t *p;
    uint32_t i;

    if (meta == NULL)
        return FLASH_ERR_PARAM;

    if (Flash_EraseMeta() != FLASH_OK)
        return FLASH_ERR_ERASE;

    FLASH_Unlock();

    p = (const uint32_t *)meta;
    for (i = 1; i < sizeof(fw_meta_t) / 4; i++)
    {
        if (FLASH_ProgramWord(META_ADDR + i * 4, p[i]) != FLASH_COMPLETE)
        {
            FLASH_Lock();
            return FLASH_ERR_WRITE;
        }
    }

    if (FLASH_ProgramWord(META_ADDR, p[0]) != FLASH_COMPLETE)
    {
        FLASH_Lock();
        return FLASH_ERR_WRITE;
    }

    FLASH_Lock();

    if (memcmp((const void *)META_ADDR, meta, sizeof(fw_meta_t)) != 0)
        return FLASH_ERR_VERIFY;
    return FLASH_OK;
}

/* ---- 读取元数据 ---- */
uint8_t Flash_LoadMeta(fw_meta_t *meta)
{
    if (meta == NULL)
        return 0;

    const fw_meta_t *stored = (const fw_meta_t *)META_ADDR;

    if (stored->magic != META_MAGIC)
        return 0;

    memcpy(meta, stored, sizeof(fw_meta_t));
    return 1;
}
