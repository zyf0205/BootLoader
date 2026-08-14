#ifndef __CRC32_H
#define __CRC32_H

#include "stm32f4xx.h"

/* ================================================================== *
 * 硬件 CRC32 (STM32F4 CRC 外设)
 * 算法: CRC-32/MPEG-2
 *   多项式: 0x04C11DB7, 初始值: 0xFFFFFFFF
 *   无输入/输出反转, 逐 32-bit 字处理 (小端)
 * ================================================================== */

void Crc32_Init(void);
void Crc32_Reset(void);

/* 逐字计算 (len 为 32-bit 字个数) */
uint32_t Crc32_CalcWords(const uint32_t *data, uint32_t word_cnt);

/* 按字节计算, 尾部不足 4 字节用 0xFF 补齐 */
uint32_t Crc32_CalcBytes(const uint8_t *data, uint32_t len);

#endif /* __CRC32_H */
