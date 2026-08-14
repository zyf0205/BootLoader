#include "crc32.h"

void Crc32_Init(void)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_CRC, ENABLE);
    CRC->CR = CRC_CR_RESET;
}

void Crc32_Reset(void)
{
    CRC->CR = CRC_CR_RESET;
}

uint32_t Crc32_CalcWords(const uint32_t *data, uint32_t word_cnt)
{
    for (uint32_t i = 0; i < word_cnt; i++)
    {
        CRC->DR = data[i];
    }
    return CRC->DR;
}

uint32_t Crc32_CalcBytes(const uint8_t *data, uint32_t len)
{
    uint32_t word_cnt = len / 4;
    uint32_t remain   = len % 4;

    /* 整字部分直接送入 CRC 引擎 */
    for (uint32_t i = 0; i < word_cnt; i++)
    {
        uint32_t word = ((uint32_t)data[i * 4 + 0] << 0)  |
                        ((uint32_t)data[i * 4 + 1] << 8)  |
                        ((uint32_t)data[i * 4 + 2] << 16) |
                        ((uint32_t)data[i * 4 + 3] << 24);
        CRC->DR = word;
    }

    /* 尾部不足 4 字节: 高位补 0xFF (与 Flash 擦除态一致) */
    if (remain > 0)
    {
        uint32_t word = 0xFFFFFFFF;
        for (uint32_t j = 0; j < remain; j++)
        {
            word &= ~(0xFFu << (j * 8));
            word |= (uint32_t)data[word_cnt * 4 + j] << (j * 8);
        }
        CRC->DR = word;
    }

    return CRC->DR;
}
