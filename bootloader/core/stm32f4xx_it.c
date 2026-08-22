#include "stm32f4xx_it.h"
#include "stm32f4xx.h"
#include "boot.h"

/* ================================================================== *
 * Cortex-M4 异常处理
 *
 * 设计原则: 各外设中断处理函数就近放在对应驱动源文件中,
 * 本文件只保留 Cortex 内核异常。
 * ================================================================== */

void NMI_Handler(void)
{
    while (1)
    {
    }
}

/* HardFault: 提取栈帧后跳转 C 入口, 便于调试器查看现场 */
__asm void HardFault_Handler(void)
{
    IMPORT HardFault_Handler_C
    TST LR, #4
    ITE EQ
    MRSEQ R0, MSP
    MRSNE R0, PSP
    B HardFault_Handler_C
}

/* C 入口: 记录一次 Fault 复位后复位系统。BKP4R 计数跨复位保持,
 * main 启动时检测连续 Fault 复位达到上限则停在升级模式,
 * 避免 Fault-复位-Fault 的无限循环 */
void HardFault_Handler_C(uint32_t *stack)
{
    Boot_RecordFaultReset();
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
