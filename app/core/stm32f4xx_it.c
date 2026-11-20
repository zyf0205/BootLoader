#include "stm32f4xx_it.h"
#include "stm32f4xx.h"
#include "boot_api.h"

/* ================================================================== *
 * Cortex-M4 异常处理
 * 设计原则: 外设中断处理函数就近放在对应驱动源文件中,
 * 本文件只保留 Cortex 内核异常。
 * ================================================================== */

/* 除 HardFault 外的内核异常同样上报后复位: 停在 while(1) 会丢掉
 * 自动恢复能力, 也无法把控制权交回升级模式 */
static void FaultReset(void)
{
    Boot_ReportFaultReset();
}

void NMI_Handler(void)
{
    FaultReset();
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

/* C 入口: 经 boot_api 上报一次 Fault 复位 (BKP4R 计数 +1, 与
 * Bootloader 共享) 后复位重启。连续达到 Bootloader 上限时由
 * Bootloader 停在升级模式, 避免 Fault-复位-Fault 无限循环。
 * stack 参数仅为约定签名, 暂未使用 */
void HardFault_Handler_C(uint32_t *stack)
{
    Boot_ReportFaultReset();
    (void)stack;
}

void MemManage_Handler(void)
{
    FaultReset();
}

void BusFault_Handler(void)
{
    FaultReset();
}

void UsageFault_Handler(void)
{
    FaultReset();
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
