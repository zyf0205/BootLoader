#ifndef __STM32F4XX_MOCK_H
#define __STM32F4XX_MOCK_H

/* 主机测试桩: 仅提供 flash.c 所需的 StdPeriph Flash 接口与类型 */
#include <stdint.h>

typedef enum {
    FLASH_BUSY = 1,
    FLASH_ERROR_PGS,
    FLASH_ERROR_PGP,
    FLASH_ERROR_PGA,
    FLASH_ERROR_WRP,
    FLASH_ERROR_OPERATION,
    FLASH_COMPLETE
} FLASH_Status;

typedef enum {
    VoltageRange_1 = 0x00,
    VoltageRange_2 = 0x01,
    VoltageRange_3 = 0x02,
    VoltageRange_4 = 0x03,
} VoltageRange_TypeDef;

#define FLASH_Sector_0  0
#define FLASH_Sector_1  1
#define FLASH_Sector_2  2
#define FLASH_Sector_3  3
#define FLASH_Sector_4  4
#define FLASH_Sector_5  5
#define FLASH_Sector_6  6
#define FLASH_Sector_7  7

#define FLASH_FLAG_EOP      0x00000001u
#define FLASH_FLAG_OPERR    0x00000002u
#define FLASH_FLAG_WRPERR   0x00000010u
#define FLASH_FLAG_PGAERR   0x00000020u
#define FLASH_FLAG_PGPERR   0x00000040u
#define FLASH_FLAG_PGSERR   0x00000080u

void FLASH_Unlock(void);
void FLASH_Lock(void);
void FLASH_ClearFlag(uint32_t flags);
FLASH_Status FLASH_EraseSector(uint32_t sector, VoltageRange_TypeDef voltage);
FLASH_Status FLASH_ProgramWord(uintptr_t addr, uint32_t data);

#endif /* __STM32F4XX_MOCK_H */
