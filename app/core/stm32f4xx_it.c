#include "stm32f4xx_it.h"
#include "stm32f4xx.h"

/* ================================================================== *
 * Cortex-M4 异常处理
 * 设计原则: 外设中断处理函数就近放在对应驱动源文件中,
 * 本文件只保留 Cortex 内核异常。
 * ================================================================== */

void NMI_Handler(void)
{
    while (1)
    {
    }
}

/* HardFault 汇编入口: 根据 EXC_RETURN 的 bit2 判断异常发生时
 * 用的是主栈 (MSP) 还是进程栈 (PSP), 将对应栈指针传入 R0 后
 * 跳转 C 入口, 方便调试器查看栈上现场 */
__asm void HardFault_Handler(void)
{
    IMPORT HardFault_Handler_C
    TST LR, #4
    ITE EQ
    MRSEQ R0, MSP
    MRSNE R0, PSP
    B HardFault_Handler_C
}

/* C 入口: APP 内发生致命异常直接复位重启, 从 Bootloader 重新走
 * 启动决策。stack 参数仅为约定签名, 暂未使用 */
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
