#ifndef __CRC32_H
#define __CRC32_H

#include "stm32f4xx.h"

/* ================================================================== *
 * 硬件 CRC32 (STM32F4 CRC 外设)
 * 算法: CRC-32/MPEG-2
 *   多项式: 0x04C11DB7, 初始值: 0xFFFFFFFF
 *   无输入/输出反转, 逐 32-bit 字处理 (小端)
 * ================================================================== */

/* 打开 CRC 外设时钟并复位计算单元 (初始化时调用一次) */
void Crc32_Init(void);

/* 复位计算单元: 每次新固件校验前调用, 从初始值重新累计 */
void Crc32_Reset(void);

/* 向计算单元送入一个 32-bit 字 (小端整字), 硬件自动迭代 */
void Crc32_Accumulate(uint32_t word);

/* 读取当前累计结果 (CRC->DR) */
uint32_t Crc32_Result(void);

#endif /* __CRC32_H */
