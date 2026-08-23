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


