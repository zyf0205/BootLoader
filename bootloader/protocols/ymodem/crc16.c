#include "crc16.h"

/* 逐位 (bit-by-bit) 实现: 每个字节先异或到 CRC 高 8 位, 再迭代
 * 8 次 —— 最高位为 1 则左移一位并异或多项式 0x1021, 否则仅左移。
 * 数据包最多 1KB, 主循环逐位计算耗时可接受, 无需 512 字节查表。 */
uint16_t Crc16_Calc(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0x0000;

    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x8000)
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            else
                crc = (uint16_t)(crc << 1);
        }
    }

    return crc;
}
