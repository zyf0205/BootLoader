#include "crc32.h"

/* ================================================================== *
 * 硬件 CRC32 薄封装
 * STM32F4 内置 CRC 计算单元, 固定多项式 0x04C11DB7:
 * 数据按 32-bit 字写入 DR 寄存器, 硬件自动迭代, 无需软件参与,
 * 比 CPU 逐位/查表计算快得多。算法规格见 crc32.h。
 * ================================================================== */

void Crc32_Init(void)
{
    /* 打开外设时钟后复位, 使累计值回到初始值 0xFFFFFFFF */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_CRC, ENABLE);
    CRC->CR = CRC_CR_RESET;
}

void Crc32_Reset(void)
{
    /* 置位 RESET 后硬件自动清零该位并重置累计值 */
    CRC->CR = CRC_CR_RESET;
}


