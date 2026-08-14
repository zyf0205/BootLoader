#include "stm32f4xx_it.h"
#include "stm32f4xx.h"

/* Cortex-M4 异常处理 (外设中断在各自驱动中定义) */

void NMI_Handler(void)
{
    while (1)
    {
    }
}

__asm void HardFault_Handler(void)
{
    IMPORT HardFault_Handler_C
    TST LR, #4
    ITE EQ
    MRSEQ R0, MSP
    MRSNE R0, PSP
    B HardFault_Handler_C
}

void HardFault_Handler_C(uint32_t *stack)
{
    NVIC_SystemReset();
    (void)stack;
}

void MemManage_Handler(void)
{
    while (1)
    {
    }
}

void BusFault_Handler(void)
{
    while (1)
    {
    }
}

void UsageFault_Handler(void)
{
    while (1)
    {
    }
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}
